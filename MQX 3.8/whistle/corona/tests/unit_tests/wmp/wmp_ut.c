
#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <mqx.h>
#include <time.h>
#include "wassert.h"
#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "pb_helper.h"
#include "whistlemessage.pb.h"
#include "datamessage.pb.h"
#include "wmp_accel_encode.h"
#include "wmp_datadump_encode.h"
#include "wmp_ut.h"
#include "wmp.h"
#include "fw_info.h"
#include "cfg_mgr.h"

#define ACCEL_MSG_BUFFER_SIZE 200
#define WMP_MSG_BUFFER_SIZE 300
#define WMP_SERIAL_NUMBER "COR-000001"
#define WMP_SERIAL_NUMBER_SIZE 10

void _generate_random_sample_data(int8_t* buffer, uint16_t size)
{
    uint16_t loc;
    int8_t x, y, z;
    x = 0;
    y = 0;
    z = 0;
    srand((int8_t) time(0));
    
    for (loc = 0; loc < size; loc+=3) {
        x += ((rand() << 1) >> 28);
        y += (rand() >> 28);
        z += (rand() >> 28);
        buffer[loc] = x;
        buffer[loc + 1] = y;
        buffer[loc + 2] = z;
    }
    
}

void _generate_static_sample_data(int8_t* buffer, uint16_t size)
{
//    int8_t baseline[] = {1,0,1,1,1,1,2,2,1,3,2,2,3,3,2};
    int8_t baseline[] = {\
            0xFD,0x00,0x25,
            0xFD,0x00,0x25,
            0x00,0x00,0x20,
            0xFE,0x00,0x17,
            0xFF,0xFF,0x1A,
            0xFF,0x00,0x20,
            0x01,0x00,0x1C,
            0x01,0xFF,0x1F};
    int i,j;
    
    for (i=0; i < size;) {
        for (j=0; j < 24 && i < size; j++)
        {
            buffer[i] = baseline[j];
            i++;
        }
    }
    
}

int wmp_accel_multipart_ut()
{
    int8_t raw_buffer[30];
    uint8_t *pAccelMsgBuf,*pWMPMsgBuf;
    pb_ostream_t accel_stream, wmp_stream;
    uint32_t thirtyTwo_bit_crc;
    
    WMP_ACCENC_partial_handle_t pADPayloadHandle;
    WMP_DATADUMP_accelData_submsg_handle_t pDDAccelMsgHandle;
    int16_t i;
    
    // Setup AccelMsg output buffer
    pAccelMsgBuf = _lwmem_alloc(ACCEL_MSG_BUFFER_SIZE * sizeof(uint8_t));
    memset(pAccelMsgBuf, 0, ACCEL_MSG_BUFFER_SIZE);
    accel_stream = pb_ostream_from_buffer(pAccelMsgBuf, ACCEL_MSG_BUFFER_SIZE);

    // Setup WMP Message output buffer
    pWMPMsgBuf = _lwmem_alloc(WMP_MSG_BUFFER_SIZE * sizeof(uint8_t));
    memset(pWMPMsgBuf, 0, WMP_MSG_BUFFER_SIZE);
    wmp_stream = pb_ostream_from_buffer(pWMPMsgBuf, WMP_MSG_BUFFER_SIZE);
    
    
    
    
    //Write DataDump AccelData sub-message tag
    WMP_DATADUMP_accelData_init(&pDDAccelMsgHandle, &accel_stream, true);

    //Write initial portion of AccelData
    WMP_ACCEL_header_encode(&accel_stream,
            16326825, //time
            true, //is_relative_time
            AccelDataEncoding_ACCEL_ENC_S8, //encoding_type (AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K0, AccelDataEncoding_ACCEL_ENC_S8)
            50, //sample frequency
            8); // range

    // Write payload header of AccelData
    WMP_ACCEL_payload_encode_init(&pADPayloadHandle, &accel_stream,
            AccelDataEncoding_ACCEL_ENC_S8,
            &thirtyTwo_bit_crc,
            true);
       
    //Write payload data
    _generate_static_sample_data(raw_buffer, 30);
    WMP_ACCEL_payload_encode_append(&pADPayloadHandle, raw_buffer, 30);
    _generate_static_sample_data(raw_buffer, 30);
    WMP_ACCEL_payload_encode_append(&pADPayloadHandle, raw_buffer, 30);
    _generate_static_sample_data(raw_buffer, 30);
    WMP_ACCEL_payload_encode_append(&pADPayloadHandle, raw_buffer, 30);
    
    WMP_ACCEL_payload_encode_close(&pADPayloadHandle);
   
    
    //Close DataDump AccelData sub-message
    WMP_DATADUMP_accelData_close(&pDDAccelMsgHandle);

/////////////////////////////////////////////////////////////////////
    //Write WMP Header
//    if (!WMP_DATADUMP_encode_wmp_and_datadump(&wmp_stream,
//            WMP_SERIAL_NUMBER, // serial number
//            WMP_SERIAL_NUMBER_SIZE, // serial number length
//            0x12345678, // magic checksum
//            54321, // current_time
//            accel_stream.bytes_written)) // payload size
//    {
//        wassert( MQX_OK == _lwmem_free(pAccelMsgBuf));
//        wassert( MQX_OK == _lwmem_free(pWMPMsgBuf));
//        return 1;
//    }
    
    
    corona_print("wmp_accel_multipart_ut (len: %d):\r\n", wmp_stream.bytes_written);
    for (i = 0; i < wmp_stream.bytes_written; i++)
    {
        if (i % 8 == 0) 
            corona_print("\r\n[%02d], ", i);
        corona_print("0x%02x, ", pWMPMsgBuf[i]);
    }
    corona_print("\r\n");

    wassert( MQX_OK == _lwmem_free(pAccelMsgBuf));
    wassert( MQX_OK == _lwmem_free(pWMPMsgBuf));
    
    return 0;
}




