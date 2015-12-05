/*
 * cfg_mgr_decode.c
 * 
 * Decode the serialized blob in SPI Flash and put it in the g_config struct.
 *
 *  Created on: May 8, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "cfg_mgr_priv.h"
#include "cfg_mgr.h"
#include "cfg_mgr_dynamic.pb.h"
#include "corona_ext_flash_layout.h"
#include "app_errors.h"
#include "cfg_mgr.h"
#include "ext_flash.h"
#include "persist_var.h"
#include <stdint.h>
#include <mqx.h>
#include <pb_decode.h>
#include "pb_helper.h"
#include <string.h>
#include "cormem.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

typedef struct {
        LinkKeyInfo_t *linkKeyInfo;
        uint8_t count;
        uint8_t max;
} CFGMGR_decode_LinkKeyInfo_args_t;

typedef struct {
        cfg_network_t *pCfgNetworkList;
        uint32_t *count;  // maps to g_dy_cfg.wifi_dy.num_networks 
        uint8_t max;
} CFGMGR_decode_Network_args_t;
////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

bool _CFG_dec_ntwk(pb_istream_t *stream, const pb_field_t *field, void *arg)
{
    Network network = {0};
    bool res;
    CFGMGR_decode_Network_args_t *net_args = (CFGMGR_decode_Network_args_t*) arg;
    WMP_sized_string_cb_args_t wpa_pwd_arg = {0};
    WMP_sized_string_cb_args_t wpa_ver_arg = {0};
    WMP_sized_string_cb_args_t wep_pwd_arg = {0};
    WMP_sized_string_cb_args_t ssid_arg = {0};
    uint8_t net_index = *(net_args->count);
    
    if ( net_index >= CFG_WIFI_MAX_NETWORKS)
    {
        stream->bytes_left = 0;
        return true;
    }
    
    memset(&(net_args->pCfgNetworkList[net_index]), 0 , sizeof(net_args->pCfgNetworkList[net_index]));

    wpa_pwd_arg.pPayload = net_args->pCfgNetworkList[net_index].security_parameters.wpa.passphrase;
    wpa_pwd_arg.payloadMaxSize = MAX_PASSPHRASE_SIZE + 1; // Account for null terminator
    network.security.wpaParams.passphrase.arg = &wpa_pwd_arg;
    network.security.wpaParams.passphrase.funcs.decode = decode_whistle_str_sized;

    wpa_ver_arg.pPayload = net_args->pCfgNetworkList[net_index].security_parameters.wpa.version;
    wpa_ver_arg.payloadMaxSize = WIFI_MAX_WPA_VER_SIZE;
    network.security.wpaParams.version.arg = &wpa_ver_arg;
    network.security.wpaParams.version.funcs.decode = decode_whistle_str_sized;    

    wep_pwd_arg.pPayload = net_args->pCfgNetworkList[net_index].security_parameters.wep.key0;
    wep_pwd_arg.payloadMaxSize = MAX_WEP_KEY_SIZE + 1; // Account for null terminator
    network.security.wepParams.key.arg = &wep_pwd_arg;
    network.security.wepParams.key.funcs.decode = decode_whistle_str_sized;
    
    ssid_arg.pPayload = net_args->pCfgNetworkList[net_index].ssid;
    ssid_arg.payloadMaxSize = MAX_SSID_LENGTH; // Already accounts for null terminator
    network.ssid.arg = &ssid_arg;
    network.ssid.funcs.decode = decode_whistle_str_sized;
    
    if (!pb_decode(stream, Network_fields, &network))
        return false;

    if (!((WMP_sized_string_cb_args_t*) (network.ssid.arg))->exists)
    {
        PROCESS_NINFO(ERR_CFGMGR_PROTOBUF, "ssid");
        return false;
    }

    if (!network.security.has_securityType)
    {
        PROCESS_NINFO(ERR_CFGMGR_PROTOBUF, "secty");
        return false;
    }
    
    switch (network.security.securityType)
    {
        case SEC_MODE_WPA:
            if (!network.security.has_wpaParams)
            {
                PROCESS_NINFO(ERR_CFGMGR_PROTOBUF, "wpa");
                return false;
            }
            
            if (!network.security.wpaParams.has_mcipher)
            {
                PROCESS_NINFO(ERR_CFGMGR_PROTOBUF, "mciph");
                return false;
            }
            net_args->pCfgNetworkList[net_index].security_parameters.wpa.mcipher =
                    network.security.wpaParams.mcipher;

            if (!network.security.wpaParams.has_ucipher)
            {
                PROCESS_NINFO(ERR_CFGMGR_PROTOBUF, "uciph");
                return false;
            }
            net_args->pCfgNetworkList[net_index].security_parameters.wpa.ucipher =
                    network.security.wpaParams.ucipher;

            if (!((WMP_sized_string_cb_args_t*) (network.security.wpaParams.passphrase.arg))->exists)
            {
                corona_print("WPA pwd mis'g\n");
            }
            
            if (!((WMP_sized_string_cb_args_t*) (network.security.wpaParams.version.arg))->exists)
            {
                PROCESS_NINFO(ERR_CFGMGR_PROTOBUF, "wpa ver");
                return false;
            }
            break;
        case SEC_MODE_WEP:
            if (!network.security.has_wepParams)
            {
                PROCESS_NINFO(ERR_CFGMGR_PROTOBUF, "wep");
                return false;
            }

            if (!network.security.wepParams.has_default_key_index)
            {
                PROCESS_NINFO(ERR_CFGMGR_PROTOBUF, "wep key");
                return false;
            }
            
            net_args->pCfgNetworkList[net_index].security_parameters.wep.default_key =
                    network.security.wepParams.default_key_index;
            
            break;
        case SEC_MODE_OPEN:
            break;
            
        default:
            PROCESS_NINFO(ERR_CFGMGR_PROTOBUF, "bad secTyp %u", network.security.securityType);
            return false;
    }

    net_args->pCfgNetworkList[net_index].security_type = network.security.securityType;
    
    *(net_args->count) = net_index + 1;
    
    return true;
}

bool _CFG_dec_lkey(pb_istream_t *stream, const pb_field_t *field, void *arg)
{
    LinkKeyInfo linkKeyInfo = {0};
    bool res;
    CFGMGR_decode_LinkKeyInfo_args_t *pBtCfg_args = (CFGMGR_decode_LinkKeyInfo_args_t*) arg;
    
    if (pBtCfg_args->count >= pBtCfg_args->max)
    {
        stream->bytes_left = 0;
        return true;
    }
    
    if (!pb_decode(stream, LinkKeyInfo_fields, &linkKeyInfo))
        return false;

    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].BD_ADDR.BD_ADDR0 = 
            linkKeyInfo.Addr.bytes[0];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].BD_ADDR.BD_ADDR1 = 
            linkKeyInfo.Addr.bytes[1];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].BD_ADDR.BD_ADDR2 = 
            linkKeyInfo.Addr.bytes[2];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].BD_ADDR.BD_ADDR3 = 
            linkKeyInfo.Addr.bytes[3];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].BD_ADDR.BD_ADDR4 = 
            linkKeyInfo.Addr.bytes[4];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].BD_ADDR.BD_ADDR5 = 
            linkKeyInfo.Addr.bytes[5];

    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key0 = 
            linkKeyInfo.Key.bytes[0];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key1 = 
            linkKeyInfo.Key.bytes[1];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key2 = 
            linkKeyInfo.Key.bytes[2];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key3 = 
            linkKeyInfo.Key.bytes[3];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key4 = 
            linkKeyInfo.Key.bytes[4];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key5 = 
            linkKeyInfo.Key.bytes[5];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key6 = 
            linkKeyInfo.Key.bytes[6];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key7 = 
            linkKeyInfo.Key.bytes[7];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key8 = 
            linkKeyInfo.Key.bytes[8];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key9 = 
            linkKeyInfo.Key.bytes[9];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key10 = 
            linkKeyInfo.Key.bytes[10];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key11 = 
            linkKeyInfo.Key.bytes[11];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key12 = 
            linkKeyInfo.Key.bytes[12];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key13 = 
            linkKeyInfo.Key.bytes[13];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key14 = 
            linkKeyInfo.Key.bytes[14];
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].LinkKey.Link_Key15 = 
            linkKeyInfo.Key.bytes[15];
    
    pBtCfg_args->linkKeyInfo[pBtCfg_args->count].usage_count = linkKeyInfo.usage_count;

    if (linkKeyInfo.has_pom)
    {
    	pBtCfg_args->linkKeyInfo[pBtCfg_args->count].pom = PomType_POM_SPP == linkKeyInfo.pom ? pomSPP : pomiAP;
    }
    else
    {
    	// before there was a pom, there was only iAP, and the world was empty
    	pBtCfg_args->linkKeyInfo[pBtCfg_args->count].pom = pomiAP;
    }

    pBtCfg_args->count++;
    return true;
}

/*
 *   Install top-level callbacks we use for encoding individual PB fields.
 */
