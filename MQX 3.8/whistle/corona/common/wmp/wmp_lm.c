/*
 * wmp_lm.c
 *
 *  Created on: Jun 14, 2013
 *      Author: SpottedLabs
 */


#include "wmp_lm.h"
#include "pb.h"
#include "pb_helper.h"
#include "pb_encode.h"
#include "pb_decode.h"
#include "whistlemessage.pb.h"
#include "cfg_mgr.h"
#include "wifi_mgr.h"
#include "wassert.h"
#include "pwr_mgr.h"
#include <mqx.h>

typedef struct _msg_field_lookup {
    WMP_generic_enum message_type;
    const pb_field_t *pb_field;
} msg_field_lookup_t;


typedef struct _encode_lm_scan_result_arg_t
{
    ATH_GET_SCAN *scan_results;
    size_t start_index;
    size_t num_results_to_send;        
} encode_lm_scan_result_arg_t;

typedef struct _encode_lm_list_result_arg_t
{
    size_t start_index;
    size_t num_results_to_send;        
} encode_lm_list_result_arg_t;

static const msg_field_lookup_t lmMsgFieldLookupTable[] = 
{
        {LmMessageType_LM_MOBILE_STAT_REQ, LmMobileStatRequest_fields},
        {LmMessageType_LM_MOBILE_STAT_RESP, LmMobileStat_fields},
        {LmMessageType_LM_MOBILE_STAT_NOTIFY, LmMobileStat_fields},
        {LmMessageType_LM_DEV_STAT_NOTIFY, LmDevStat_fields},
        {LmMessageType_LM_WIFI_SCAN_RESP, LmWiFiScanResponse_fields},
        {LmMessageType_LM_WIFI_TEST_RESP, LmWiFiTestResponse_fields},
        {LmMessageType_LM_WIFI_ADD_RESP, LmWiFiAddResponse_fields},
        {LmMessageType_LM_WIFI_REM_RESP, LmWiFiRemResponse_fields},
        {LmMessageType_LM_WIFI_LIST_RESP, LmWiFiListResponse_fields},
};

static const pb_field_t* wmp_lm_get_fields(LmMessageType lmMsgType)
{
    int i;
    for (i = 0; i < sizeof(lmMsgFieldLookupTable); i++)
    {
        if (lmMsgFieldLookupTable[i].message_type == lmMsgType)
        {
            return lmMsgFieldLookupTable[i].pb_field;
        }
    }
    return NULL;
}

static const char *NO_SER_WARNING = "WARN no WMP ser#\n";
static const char *LM_FAIL = "FAIL:";
static const char *LM_FAIL_WMP = "FAIL WMP";

extern char g_wifi_mac[];
extern char g_wifi_ver[];


/*
 * GENERATOR CBs
 */

bool WMP_lm_scan_results_to_network_list(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    encode_lm_scan_result_arg_t *scanArgs = (encode_lm_scan_result_arg_t *) arg;
    size_t i;
    LmWiFiNetwork wifiNetwork;
    security_parameters_t security_parameters;
    
    for (i = scanArgs->start_index; 
         i < (scanArgs->start_index + scanArgs->num_results_to_send) && 
                 i < scanArgs->scan_results->num_entries;
         i++)
    {
        char temp_ssid[MAX_SSID_LENGTH];
        
        memset(&wifiNetwork, 0, sizeof(wifiNetwork));
        memset(&security_parameters, 0, sizeof(security_parameters));
        
        WIFIMGR_ATH_GET_SCAN_to_security_param(&(scanArgs->scan_results->scan_list[i]), 
        		NULL, NULL, &security_parameters);
        
        if (scanArgs->scan_results->scan_list[i].ssid_len < 0 || scanArgs->scan_results->scan_list[i].ssid_len > MAX_SSID_LENGTH - 1)
        {
        	continue;
        }
        
    	memcpy(temp_ssid, scanArgs->scan_results->scan_list[i].ssid, scanArgs->scan_results->scan_list[i].ssid_len);
    	temp_ssid[scanArgs->scan_results->scan_list[i].ssid_len] = 0;

        WIFIMGR_security_param_to_lmm_network_encode(temp_ssid, &security_parameters, &wifiNetwork);
        
        if (!pb_encode_tag_for_field(stream, field))
            return false;
        
        if (!pb_encode_submessage(stream, LmWiFiNetwork_fields, &wifiNetwork))
            return false;
    }
    return true;
}

