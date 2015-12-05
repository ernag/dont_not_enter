/*
 * wmp_datadump_encode.c
 * 
 * File for encoding data to be sent to EVTMGR/server.
 *
 *  Created on: the future
 *      Author: braveheart
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "wmp_datadump_encode.h"
#include "pb_helper.h"
#include "pb_encode.h"
#include "whistlemessage.pb.h"
#include "datamessage.pb.h"
#include "wassert.h"
#include "evt_mgr.h"
#include "time_util.h"
#include "cfg_mgr.h"
#include "con_mgr.h"
#include <mqx.h>
#include "cormem.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
// Encoded breadcrumb data is rather small-ish.
#define BREADCRUMB_ENC_MAX_LEN      sizeof(BreadCrumb)
#define ACCELSLEEP_ENC_MAX_LEN      sizeof(AccelSleep)
#define REBOOT_ENC_MAX_LEN          sizeof(Reboot)
#define BATTERYSTATUS_ENC_MAX_LEN   sizeof(BatteryStatus)

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////
bool WMP_DATADUMP_accelData_init(WMP_DATADUMP_accelData_submsg_handle_t *pHandle, pb_ostream_t *stream, bool write_header)
{
    pHandle->data_accel_msg_states.stream_state_payload_header_start = NULL;
    pHandle->data_accel_msg_states.stream_state_payload_start = NULL;
    pHandle->stream = stream;
    pHandle->write_header = write_header;
    
    if (write_header)
    {
        if (!WMP_write_fake_field_tag_length(stream, &(pHandle->data_accel_msg_states), DATADUMP_ACCELDATA_SUBMSG_HEADER_SIZE))
            return false;
    }
    
    return true;
}

bool WMP_DATADUMP_accelData_close(WMP_DATADUMP_accelData_submsg_handle_t *pHandle)
{
    static uint16_t accel_idx = BOGUS_TAG;
    
    if(BOGUS_TAG == accel_idx)
    {
        accel_idx = _WMP_get_idx_from_tag(DataDump_fields, ACCEL_DATA_TAG);
    }
    
    if (pHandle->write_header)
    {
        //Re-write the DataDump.AccelData header
        if ( !WMP_rewrite_real_field_tag_length(pHandle->stream, 
                &(DataDump_fields[accel_idx]), 
                &(pHandle->data_accel_msg_states)) )
            return false;
    }
    return true;
}

// This function is intended to provide a 
bool WMP_DATADUMP_payload_stub_cb(pb_ostream_t *stream, const pb_field_t *field, const void *encode_args)
{   
    stream->bytes_written += ((WMP_DATADUMP_accelData_stub_args_t*) encode_args)->payload_size;
    return(true);
}

// This function is intended to provide a 
static bool _WMP_DATADUMP_cb(pb_ostream_t *stream, const pb_field_t *field, const void *encode_args)
{   
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    if (!pb_encode_submessage(stream, DataDump_fields, encode_args))
        return false;
    
    return true;
}

void _WMP_DATADUMP_encode_wmp_datadump_start_seq(  pb_ostream_t *stream,
        									uint64_t current_time,
                                            size_t payload_length )
{
    WhistleMessage *pMsg;
    DataDump *pDataDump;
    WMP_DATADUMP_accelData_stub_args_t stub_args;
    
    WMP_whistle_msg_alloc(&pMsg);
    
    pDataDump = walloc(sizeof(DataDump));
    
    /*
     * 
     * WARNING!!!
     * If adding another field to the WhistleMessage object then
     * consider increasing the WMP_HEADER_LEN in evt_mgr_upload.c
     * 
     */
    pMsg->has_dataDump = true;
    
    pMsg->dataDump.arg = pDataDump;
    pMsg->dataDump.funcs.encode = &_WMP_DATADUMP_cb;
    
    pDataDump->has_relTimeAtSend = true;
    pDataDump->relTimeAtSend = current_time;
    pDataDump->btProximity.funcs.encode = NULL;
    pDataDump->accelSleep.funcs.encode = NULL;
    stub_args.payload_size = payload_length;
    pDataDump->accelData.arg = &stub_args;
    pDataDump->accelData.funcs.encode = &WMP_DATADUMP_payload_stub_cb;
    
    wassert( pb_encode(stream, WhistleMessage_fields, pMsg) );
    WMP_whistle_msg_free(&pMsg);
    wfree(pDataDump);
}