int wmp_accel_singleshot_ut()
{
    //uint8_t raw_buffer[2000];
    int8_t raw_buffer[] = {1,0,1,1,1,1,2,2,1,3,2,2,3,3,2};
    uint8_t encoded_buffer[20];
    pb_ostream_t out_stream;
    AccelData data = {0};
    WMP_ACCENC_payload_cb_args_t encodingArgs;
    int16_t i;
    
    memset(encoded_buffer, 0, 20);
    //memset(raw_buffer, 0, 2000);
    
    //_generate_sample_code(raw_buffer, 10);
    
    out_stream = pb_ostream_from_buffer(encoded_buffer, 20);
    
    data.has_relativeStartTime = true;
    data.relativeStartTime = 12356;
    
    data.has_sampleFrequency = true;
    data.sampleFrequency = 50;

    
    data.has_encodingType = true;
    data.encodingType = AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K0;
    
    encodingArgs.encodingType = data.encodingType;
    encodingArgs.src_payload = raw_buffer;
    encodingArgs.src_payload_size = 15;
    encodingArgs.thirtytwo_bit_crc = &(data.thirtyTwoBitCRCPostPayload);
    
    data.has_payload = true;
    data.payload.funcs.encode = &WMP_ACCEL_payload_encode_oneshot_cb;
    data.payload.arg = &encodingArgs;
    
    if (!pb_encode(&out_stream, AccelData_fields, &data))
        return 1;

    corona_print("wmp_accel_singleshot_ut:\r\n");
    for (i=0; i<out_stream.bytes_written; i++)
    {
        if (i % 8 == 0)
            corona_print("\r\n[%02d], ", i);
    	corona_print("0x%02x, ", encoded_buffer[i]);
    }
    return 0;
}


void wmp_test_partial_encode()
{
    int8_t raw_buffer[30];
    uint8_t encoded_buffer[50];
    uint32_t crc;
    pb_ostream_t out_stream;
    WMP_ACCENC_partial_handle_t partial_message_handle;
    int i;
    
    corona_print("wmp_test_partial_encode Diff-K0\r\n");

    out_stream = pb_ostream_from_buffer(encoded_buffer, 50);
    
    if (!WMP_ACCEL_payload_encode_init(&partial_message_handle, &out_stream, AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K0, &crc, false))
    {
        corona_print("ERROR: Partial Init\r\n");
        corona_print("ERROR: stream.bytes_written: %d\r\n", out_stream.bytes_written);
        return;
    }
    
    _generate_static_sample_data(raw_buffer, 15);
    if (!WMP_ACCEL_payload_encode_append(&partial_message_handle, raw_buffer, 15))
    {
        corona_print("ERROR: Partial Write\r\n");
        corona_print("ERROR: stream.bytes_written: %d\r\n", out_stream.bytes_written);
        return;
    }

    _generate_static_sample_data(raw_buffer, 15);
    if (!WMP_ACCEL_payload_encode_append(&partial_message_handle, raw_buffer, 15))
    {
        corona_print("ERROR: Partial Write\r\n");
        corona_print("ERROR: stream.bytes_written: %d\r\n", out_stream.bytes_written);
        return;
    }


    corona_print(" output(len=%d):\r\n", out_stream.bytes_written);
    for (i=0; i<out_stream.bytes_written; i++)
    {
        if (i % 8 == 0)
            corona_print("\r\n[%02d], ", i);
        corona_print("0x%02x, ", encoded_buffer[i]);
    }
    
    

    corona_print("\r\nwmp_test_partial_encode S8\r\n");
    
    // reset outstream
    out_stream = pb_ostream_from_buffer(encoded_buffer, 50);

    if (!WMP_ACCEL_payload_encode_init(&partial_message_handle, &out_stream, AccelDataEncoding_ACCEL_ENC_S8, &crc, false))
    {
        corona_print("ERROR: Partial Init\r\n");
        corona_print("ERROR: stream.bytes_written: %d\r\n", out_stream.bytes_written);
        return;
    }

    _generate_static_sample_data(raw_buffer, 15);
    if (!WMP_ACCEL_payload_encode_append(&partial_message_handle, raw_buffer, 15))
    {
        corona_print("ERROR: Partial Write\r\n");
        corona_print("ERROR: stream.bytes_written: %d\r\n", out_stream.bytes_written);
        return;
    }
    
    _generate_static_sample_data(raw_buffer, 15);
    if (!WMP_ACCEL_payload_encode_append(&partial_message_handle, raw_buffer, 15))
    {
        corona_print("ERROR: Partial Write\r\n");
        corona_print("ERROR: stream.bytes_written: %d\r\n", out_stream.bytes_written);
        return;
    }
    
    corona_print(" output(len=%d):\r\n", out_stream.bytes_written);
    for (i=0; i<out_stream.bytes_written; i++)
    {
        if (i % 8 == 0)
            corona_print("\r\n[%02d], ", i);
        corona_print("0x%02x, ", encoded_buffer[i]);
    }
    
    return;
}