bool WMP_lm_wifi_cfg_to_network_list(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    encode_lm_list_result_arg_t *listArgs = (encode_lm_list_result_arg_t *) arg;
    size_t i;
    LmWiFiNetwork wifiNetwork;
    
    for (i = listArgs->start_index; 
         i < (listArgs->start_index + listArgs->num_results_to_send) && 
                 i < g_dy_cfg.wifi_dy.num_networks;
         i++)
    {
        memset(&wifiNetwork, 0, sizeof(wifiNetwork));
        WMP_cfg_network_type_to_lmm_network_encode(&(g_dy_cfg.wifi_dy.network[i]),
                &wifiNetwork);
        
        if (!pb_encode_tag_for_field(stream, field))
            return false;
        
        if (!pb_encode_submessage(stream, LmWiFiNetwork_fields, &wifiNetwork))
            return false;
    }
    return true;
}

/*
 * GENERATORS
 */

boolean _WMP_lm_setup_req(pb_ostream_t *encode_stream, LmMessageType lmMsgType, void* lm_msg_obj)
{
    boolean ret;
    
    WhistleMessage *msg;
    encode_submsg_args_t lmArgs = {0};
    
    WMP_whistle_msg_alloc(&msg);
    
    msg->has_objectType = true;
    msg->objectType = WhistleMessageType_LOCAL_DEV_MGMT;
    msg->has_transactionType = true;
    msg->transactionType = TransactionType_TRANS_REQUEST;
    
    msg->serialNumber.arg = g_st_cfg.fac_st.serial_number;
    msg->serialNumber.funcs.encode = encode_whistle_str;

    msg->has_localMgmtMsg = true;
    msg->localMgmtMsg.has_messageType = true;
    msg->localMgmtMsg.messageType = lmMsgType;
    
    lmArgs.pb_obj = lm_msg_obj;
    lmArgs.fields = wmp_lm_get_fields(lmMsgType);
    
    msg->localMgmtMsg.payload.arg = &lmArgs;
    msg->localMgmtMsg.payload.funcs.encode = encode_submsg_with_tag;
    
    ret = pb_encode(encode_stream, WhistleMessage_fields, msg);
    WMP_whistle_msg_free(&msg);
    return ret;
}

size_t WMP_lm_checkin_req_gen(uint8_t *pEncodeBuffer, size_t bufferSize, int key)
{
    LmMobileStatRequest lmMobStatReq = {0};
    pb_ostream_t encode_stream = {0,0,0,0};
    bool res;

    if (NULL != pEncodeBuffer)
    {
        encode_stream = pb_ostream_from_buffer(pEncodeBuffer, bufferSize);
    }
    
    lmMobStatReq.has_initType = true;
    lmMobStatReq.initType = LmInitType_LM_INIT_MANUAL;
    
    lmMobStatReq.has_mobileBTMAC = true;
    memcpy( &lmMobStatReq.mobileBTMAC, &(g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR), sizeof(g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR) );
    
    lmMobStatReq.has_batteryLevel = true;
    lmMobStatReq.batteryLevel = PWRMGR_get_batt_percent();
    
    if( g_wifi_mac[0] )
    {
        lmMobStatReq.has_wifiMac = true;
        lmMobStatReq.wifiMac.arg = g_wifi_mac;
        lmMobStatReq.wifiMac.funcs.encode = encode_whistle_str;
    }
    
    if( g_wifi_ver[0] )
    {
        lmMobStatReq.has_wifiVer = true;
        lmMobStatReq.wifiVer.arg = g_wifi_ver;
        lmMobStatReq.wifiVer.funcs.encode = encode_whistle_str;
    }

    wassert(_WMP_lm_setup_req(&encode_stream, LmMessageType_LM_MOBILE_STAT_REQ, (void*) &lmMobStatReq));
    
    return encode_stream.bytes_written;
}


size_t WMP_lm_dev_stat_notify_gen(uint8_t *pEncodeBuffer, size_t buffer_size, LmDevStatus status)
{
    LmDevStat lmDevStat = {0};
    pb_ostream_t encode_stream = {0,0,0,0};
    bool res;
    
    if (NULL != pEncodeBuffer)
    {
        encode_stream = pb_ostream_from_buffer(pEncodeBuffer, buffer_size);
    }

    lmDevStat.has_status = true;
    lmDevStat.status = status;
    
    wassert(_WMP_lm_setup_req(&encode_stream, LmMessageType_LM_DEV_STAT_NOTIFY, (void*) &lmDevStat));

    return encode_stream.bytes_written;
}