void _WMP_DATADUMP_encode_wmp_and_datadump_header(  pb_ostream_t *stream,
                                            char* pSerialNumber, 
                                            size_t serial_number_size, 
                                            uint32_t magicChecksum,
                                            uint32_t upload_progress)
{
    WhistleMessage *pMsg;
    WMP_sized_string_cb_args_t serial_number_encode_args;
    
    WMP_whistle_msg_alloc(&pMsg);
    
    /*
     * 
     * WARNING!!!
     * If adding another field to the WhistleMessage object then
     * consider increasing the WMP_HEADER_LEN in evt_mgr_upload.c
     * 
     */
    pMsg->has_objectType = true;
    pMsg->objectType = WhistleMessageType_DATA_DUMP;
    pMsg->has_transactionType = true;
    pMsg->transactionType = TransactionType_TRANS_REQUEST;
    pMsg->has_magicChecksum = true;
    pMsg->magicChecksum = magicChecksum;
    pMsg->has_serialNumber = true;
    serial_number_encode_args.pPayload = pSerialNumber;
    serial_number_encode_args.payloadMaxSize = serial_number_size;
    pMsg->serialNumber.arg = &serial_number_encode_args;
    pMsg->serialNumber.funcs.encode = &encode_whistle_str_sized;
    
    pMsg->has_progress = true;
    pMsg->progress = upload_progress;

    wassert( pb_encode(stream, WhistleMessage_fields, pMsg) );
    WMP_whistle_msg_free(&pMsg);
}


void WMP_DATADUMP_encode_wmp_and_datadump(  pb_ostream_t *stream,
                                            char* pSerialNumber, 
                                            size_t serial_number_size, 
                                            uint32_t magicChecksum,
                                            uint64_t current_time,
                                            uint32_t upload_percentage,
                                            size_t payload_length)
{
	_WMP_DATADUMP_encode_wmp_and_datadump_header(stream,
												pSerialNumber, 
												serial_number_size, 
												magicChecksum,
												upload_percentage);
	_WMP_DATADUMP_encode_wmp_datadump_start_seq(stream, current_time, payload_length);
}

#if 0
static bool _WMP_DATADUMP_serverTimeSync_encode_cb(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    if (!pb_encode_submessage(stream, ServerTimeSync_fields, arg))
        return false;
    
    return true;
}
#endif

#if 0
bool WMP_encode_DATADUMP_serverTimeSync(int64_t server_time, uint8_t *pBuffer, size_t max_len)
{
    DataDump msg = {0};
    ServerTimeSync timeSyncMsg;
    pb_ostream_t stream;
    
    stream = pb_ostream_from_buffer(pBuffer, max_len);

    timeSyncMsg.has_serverAbsoluteTime = TRUE;
    timeSyncMsg.serverAbsoluteTime = server_time;
    
    msg.has_relTimeAtSend = FALSE;
    msg.serverTimeSync.arg = &timeSyncMsg;
    msg.serverTimeSync.funcs.encode = _WMP_DATADUMP_serverTimeSync_encode_cb;

    return pb_encode(&stream, DataDump_fields, &msg);
}
#endif

// This callback executes when EVTMGR is done posting, so it doesn't block.
// Callers of EVTMGR_post() must be careful not to take away the resources/pointers that post() uses,
//   until this callback returns.
static void _WMP_event_post_cbk(err_t err, evt_obj_ptr_t pObj)
{
    wassert( ERR_OK == err );
    wfree( pObj->pPayload );
    wfree( pObj );
}

static bool _WMP_DATADUMP_BtProx_encode_cb(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    if (!pb_encode_submessage(stream, BtProximity_fields, arg))
        return false;
    
    return true;
}