static void _CFG_install_dec_cbk(ConfigDynamic *pCfg, CFGMGR_decode_LinkKeyInfo_args_t *btLinkInfo_args, CFGMGR_decode_Network_args_t *pNetworkArgs)
{   
    memset(btLinkInfo_args, 0, sizeof(*btLinkInfo_args));
    memset(pNetworkArgs, 0, sizeof(*pNetworkArgs));
    /*
     *   User Info Callbacks
     */
    //pCfg->userInfo.network.funcs.decode = &decode_networks;
    pCfg->userInfo.dogName.arg = (uint8_t*)g_dy_cfg.usr_dy.dog_name;
    pCfg->userInfo.dogName.funcs.decode = &decode_whistle_str;

    pNetworkArgs->pCfgNetworkList = (cfg_network_t*) g_dy_cfg.wifi_dy.network;
    
    // sanity check
    if (CFG_WIFI_MAX_NETWORKS < g_dy_cfg.wifi_dy.num_networks || 0 > g_dy_cfg.wifi_dy.num_networks)
    {
		PROCESS_NINFO(ERR_CFGMGR_DECODE_ERROR, "%d", g_dy_cfg.wifi_dy.num_networks);
		persist_set_untimely_crumb( BreadCrumbEvent_DYNAMIC_CFG_DEC_OUT_OF_BOUNDS );
    	g_dy_cfg.wifi_dy.num_networks = CFG_WIFI_MAX_NETWORKS;
    }
    
    pNetworkArgs->count = &(g_dy_cfg.wifi_dy.num_networks);
    *(pNetworkArgs->count) = 0;
    pCfg->userInfo.userNetwork.network.arg = (void*) pNetworkArgs;
    pCfg->userInfo.userNetwork.network.funcs.decode = _CFG_dec_ntwk;
    
    btLinkInfo_args->linkKeyInfo = (LinkKeyInfo_t*) g_dy_cfg.bt_dy.LinkKeyInfo;
    btLinkInfo_args->count = 0;
    btLinkInfo_args->max = MAX_BT_REMOTES;
    pCfg->userInfo.btCfg.LinkKeyInfo.arg = (void*) btLinkInfo_args;
    pCfg->userInfo.btCfg.LinkKeyInfo.funcs.decode = _CFG_dec_lkey;
    
    pCfg->userInfo.userNetwork.mru_ssid.arg = (uint8_t*)g_dy_cfg.wifi_dy.mru_ssid;
    pCfg->userInfo.userNetwork.mru_ssid.funcs.decode = &decode_whistle_str;

    /*
     *   Sys Info Callbacks
     */
    
}