size_t WMP_lm_scan_result_gen(uint8_t *pEncodeBuffer, size_t buffer_size, ATH_GET_SCAN *scan_results, size_t start_index, size_t num_results_to_send)
{
    LmWiFiScanResponse lmWifiScanResp = {0};
    encode_lm_scan_result_arg_t scanArgs = {0};
    pb_ostream_t encode_stream = {0,0,0,0};
    bool res;

    if (pEncodeBuffer != NULL){
        encode_stream = pb_ostream_from_buffer(pEncodeBuffer, buffer_size);
    }

    scanArgs.start_index = start_index;
    scanArgs.num_results_to_send = num_results_to_send;
    scanArgs.scan_results = scan_results;
    lmWifiScanResp.network.arg = &scanArgs;
    lmWifiScanResp.network.funcs.encode = WMP_lm_scan_results_to_network_list;
    
    wassert(_WMP_lm_setup_req(&encode_stream, LmMessageType_LM_WIFI_SCAN_RESP, (void*) &lmWifiScanResp));

    return encode_stream.bytes_written;
}

size_t WMP_lm_list_result_gen(uint8_t *pEncodeBuffer, size_t buffer_size, size_t start_index, size_t num_results_to_send)
{
    LmWiFiListResponse lmWifiListResp = {0};
    encode_lm_list_result_arg_t listArgs = {0};
    pb_ostream_t encode_stream = {0,0,0,0};
    bool res;

    if (pEncodeBuffer != NULL)
    {
        encode_stream = pb_ostream_from_buffer(pEncodeBuffer, buffer_size);
    }

    listArgs.start_index = start_index;
    listArgs.num_results_to_send = num_results_to_send;
    lmWifiListResp.network.arg = &listArgs;
    lmWifiListResp.network.funcs.encode = WMP_lm_wifi_cfg_to_network_list;
    
    wassert(_WMP_lm_setup_req(&encode_stream, LmMessageType_LM_WIFI_LIST_RESP, (void*) &lmWifiListResp));

    return encode_stream.bytes_written;
}

size_t WMP_lm_test_result_gen(uint8_t *pEncodeBuffer, 
                    size_t buffer_size, LmWiFiTestStatus testResult,
                    char *ssid, security_parameters_t *security_param)
{
    LmWiFiTestResponse lmWiFiTestResp = {0};
    pb_ostream_t encode_stream = {0,0,0,0};
    bool res;

    if (NULL != pEncodeBuffer)
    {
        encode_stream = pb_ostream_from_buffer(pEncodeBuffer, buffer_size);
    }

    lmWiFiTestResp.has_status = true;
    lmWiFiTestResp.status = testResult;
    
    lmWiFiTestResp.has_network = true;
    if (ERR_OK != WIFIMGR_security_param_to_lmm_network_encode(ssid, 
            security_param, 
            &(lmWiFiTestResp.network)))
        return -1;
    
    wassert(_WMP_lm_setup_req(&encode_stream, LmMessageType_LM_WIFI_TEST_RESP, (void*) &lmWiFiTestResp));

    return encode_stream.bytes_written;
}

size_t WMP_lm_add_resp_gen(uint8_t *pEncodeBuffer, 
                    size_t buffer_size, LmWiFiAddStatus addResult,
                    char *ssid, security_parameters_t *security_param)
{
    LmWiFiAddResponse lmWiFiAddResp = {0};
    pb_ostream_t encode_stream = {0,0,0,0};
    bool res;

    if (NULL != pEncodeBuffer)
    {
        encode_stream = pb_ostream_from_buffer(pEncodeBuffer, buffer_size);
    }

    lmWiFiAddResp.has_status = true;
    lmWiFiAddResp.status = addResult;
    
    lmWiFiAddResp.has_network = true;
    if (ERR_OK != WIFIMGR_security_param_to_lmm_network_encode(ssid, 
            security_param, 
            &(lmWiFiAddResp.network)))
        return -1;
    
    wassert(_WMP_lm_setup_req(&encode_stream, LmMessageType_LM_WIFI_ADD_RESP, (void*) &lmWiFiAddResp));

    return encode_stream.bytes_written;
}

size_t WMP_lm_rem_resp_gen(uint8_t *pEncodeBuffer, 
                    size_t buffer_size, LmWiFiRemStatus result,
                    char *ssid)
{
    LmWiFiRemResponse lmWiFiRemResp = {0};
    pb_ostream_t encode_stream = {0,0,0,0};
    bool res;

    if (NULL != pEncodeBuffer)
    {
        encode_stream = pb_ostream_from_buffer(pEncodeBuffer, buffer_size);
    }

    lmWiFiRemResp.has_status = true;
    lmWiFiRemResp.status = result;
    
    lmWiFiRemResp.has_ssid = true;
    lmWiFiRemResp.ssid.arg = ssid;
    lmWiFiRemResp.ssid.funcs.encode = encode_whistle_str;
    
    wassert(_WMP_lm_setup_req(&encode_stream, LmMessageType_LM_WIFI_REM_RESP, (void*) &lmWiFiRemResp));

    return encode_stream.bytes_written;
}

