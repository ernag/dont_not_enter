//
//  main.c
//  nanopb_test
//
//  Created by Kevin Lloyd on 4/4/13.
//  Copyright (c) 2013 Kevin Lloyd. All rights reserved.
//

#include <mqx.h>
#include <bsp.h>
#include <time.h>
#include <string.h>
#include "cormem.h"
#include "accel_hal.h"
#include "pb.h"
#include "pb_encode.h"
#include "bitwriter.h"
#include "crc_util.h"
#include "wassert.h"
#include "wmp.h"
#include "wmp_accel_encode.h"
#include "whistlemessage.pb.h"
#include "datamessage.pb.h"


// TODO: Take the data directly from the DMA buffer which is 16 bit and encode it

// We must use a 16 bit array to hold the output of the derivative
// This buffer must be at least as big as the maximum number of samples that can be appended to the WMP message at once
static int16_t g_diff_payload[ACCEL_FIFO_WTM_THRESHOLD * 3];

bool WMP_ACCEL_payload_encode_riceSigned_init(ACCENC_bitwriter_t *bw, pb_ostream_t *out_stream, uint32_t *crc_ptr)
{
    ACCENC_bitwriter_init(bw, out_stream, crc_ptr);
    return TRUE;
}

bool WMP_ACCEL_payload_encode_riceSigned_closed(ACCENC_bitwriter_t *bw)
{
    ACCENC_bitwriter_close(bw);
    return TRUE;
}

// We must use a 16 bit array to hold the output of the derivative
bool _WMP_ACCEL_payload_encode_riceSigned(int16_t *source, size_t source_len, uint8_t k, ACCENC_bitwriter_t *bw) {
    size_t i;
    uint8_t j;
    int16_t snum;
    uint32_t unum, q, v;
    bool return_val=true;
    
    // Rather than polluting the code with ifs and returns I just return at the end. Not sure this is a great idea...
    for (i=0; i < source_len; i++)
    {
        //TODO: maybe remove snum and directly access source[i]?
        snum = source[i];
        unum = ( snum >= 0 ? 2 * snum : (-2 * snum) -1 );
        q = unum >> k;
        
        if (q > 0)
        {
            return_val &= ACCENC_bitwriter_inc(bw, q);
        }
        
        return_val &= ACCENC_bitwriter_put_false(bw);
        
        v = 1;
        for (j=0; j < k; j++)
        {
            if ((v & unum) > 0)
            {
                return_val &= ACCENC_bitwriter_inc(bw, 1);
            } else
            {
                return_val &= ACCENC_bitwriter_put_false(bw);
            }
            v <<= 1;
        }
    }
    return return_val;
}

/*
 source: source of int8_t accel readings assuming ordered as triplets
 source_len: number of readings to process (e.g., {1,2,3,4,5,6} would be 6 readings)
 seed: triplet of numbers to be the basis for comparing the first three numbers in source.
 */
void _WMP_ACCEL_payload_inline_triplet_diff(int8_t *source, int16_t *output, size_t source_len, int8_t last[3])
{ 
    size_t i;
    wassert(source_len <= (ACCEL_FIFO_WTM_THRESHOLD * 3));
    for (i=0; i<source_len; i+=3)
    {
        output[i] = source[i] - last[0];
        output[i+1] = source[i+1] - last[1];
        output[i+2] = source[i+2] - last[2];
        memcpy(last, &(source[i]), 3);
    }
}


bool WMP_ACCEL_payload_encode_s8(int8_t *source, size_t source_len, pb_ostream_t *out_stream, uint32_t *pCrc)
{    
    int i = 0;
    return pb_write(out_stream, (uint8_t*) source, source_len);
}

bool WMP_ACCEL_payload_encode_s16(int16_t *source, size_t source_len, pb_ostream_t *out_stream, uint32_t *pCrc)
{    
    int i = 0;
    source_len *= 2; // Since actually 16 bit numbers instead of 8 bit
    
    // Note: This assumes we have the same endianness on all platforms
    // TODO: Make this invariant with respect to endianness
    if (!pb_write(out_stream, (uint8_t*) source, source_len)) 
        return false;
    
    *pCrc = UTIL_crc32_progressive_calc((uint8_t*) source, source_len, *pCrc);
    return true;
}

bool _WMP_ACCEL_payload_encoding_requires_diff(AccelDataEncoding type)
{
    switch (type)
    {
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K0:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K1:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K2:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K3:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K4:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K5:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K6:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K7:
        case AccelDataEncoding_ACCEL_ENC_DIFF_S16:
            return true;
        default:
            return false;
    }
    return false;
}


