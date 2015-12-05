
/*
 * fwu_wmp.c
 *
 *  Created on: Sep 25, 2013
 *      Author: Kevin Lloyd
 */

#include <string.h>
#include <mqx.h>
#include "wassert.h"
#include "app_errors.h"
#include "cfg_mgr.h"
#include "fw_info.h"
#include "persist_var.h"
#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "pb_helper.h"
#include "whistlemessage.pb.h"
#include "wmp.h"
#include "con_mgr.h"


#ifndef  FWUMGR_PRINT
#define FWUMGR_PRINT    corona_print
#endif

#define APP_VER_STRING_SIZE                   (48)


/*FUNCTION****************************************************************
* 
* Function Name    : FWUMGR_wmp_gen_checkin_message
* Returned Value   : -1 for error or size of encode
* Comments         :
*    construct a WMP version of a checking message for the caller
*
*END*********************************************************************/

static int_32 _FWUMGR_wmp_gen_checkin_message(WhistleMessage *pWhistleMsg, uint8_t *pEncodeBuffer, size_t buffer_size)
{
    size_t temp_len;
    WMP_sized_string_cb_args_t serial_number_encode_args;
    WMP_sized_string_cb_args_t boot1_encode_args;
    WMP_sized_string_cb_args_t boot2_encode_args; 
    WMP_sized_string_cb_args_t app_encode_args;
    RmCheckInTransportType transport_type = RmCheckInTransportType_TRANSPORT_UNKNOWN;

    pb_ostream_t encode_stream;

    char app_ver_str[APP_VER_STRING_SIZE];
    char boot1_ver_str[APP_VER_STRING_SIZE];
    char boot2_ver_str[APP_VER_STRING_SIZE];

    bool res;
    
    /*
     * Initialize buffers
     */
    
    memset(pWhistleMsg, 0, sizeof(*pWhistleMsg));
   
    if (pEncodeBuffer != NULL)
    {
        encode_stream = pb_ostream_from_buffer(pEncodeBuffer, buffer_size);
    }
        
    temp_len = sizeof(*pWhistleMsg);
    memset(pWhistleMsg, 0, sizeof(*pWhistleMsg));
    

   /*
    * Determine Transport Type
    */
    switch (CONMGR_get_current_actual_connection_type())
    {
		case CONMGR_CONNECTION_TYPE_WIFI:
			transport_type = RmCheckInTransportType_TRANSPORT_WIFI;
			break;
		case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
			if (pomSPP == BTMGR_get_current_pom())
			{
				transport_type = RmCheckInTransportType_TRANSPORT_BTC_ANDROID;
			}
			else //pomiAP
			{
				transport_type = RmCheckInTransportType_TRANSPORT_BTC_IOS;
			}
			break;
		default:
			transport_type = RmCheckInTransportType_TRANSPORT_UNKNOWN;
			break;
    }
    
    /*
     * Populate message contents
     */
    
    pWhistleMsg->has_objectType = true;
    pWhistleMsg->has_transactionType = true;
    pWhistleMsg->has_serialNumber = true;
    pWhistleMsg->has_remoteMgmtMsg = true;
    
    pWhistleMsg->objectType = WhistleMessageType_REMOTE_DEV_MGMT;
    pWhistleMsg->transactionType = TransactionType_TRANS_REQUEST;
    serial_number_encode_args.pPayload = g_st_cfg.fac_st.serial_number; 
    serial_number_encode_args.payloadMaxSize = strlen(g_st_cfg.fac_st.serial_number);
    pWhistleMsg->serialNumber.arg = &serial_number_encode_args;
    pWhistleMsg->serialNumber.funcs.encode = &encode_whistle_str_sized;
    
    pWhistleMsg->remoteMgmtMsg.has_messageType = true;
    pWhistleMsg->remoteMgmtMsg.has_checkInRequest = true;

    pWhistleMsg->remoteMgmtMsg.messageType = RmMessageType_RM_CHECK_IN;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_requestType = true;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_currentAppVersion = true;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_currentBoot2Version = true;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_batteryLevel = true;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_batteryCounts = true;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_temperatureCounts = true;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_temperatureCounts = true;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_wifiFWVersion = 
    		WIFIMGR_get_wifi_fw_version(&pWhistleMsg->remoteMgmtMsg.checkInRequest.wifiFWVersion);  // WIFI FW Version;
#if (CALCULATE_MINUTES_ACTIVE)
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_currentMinutesActive = true;
#else
    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_currentMinutesActive = false;
#endif

    pWhistleMsg->remoteMgmtMsg.checkInRequest.requestType = pmem_flag_get(PV_FLAG_IS_SYNC_MANUAL) ?
    		RmCheckInRequestType_RM_CHECK_IN_MANUAL_SYNC : RmCheckInRequestType_RM_CHECK_IN_AUTO_SYNC;

    PMEM_FLAG_SET(PV_FLAG_IS_SYNC_MANUAL, 0); // reset now that we've consumed

    temp_len = get_version_string(boot1_ver_str, APP_VER_STRING_SIZE, FirmwareVersionImage_BOOT1);
    boot1_encode_args.pPayload = boot1_ver_str;
    boot1_encode_args.payloadMaxSize = temp_len;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.currentBoot1Version.arg = &boot1_encode_args;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.currentBoot1Version.funcs.encode = &encode_whistle_str_sized;

    temp_len = get_version_string(boot2_ver_str, APP_VER_STRING_SIZE, FirmwareVersionImage_BOOT2);
    boot2_encode_args.pPayload = boot2_ver_str;
    boot2_encode_args.payloadMaxSize = temp_len;  
    pWhistleMsg->remoteMgmtMsg.checkInRequest.currentBoot2Version.arg = &boot2_encode_args;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.currentBoot2Version.funcs.encode = &encode_whistle_str_sized;

    temp_len = get_version_string(app_ver_str, APP_VER_STRING_SIZE, FirmwareVersionImage_APP);
    app_encode_args.pPayload = app_ver_str;
    app_encode_args.payloadMaxSize = temp_len;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.currentAppVersion.arg = &app_encode_args;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.currentAppVersion.funcs.encode = &encode_whistle_str_sized;
    
    // Use the battery status that was stored by the connection manager prior to creating the connection
    pWhistleMsg->remoteMgmtMsg.checkInRequest.batteryLevel = PWRMGR_get_stored_batt_percent();
    pWhistleMsg->remoteMgmtMsg.checkInRequest.batteryCounts = PWRMGR_get_stored_batt_counts();
    pWhistleMsg->remoteMgmtMsg.checkInRequest.temperatureCounts = PWRMGR_get_stored_temp_counts();
#if (CALCULATE_MINUTES_ACTIVE)
    pWhistleMsg->remoteMgmtMsg.checkInRequest.currentMinutesActive = MIN_ACT_current_minutes_active();
#endif
    

    pWhistleMsg->remoteMgmtMsg.checkInRequest.has_transportType = true;
    pWhistleMsg->remoteMgmtMsg.checkInRequest.transportType = transport_type;
    

    if ( TRUE != pb_encode(&encode_stream, WhistleMessage_fields, pWhistleMsg))
    {
    	return -1;
    }
    else
    {
    	return (encode_stream.bytes_written);
    }
}