/*
 * RECEIVERS
 */

uint8_t WMP_lm_msg_validate(WhistleMessage *msg)
{
    uint8_t res = 0;
    if (!msg->has_serialNumber)
    {
        corona_print(NO_SER_WARNING);
    }

    if (!msg->has_objectType)
    {
        corona_print("%s !OT\n", LM_FAIL_WMP);
        res -= 1;
    }
    else if (msg->objectType != WhistleMessageType_LOCAL_DEV_MGMT)
    {
        corona_print("%s OT !LDM\n", LM_FAIL_WMP);
        res -= 1;
    }
    
    if (!msg->has_transactionType)
    {
        corona_print("%s !TT\n", LM_FAIL_WMP);
        res -= 1;
    }
    
    if (!msg->localMgmtMsg.has_messageType)
    {
        corona_print("%s !LM MT\n", LM_FAIL);
        res -= 1;
    }
    
    return res;
}

/*
 * Function: WMP_lm_receive_deferred_generic
 * 
 * Purpose:
 *   This function parses the high level WMP however leaves the LM message
 *   to be decoded later
 */
LmMessageType WMP_lm_receive_deferred_generic(uint8_t *pSerializedMsg, size_t msgSize, void **pSubMsgBuf, size_t *pSubMsgSize)
{
    WhistleMessage *msg;
    int res=0;
    pb_istream_t in_stream;
    decode_deferred_submsg_args_t subMsgArgs = {0};

    *pSubMsgBuf = NULL;
    *pSubMsgSize = 0;
    
    WMP_whistle_msg_alloc(&msg);
    
    msg->localMgmtMsg.payload.arg = &subMsgArgs;
    msg->localMgmtMsg.payload.funcs.decode = decode_whistle_deferred_detached_submsg;
        
    /* Construct a pb_istream_t for reading from the buffer */
    in_stream = pb_istream_from_buffer(pSerializedMsg, msgSize);
    
    if(!pb_decode(&in_stream, WhistleMessage_fields, msg))
    {
        WPRINT_ERR("%s dec", LM_FAIL);
        WMP_whistle_msg_free(&msg);
        return -1;
    }
    
    res = WMP_lm_msg_validate(msg);
    
    if (res < 0 )
    {
        WMP_whistle_msg_free(&msg);
        return res;
    }

    WMP_whistle_msg_free(&msg);

    *pSubMsgBuf = subMsgArgs.pMsg;
    *pSubMsgSize = subMsgArgs.msg_size;

    return msg->localMgmtMsg.messageType;
}

int WMP_lm_receive_explicit(uint8_t *pSerializedMsg, size_t msgSize, void *pLmMsg, LmMessageType lmMsgType)
{
    WhistleMessage *msg;
    int res=0;
    pb_istream_t in_stream;
    decode_explicit_submsg_args_t subMsgArgs = {0};

    WMP_whistle_msg_alloc(&msg);

    msg->localMgmtMsg.payload.arg = &subMsgArgs;
    msg->localMgmtMsg.payload.funcs.decode = decode_whistle_explicit_detached_submsg_parse;

    subMsgArgs.desiredMsgType = (WMP_generic_enum) lmMsgType;
    subMsgArgs.pActualMsgType = (WMP_generic_enum *) &(msg->localMgmtMsg.messageType);
    subMsgArgs.fields = wmp_lm_get_fields(lmMsgType);
    subMsgArgs.pb_obj = pLmMsg;
    
    if (subMsgArgs.fields == 0)
    {
        WMP_whistle_msg_free(&msg);
        WPRINT_ERR("WMP rx map2", LM_FAIL_WMP);
        return -1;
    }

    /* Construct a pb_istream_t for reading from the buffer */
    in_stream = pb_istream_from_buffer(pSerializedMsg, msgSize);
    
    if(!pb_decode(&in_stream, WhistleMessage_fields, msg))
    {
        WPRINT_ERR("%s dec", LM_FAIL);
        WMP_whistle_msg_free(&msg);
        return -1;
    }
    
    res = WMP_lm_msg_validate(msg);

    if (*(subMsgArgs.pActualMsgType) != subMsgArgs.desiredMsgType)
    {
        WPRINT_ERR("%s bad sub", LM_FAIL_WMP);
        res -= 1;
    } 
    
    if (res < 0 )
    {
        WMP_whistle_msg_free(&msg);
        return res;
    }
    
    switch (msg->localMgmtMsg.messageType)
    {
        case LmMessageType_LM_MOBILE_STAT_RESP:
            if (msg->localMgmtMsg.messageType != LmMessageType_LM_MOBILE_STAT_RESP)
            {
                corona_print("%s LM MT !ChkInRsp\n", LM_FAIL);
                res -= 1;
            }
            else if (!((LmMobileStat*) pLmMsg)->has_status)
            {
                corona_print("%s LMChkInRsp !stat\n", LM_FAIL);
                res -= 1;
            }
            break;
        default:
            corona_print("%s bad msg type\n", LM_FAIL);
            res -= 1;
    }

    WMP_whistle_msg_free(&msg);
    return res;
}