static bool _WMP_DATADUMP_accelsleep_encode_cb(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    if (!pb_encode_submessage(stream, AccelSleep_fields, arg))
        return false;
    
    return true;
}

static bool _WMP_DATADUMP_fwerr_encode_cb(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    if (!pb_encode_submessage(stream, FwError_fields, arg))
        return false;
    
    return true;
}

static bool _WMP_DATADUMP_reboot_encode_cb(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    if (!pb_encode_submessage(stream, Reboot_fields, arg))
        return false;
    
    return true;
}

static bool _WMP_DATADUMP_breadcrumb_encode_cb(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    if (!pb_encode_submessage(stream, BreadCrumb_fields, arg))
        return false;
    
    return true;
}

static bool _WMP_DATADUMP_battery_status_encode_cb(pb_ostream_t *stream, const pb_field_t *field, const void *arg)
{
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    
    if (!pb_encode_submessage(stream, BatteryStatus_fields, arg))
        return false;
    
    return true;
}

static uint32_t _WMP_DATADUMP_get_status(void)
{
	uint32_t status = 0;
    
    switch (CONMGR_get_current_actual_connection_type())
    {
    case CONMGR_CONNECTION_TYPE_WIFI:
    	status |= FwErrorStatusConnectionStatus_WIFI_ON;
    	break;
    case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
    	status |= FwErrorStatusConnectionStatus_BTC_ON;
    	break;
    case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
    	status |= FwErrorStatusConnectionStatus_BTLE_ON;
    	break;
    default:
    	status |= FwErrorStatusConnectionStatus_NO_CONNECTION;
    	break;
    }
    
    // Add other fields/flags to status here.
    
    return status;
}

void WMP_DATADUMP_get_progress(uint32_t completed, uint32_t scale, uint32_t *progress)
{
    *progress = ( (completed*progressType_PROGRESS_LAST_MSG) / scale );

    if( *progress > progressType_PROGRESS_LAST_MSG )
    {
        *progress = progressType_PROGRESS_LAST_MSG;
    }
    else if( *progress < progressType_PROGRESS_FIRST_MSG )
    {
        *progress = progressType_PROGRESS_FIRST_MSG;
    }
}

void WMP_encode_DATADUMP_BtProx(uint64_t absStartTime, 
                                uint64_t relStartTime,
                                uint64_t macAddress,
                                uint8_t *pBuffer,
                                uint16_t *pBytesWritten,
                                size_t max_len)
{
    DataDump *pMsg;
    BtProximity *pBtMsg;
    pb_ostream_t stream;
    
    stream = pb_ostream_from_buffer(pBuffer, max_len);
    
    pBtMsg = walloc(sizeof(BtProximity));
    
    pBtMsg->has_macAddress = TRUE;
    pBtMsg->has_absoluteStartTime = TRUE;
    pBtMsg->has_relativeStartTime = TRUE;
    
    pBtMsg->macAddress = macAddress;
    pBtMsg->relativeStartTime = relStartTime;
    pBtMsg->absoluteStartTime = absStartTime;
    
    pMsg = walloc(sizeof(DataDump));
    
    pMsg->btProximity.arg = pBtMsg;
    pMsg->btProximity.funcs.encode = _WMP_DATADUMP_BtProx_encode_cb;
    
    wassert( pb_encode(&stream, DataDump_fields, pMsg) );
    wfree(pMsg);
    wfree(pBtMsg);
    
    wassert( stream.bytes_written <= max_len );
    *pBytesWritten = (uint_16)stream.bytes_written;
}