/*FUNCTION****************************************************************
* 
* Function Name    : _FWUMGR_wmp_gen_manifest_request_message
* Returned Value   : Size of encoded message
* Comments         :
*    construct and encode a WMP version of a manifest request into pEncodeBuffer 
*
*END*********************************************************************/

static size_t _FWUMGR_wmp_gen_manifest_request_message(uint8_t *pEncodeBuffer,size_t buffer_size)
{
    WhistleMessage *pWhistleMsg;
    pb_ostream_t encode_stream = {0,0,0,0};
    WMP_sized_string_cb_args_t serial_number_encode_args;
    WMP_sized_string_cb_args_t boot1_encode_args;
    WMP_sized_string_cb_args_t boot2_encode_args; 
    WMP_sized_string_cb_args_t app_encode_args;
    size_t temp_len;
    bool res;
    
    char app_ver_str[APP_VER_STRING_SIZE];
    char boot1_ver_str[APP_VER_STRING_SIZE];
    char boot2_ver_str[APP_VER_STRING_SIZE];


    WMP_whistle_msg_alloc(&pWhistleMsg);
    
    if (pEncodeBuffer != NULL)
    {
        encode_stream = pb_ostream_from_buffer(pEncodeBuffer, buffer_size);
    }
    
    temp_len = sizeof(*pWhistleMsg);
    memset(pWhistleMsg, 0, sizeof(*pWhistleMsg));
    
    pWhistleMsg->has_objectType = true;
    pWhistleMsg->has_transactionType = true;
    pWhistleMsg->has_magicChecksum = false;
    pWhistleMsg->has_serialNumber = true;
    pWhistleMsg->has_dataDump = false;
    pWhistleMsg->dataDump.funcs.encode = NULL;
    pWhistleMsg->has_remoteMgmtMsg = true;
    pWhistleMsg->has_localMgmtMsg = false;
    
    pWhistleMsg->objectType = WhistleMessageType_REMOTE_DEV_MGMT;
    pWhistleMsg->transactionType = TransactionType_TRANS_REQUEST;
    serial_number_encode_args.pPayload = g_st_cfg.fac_st.serial_number; 
    serial_number_encode_args.payloadMaxSize = strlen(g_st_cfg.fac_st.serial_number);
    pWhistleMsg->serialNumber.arg = &serial_number_encode_args;
    pWhistleMsg->serialNumber.funcs.encode = &encode_whistle_str_sized;
    
    pWhistleMsg->remoteMgmtMsg.has_messageType = true;
    pWhistleMsg->remoteMgmtMsg.has_checkInRequest = false;
    pWhistleMsg->remoteMgmtMsg.has_checkInResponse = false;
    pWhistleMsg->remoteMgmtMsg.has_configRequest = false;
    pWhistleMsg->remoteMgmtMsg.has_configResponse = false;
    pWhistleMsg->remoteMgmtMsg.has_fwuManifestRequest = true;
    pWhistleMsg->remoteMgmtMsg.has_fwuManifestResponse = false;
    pWhistleMsg->remoteMgmtMsg.has_fwuStatusPost = false;

    pWhistleMsg->remoteMgmtMsg.messageType = RmMessageType_RM_FWU_MANIFEST;
    pWhistleMsg->remoteMgmtMsg.fwuManifestRequest.has_currentAppVersion = true;
    pWhistleMsg->remoteMgmtMsg.fwuManifestRequest.has_currentBoot1Version = true;
    pWhistleMsg->remoteMgmtMsg.fwuManifestRequest.has_currentBoot2Version = true;
    
    temp_len = get_version_string(boot1_ver_str, APP_VER_STRING_SIZE, FirmwareVersionImage_BOOT1);
    boot1_encode_args.pPayload = boot1_ver_str;
    boot1_encode_args.payloadMaxSize = temp_len;
    pWhistleMsg->remoteMgmtMsg.fwuManifestRequest.currentBoot1Version.arg = &boot1_encode_args;
    pWhistleMsg->remoteMgmtMsg.fwuManifestRequest.currentBoot1Version.funcs.encode = &encode_whistle_str_sized;
    
    temp_len = get_version_string(boot2_ver_str, APP_VER_STRING_SIZE, FirmwareVersionImage_BOOT2);
    boot2_encode_args.pPayload = boot2_ver_str;
    boot2_encode_args.payloadMaxSize = temp_len;  
    pWhistleMsg->remoteMgmtMsg.fwuManifestRequest.currentBoot2Version.arg = &boot2_encode_args;
    pWhistleMsg->remoteMgmtMsg.fwuManifestRequest.currentBoot2Version.funcs.encode = &encode_whistle_str_sized;
    
    temp_len = get_version_string(app_ver_str, APP_VER_STRING_SIZE, FirmwareVersionImage_APP);
    app_encode_args.pPayload = app_ver_str;
    app_encode_args.payloadMaxSize = temp_len;
    pWhistleMsg->remoteMgmtMsg.fwuManifestRequest.currentAppVersion.arg = &app_encode_args;
    pWhistleMsg->remoteMgmtMsg.fwuManifestRequest.currentAppVersion.funcs.encode = &encode_whistle_str_sized;

    WMP_whistle_msg_free(&pWhistleMsg);
    
    if (TRUE != pb_encode(&encode_stream, WhistleMessage_fields, pWhistleMsg))
    {
    	return -1;
    }
    else
    {
    	return encode_stream.bytes_written;
    }
}