int wmp_test_rice_encoding(void)
{
    pb_ostream_t out_stream;
    uint8_t out_buffer[300], crc;
    ACCENC_bitwriter_t bw;
    int res;

    int8_t raw_payload_1_len = 21;
    int8_t raw_payload_1[] = { // 21 long
            5,4,2,
            6,8,10,
            2,3,4,
            4,4,4,
            6,3,2,
            7,3,8,
            1,2,3
    };
    int8_t raw_payload_1_enc_k2_len = 21;
    int8_t raw_payload_1_enc_k2 [] = { // 21 long
            0xce, 0x23, 0x8f, 0x1f, 0x11, 0x38, 
            0xc6, 0x31, 0xc4, 0xc7, 0x33, 0xe0, 
            0xc4, 0xff
    };
    
    int8_t raw_payload_2_len = 18;
    int8_t raw_payload_2[] = { // 18 long
            3,3,3,
            5,9,10,
            0,0,0,
            -5,-2,9,
            -5,6,0,
            -4,4,3
    };

    out_stream = pb_ostream_from_buffer(out_buffer, 300);
    
    WMP_ACCEL_payload_encode_riceSigned_init(&bw, &out_stream, &crc);
    _WMP_ACCEL_payload_encode_riceSigned(raw_payload_1, raw_payload_1_len, 2, &bw);
    WMP_ACCEL_payload_encode_riceSigned_closed(&bw);
    
    res = memcmp(raw_payload_1_enc_k2, out_buffer, raw_payload_1_enc_k2_len);
    if (res != 0)
        corona_print("RICE ENCODING K2 FAILED");
    
    return res;
}