/*
 *   Populate all sysInfo static fields.
 */
static void _CFGMGR_sysInfo_static_fields(SystemInfo *pSysInfo)
{
    if(pSysInfo->has_installFlag)
    {
        g_dy_cfg.sysinfo_dy.installFlag = pSysInfo->installFlag;
    }
    
    if(pSysInfo->has_lastFWUAttempt)
    {
        g_dy_cfg.sysinfo_dy.lastFWUAttempt = pSysInfo->lastFWUAttempt;
    }
    
    if(pSysInfo->has_deviceHasCheckedIn)
    {
        g_dy_cfg.sysinfo_dy.deviceHasCheckedIn = (uint8_t)pSysInfo->deviceHasCheckedIn;
    }
}

/*
 *   Populate all userInfo static fields.
 */
static void _CFGMGR_userInfo_static_fields(UserInfo *pUserInfo)
{
    if(pUserInfo->activityCfg.has_daily_minutes_active_goal)
    {
        g_dy_cfg.usr_dy.daily_minutes_active_goal = (uint16_t)pUserInfo->activityCfg.daily_minutes_active_goal;
    }
    
    if(pUserInfo->timeCfg.has_timezone_offset)
    {
        g_dy_cfg.usr_dy.timezone_offset = pUserInfo->timeCfg.timezone_offset;
    }
    
    if(pUserInfo->btCfg.has_mru_index)
    {
		// sanity check
    	if (MAX_BT_REMOTES <= pUserInfo->btCfg.mru_index || 0 > pUserInfo->btCfg.mru_index)
    	{
    		PROCESS_NINFO(ERR_CFGMGR_DECODE_ERROR, "%d", pUserInfo->btCfg.mru_index);
    		persist_set_untimely_crumb( BreadCrumbEvent_DYNAMIC_CFG_DEC_OUT_OF_BOUNDS );
    		pUserInfo->btCfg.mru_index = 0;
    	}

    	g_dy_cfg.bt_dy.mru_index = pUserInfo->btCfg.mru_index;
    }
}

