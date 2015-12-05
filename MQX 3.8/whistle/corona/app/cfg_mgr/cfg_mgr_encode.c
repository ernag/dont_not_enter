/*
 * cfg_mgr_encode.c
 * 
 *  Encode all the configs into a serialized blob.
 *
 *  Created on: May 8, 2013
 *      Author: Ernie
 *      
 *      *** GENERATING a .pb file from a .proto file ***
 *      C:\Users\SpottedLabs\Documents\GitHub\corona_fw\MQX 3.8\whistle\corona\app\cfg_mgr>protoc -o cfg_mgr_dynamic.pb cfg_mgr_dynamic.proto
 *      
 *      *** GENERATING a .c/.h files with the .pb file ***
 *      C:\Program Files\protoc-2.5.0\protobuf-2.5.0\python>python "C:\Users\SpottedLabs\Documents\GitHub\corona_fw\MQX 3.8\whistle\corona\app\
 *          wmp\nanopb\generator\nanopb_generator.py" "C:\Users\SpottedLabs\Documents\GitHub\corona_fw\MQX 3.8\whistle\corona\app\cfg_mgr\cfg_mgr_dynamic.pb"
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "cfg_mgr_priv.h"
#include "cfg_mgr.h"
#include "cfg_mgr_dynamic.pb.h"
#include "corona_ext_flash_layout.h"
#include "pb_helper.h"
#include "app_errors.h"
#include "ext_flash.h"
#include "wassert.h"
#include "cormem.h"
#include "persist_var.h"
#include <stdint.h>
#include <mqx.h>
#include <pb_encode.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

// NOTE:  NEVER Change this value!
#define CFGMGR_CONSTANT_SIGNATURE   ( 0xb00face5 )   // BETA Signature is 0xb00face5, do not try and change it.

/*
 *   Serialized buffer should be much smaller than g_cfg size.
 *    We use this define to malloc enough memory for encoded data + the cfg header.
 *   TODO - Use malloc for all the strings in g_cfg, which will mean
 *          increasing the size of this define, but will save ~700 bytes of RAM.
 */
#define ENCODE_STREAM_BUF_MAX_SZ    (  (sizeof(g_dy_cfg) / 2 ) + sizeof(cfg_header_t) )

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Get a single network object ready for encoding.
 */

bool _CFG_enc_ntwk(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    Network network;
    wifi_dynamic_configs_t *pWiFiCfg = (wifi_dynamic_configs_t*) arg;
    cfg_network_t *pNetwork;
    int i;
    
    // sanity check
    if (CFG_WIFI_MAX_NETWORKS < pWiFiCfg->num_networks || 0 > pWiFiCfg->num_networks)
    {
        // Do not rely exclusively on errors in CFGMGR.  EVTMGR is usually not up yet.
		PROCESS_NINFO(ERR_CFGMGR_ENCODE_ERROR, "%d", pWiFiCfg->num_networks);
		persist_set_untimely_crumb( BreadCrumbEvent_DYNAMIC_CFG_ENC_OUT_OF_BOUNDS );
		
    	pWiFiCfg->num_networks = CFG_WIFI_MAX_NETWORKS;
    	
    	// TODO:  We might actually want to return false here to reset CFGMGR ?
    }
    
    for (i = 0; i < pWiFiCfg->num_networks; i++)
    {
        memset(&network, 0, sizeof(network));
        pNetwork = &(pWiFiCfg->network[i]);
        
        if( 0 == strlen(pNetwork->ssid) )
        {
            continue;
        }
        
        network.has_ssid = TRUE;
        network.ssid.arg = (uint8_t *)pNetwork->ssid;
        network.ssid.funcs.encode = &encode_whistle_str;

        network.has_security = TRUE;
        network.security.has_securityType = TRUE;
        network.security.securityType = pNetwork->security_type;
        
        switch( pNetwork->security_type )
        {
            case SEC_MODE_WPA:                
                network.security.has_wpaParams = TRUE;
                
                network.security.wpaParams.has_mcipher = TRUE;
                network.security.wpaParams.mcipher = pNetwork->security_parameters.wpa.mcipher;
                
                network.security.wpaParams.has_ucipher = TRUE;
                network.security.wpaParams.ucipher = pNetwork->security_parameters.wpa.ucipher;
                
                network.security.wpaParams.has_passphrase = TRUE;
                network.security.wpaParams.passphrase.arg = (uint8_t *)pNetwork->security_parameters.wpa.passphrase;
                network.security.wpaParams.passphrase.funcs.encode = &encode_whistle_str;
                
                network.security.wpaParams.has_version = TRUE;
                network.security.wpaParams.version.arg = (uint8_t *)pNetwork->security_parameters.wpa.version;
                network.security.wpaParams.version.funcs.encode = &encode_whistle_str;
                break;
                
            case SEC_MODE_WEP:
                network.security.has_wepParams = TRUE;
                network.security.wepParams.has_default_key_index = TRUE;
                network.security.wepParams.default_key_index = pNetwork->security_parameters.wep.default_key;

                network.security.wepParams.has_key = TRUE;
                network.security.wepParams.key.arg = pNetwork->security_parameters.wep.key0;
                network.security.wepParams.key.funcs.encode = &encode_whistle_str;
                break;
                
            case SEC_MODE_OPEN:
                break;
        }
        
        if (!pb_encode_tag_for_field(stream, field))
            return false;
        
        if (!pb_encode_submessage(stream, Network_fields, &network))
            return false;
    }
    
    return true;
}