int wmp_test_accel_encode(void)
{
    WMP_ACCENC_partial_handle_t payload_handle;
    pb_ostream_t out_stream;
    uint8_t out_buffer[300];
    uint32_t crc;
    int res, i;
    
    int8_t raw_payload_1_len = 21;
    int8_t raw_payload_1[] = { // 21 long
            5,4,2,
            6,8,10,
            2,3,4,
            4,4,4,
            6,3,2,
            7,3,8,
            1,2,3
    };

    int8_t raw_payload_2_len = 18;
    int8_t raw_payload_2[] = { // 18 long
            3,3,3,
            5,9,10,
            0,0,0,
            -5,-2,9,
            -5,6,0,
            -4,4,3
    };
    
    int8_t raw_payload_3_len = 12;
    int8_t raw_payload_3[] = { // 12 long
            4,3,2,
            0,0,0,
            0,0,0,
            0,2,3
    };

    int8_t expected_diff_s8_output_len = 51;
    uint8_t expected_diff_s8_output[] = {
            0x5, 0x4, 0x2, 
            0x1, 0x4, 0x8, 
            0xfc, 0xfb, 0xfa, 
            0x2, 0x1, 0x0, 
            0x2, 0xff, 0xfe, 
            0x1, 0x0, 0x6, 
            0xfa, 0xff, 0xfb, 
            0x2, 0x1, 0x0, 
            0x2, 0x6, 0x7, 
            0xfb, 0xf7, 0xf6, 
            0xfb, 0xfe, 0x9, 
            0x0, 0x8, 0xf7, 
            0x1, 0xfe, 0x3, 
            0x8, 0xff, 0xff, 
            0xfc, 0xfd, 0xfe, 
            0x0, 0x0, 0x0, 
            0x0, 0x2, 0x3, 
    };
    int8_t expected_diff_k2_output_len = 35;
//    uint8_t expected_diff_k2_output[] = {
//            0xce, 0x20, 0xe3, 0xc5, 0xeb, 0x70, 0x44, 0x26, 0x47, 
//            0x1b, 0x5a, 0x82, 0x23, 0x8e, 0x75, 0xeb, 0xde, 0x9f, 
//            0x91, 0xe3, 0xd1, 0x73, 0xe1, 0x2b, 0xa6, 0x0, 0x11, 
//            0x3f
//    };
    uint8_t expected_diff_k2_output[] = {
            0xce, 0x22, 0xc7, 0x1a, 0xf5, 0xfb, 0xe3, 0x8e,
            0x03, 0x68, 0xf8, 0xe9, 0x7d, 0x78, 0x8c, 0x8c,
            0xf3, 0xd7, 0xf5, 0xfe, 0x87, 0x3f, 0xf3, 0x9f, 
            0x8f, 0xf6, 0x7d, 0xfe, 0x39, 0x37, 0xf6, 0xd6,
            0x26, 0x04, 0x4f};

    out_stream = pb_ostream_from_buffer(out_buffer, 300);
    
    // Write payload header of AccelData
    WMP_ACCEL_payload_encode_init(&payload_handle, &out_stream,
            AccelDataEncoding_ACCEL_ENC_DIFF_S16,
            &crc,
            false);
    corona_print("Input 1: %d bytes,\r\n      2: %d bytes,\r\n      3: %d bytes\r\n", raw_payload_1_len, raw_payload_2_len, raw_payload_3_len); fflush(stdout);
    WMP_ACCEL_payload_encode_append(&payload_handle, raw_payload_1, raw_payload_1_len);

    WMP_ACCEL_payload_encode_append(&payload_handle, raw_payload_2, raw_payload_2_len);

    WMP_ACCEL_payload_encode_append(&payload_handle, raw_payload_3, raw_payload_3_len);
    
    WMP_ACCEL_payload_encode_close(&payload_handle);
    
    res = memcmp(expected_diff_s8_output, out_buffer, expected_diff_s8_output_len);
    if (res == 0 )
    {
        corona_print("Diff S8 encoding: PASS\r\n"); fflush(stdout);
    }
    else
    {
        corona_print("Diff S8 encoding: FAIL\r\n"); fflush(stdout);
        for (i = 0; i < out_stream.bytes_written; i++)
        {
            corona_print("Encoded payload\r\n");
            corona_print(" 0x%02x, ", out_buffer[i]);
            if (i % 3 == 0)
                corona_print("\r\n");
        }
    }
    

    
    out_stream = pb_ostream_from_buffer(out_buffer, 300);
    
    // Write payload header of AccelData
    WMP_ACCEL_payload_encode_init(&payload_handle, &out_stream,
            AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K2,
            &crc,
            false);
    WMP_ACCEL_payload_encode_append(&payload_handle, raw_payload_1, raw_payload_1_len);

    WMP_ACCEL_payload_encode_append(&payload_handle, raw_payload_2, raw_payload_2_len);

    WMP_ACCEL_payload_encode_append(&payload_handle, raw_payload_3, raw_payload_3_len);
    
    WMP_ACCEL_payload_encode_close(&payload_handle);
    
    res = memcmp(expected_diff_k2_output, out_buffer, expected_diff_k2_output_len);
    if (res == 0 )
    {
        corona_print("Diff K2 encoding: PASS\r\n"); fflush(stdout);
    }
    else
    {
        corona_print("Diff K2 encoding: FAIL\r\n"); fflush(stdout);
    }

    corona_print("Accel encoding test end\r\n");
    
    return 0;
}