bool _WMP_ACCEL_payload_encoding_requires_riceSigned(AccelDataEncoding type)
{
    switch (type)
    {
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K0:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K1:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K2:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K3:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K4:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K5:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K6:
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K7:
            return true;
        default:
            return false;
    }
    return false;
}

bool WMP_ACCEL_payload_encode_init(WMP_ACCENC_partial_handle_t *pPartialMessageHandle,
        pb_ostream_t *stream,
        AccelDataEncoding encodingType,
        uint32_t *thirtytwo_bit_crc,
        bool write_header)
{
    pPartialMessageHandle->_accel_inline_diff_baseline[0] = 0;
    pPartialMessageHandle->_accel_inline_diff_baseline[1] = 0;
    pPartialMessageHandle->_accel_inline_diff_baseline[2] = 0;
    pPartialMessageHandle->thirtytwo_bit_crc = thirtytwo_bit_crc;
    *(pPartialMessageHandle->thirtytwo_bit_crc) = PROGRESSIVE_CRC_INIT;
    pPartialMessageHandle->encodingType = encodingType;
    pPartialMessageHandle->write_header = write_header;
    pPartialMessageHandle->stream = stream;
    pPartialMessageHandle->accel_payload_states.stream_state_payload_header_start = NULL;
    pPartialMessageHandle->accel_payload_states.stream_state_payload_start = NULL;
    pPartialMessageHandle->bit_writer.out_stream = NULL;
    
    if (_WMP_ACCEL_payload_encoding_requires_riceSigned(encodingType))
        WMP_ACCEL_payload_encode_riceSigned_init(&(pPartialMessageHandle->bit_writer), stream, thirtytwo_bit_crc);
    
    if (write_header)
    {
        if (!WMP_write_fake_field_tag_length(stream, &(pPartialMessageHandle->accel_payload_states), ACCEL_PAYLOAD_HEADER_SIZE))
            return false;
    } else
    {
        // We aren't writing a header but should still set these variables
        pPartialMessageHandle->accel_payload_states.stream_state_payload_header_start = stream->state;
        pPartialMessageHandle->accel_payload_states.stream_state_payload_start = stream->state;
    }
    
    return true;
}

bool WMP_ACCEL_payload_encode_close(WMP_ACCENC_partial_handle_t *pPartialMessageHandle)
{
    static uint16_t accel_idx = BOGUS_TAG;
    uint8_t* payload_start;
    uint32_t payload_len_bytes;
    
    if(BOGUS_TAG == accel_idx)
    {
        accel_idx = _WMP_get_idx_from_tag(AccelData_fields, ACCEL_DATA_PAYLOAD_TAG);
    }
    
    if (_WMP_ACCEL_payload_encoding_requires_riceSigned(pPartialMessageHandle->encodingType))
        WMP_ACCEL_payload_encode_riceSigned_closed(&(pPartialMessageHandle->bit_writer));
    
    // Calculate CRC prior to moving data so payload start is still correct
    payload_start = (uint8_t*) pPartialMessageHandle->accel_payload_states.stream_state_payload_start;
    payload_len_bytes = (uint8_t*) pPartialMessageHandle->stream->state - payload_start;
    *(pPartialMessageHandle->thirtytwo_bit_crc) = UTIL_crc32_progressive_calc(payload_start, payload_len_bytes, 
                                                                              *(pPartialMessageHandle->thirtytwo_bit_crc));  
    
    if (pPartialMessageHandle->write_header)
    {
        //Re-write the AccelData.Payload header
        if (!WMP_rewrite_real_field_tag_length(pPartialMessageHandle->stream,
        		&(AccelData_fields[accel_idx]),
        		&(pPartialMessageHandle->accel_payload_states)))
            return false;
    }
    return true;
}

