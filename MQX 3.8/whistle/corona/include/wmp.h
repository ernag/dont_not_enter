/*
 * wmp.h
 *
 *  Created on: Apr 11, 2013
 *      Author: SpottedLabs
 */

#ifndef WMP_H_
#define WMP_H_

#define ACCEL_DATA_TAG  20
#define ACCEL_DATA_PAYLOAD_TAG  20
#define BOGUS_TAG       0xFFFF

typedef struct _WMP_encode_partial_state_args_t
{
    void *stream_state_payload_header_start;
    void *stream_state_payload_start;
} WMP_encode_partial_state_args;

bool WMP_write_fake_field_tag_length(pb_ostream_t *stream, 
        WMP_encode_partial_state_args *partial_args, 
        size_t anticipated_size);

bool WMP_rewrite_real_field_tag_length(pb_ostream_t *out_stream,
        const pb_field_t *field,
        WMP_encode_partial_state_args *partial_args);

uint16_t _WMP_get_idx_from_tag(const pb_field_t *pFields, const uint16_t tag);

void _WMP_whistle_msg_alloc( void **pMsg );
void _WMP_whistle_msg_free( void **pMsg );

#define WMP_whistle_msg_alloc(x)    _WMP_whistle_msg_alloc((void **)x)
#define WMP_whistle_msg_free(x)     _WMP_whistle_msg_free((void **)x)

#endif /* WMP_H_ */