int wmp_gen_checkin_message()
{
    uint8_t encoded_buffer[100];
    uint8_t pHttpBuf[400];
    pb_ostream_t encode_stream;
    WhistleMessage msg;
    size_t temp_len, httpBufLen;
    char app_ver_str[48], boot1_ver_str[48], boot2_ver_str[48];
    bool res;

    encode_stream = pb_ostream_from_buffer(encoded_buffer, 100);
    
    msg.has_objectType = true;
    msg.has_transactionType = true;
    msg.has_magicChecksum = false;
    msg.has_serialNumber = true;
    msg.has_dataDump = false;
    msg.dataDump.funcs.encode = NULL;
    msg.has_localMgmtMsg = false;
    msg.has_remoteMgmtMsg = true;
    
    msg.objectType = WhistleMessageType_REMOTE_DEV_MGMT;
    msg.transactionType = TransactionType_TRANS_REQUEST;
    // Should change to use serial number g_config.devinfo.serial_number; // serial number
    msg.serialNumber.arg = WMP_SERIAL_NUMBER;
    msg.serialNumber.funcs.encode = &encode_whistle_str;
    
    msg.remoteMgmtMsg.has_messageType = true;
    msg.remoteMgmtMsg.has_checkInRequest = true;
    msg.remoteMgmtMsg.has_checkInResponse = false;
    msg.remoteMgmtMsg.has_configRequest = false;
    msg.remoteMgmtMsg.has_configResponse = false;
    msg.remoteMgmtMsg.has_fwuManifestRequest = false;
    msg.remoteMgmtMsg.has_fwuManifestResponse = false;
    msg.remoteMgmtMsg.has_fwuStatusPost = false;

    msg.remoteMgmtMsg.messageType = RmMessageType_RM_CHECK_IN;
    msg.remoteMgmtMsg.checkInRequest.has_requestType = true;
    msg.remoteMgmtMsg.checkInRequest.has_batteryLevel = false;
    msg.remoteMgmtMsg.checkInRequest.has_currentAppVersion = true;
    msg.remoteMgmtMsg.checkInRequest.has_currentBoot1Version = false;
    msg.remoteMgmtMsg.checkInRequest.has_currentBoot2Version = false;
    msg.remoteMgmtMsg.checkInRequest.currentBoot1Version.funcs.encode = NULL;
    msg.remoteMgmtMsg.checkInRequest.currentBoot2Version.funcs.encode = NULL;
    
    msg.remoteMgmtMsg.checkInRequest.requestType = RmCheckInRequestType_RM_CHECK_IN_MANUAL_SYNC;
    
    temp_len = get_version_string(boot1_ver_str, 48, FirmwareVersionImage_BOOT1);
    msg.remoteMgmtMsg.checkInRequest.currentBoot1Version.arg = boot1_ver_str;
    msg.remoteMgmtMsg.checkInRequest.currentBoot1Version.funcs.encode = &encode_whistle_str;
    
    temp_len = get_version_string(boot2_ver_str, 48, FirmwareVersionImage_BOOT2);
    msg.remoteMgmtMsg.checkInRequest.currentBoot2Version.arg = boot2_ver_str;
    msg.remoteMgmtMsg.checkInRequest.currentBoot2Version.funcs.encode = &encode_whistle_str;
    
    temp_len = get_version_string(app_ver_str, 48, FirmwareVersionImage_APP);
    msg.remoteMgmtMsg.checkInRequest.currentAppVersion.arg = app_ver_str;
    msg.remoteMgmtMsg.checkInRequest.currentAppVersion.funcs.encode = &encode_whistle_str;
    
    res = pb_encode(&encode_stream, WhistleMessage_fields, &msg);
    
    printf("Size of wmp msg: %d bytes\r\n", sizeof(msg)); fflush(stdout);
    
    WHTTP_build_http_header(pHttpBuf, WMP_SERIAL_NUMBER, "staging.whistle.com",
                            80,
                            &httpBufLen,
                            encode_stream.bytes_written);
    
    return 0;
}