bool _CFG_en_lKey(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    LinkKeyInfo PBLinkInfo;
    LinkKeyInfo_t *pMemLinkKeyInfo = (LinkKeyInfo_t*) arg;
    int i;
    
    for (i=0; i < MAX_BT_REMOTES; i++)
    {
        memset(&PBLinkInfo, 0, sizeof(PBLinkInfo));
        
        // Skip entries that have a BD ADDR of 0000000
        if (0 == pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR0 && 
                0 == pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR1 && 
                0 == pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR2 && 
                0 == pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR3 && 
                0 == pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR4 && 
                0 == pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR5)
            continue;

        PBLinkInfo.has_Addr = TRUE;
        PBLinkInfo.Addr.size = 6;
        PBLinkInfo.Addr.bytes[0] = pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR0;
        PBLinkInfo.Addr.bytes[1] = pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR1;
        PBLinkInfo.Addr.bytes[2] = pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR2;
        PBLinkInfo.Addr.bytes[3] = pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR3;
        PBLinkInfo.Addr.bytes[4] = pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR4;
        PBLinkInfo.Addr.bytes[5] = pMemLinkKeyInfo[i].BD_ADDR.BD_ADDR5;
        
        PBLinkInfo.has_Key = TRUE;
        PBLinkInfo.Key.size = 16;
        PBLinkInfo.Key.bytes[0] = pMemLinkKeyInfo[i].LinkKey.Link_Key0;
        PBLinkInfo.Key.bytes[1] = pMemLinkKeyInfo[i].LinkKey.Link_Key1;
        PBLinkInfo.Key.bytes[2] = pMemLinkKeyInfo[i].LinkKey.Link_Key2;
        PBLinkInfo.Key.bytes[3] = pMemLinkKeyInfo[i].LinkKey.Link_Key3;
        PBLinkInfo.Key.bytes[4] = pMemLinkKeyInfo[i].LinkKey.Link_Key4;
        PBLinkInfo.Key.bytes[5] = pMemLinkKeyInfo[i].LinkKey.Link_Key5;
        PBLinkInfo.Key.bytes[6] = pMemLinkKeyInfo[i].LinkKey.Link_Key6;
        PBLinkInfo.Key.bytes[7] = pMemLinkKeyInfo[i].LinkKey.Link_Key7;
        PBLinkInfo.Key.bytes[8] = pMemLinkKeyInfo[i].LinkKey.Link_Key8;
        PBLinkInfo.Key.bytes[9] = pMemLinkKeyInfo[i].LinkKey.Link_Key9;
        PBLinkInfo.Key.bytes[10] = pMemLinkKeyInfo[i].LinkKey.Link_Key10;
        PBLinkInfo.Key.bytes[11] = pMemLinkKeyInfo[i].LinkKey.Link_Key11;
        PBLinkInfo.Key.bytes[12] = pMemLinkKeyInfo[i].LinkKey.Link_Key12;
        PBLinkInfo.Key.bytes[13] = pMemLinkKeyInfo[i].LinkKey.Link_Key13;
        PBLinkInfo.Key.bytes[14] = pMemLinkKeyInfo[i].LinkKey.Link_Key14;
        PBLinkInfo.Key.bytes[15] = pMemLinkKeyInfo[i].LinkKey.Link_Key15;
        
        PBLinkInfo.has_usage_count = TRUE;
        PBLinkInfo.usage_count = pMemLinkKeyInfo[i].usage_count;
        
        PBLinkInfo.has_pom = TRUE;
        PBLinkInfo.pom = pomSPP == pMemLinkKeyInfo[i].pom ? PomType_POM_SPP : PomType_POM_IAP;
        
        if (!pb_encode_tag_for_field(stream, field))
            return false;
        
        if (!pb_encode_submessage(stream, LinkKeyInfo_fields, &PBLinkInfo))
            return false;
    }
    
    return true;
}