int WMP_lm_checkin_resp_parse(uint8_t *pSerializedMsg, size_t msgSize, LmMobileStat *plmMobileStat)
{
    return WMP_lm_receive_explicit(pSerializedMsg, msgSize, plmMobileStat, LmMessageType_LM_MOBILE_STAT_RESP);
}

/*
 * Convinience functions
 */

err_t WMP_cfg_network_type_to_lmm_network_encode(cfg_network_t *pCfgNetwork, LmWiFiNetwork *pOutLmmNetwork)
{
    if (NULL != pCfgNetwork->ssid)
    {
        pOutLmmNetwork->ssid.arg = (void *) pCfgNetwork->ssid;
        pOutLmmNetwork->ssid.funcs.encode = encode_whistle_str;
    }
    else
        return ERR_GENERIC;
    
    switch(pCfgNetwork->security_type)
    {
        case SEC_MODE_WPA:
            pOutLmmNetwork->has_type = true;
            if (0 == strcmp(pCfgNetwork->security_parameters.wpa.version, WPA_PARAM_VERSION_WPA))
                pOutLmmNetwork->type = LmWiFiNetworkType_WIFI_NETWORK_TYPE_WPA;
            else if (0 == strcmp(pCfgNetwork->security_parameters.wpa.version, WPA_PARAM_VERSION_WPA2))
                pOutLmmNetwork->type = LmWiFiNetworkType_WIFI_NETWORK_TYPE_WPA2;
            else
                return -1;
            
            pOutLmmNetwork->has_uCipher = true;
            switch(pCfgNetwork->security_parameters.wpa.ucipher)
            {
                case ATH_CIPHER_TYPE_TKIP:
                    pOutLmmNetwork->uCipher = LmWiFiNetworkuCipher_WIFI_NETWORK_UCIPHER_TKIP;
                    break;
                case ATH_CIPHER_TYPE_CCMP:
                    pOutLmmNetwork->uCipher = LmWiFiNetworkuCipher_WIFI_NETWORK_UCIPHER_CCMP;
                    break;
                default:
                    return ERR_GENERIC;
            }
            
            pOutLmmNetwork->has_mCipher = true;
            switch(pCfgNetwork->security_parameters.wpa.mcipher)
            {
                case TKIP_CRYPT:
                    pOutLmmNetwork->mCipher = LmWiFiNetworkmCipher_WIFI_NETWORK_MCIPHER_TKIP;
                    break;
                case AES_CRYPT:
                    pOutLmmNetwork->mCipher = LmWiFiNetworkmCipher_WIFI_NETWORK_MCIPHER_AES;
                    break;
                default:
                    return ERR_GENERIC;
            }

            if (NULL != pCfgNetwork->security_parameters.wpa.passphrase)
            {
                pOutLmmNetwork->password.arg = (void *) pCfgNetwork->security_parameters.wpa.passphrase;
                pOutLmmNetwork->password.funcs.encode = encode_whistle_str;
            }
            break;
        case SEC_MODE_WEP:
            pOutLmmNetwork->has_type = true;
            pOutLmmNetwork->type = LmWiFiNetworkType_WIFI_NETWORK_TYPE_WEP;

            if (NULL != pCfgNetwork->security_parameters.wep.key0)
            {
                pOutLmmNetwork->password.arg = pCfgNetwork->security_parameters.wep.key0;
                pOutLmmNetwork->password.funcs.encode = encode_whistle_str;
            }
            break;
        case SEC_MODE_OPEN:
            pOutLmmNetwork->has_type = true;
            pOutLmmNetwork->type = LmWiFiNetworkType_WIFI_NETWORK_TYPE_NONE;
            break;
            
        default:
            WPRINT_ERR("dec ntwk");
            return -4;
    }
    
    return ERR_OK;
}