int wmp_parse_checkin_resp()
{
    /*
     * http_response HTTP header:
     *  HTTP/1.1 200 OK
        Cache-Control: max-age=0, private, must-revalidate
        Content-Type: text/html; charset=utf-8
        Date: Wed, 15 May 2013 00:11:26 GMT
        Etag: "09e8a0a7c663b616c39691525129c33a"
        Set-Cookie: _server_session=BAh7BkkiD3Nlc3Npb25faWQGOgZFRkkiJWUyOWIzYTU0NjcwY2ZhMDEwOWFhNzBlYTJmZGY3NTg0BjsAVA%3D%3D--6e97b22793723485e16539b463ddd8c1049b774c; path=/; HttpOnly
        Status: 200 OK
        X-Rack-Cache: invalidate, pass
        X-Request-Id: 20bb871f112f53383883685a868400fd
        X-Runtime: 1.147002
        X-Ua-Compatible: IE=Edge,chrome=1
        Content-Length: 57
        Connection: Close
     * 
     */
    unsigned char http_response[] = {
      0x48, 0x54, 0x54, 0x50, 0x2f, 0x31, 0x2e, 0x31, 0x20, 0x32, 0x30, 0x30,
      0x20, 0x4f, 0x4b, 0x0d, 0x0a, 0x43, 0x61, 0x63, 0x68, 0x65, 0x2d, 0x43,
      0x6f, 0x6e, 0x74, 0x72, 0x6f, 0x6c, 0x3a, 0x20, 0x6d, 0x61, 0x78, 0x2d,
      0x61, 0x67, 0x65, 0x3d, 0x30, 0x2c, 0x20, 0x70, 0x72, 0x69, 0x76, 0x61,
      0x74, 0x65, 0x2c, 0x20, 0x6d, 0x75, 0x73, 0x74, 0x2d, 0x72, 0x65, 0x76,
      0x61, 0x6c, 0x69, 0x64, 0x61, 0x74, 0x65, 0x0d, 0x0a, 0x43, 0x6f, 0x6e,
      0x74, 0x65, 0x6e, 0x74, 0x2d, 0x54, 0x79, 0x70, 0x65, 0x3a, 0x20, 0x74,
      0x65, 0x78, 0x74, 0x2f, 0x68, 0x74, 0x6d, 0x6c, 0x3b, 0x20, 0x63, 0x68,
      0x61, 0x72, 0x73, 0x65, 0x74, 0x3d, 0x75, 0x74, 0x66, 0x2d, 0x38, 0x0d,
      0x0a, 0x44, 0x61, 0x74, 0x65, 0x3a, 0x20, 0x57, 0x65, 0x64, 0x2c, 0x20,
      0x31, 0x35, 0x20, 0x4d, 0x61, 0x79, 0x20, 0x32, 0x30, 0x31, 0x33, 0x20,
      0x30, 0x30, 0x3a, 0x31, 0x31, 0x3a, 0x32, 0x36, 0x20, 0x47, 0x4d, 0x54,
      0x0d, 0x0a, 0x45, 0x74, 0x61, 0x67, 0x3a, 0x20, 0x22, 0x30, 0x39, 0x65,
      0x38, 0x61, 0x30, 0x61, 0x37, 0x63, 0x36, 0x36, 0x33, 0x62, 0x36, 0x31,
      0x36, 0x63, 0x33, 0x39, 0x36, 0x39, 0x31, 0x35, 0x32, 0x35, 0x31, 0x32,
      0x39, 0x63, 0x33, 0x33, 0x61, 0x22, 0x0d, 0x0a, 0x53, 0x65, 0x74, 0x2d,
      0x43, 0x6f, 0x6f, 0x6b, 0x69, 0x65, 0x3a, 0x20, 0x5f, 0x73, 0x65, 0x72,
      0x76, 0x65, 0x72, 0x5f, 0x73, 0x65, 0x73, 0x73, 0x69, 0x6f, 0x6e, 0x3d,
      0x42, 0x41, 0x68, 0x37, 0x42, 0x6b, 0x6b, 0x69, 0x44, 0x33, 0x4e, 0x6c,
      0x63, 0x33, 0x4e, 0x70, 0x62, 0x32, 0x35, 0x66, 0x61, 0x57, 0x51, 0x47,
      0x4f, 0x67, 0x5a, 0x46, 0x52, 0x6b, 0x6b, 0x69, 0x4a, 0x57, 0x55, 0x79,
      0x4f, 0x57, 0x49, 0x7a, 0x59, 0x54, 0x55, 0x30, 0x4e, 0x6a, 0x63, 0x77,
      0x59, 0x32, 0x5a, 0x68, 0x4d, 0x44, 0x45, 0x77, 0x4f, 0x57, 0x46, 0x68,
      0x4e, 0x7a, 0x42, 0x6c, 0x59, 0x54, 0x4a, 0x6d, 0x5a, 0x47, 0x59, 0x33,
      0x4e, 0x54, 0x67, 0x30, 0x42, 0x6a, 0x73, 0x41, 0x56, 0x41, 0x25, 0x33,
      0x44, 0x25, 0x33, 0x44, 0x2d, 0x2d, 0x36, 0x65, 0x39, 0x37, 0x62, 0x32,
      0x32, 0x37, 0x39, 0x33, 0x37, 0x32, 0x33, 0x34, 0x38, 0x35, 0x65, 0x31,
      0x36, 0x35, 0x33, 0x39, 0x62, 0x34, 0x36, 0x33, 0x64, 0x64, 0x64, 0x38,
      0x63, 0x31, 0x30, 0x34, 0x39, 0x62, 0x37, 0x37, 0x34, 0x63, 0x3b, 0x20,
      0x70, 0x61, 0x74, 0x68, 0x3d, 0x2f, 0x3b, 0x20, 0x48, 0x74, 0x74, 0x70,
      0x4f, 0x6e, 0x6c, 0x79, 0x0d, 0x0a, 0x53, 0x74, 0x61, 0x74, 0x75, 0x73,
      0x3a, 0x20, 0x32, 0x30, 0x30, 0x20, 0x4f, 0x4b, 0x0d, 0x0a, 0x58, 0x2d,
      0x52, 0x61, 0x63, 0x6b, 0x2d, 0x43, 0x61, 0x63, 0x68, 0x65, 0x3a, 0x20,
      0x69, 0x6e, 0x76, 0x61, 0x6c, 0x69, 0x64, 0x61, 0x74, 0x65, 0x2c, 0x20,
      0x70, 0x61, 0x73, 0x73, 0x0d, 0x0a, 0x58, 0x2d, 0x52, 0x65, 0x71, 0x75,
      0x65, 0x73, 0x74, 0x2d, 0x49, 0x64, 0x3a, 0x20, 0x32, 0x30, 0x62, 0x62,
      0x38, 0x37, 0x31, 0x66, 0x31, 0x31, 0x32, 0x66, 0x35, 0x33, 0x33, 0x38,
      0x33, 0x38, 0x38, 0x33, 0x36, 0x38, 0x35, 0x61, 0x38, 0x36, 0x38, 0x34,
      0x30, 0x30, 0x66, 0x64, 0x0d, 0x0a, 0x58, 0x2d, 0x52, 0x75, 0x6e, 0x74,
      0x69, 0x6d, 0x65, 0x3a, 0x20, 0x31, 0x2e, 0x31, 0x34, 0x37, 0x30, 0x30,
      0x32, 0x0d, 0x0a, 0x58, 0x2d, 0x55, 0x61, 0x2d, 0x43, 0x6f, 0x6d, 0x70,
      0x61, 0x74, 0x69, 0x62, 0x6c, 0x65, 0x3a, 0x20, 0x49, 0x45, 0x3d, 0x45,
      0x64, 0x67, 0x65, 0x2c, 0x63, 0x68, 0x72, 0x6f, 0x6d, 0x65, 0x3d, 0x31,
      0x0d, 0x0a, 0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x4c, 0x65,
      0x6e, 0x67, 0x74, 0x68, 0x3a, 0x20, 0x35, 0x37, 0x0d, 0x0a, 0x43, 0x6f,
      0x6e, 0x6e, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x3a, 0x20, 0x43, 0x6c,
      0x6f, 0x73, 0x65, 0x0d, 0x0a, 0x0d, 0x0a, 0x08, 0x02, 0x22, 0x0a, 0x31,
      0x31, 0x31, 0x31, 0x31, 0x61, 0x61, 0x61, 0x61, 0x61, 0x32, 0x29, 0x08,
      0x01, 0x1a, 0x25, 0x08, 0x01, 0x10, 0xec, 0xf3, 0x9b, 0xac, 0xea, 0x27,
      0x1a, 0x18, 0x43, 0x4f, 0x52, 0x5f, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31,
      0x5f, 0x64, 0x6f, 0x67, 0x27, 0x73, 0x20, 0x57, 0x68, 0x69, 0x73, 0x74,
      0x6c, 0x65, 0x20, 0x1e
    };
    size_t http_response_len = 616;
    pb_istream_t stream;
    WhistleMessage msg = {0};
    bool result;
    int16_t http_header_length;
    
    http_header_length = WHTTP_return_header_size((char*) http_response, http_response_len);
    
    /* Construct a pb_istream_t for reading from the buffer */
    stream = pb_istream_from_buffer((uint8_t*) http_response + http_header_length, http_response_len - http_header_length);
    
    result = pb_decode(&stream, WhistleMessage_fields, &msg);
    if (result < 0)
    {
        return -1;
    }
    
    corona_print("Whistle Message Type: ");
    switch(msg.objectType)
    {
        case WhistleMessageType_DATA_DUMP:
            break;
        case WhistleMessageType_REMOTE_DEV_MGMT:
            corona_print("Remote Dev Mgmt\r\n");
            corona_print("Message Type: %d\r\n", msg.remoteMgmtMsg.messageType);
            break;
        case WhistleMessageType_LOCAL_DEV_MGMT:
            break;
        default:
            corona_print("unknown\r\n");
    }
    /*
    **  it looks like the default return here should be 0, since errors above are -1
    */
    return(0);
}