/*
 *   Install all of the callbacks we use for encoding individual PB fields.
 */

static void _CFG_install_en_cbks(ConfigDynamic *pCfg)
{
    /*
     *   Sys Info Callbacks
     */
    
    
    /*
     *   User Info Callbacks
     */
    //pCfg->userInfo.network.funcs.encode = &encode_networks;  // back when this was repeated.
    pCfg->userInfo.dogName.funcs.encode = &encode_whistle_str;
    pCfg->userInfo.dogName.arg = (uint8_t*)g_dy_cfg.usr_dy.dog_name;

    pCfg->userInfo.userNetwork.network.arg = (void *) &(g_dy_cfg.wifi_dy);
    pCfg->userInfo.userNetwork.network.funcs.encode = _CFG_enc_ntwk;
    
    pCfg->userInfo.btCfg.LinkKeyInfo.arg = (void*) g_dy_cfg.bt_dy.LinkKeyInfo;
    pCfg->userInfo.btCfg.LinkKeyInfo.funcs.encode = _CFG_en_lKey;
    
    pCfg->userInfo.userNetwork.mru_ssid.funcs.encode = &encode_whistle_str;
    pCfg->userInfo.userNetwork.mru_ssid.arg = (uint8_t*)g_dy_cfg.wifi_dy.mru_ssid;
}

/*
 *   Tell PB which optional top-level configs are present for this encoding.
 */
static void _CFGMGR_optional_declarations(ConfigDynamic *pCfg)
{
    /*
     *   Top-Level Objects
     */
    pCfg->has_sysInfo = true;
    pCfg->has_userInfo = true;
    
    /*
     *   System Info Objects
     */
    pCfg->sysInfo.has_installFlag = true;
    pCfg->sysInfo.has_lastFWUAttempt = true;
    pCfg->sysInfo.has_deviceHasCheckedIn = true;
    
    /*
     *   User Info Objects
     */
    pCfg->userInfo.has_btCfg = true;
    pCfg->userInfo.has_dogName = true;
    pCfg->userInfo.has_userNetwork = true;
    pCfg->userInfo.has_activityCfg = true;
    pCfg->userInfo.has_timeCfg = true;
    
    /*
     *   Activity
     */
    pCfg->userInfo.activityCfg.has_daily_minutes_active_goal = true;
    
    /*
     *   Time
     */
    pCfg->userInfo.timeCfg.has_timezone_offset = true;
    
    
    /*
     * Network
     */
    pCfg->userInfo.userNetwork.has_mru_ssid = true;
    
    /*
     * BT
     */
    pCfg->userInfo.btCfg.has_mru_index = true;
}

/*
 *   Populates all of the static PB configs, based on our static struct.
 */
static void _CFGMGR_populate_static_encode_fields(ConfigDynamic *pCfg)
{
    /*
     *   User Info
     */
    pCfg->userInfo.activityCfg.daily_minutes_active_goal = (uint32_t)g_dy_cfg.usr_dy.daily_minutes_active_goal;
    pCfg->userInfo.timeCfg.timezone_offset = g_dy_cfg.usr_dy.timezone_offset;
    
    /*
     * BT
     */
	// sanity check
	if (MAX_BT_REMOTES <= g_dy_cfg.bt_dy.mru_index || 0 > g_dy_cfg.bt_dy.mru_index)
	{
		PROCESS_NINFO(ERR_CFGMGR_ENCODE_ERROR, "%d", g_dy_cfg.bt_dy.mru_index);
		persist_set_untimely_crumb( BreadCrumbEvent_DYNAMIC_CFG_ENC_OUT_OF_BOUNDS );
		g_dy_cfg.bt_dy.mru_index = 0;
	}

    pCfg->userInfo.btCfg.mru_index = g_dy_cfg.bt_dy.mru_index;
    
    /*
     *   System Info
     */
    pCfg->sysInfo.installFlag = g_dy_cfg.sysinfo_dy.installFlag;
    pCfg->sysInfo.lastFWUAttempt = g_dy_cfg.sysinfo_dy.lastFWUAttempt;
    pCfg->sysInfo.deviceHasCheckedIn = (uint32_t)g_dy_cfg.sysinfo_dy.deviceHasCheckedIn;
    
    /*
     *   OTP
     */
}

