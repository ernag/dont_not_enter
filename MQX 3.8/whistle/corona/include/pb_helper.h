/*
 * pb_helper.h
 *
 *  Created on: May 14, 2013
 *      Author: SpottedLabs
 */

#ifndef PB_HELPER_H_
#define PB_HELPER_H_

typedef enum _WMP_generic_enum {
	UNUSED
} WMP_generic_enum;

typedef struct _WMP_sized_string_cb_args_t {
    char *pPayload;
    size_t payloadMaxSize;
    bool exists; // equivalent to has_ field
} WMP_sized_string_cb_args_t;

typedef struct _encode_submsg_args_t {
    const pb_field_t *fields;
    void *pb_obj;
    bool exists; // equivalent to has_ field
} encode_submsg_args_t;

typedef struct _decode_explicit_submsg_args_t {
    WMP_generic_enum desiredMsgType;
    WMP_generic_enum *pActualMsgType;
    const pb_field_t *fields;
    void *pb_obj; // object to store decoded values into
    bool exists; // equivalent to has_ field
} decode_explicit_submsg_args_t;

//typedef struct _decode_generic_submsg_args_t {
//    WMP_generic_enum *pActualMsgType; // pointer to the field in the parent WMP message that holds the message type
//    const pb_field_t* (*field_lookup)(WMP_generic_enum desiredMsgType);
//    void *pb_obj; // object to store decoded values into
//    bool exists; // equivalent to has_ field
//} decode_generic_submsg_args_t;

typedef struct _decode_deferred_submsg_args_t {
    void *pMsg;
    size_t msg_size;
    bool exists; // equivalent to has_ field
} decode_deferred_submsg_args_t;

bool decode_whistle_str(pb_istream_t *stream, const pb_field_t *field, void *arg);

bool decode_whistle_str_sized(pb_istream_t *stream, const pb_field_t *field, void *arg);

bool decode_whistle_submsg(pb_istream_t *stream, const pb_field_t *field, void *arg);

bool decode_whistle_explicit_detached_submsg_parse(pb_istream_t *stream, const pb_field_t *field, void *arg);

bool decode_whistle_deferred_detached_submsg(pb_istream_t *stream, const pb_field_t *field, void *arg);

bool encode_whistle_str(pb_ostream_t *stream, const pb_field_t *field, const void *encode_args);

bool encode_whistle_str_sized(pb_ostream_t *stream, 
        const pb_field_t *field, 
        const void *encode_args);

bool encode_submsg_with_tag(pb_ostream_t *stream, const pb_field_t *field, const void *encode_args);



#endif /* PB_HELPER_H_ */