void WMP_post_accelSleep(const uint64_t *pAbsTime, const uint64_t *pRelTime, const uint64_t *pDuration)
{
    DataDump *pMsg;
    pb_ostream_t stream;
    evt_obj_ptr_t pObj;
    AccelSleep accelSleep;
    
    /*
     *   TODO:  We currently flush about 23 bytes at a time to EVTMGR for this.
     *          So we waste ~(256-23) bytes of SPI Flash each time we do this!
     *          An improvement would be to queue these up and flush them when we:
     *            a)  get an explicit flush request.
     *            b)  get ( 256 / ACCELSLEEP_ENC_MAX_LEN ) of them.
     *            
     *   TODO:  When implementing the above, do the same thing for breadcrumbs, and 
     *          leverage a generic system that can handle either one.
     */
    
    pObj = walloc(sizeof(evt_obj_t));
    
    pObj->pPayload = walloc(ACCELSLEEP_ENC_MAX_LEN);
    
    stream = pb_ostream_from_buffer(pObj->pPayload, ACCELSLEEP_ENC_MAX_LEN);
    
    accelSleep.has_absTimeStart = TRUE;
    accelSleep.has_relTimeStart = TRUE;
    accelSleep.has_duration = TRUE;

    accelSleep.absTimeStart = *pAbsTime;
    accelSleep.relTimeStart = *pRelTime;
    accelSleep.duration = *pDuration;
    
    pMsg = walloc(sizeof(DataDump));
    
    pMsg->accelSleep.arg = &accelSleep;
    pMsg->accelSleep.funcs.encode = _WMP_DATADUMP_accelsleep_encode_cb;

    wassert( pb_encode(&stream, DataDump_fields, pMsg) );
    wfree(pMsg);
    
    wassert( stream.bytes_written <= ACCELSLEEP_ENC_MAX_LEN );
    pObj->payload_len = (uint16_t)stream.bytes_written;
    
    if( 0 == pObj->payload_len )
    {
        PROCESS_NINFO( ERR_ZERO_LEN_WMP, NULL );
        _WMP_event_post_cbk(ERR_OK, pObj);
    }
    else
    {
        EVTMGR_post(pObj, _WMP_event_post_cbk);
    }
}

/*
 *   This function should be called when we have an error or info we want the server to know about.
 */
uint32_t WMP_post_fw_info( err_t err, 
                        const char *pDescription, 
                        const char *pFile,
                        const char *pFunc,
                        uint32_t   line)
{
    pb_ostream_t stream;
    DataDump *pMsg;
    evt_obj_ptr_t pObj;
    FwError *pFwErr;
    uint32_t max_enc_len = sizeof(FwError);
    uint32_t status;
    
    pObj = walloc(sizeof(evt_obj_t));
    
    max_enc_len += (pDescription ? strlen(pDescription) + 1 : 1);
    max_enc_len += (pFile ? strlen(pFile) + 1 : 1);
    max_enc_len += (pFunc ? strlen(pFunc) + 1 : 1);
    
    pObj->pPayload = walloc(max_enc_len);
    
    stream = pb_ostream_from_buffer(pObj->pPayload, max_enc_len);
    
    pFwErr = walloc(sizeof(FwError));

    pFwErr->has_absoluteTime = TRUE;
    pFwErr->has_relativeTime = TRUE;
    pFwErr->has_fwErrCode = TRUE;
    pFwErr->has_description_str = (NULL == pDescription) ? FALSE:TRUE;
    pFwErr->has_file_str = (NULL == pFile) ? FALSE:TRUE;
    pFwErr->has_function_str = (NULL == pFunc) ? FALSE:TRUE;
    pFwErr->has_line = TRUE;
    pFwErr->has_status = TRUE;
    pFwErr->status = status = _WMP_DATADUMP_get_status();

    UTIL_time_abs_rel(&pFwErr->relativeTime, &pFwErr->absoluteTime);
    pFwErr->fwErrCode = (uint32_t)err;
    pFwErr->line = line;
    
    pFwErr->file_str.funcs.encode = (pFwErr->has_file_str) ?  &encode_whistle_str : NULL;
    pFwErr->file_str.arg = (uint8_t *)pFile;
    
    pFwErr->description_str.funcs.encode = (pFwErr->has_description_str) ?  &encode_whistle_str : NULL;
    pFwErr->description_str.arg = (uint8_t *)pDescription;
    
    pFwErr->function_str.funcs.encode = (pFwErr->has_function_str) ?  &encode_whistle_str : NULL;
    pFwErr->function_str.arg = (uint8_t *)pFunc;
    
    pMsg = walloc(sizeof(DataDump));
    
    pMsg->err.arg = pFwErr;
    pMsg->err.funcs.encode = _WMP_DATADUMP_fwerr_encode_cb;

    wassert( pb_encode(&stream, DataDump_fields, pMsg) );
    
    wfree(pFwErr);
    wfree(pMsg);
    
    wassert( stream.bytes_written <= max_enc_len );
    pObj->payload_len = (uint16_t)stream.bytes_written;
    
    /*
     *   NOTE:  Do not use PROCESS_NINFO here, since its recursive.
     *          Just assert that we have a good payload length, yo.
     */
    wassert( pObj->payload_len > 0 );
    
    EVTMGR_post(pObj, _WMP_event_post_cbk);
    
    return status;
}