/*
 *   Return state of the ping pong buffer for CFGMGR.
 */
err_t _CFGMGR_get_pingpong_state( cfg_pp_state_t *pState, 
                                  uint32_t *pNewPingPongCount,
                                  cfg_header_t *pNewestSectorHeader )
{
    cfg_header_t sector1;
    cfg_header_t sector2;
    
    *pNewPingPongCount = 0;
    
    wson_read(EXT_FLASH_CFG_SERIALIZED1_ADDR, (uint8_t *)&sector1, sizeof(cfg_header_t));
    wson_read(EXT_FLASH_CFG_SERIALIZED2_ADDR, (uint8_t *)&sector2, sizeof(cfg_header_t));
    
    // Case 1:  Both headers are invalid.
    if( (sector1.signature != CFGMGR_CONSTANT_SIGNATURE) && ((sector2.signature != CFGMGR_CONSTANT_SIGNATURE)) )
    {
        *pState = CFG_PP_BOTH_SECTORS_INVALID;
        *pNewPingPongCount = 0;
    }
    
    // Case 2: Header 1 is valid.  Header 2 is invalid.  In this case use sector 2.
    else if( (sector1.signature == CFGMGR_CONSTANT_SIGNATURE) && ((sector2.signature != CFGMGR_CONSTANT_SIGNATURE)) )
    {
        *pState = CFG_PP_ONLY_SECTOR2_INVALID;
        *pNewPingPongCount = sector1.ping_pong_count + 1;
        memcpy(pNewestSectorHeader, &sector1, sizeof(cfg_header_t));
    }
    
    // Case 3: Header 1 is invalid.  Header 2 is valid.  In this case use sector 21
    else if( (sector1.signature != CFGMGR_CONSTANT_SIGNATURE) && ((sector2.signature == CFGMGR_CONSTANT_SIGNATURE)) )
    {
        *pState = CFG_PP_ONLY_SECTOR1_INVALID;
        *pNewPingPongCount = sector2.ping_pong_count + 1;
        memcpy(pNewestSectorHeader, &sector2, sizeof(cfg_header_t));
    }
    
    // Case 4: Both headers are valid.  In this case use sector with lower ping_pong_count.
    else if( (sector1.signature == CFGMGR_CONSTANT_SIGNATURE) && ((sector2.signature == CFGMGR_CONSTANT_SIGNATURE)) )
    {
        if(sector1.ping_pong_count > sector2.ping_pong_count)
        {
            // Sector 1 is newer, so use Sector 2 now and leave Sector 1 intact.
            *pState = CFG_PP_SECTOR1_NEWER;
            *pNewPingPongCount = sector1.ping_pong_count + 1;
            memcpy(pNewestSectorHeader, &sector1, sizeof(cfg_header_t));
        }
        else
        {
            // Sector 2 is newer, so use Sector 1 now and leave Sector 2 intact.
            *pState = CFG_PP_SECTOR2_NEWER;
            *pNewPingPongCount = sector2.ping_pong_count + 1;
            memcpy(pNewestSectorHeader, &sector2, sizeof(cfg_header_t));
        }
    }
    
    return ERR_OK;
}


/*
 *   Returns the header we need to write prior to writing encoded data.
 */
