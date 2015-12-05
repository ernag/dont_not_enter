//
//  stream_bitwriter.c
//  nanopb_test
//
//  Created by Kevin Lloyd on 4/8/13.
//  Copyright (c) 2013 Kevin Lloyd. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bitwriter.h"
#include "pb_encode.h"


#if (USE_32BIT_BUFFER)
// TODO: Make this whole operation work using a 32 or 64 bit internal buffer
//          Note that the size of the Native Americans doing the writing will effect this.
//          I am assuming we will only use little endian
static const uint32_t g_byte_mask[32] = {0xFFFFFF7F, 0xFFFFFFBF, 0xFFFFFFDF, 0xFFFFFFEF, 
                                         0xFFFFFFF7, 0xFFFFFFFB, 0xFFFFFFFD, 0xFFFFFFFE,
                                         0xFFFF7FFF, 0xFFFFBFFF, 0xFFFFDFFF, 0xFFFFEFFF,
                                         0xFFFFF7FF, 0xFFFFFBFF, 0xFFFFFDFF, 0xFFFFFEFF,
                                         0xFF7FFFFF, 0xFFBFFFFF, 0xFFDFFFFF, 0xFFEFFFFF,
                                         0xFFF7FFFF, 0xFFFBFFFF, 0xFFFDFFFF, 0xFFFEFFFF,
                                         0x7FFFFFFF, 0xBFFFFFFF, 0xDFFFFFFF, 0xEFFFFFFF,
                                         0xF7FFFFFF, 0xFBFFFFFF, 0xFDFFFFFF, 0xFEFFFFFF};

static const uint32_t g_initial_value = 0xFFFFFFFF;
static const uint32_t g_max_offset = 32;
static const uint32_t g_bytes_to_flush = 4;
#else
// -- Global Private Data
// TODO: Make this whole operation work using a 32 or 64 bit internal buffer
//          Note that Native American-ness will most likely effect this
static const uint8_t g_byte_mask[8] = {0x7F, 0xBF, 0xDF, 0xEF, 
                                       0xF7, 0xFB, 0xFD, 0xFE};
static const uint8_t g_initial_value = 0xFF;
static const uint32_t g_max_offset = 8;
static const uint32_t g_bytes_to_flush = 1;
#endif

// -- Private Prototypes
static bool _ACCENC_bitwriter_flush(ACCENC_bitwriter_t *bw);

/* Flush the current internal buffer to the protobuf stream.
 */
static bool _ACCENC_bitwriter_flush(ACCENC_bitwriter_t *bw) {
    int i;
    if (bw->bit_offset == 0)
        return true;
    
    if (!pb_write(bw->out_stream, (uint8_t *)&(bw->internal_buffer), g_bytes_to_flush))
        return false;
    
    bw->internal_buffer = g_initial_value;
    return true;
}

/* Initialize the bitwriter struct
 */
void ACCENC_bitwriter_init(ACCENC_bitwriter_t *bw,
                        pb_ostream_t *out_stream,
                        uint32_t *crc_ptr) {
    bw->out_stream = out_stream;
    bw->internal_buffer = g_initial_value;
    bw->bit_offset = 0;
    bw->crc = crc_ptr;
}

/* Increment the current bitwriter offset.  This is equivalent to writing 
 *  n_bits 1s to the internal buffer.  This was done to avoid having to write
 *  each bit individually 
 */
bool ACCENC_bitwriter_inc(ACCENC_bitwriter_t *bw, uint32_t n_bits)
{
    bw->bit_offset += n_bits;
    while (bw->bit_offset >= g_max_offset)
    {
        if (!_ACCENC_bitwriter_flush(bw))
            return false;
        
        bw->bit_offset -= g_max_offset;
    }
    return true;
}

/* Write a 0 to the internal buffer and increment the pointer
 */
bool ACCENC_bitwriter_put_false(ACCENC_bitwriter_t *bw)
{
    bw->internal_buffer &= g_byte_mask[bw->bit_offset];
    
    return ACCENC_bitwriter_inc(bw, 1);
}

/* Close the current stream by flushing it if we have a partially filled block
 */
bool ACCENC_bitwriter_close(ACCENC_bitwriter_t *bw) 
{ 
    if (bw == NULL)
        return false;
    
    else if (bw->bit_offset != 0)
    {   
        // TODO: Make is so that only the last used byte is flushed instead of the full block size
        return _ACCENC_bitwriter_flush(bw);
    }    
    
    // If the bit_offset was zero that means we just finished a byte so we are done
    return true;
}