/*
 *   This function should be called when we reboot to log the reboot event.
 *     So we call it when we boot up, and when we gracefully shut down.
 */
void WMP_post_reboot(persist_reboot_reason_t reason, uint32_t thread, uint32_t bootingUp)
{
    pb_ostream_t stream;
    DataDump *pMsg;
    evt_obj_ptr_t pObj;
    Reboot *pReboot;
    
    pObj = walloc(sizeof(evt_obj_t));
    pObj->pPayload = walloc(REBOOT_ENC_MAX_LEN);
    
    stream = pb_ostream_from_buffer(pObj->pPayload, REBOOT_ENC_MAX_LEN);
    
    pReboot = walloc(sizeof(Reboot));

    pReboot->has_absoluteTime = TRUE;
    pReboot->has_relativeTime = TRUE;
    pReboot->has_reason = TRUE;
    pReboot->has_bootingUp = TRUE;
    pReboot->has_thread = TRUE;

    UTIL_time_abs_rel(&pReboot->relativeTime, &pReboot->absoluteTime);
    pReboot->bootingUp = bootingUp;
    pReboot->reason = (uint32_t)reason;
    pReboot->thread = thread;
    
    pMsg = walloc(sizeof(DataDump));
    
    pMsg->reboot.arg = pReboot;
    pMsg->reboot.funcs.encode = _WMP_DATADUMP_reboot_encode_cb;

    wassert( pb_encode(&stream, DataDump_fields, pMsg) );
    
    wfree(pReboot);
    wfree(pMsg);
    
    wassert( stream.bytes_written <= REBOOT_ENC_MAX_LEN );
    pObj->payload_len = (uint16_t)stream.bytes_written;
    
    if( 0 == pObj->payload_len )
    {
        PROCESS_NINFO( ERR_ZERO_LEN_WMP, NULL );
        _WMP_event_post_cbk(ERR_OK, pObj);
    }
    else
    {
        EVTMGR_post(pObj, _WMP_event_post_cbk);
    }
}

/*
 *   This is a rather generic function for posting a "this thing happened at this time"
 *     type of event to the event manager's stream.  That means the server will end up knowing about it.
 *     
 *     https://whistle.atlassian.net/wiki/display/COR/EVTMGR%3A+Adding+a+New+Event#EVTMGR%3AAddingaNewEvent-BreadcrumbEvent
 */