/*FUNCTION****************************************************************
* 
* Function Name    : FWUMGR_wmp_pasre_manifest_response
* Returned Value   : None
* Comments         :
*    construct a WMP version of a manifest request 
*
*END*********************************************************************/

static err_t _FWUMGR_wmp_parse_manifest_response(uint8_t *pEncBuffer, size_t enc_buffer_len, char *pPkgUrl, const size_t pkg_url_len)
{
    WhistleMessage *pWhistleMsg;
	WMP_sized_string_cb_args_t package_url;
    pb_istream_t stream;
    err_t err = ERR_OK;
    
    WMP_whistle_msg_alloc(&pWhistleMsg);
    
    err = ERR_OK;
    
    package_url.pPayload = pPkgUrl;
    package_url.payloadMaxSize = pkg_url_len;
    package_url.exists = false;
    
    if (enc_buffer_len <= 0)
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_MANIFEST_PARSE, "buflen");
    	goto _FWUMGR_wmp_parse_manifest_response_done; 
    }

    /* Construct a pb_istream_t for reading from the buffer */
    stream = pb_istream_from_buffer(pEncBuffer, enc_buffer_len);

    memset((void*) pWhistleMsg, 0, sizeof(WhistleMessage));

    pWhistleMsg->remoteMgmtMsg.fwuManifestResponse.packageUrl.arg = &package_url;
    pWhistleMsg->remoteMgmtMsg.fwuManifestResponse.packageUrl.funcs.decode = &decode_whistle_str_sized;

    if ( pb_decode(&stream, WhistleMessage_fields, pWhistleMsg) < 0 )
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_MANIFEST_PARSE, "decode");
    	goto _FWUMGR_wmp_parse_manifest_response_done;
    }


    if (pWhistleMsg->has_remoteMgmtMsg != TRUE)
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_MANIFEST_PARSE, "no rmt msg");
    	goto _FWUMGR_wmp_parse_manifest_response_done;
    }
    if (pWhistleMsg->remoteMgmtMsg.has_fwuManifestResponse != TRUE)
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_MANIFEST_PARSE, "no resp\n");  // TODO:  should never happen?
    	goto _FWUMGR_wmp_parse_manifest_response_done;
    }
