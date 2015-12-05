//
//  accel_encode.h
//  nanopb_test
//
//  Created by Kevin Lloyd on 4/9/13.
//  Copyright (c) 2013 Kevin Lloyd. All rights reserved.
//

#ifndef wmp_accel_encode_h
#define wmp_accel_encode_h

#include "whistlemessage.pb.h"
#include "datamessage.pb.h"
#include "bitwriter.h"
#include "wmp.h"

#define ACCEL_PAYLOAD_HEADER_SIZE 4 //1 byte for tag, 3 bytes for length

// Used for oneshot callback if AccelData payload callback is directly called.
typedef struct _WMP_ACCENC_payload_cb_args_t {
    AccelDataEncoding encodingType;
    int8_t *src_payload;
    size_t src_payload_size;
    uint32_t *thirtytwo_bit_crc;
} WMP_ACCENC_payload_cb_args_t;

typedef struct _WMP_ACCENC_partial_handle_t
{
    pb_ostream_t *stream;
    AccelDataEncoding encodingType;
    uint32_t *thirtytwo_bit_crc;
    int8_t _accel_inline_diff_baseline[3];
    bool write_header;
    WMP_encode_partial_state_args accel_payload_states;
    ACCENC_bitwriter_t bit_writer;
} WMP_ACCENC_partial_handle_t;

bool WMP_ACCEL_payload_encode_oneshot_cb(pb_ostream_t *stream, const pb_field_t *field, const void *arg);
bool WMP_ACCEL_payload_encode_init(WMP_ACCENC_partial_handle_t *pPartialMessageHandle,
        pb_ostream_t *stream,
        AccelDataEncoding encodingType,
        uint32_t *thirtytwo_bit_crc,
        bool write_header);
bool WMP_ACCEL_payload_encode_append(WMP_ACCENC_partial_handle_t *pPartialHandle, 
        int8_t *src_payload,
        size_t src_payload_size);
bool WMP_ACCEL_payload_encode_close(WMP_ACCENC_partial_handle_t *pPartialHandle);
void WMP_ACCEL_header_encode(pb_ostream_t *stream, 
        uint_64 rel_time, 
        uint_64 abs_time, 
        AccelDataEncoding encoding_type, 
        uint_8 sample_freq,
        uint_8 full_scale);
void WMP_ACCEL_footer_encode(pb_ostream_t *stream, uint_32 payload_crc32);
#endif