void WMP_post_breadcrumb(BreadCrumbEvent event)
{
    pb_ostream_t stream;
    DataDump *pMsg;
    evt_obj_ptr_t pObj;
    BreadCrumb *pCrumb;
    
    corona_print("\t*\tCRUMB\t%d\t*\n", event);
    
    pObj = walloc(sizeof(evt_obj_t));
    pObj->pPayload = walloc(BREADCRUMB_ENC_MAX_LEN);
    
    stream = pb_ostream_from_buffer(pObj->pPayload, BREADCRUMB_ENC_MAX_LEN);
    
    pCrumb = walloc(sizeof(BreadCrumb));

    pCrumb->has_absoluteTime = TRUE;
    pCrumb->has_relativeTime = TRUE;
    pCrumb->has_eventType = TRUE;

    UTIL_time_abs_rel(&pCrumb->relativeTime, &pCrumb->absoluteTime);
    pCrumb->eventType = event;
    
    pMsg = walloc(sizeof(DataDump));
    
    pMsg->crumb.arg = pCrumb;
    pMsg->crumb.funcs.encode = _WMP_DATADUMP_breadcrumb_encode_cb;

    wassert( pb_encode(&stream, DataDump_fields, pMsg) );
    
    wfree(pCrumb);
    wfree(pMsg);
    
    wassert( stream.bytes_written <= BREADCRUMB_ENC_MAX_LEN );
    pObj->payload_len = (uint16_t)stream.bytes_written;
    
    if( 0 == pObj->payload_len )
    {
        PROCESS_NINFO( ERR_ZERO_LEN_WMP, NULL );
        _WMP_event_post_cbk(ERR_OK, pObj);
    }
    else
    {
        EVTMGR_post(pObj, _WMP_event_post_cbk);
    }
}

/*
 *   This function should be called when we want to record the battery status.
 */
void WMP_post_battery_status(uint32_t batteryCounts,
                             uint32_t batteryPercentage,
                             uint32_t temperatureCounts,
                             uint32_t batteryMillivolts,
                             uint32_t zeroBatteryThresh,
                             uint8_t  wifi_is_on,
                             uint8_t  bt_is_on)
{
    pb_ostream_t stream;
    DataDump *pMsg;
    evt_obj_ptr_t pObj;
    BatteryStatus *pBatteryStatus;
    uint32_t status_flags = 0;  
    
    pObj = walloc(sizeof(evt_obj_t));
    pObj->pPayload = walloc(BATTERYSTATUS_ENC_MAX_LEN);
    
    stream = pb_ostream_from_buffer(pObj->pPayload, BATTERYSTATUS_ENC_MAX_LEN);
    
    pBatteryStatus = walloc(sizeof(BatteryStatus));

    pBatteryStatus->has_absoluteTime = TRUE;
    pBatteryStatus->has_relativeTime = TRUE;
    pBatteryStatus->has_batteryCounts = TRUE;
    pBatteryStatus->has_batteryPercentage = TRUE;
    pBatteryStatus->has_temperatureCounts = TRUE;
    pBatteryStatus->has_statusFlags = TRUE;
    pBatteryStatus->has_batteryMillivolts = TRUE;
    pBatteryStatus->has_zeroBattMvThresh = TRUE;
    
    UTIL_time_abs_rel(&pBatteryStatus->relativeTime, &pBatteryStatus->absoluteTime);
    pBatteryStatus->batteryCounts = batteryCounts;
    pBatteryStatus->batteryPercentage = batteryPercentage;
    pBatteryStatus->temperatureCounts = temperatureCounts;
    pBatteryStatus->batteryMillivolts = batteryMillivolts;
    pBatteryStatus->zeroBattMvThresh = zeroBatteryThresh;
    
    pBatteryStatus->statusFlags |= wifi_is_on ? BatteryStatus_StatusFlags_wifi_on : 0;
    pBatteryStatus->statusFlags |= bt_is_on ? BatteryStatus_StatusFlags_bt_on : 0;  
    
    pMsg = walloc(sizeof(DataDump));
    
    pMsg->batteryStatus.arg = pBatteryStatus;
    pMsg->batteryStatus.funcs.encode = _WMP_DATADUMP_battery_status_encode_cb;

    wassert( pb_encode(&stream, DataDump_fields, pMsg) );
    
    wfree(pBatteryStatus);
    wfree(pMsg);
    
    wassert( stream.bytes_written <= BATTERYSTATUS_ENC_MAX_LEN );
    pObj->payload_len = (uint16_t)stream.bytes_written;
    
    if( 0 == pObj->payload_len )
    {
        PROCESS_NINFO( ERR_ZERO_LEN_WMP, NULL );
        _WMP_event_post_cbk(ERR_OK, pObj);
    }
    else
    {
        EVTMGR_post(pObj, _WMP_event_post_cbk);
    }
}