bool WMP_ACCEL_payload_encode_append(WMP_ACCENC_partial_handle_t *pPartialMsgHandle, 
        int8_t *input_payload,
        size_t src_payload_size)
{
    // If the compression mechanism needs a diff the src_payload must be 16 bit due to the derivative doubling the range
    // Otherwise keep the src_payload as the int8_t payload 
    void *src_payload;
    if (_WMP_ACCEL_payload_encoding_requires_diff(pPartialMsgHandle->encodingType))
    {
        _WMP_ACCEL_payload_inline_triplet_diff(input_payload,
                g_diff_payload,
                src_payload_size, 
                pPartialMsgHandle->_accel_inline_diff_baseline);
        src_payload = (void *) g_diff_payload;
    } else
    {
        src_payload = (void *) input_payload;
    }
    
    switch (pPartialMsgHandle->encodingType)
    {
        case AccelDataEncoding_ACCEL_ENC_S8:
            if (!WMP_ACCEL_payload_encode_s8((int8_t *)src_payload, src_payload_size, pPartialMsgHandle->stream, pPartialMsgHandle->thirtytwo_bit_crc))
                return false;
            break;
        case AccelDataEncoding_ACCEL_ENC_DIFF_S16:
            if (!WMP_ACCEL_payload_encode_s16((int16_t *)src_payload, src_payload_size, pPartialMsgHandle->stream, pPartialMsgHandle->thirtytwo_bit_crc))
                return false;
            break;
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K0:
            if (!_WMP_ACCEL_payload_encode_riceSigned((int16_t *)src_payload, src_payload_size, 0, &(pPartialMsgHandle->bit_writer)))
                return false;
            break;
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K1:
            if (!_WMP_ACCEL_payload_encode_riceSigned((int16_t *)src_payload, src_payload_size, 1, &(pPartialMsgHandle->bit_writer)))
                return false;
            break;
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K2:
            if (!_WMP_ACCEL_payload_encode_riceSigned((int16_t *)src_payload, src_payload_size, 2, &(pPartialMsgHandle->bit_writer)))
                return false;
            break;
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K3:
            if (!_WMP_ACCEL_payload_encode_riceSigned((int16_t *)src_payload, src_payload_size, 3, &(pPartialMsgHandle->bit_writer)))
                return false;
            break;
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K4:
            if (!_WMP_ACCEL_payload_encode_riceSigned((int16_t *)src_payload, src_payload_size, 4, &(pPartialMsgHandle->bit_writer)))
                return false;
            break;
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K5:
            if (!_WMP_ACCEL_payload_encode_riceSigned((int16_t *)src_payload, src_payload_size, 5, &(pPartialMsgHandle->bit_writer)))
                return false;
            break;
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K6:
            if (!_WMP_ACCEL_payload_encode_riceSigned((int16_t *)src_payload, src_payload_size, 6, &(pPartialMsgHandle->bit_writer)))
                return false;
            break;
        case AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K7:
            if (!_WMP_ACCEL_payload_encode_riceSigned((int16_t *)src_payload, src_payload_size, 7, &(pPartialMsgHandle->bit_writer)))
                return false;
            break;
        default:
            return false;
    }

    return true;
}

bool WMP_ACCEL_payload_encode_oneshot_cb(pb_ostream_t *stream, const pb_field_t *field, const void *encode_args)
{
    WMP_ACCENC_partial_handle_t partial_msg_handle;
    
    WMP_ACCEL_payload_encode_init(&partial_msg_handle, 
            stream,
            ((WMP_ACCENC_payload_cb_args_t*) encode_args)->encodingType,
            (uint32_t*) ((WMP_ACCENC_payload_cb_args_t*) encode_args)->thirtytwo_bit_crc,
            true);
    
    if (!WMP_ACCEL_payload_encode_append(&partial_msg_handle,
            ((WMP_ACCENC_payload_cb_args_t*) encode_args)->src_payload,
            ((WMP_ACCENC_payload_cb_args_t*) encode_args)->src_payload_size))
        return false;
    
    if (!WMP_ACCEL_payload_encode_close(&partial_msg_handle))
        return false;
    
    return true;
}

void WMP_ACCEL_header_encode(pb_ostream_t *stream, 
        uint_64 rel_time, 
        uint_64 abs_time, 
        AccelDataEncoding encoding_type, 
        uint_8 sample_freq,
        uint_8 full_scale
        )
{
    AccelData *pData;
    
    pData = walloc( sizeof(AccelData) );
    
    pData->has_encodingType = true;
    pData->encodingType = encoding_type;
    pData->has_sampleFrequency = true;
    pData->sampleFrequency = sample_freq;
    pData->has_relativeStartTime = true;
    pData->relativeStartTime = rel_time;
    pData->has_range = true;
    pData->range = full_scale;
    
    if (abs_time != 0)
    {
        pData->has_absoluteStartTime = true;
        pData->absoluteStartTime = abs_time;
    }
    else
        pData->has_absoluteStartTime = false;
        
    wassert( pb_encode(stream, AccelData_fields, pData) );
    wfree(pData);
}

void WMP_ACCEL_footer_encode(pb_ostream_t *stream, uint_32 payload_crc32)
{
    AccelData *pData;
    
    pData = walloc( sizeof(AccelData) );
    
    pData->has_thirtyTwoBitCRCPostPayload = true;
    pData->thirtyTwoBitCRCPostPayload = payload_crc32;
    
    wassert( pb_encode(stream, AccelData_fields, pData) );
    wfree(pData);
}