/*
 *   Populates all of the static PB configs, based on our static struct.
 */
static void _CFGMGR_populate_static_decode_fields(ConfigDynamic *pCfg)
{
    if(pCfg->has_sysInfo)
    {
        _CFGMGR_sysInfo_static_fields(&(pCfg->sysInfo));
    }
    
    if(pCfg->has_userInfo)
    {
        _CFGMGR_userInfo_static_fields(&(pCfg->userInfo));
    }
}


/*
 *   This will decode the serialized blob into g_config struct.
 *     Avoid calling this frequently (as that shouldn't be necessary anyway except at boot).
 */
err_t _CFG_dec(void)
{
    ConfigDynamic *pWhistleCfg;
    cfg_header_t header;
    cfg_pp_state_t state;
    uint32_t ping_pong_count;
    pb_istream_t stream;
    err_t err = ERR_OK;
    uint8_t *pBuf;
    CFGMGR_decode_LinkKeyInfo_args_t linkKeyArgs = {0};
    CFGMGR_decode_Network_args_t networkArgs = {0};
    
    /*
     *   Figure out which sector we should read serialized data from.
     */
    if( ERR_OK != _CFGMGR_get_pingpong_state(&state, &ping_pong_count, &header) )
    {
        return ERR_CFGMGR_EXT_FLASH;
    }
    
    /*
     *   Make sure there is actually valid data to decode.
     */
    switch(state)
    {
        // need at least 1 valid sector here to read from.
        case CFG_PP_ONLY_SECTOR1_INVALID:
        case CFG_PP_ONLY_SECTOR2_INVALID:
        case CFG_PP_SECTOR1_NEWER:
        case CFG_PP_SECTOR2_NEWER:
            break;
        
        default:
        case CFG_PP_BOTH_SECTORS_INVALID:
            return ERR_CFGMGR_NOTHING_TO_DECODE;
    }
    
    corona_print("CFG DEC %x\n", header.this_address);
    
    pBuf = walloc( header.length );
    
    /*
     *   Read the stream from external SPI Flash storage.
     */
    wson_read(header.this_address + sizeof(header), pBuf, header.length);
    
    /*
     *   Set up the PB's to accept the stream.
     */
    stream = pb_istream_from_buffer(pBuf, header.length);
    
    pWhistleCfg = walloc( sizeof(ConfigDynamic) );
    
    _CFG_install_dec_cbk( pWhistleCfg, &linkKeyArgs, &networkArgs );
    
    /*
     *   Decode the stream.
     */
    if ( !pb_decode(&stream, ConfigDynamic_fields, pWhistleCfg) )
    {
        // Yikes, this is not good!  Erase the offending sector and drop a crumb.
        persist_set_untimely_crumb( BreadCrumbEvent_DYNAMIC_CFG_DECODE_FAILED );
        wson_erase( header.this_address );
        goto done_decoding;
    }
    
    _CFGMGR_populate_static_decode_fields( pWhistleCfg );
    
done_decoding:
    wfree(pBuf);
    wfree(pWhistleCfg);
    
    return err;
}