err_t _CFGMGR_prep_header_for_encoding(cfg_header_t *pHeader, uint32_t data_length)
{
    cfg_pp_state_t state;
    uint32_t ping_pong_count;
    
    if( ERR_OK != _CFGMGR_get_pingpong_state(&state, &ping_pong_count, pHeader) )
    {
        return ERR_CFGMGR_EXT_FLASH;
    }
    
    memset(pHeader, 0, sizeof(cfg_header_t));  // we don't care about pHeader info from above, so OK to blast.
    pHeader->length = data_length;
    pHeader->signature = CFGMGR_CONSTANT_SIGNATURE;
    pHeader->ping_pong_count = ping_pong_count;
    
    switch(state)
    {
        // Case 1:  Both headers are invalid.  In this case use sector 1.
        case CFG_PP_BOTH_SECTORS_INVALID:
            pHeader->this_address = EXT_FLASH_CFG_SERIALIZED1_ADDR;
            break;
            
        // Case 2: Header 1 is valid.  Header 2 is invalid.  In this case use sector 2.
        case CFG_PP_ONLY_SECTOR2_INVALID:
            pHeader->this_address = EXT_FLASH_CFG_SERIALIZED2_ADDR;
            break;
            
        // Case 3: Header 1 is invalid.  Header 2 is valid.  In this case use sector 21
        case CFG_PP_ONLY_SECTOR1_INVALID:
            pHeader->this_address = EXT_FLASH_CFG_SERIALIZED1_ADDR;
            break;
            
        // Case 4:  // Sector 1 is newer, so use Sector 2 now and leave Sector 1 intact.
        case CFG_PP_SECTOR1_NEWER:
            pHeader->this_address = EXT_FLASH_CFG_SERIALIZED2_ADDR;
            break;
            
        // Case 5:  // Sector 2 is newer, so use Sector 1 now and leave Sector 2 intact.
        case CFG_PP_SECTOR2_NEWER:
            pHeader->this_address = EXT_FLASH_CFG_SERIALIZED1_ADDR;
            break;
            
        default:
            wassert(0);
    }
    
    return ERR_OK;
}

/*
 *   This will encode all of the configs into a serialized blob,
 *      and flush them to SPI Flash.  Do not call this frequently!
 */
err_t _CFG_enc(void)
{
    ConfigDynamic *pWhistleCfg;
    pb_ostream_t stream = {0, 0, 0, 0};
    size_t serializedSize = 0;
    cfg_header_t header;
    uint8_t *pBuf;
    err_t err = ERR_OK;
    
    wassert( CFGMGR_CONSTANT_SIGNATURE == 0xb00face5 );   // assures no rascals try and change this value !
    
    pWhistleCfg = walloc(sizeof(ConfigDynamic) );
    
    _CFGMGR_optional_declarations(pWhistleCfg);
    _CFG_install_en_cbks(pWhistleCfg);
    _CFGMGR_populate_static_encode_fields(pWhistleCfg);
    
    /*
     * Determine serialized string size by encoding to NULL Buf
     */
    pb_encode(&stream, ConfigDynamic_fields, pWhistleCfg);
    serializedSize = stream.bytes_written;
    
    /*
     * Allocate space and setup serialize stream
     */
    pBuf = walloc( sizeof(cfg_header_t) + serializedSize );  // serialized data is guaranteed to be smaller than un-serialized configs.
    
    pBuf += sizeof(cfg_header_t);  // skip past designated header section, to put serialized data.
    
    stream = pb_ostream_from_buffer( pBuf, serializedSize );
    
    /*
     * Serialize into the buffer
     */
    
    if( !pb_encode(&stream, ConfigDynamic_fields, pWhistleCfg) )
    {
        err = PROCESS_NINFO(ERR_CFGMGR_ENCODE_FAILED, NULL);
        persist_set_untimely_crumb( BreadCrumbEvent_DYNAMIC_CFG_ENCODE_FAILED );
        
        pBuf -= sizeof(cfg_header_t);  // backup to original address, so we can free.
        goto done_encoding;
    }
    
    /*
     *   We only have 1 x 4KB sector for serialized config, so make sure we aren't writing more.
     */
    wassert( (stream.bytes_written + sizeof(cfg_header_t)) <= EXT_FLASH_4KB_SZ);
    
    /*
     *   Check to make sure we didn't overflow the buffer we allocated.
     */
    wassert(stream.bytes_written <= serializedSize);
    
    /*
     *   Create a new header for this encoded data (this will use ping pong address).
     */
    wassert( ERR_OK == _CFGMGR_prep_header_for_encoding(&header, stream.bytes_written) );
    
    corona_print("CFG Enc %x\n\tsz g_dy %i\n\tsz enc buf %i\n\twritten %i\n\n", 
                   header.this_address,
                   sizeof(g_dy_cfg),
                   serializedSize,
                   stream.bytes_written); 
    
    pBuf -= sizeof(cfg_header_t); // back up to original address, so we can copy config header, and flush.
    memcpy(pBuf, &header, sizeof(cfg_header_t));

    /*
     *   Flush latest serialized config data to SPI Flash.
     */
    wson_write(header.this_address, pBuf, (sizeof(cfg_header_t) + header.length), 1);
    
done_encoding:
    wfree(pBuf);
    wfree(pWhistleCfg);
    
    return err;
}