int wmp_lm_checkin_req_gen()
{
    WhistleMessage msg = {0};
    LmMobileStatRequest lmMobStatReq = {0};
    encode_submsg_args_t lmArgs;
    uint_8 encode_buffer[50];
    pb_ostream_t encode_stream;
    bool res;

    encode_stream = pb_ostream_from_buffer(encode_buffer, 50);
    
    msg.has_objectType = true;
    msg.objectType = WhistleMessageType_LOCAL_DEV_MGMT;
    msg.has_transactionType = true;
    msg.transactionType = TransactionType_TRANS_REQUEST;
    
    msg.serialNumber.arg = g_st_cfg.fac_st.serial_number;
    msg.serialNumber.funcs.encode = encode_whistle_str;
    
    msg.has_localMgmtMsg = true;
    msg.localMgmtMsg.has_messageType = true;
    msg.localMgmtMsg.messageType = LmMessageType_LM_MOBILE_STAT_REQ;
    lmArgs.pb_obj = &lmMobStatReq;
    lmArgs.fields = LmMobileStatRequest_fields;
    msg.localMgmtMsg.payload.arg = &lmArgs;
    msg.localMgmtMsg.payload.funcs.encode = encode_submsg_with_tag;
    
    lmMobStatReq.has_initType = true;
    lmMobStatReq.initType = LmInitType_LM_INIT_MANUAL;

    res = pb_encode(&encode_stream, WhistleMessage_fields, &msg);
    
    return 0;
}

