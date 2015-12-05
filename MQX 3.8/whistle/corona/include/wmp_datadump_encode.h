/*
 * wmp_datadump_encode.h
 *
 *  Created on: Apr 15, 2013
 *      Author: SpottedLabs
 */

#ifndef WMP_DATADUMP_ENCODE_H_
#define WMP_DATADUMP_ENCODE_H_

#include "datamessage.pb.h"
#include "whistlemessage.pb.h"
#include "wmp.h"
#include "app_errors.h"
#include "pmem.h"

#define DATADUMP_ACCELDATA_SUBMSG_HEADER_SIZE 4

typedef struct _WMP_DATADUMP_accelData_submsg_handle_t
{
    pb_ostream_t *stream;
    bool write_header;
    WMP_encode_partial_state_args data_accel_msg_states;
} WMP_DATADUMP_accelData_submsg_handle_t;

typedef struct _WMP_DATADUMP_accelData_stub_args_t
{
    uint8_t *pPayload;
    size_t payload_size;
} WMP_DATADUMP_accelData_stub_args_t;


bool WMP_DATADUMP_accelData_init(WMP_DATADUMP_accelData_submsg_handle_t *pHandle, 
        pb_ostream_t *stream, bool write_header);
bool WMP_DATADUMP_accelData_close(WMP_DATADUMP_accelData_submsg_handle_t *pHandle);
bool WMP_DATADUMP_payload_stub_cb(pb_ostream_t *stream, const pb_field_t *field, const void *encode_args);
void WMP_DATADUMP_encode_wmp_and_datadump(  pb_ostream_t *stream,
                                            char* pSerialNumber, 
                                            size_t serial_number_size, 
                                            uint32_t magicChecksum,
                                            uint64_t current_time,
                                            uint32_t upload_percentage,
                                            size_t payload_length);
void WMP_DATADUMP_get_progress(uint32_t completed, uint32_t scale, uint32_t *progress);
//bool WMP_encode_DATADUMP_serverTimeSync(int64_t server_time, uint8_t *pBuffer, size_t max_len);
void WMP_encode_DATADUMP_BtProx(uint64_t absStartTime,
                                uint64_t relStartTime,
                                uint64_t macAddress,
                                uint8_t *pBuffer,
                                uint16_t *pBytesWritten,
                                size_t max_len);

void WMP_post_accelSleep(const uint64_t *pAbsTime, const uint64_t *pRelTime, const uint64_t *pDuration);
void WMP_post_reboot(persist_reboot_reason_t reason, uint32_t thread, uint32_t bootingUp);
void WMP_post_breadcrumb(BreadCrumbEvent event);
uint32_t WMP_post_fw_info( err_t err, 
                        const char *pDescription, 
                        const char *pFile,
                        const char *pFunc,
                        uint32_t   line);
void WMP_post_battery_status(uint32_t batteryCounts,
                             uint32_t batteryPercentage,
                             uint32_t temperatureCounts,
                             uint32_t batteryMillivolts,
                             uint32_t zeroBatteryThresh,
                             uint8_t  wifi_is_on,
                             uint8_t  bt_is_on);

#endif /* WMP_DATADUMP_ENCODE_H_ */