//    if (((WMP_sized_string_cb_args_t*)(pWhistleMsg->remoteMgmtMsg.fwuManifestResponse.firmwareUpdateVersion.arg))->exists != TRUE )
//    {
//    	err = PROCESS_NINFO(ERR_CFGMGR_DECODE_FAILED, "chkn: no pkg ver\n");
//    	goto _FWUMGR_wmp_parse_manifest_response_done;
//    }
//    else
//    {
//    	FWUMGR_PRINT("Pkg serv num: %s\n", ((WMP_sized_string_cb_args_t*)(pWhistleMsg->remoteMgmtMsg.fwuManifestResponse.firmwareUpdateVersion.arg))->pPayload);
//    }

    if (package_url.exists != TRUE )
    {
    	FWUMGR_PRINT("chkn: no URL\n");
    	err = ERR_GENERIC;
    	goto _FWUMGR_wmp_parse_manifest_response_done;
    }
    else
    {
//    	FWUMGR_PRINT("FWU: FWU Pkg URL: %s\r\n", ((WMP_string_cb_args_t*)(whistle_msg->remoteMgmtMsg.fwuManifestResponse.packageUrl.arg))->pPayload);
    }
    
_FWUMGR_wmp_parse_manifest_response_done:
    WMP_whistle_msg_free(&pWhistleMsg);
    
    return err;
}
