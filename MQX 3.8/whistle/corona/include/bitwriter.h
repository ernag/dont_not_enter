//
//  stream_bitwriter.h
//  nanopb_test
//
//  Created by Kevin Lloyd on 4/7/13.
//  Copyright (c) 2013 Kevin Lloyd. All rights reserved.
//

#ifndef nanopb_test_stream_bitwriter_h
#define nanopb_test_stream_bitwriter_h

#include <stdint.h>
#include "pb.h"

#define USE_32BIT_BUFFER 1

typedef struct _ACCENC_bitwriter_t {
    pb_ostream_t *out_stream;
    uint32_t bit_offset;
#if (USE_32BIT_BUFFER)
    uint32_t internal_buffer;
#else
    uint8_t internal_buffer;
#endif
    
    uint32_t *crc;
} ACCENC_bitwriter_t;

void ACCENC_bitwriter_init(ACCENC_bitwriter_t *bw, pb_ostream_t *out_stream, uint32_t *crc_ptr);

bool ACCENC_bitwriter_inc(ACCENC_bitwriter_t *bw, uint32_t n_bits);

bool ACCENC_bitwriter_put_false(ACCENC_bitwriter_t *bw);

bool ACCENC_bitwriter_close(ACCENC_bitwriter_t *bw);

#endif
