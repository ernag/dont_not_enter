#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqx.h>
#include "wassert.h"
#include "cormem.h"
#include "pb.h"
#include "pb_encode.h"
#include "pb_encode_whistle.h"
#include "whistlemessage.pb.h"
#include "wmp.h"
#include "wmp_datadump_encode.h"
#include "corona_console.h"

/*
 *   Allocate memory for a whistle message.
 *   Do this in a single place so we don't waste memory on a bunch of wassert()'s.
 */
void _WMP_whistle_msg_alloc( void **pMsg )
{
    *pMsg = walloc( sizeof(WhistleMessage) );
}

/*
 *   Free memory for a whistle message.
 */
void _WMP_whistle_msg_free( void **pMsg )
{
    wfree(*pMsg);
}

bool WMP_write_fake_field_tag_length(pb_ostream_t *stream, WMP_encode_partial_state_args *partial_args, size_t anticipated_size)
{
    // Move out_stream pointer to leave room for writing header.
    uint8_t *pPaddingArray;
    
    partial_args->stream_state_payload_header_start = stream->state;
    
    pPaddingArray = walloc(anticipated_size * sizeof(uint8_t));
    
    memset(pPaddingArray, 0xAB, anticipated_size);
    if (!pb_write(stream, pPaddingArray, anticipated_size))
    {
        wfree(pPaddingArray);
        return false;
    }
    
    wfree(pPaddingArray);
    
    partial_args->stream_state_payload_start = stream->state;
    return true;
}

/*
 *   Return an index into a pb_field_t array, given a tag.
 */
uint16_t _WMP_get_idx_from_tag(const pb_field_t *pFields, const uint16_t tag)
{
    uint16_t idx = 0;
    
    while( 0 != pFields[idx].tag )
    {
        if( pFields[idx].tag == tag )
        {
            goto done_idx_tag;
        }
        
        idx++;
    }

    wassert(0);
    
done_idx_tag:
    return idx;
}

bool WMP_rewrite_real_field_tag_length(pb_ostream_t *out_stream,
                                const pb_field_t *field,
                                WMP_encode_partial_state_args *partial_args) {
    // Setup stream for calculating header size (without actually writting)
    pb_ostream_t stream = {0,0,0,0};
    
    // typecasting to uint8_t* below breaks ambiguity of the function :(
    size_t header_len = (uint8_t*) partial_args->stream_state_payload_start - 
            (uint8_t*) partial_args->stream_state_payload_header_start;
    size_t payload_len = (uint8_t*) out_stream->state - (uint8_t*) partial_args->stream_state_payload_start;
    uint8_t *encoded_data_ptr = (uint8_t*) partial_args->stream_state_payload_start;
    
    // Calculate header size
    if (!pb_encode_tag_for_field(&stream, field))
        return false;
    if (!pb_encode_varint(&stream, (uint64_t) payload_len))
        return false;
    
    // Check if header size different than previously written and if so move contents
    if (stream.bytes_written != header_len)
    {
        corona_print("WMP:WARN: shift data on tag rewr (%d bytes)\n", stream.bytes_written - header_len);
        pb_ostream_buffer_shift_contents(out_stream,
                                         encoded_data_ptr,
                                         payload_len,
                                         stream.bytes_written - header_len);
    }
    
    // Set stream to actually write to head of buffer
    pb_ostream_from_buffer_copy_reset(&stream, out_stream);
    stream.state = partial_args->stream_state_payload_header_start;
    
    if (!pb_encode_tag_for_field(&stream, field))
        return false;
    if (!pb_encode_varint(&stream, (uint64_t) payload_len))
        return false;
    
    return true;
}