int wmp_lm_checkin_resp_parse()
{
    /*
     * objectType: LOCAL_DEV_MGMT
     * transactionType: TRANS_RESPONSE_ACK
     * serialNumber: "COR-000009"
     * localMgmtMsg {
     *   messageType: LM_CHECK_IN_RESP
     *   payload: "\010\r"
     * }
     * 
     * payload as LmCheckInResponse:
     * mobileStatus: MOBILE_STATUS_DEV_MGMT
     */
    unsigned char test_wmp[] = {
      0x08, 0x03, 0x10, 0x02, 0x22, 0x0a, 0x43, 0x4f, 0x52, 0x2d, 0x30, 0x30,
      0x30, 0x30, 0x30, 0x39, 0x3a, 0x07, 0x08, 0x02, 0xaa, 0x01, 0x02, 0x08,
      0x0d
    };
    unsigned int test_wmp_len = 25;
    
    WhistleMessage msg = {0};
    LmMobileStat lmMobileStat = {0};
    encode_submsg_args_t lmArgs = {0};
    uint_8 encode_buffer[50];
    pb_istream_t stream;
    bool res=0;

    /* Construct a pb_istream_t for reading from the buffer */
    stream = pb_istream_from_buffer(test_wmp, test_wmp_len);
    lmArgs.pb_obj = &lmMobileStat;
    lmArgs.fields = LmMobileStat_fields;
    msg.localMgmtMsg.payload.arg = &lmArgs;
    msg.localMgmtMsg.payload.funcs.decode = decode_whistle_submsg;
    
    if(!pb_decode(&stream, WhistleMessage_fields, &msg))
    {
        corona_print("FAIL: could not decode\r\n");
        return -1;
    }
    
    if (!msg.has_serialNumber)
    {
        corona_print("WARN: no WMP ser #\n");
        res -= 1;
    }

    if (!msg.has_objectType)
    {
        corona_print("FAIL: WMP ObjectType not present\r\n");
        res -= 1;
    }
    else if (msg.objectType != WhistleMessageType_LOCAL_DEV_MGMT)
    {
        corona_print("FAIL: WMP ObjectType not LocalDevMgmt\r\n");
        res -= 1;
    }
    
    if (!msg.has_transactionType)
    {
        corona_print("FAIL: WMP TransactionType not present\r\n");
        res -= 1;
    }
    else if (msg.transactionType != TransactionType_TRANS_RESPONSE_ACK)
    {
        corona_print("FAIL: WMP TransactionType not Response ACK\r\n");
        res -= 1;
    }
    
    if (!msg.localMgmtMsg.has_messageType)
    {
        corona_print("FAIL: LocalMgmt MsgType not present\r\n");
        res -= 1;
    }
    
    // NOTE: if checking for a asyn notification from the mobile app then the
    //         messageType would be LmMessageType_LM_MOBILE_STAT_NOTIFY
    else if (msg.localMgmtMsg.messageType != LmMessageType_LM_MOBILE_STAT_RESP)
    {
        corona_print("FAIL: LocalMgmt MsgType not CheckInResp\r\n");
        res -= 1;
    }
    
    if (!lmArgs.exists)
    {
        corona_print("FAIL: LMCheckInResp not present\r\n");
        res -= 1;
    }
    else
    {
        if (!lmMobileStat.has_status)
        {
            corona_print("FAIL: LMCheckInResp Status not present\r\n");
            res -= 1;
        }
        else if (lmMobileStat.status != LmMobileStatus_MOBILE_STATUS_DEV_MGMT)
        {
            corona_print("FAIL: LMCheckInResp Status not MobileStatusDevMgmt\r\n");
            res -= 1;
        }
    }
    if (res >= 0)
        corona_print("PASS!\r\n");
    
    return res;
}
#endif
