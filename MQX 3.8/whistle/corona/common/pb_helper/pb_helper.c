#include <mqx.h>
#include "pb.h"
#include "pb_decode.h"
#include "pb_helper.h"
#include "wassert.h"
#include "app_errors.h"

/*
 *   Decodes any arbitrary Whistle Config String.
 *     arg should be address of the string in static RAM buffer you want to write to.
 */
bool decode_whistle_str(pb_istream_t *stream, const pb_field_t *field, void *arg)
{
    wassert(arg != NULL);
    
    if ( !pb_read(stream, arg, stream->bytes_left) )  // this puts the decoded string into whatever buffer we passed in.
        return false;
    
    //corona_print("dws: %s\n", (char *)arg); fflush(stdout); // debug only.
    return true;
}

bool decode_whistle_str_sized(pb_istream_t *stream, const pb_field_t *field, void *arg)
{
    size_t copy_size = 0;
    
    wassert(arg != NULL);
    
    if (stream->bytes_left > ((WMP_sized_string_cb_args_t*) arg)->payloadMaxSize - 1)
        copy_size = ((WMP_sized_string_cb_args_t*) arg)->payloadMaxSize - 1;
    else
        copy_size = stream->bytes_left;

    if ( !pb_read(stream, 
            (uint8_t *) ((WMP_sized_string_cb_args_t*) arg)->pPayload,
            copy_size) )  // this puts the decoded string into whatever buffer we passed in.
        return false;
    
    ((uint8_t *) ((WMP_sized_string_cb_args_t*) arg)->pPayload)[copy_size] = 0;
    
    ((WMP_sized_string_cb_args_t*) arg)->exists = true;
    
    return true;
}

bool decode_whistle_submsg(pb_istream_t *stream, const pb_field_t *field, void *arg)
{   
    encode_submsg_args_t *submsg_args = (encode_submsg_args_t*) arg;
    
    // TODO: I think we might be able to just use (const pb_field_t*)field->ptr instead of submsg_args->fields
    if (pb_decode(stream, submsg_args->fields, submsg_args->pb_obj))
    {
        submsg_args->exists = true;
        return true;
    }
    else
        return false;
}

/*
 * Function: decode_whistle_halt_submsg_parse
 * 
 * Purpose:
 *   This callback is used to decode a detached submsg (i.e. payload is casted to 'bytes'
 *   and type needs to be inferred from sibling field 'msg_type'). This callback also
 *   explicitly makes sure that the msg_type is of a particular type (specified in the
 *   args var). 
 *    
 */
bool decode_whistle_explicit_detached_submsg_parse(pb_istream_t *stream, const pb_field_t *field, void *arg)
{   
    decode_explicit_submsg_args_t *submsg_args = (decode_explicit_submsg_args_t*) arg;
    
    if (*(submsg_args->pActualMsgType) != submsg_args->desiredMsgType)
    {
        WPRINT_ERR("msg type mismatch");
        return false;
    }
    
    if (pb_decode(stream, submsg_args->fields, submsg_args->pb_obj))
    {
        submsg_args->exists = true;
        return true;
    }
    else
        return false;
}

/*
 * Function: decode_whistle_generic_detached_submsg
 * 
 * Purpose:
 *   This callback is used to decode a detached submsg whose type is not yet
 *   known.
 *   NOTE: this function will malloc the space for the ProtoBuf object
 */
/*
 * Did not compelete this function, still would need to add the dynamic malloc
 * of the pb_obj based on the interpreted pActualMsgType
 */
//bool decode_whistle_generic_detached_submsg(pb_istream_t *stream, const pb_field_t *field, void *arg)
//{   
//    pb_field_t *field;
//    
//    decode_generic_submsg_args_t *submsg_args = (decode_generic_submsg_args_t*) arg;
//    
//    field = submsg_args->field_lookup(*(submsg_args->pActualMsgType));
//    if (field == NULL)
//    {
//        corona_print("PB ERROR: no field found for msgType\n");
//        return false;
//    }
//    
//    if (pb_decode(stream, submsg_args->fields, submsg_args->pb_obj))
//    {
//        submsg_args->exists = true;
//        return true;
//    }
//    else
//        return false;
//} 

bool decode_whistle_deferred_detached_submsg(pb_istream_t *stream, const pb_field_t *field, void *arg)
{
    decode_deferred_submsg_args_t *submsg_args = (decode_deferred_submsg_args_t*) arg;

    // We know that this function is being called from decode_callback_field where the stream
    //   that we is passed is a sub-stream for this particular field where the length is the
    //   length of the string/array for this field. Thus to skip it we simply
    //   must clear the bytes_left for that sub-stream.
    submsg_args->pMsg = stream->state;
    submsg_args->msg_size = stream->bytes_left;
    submsg_args->exists = true;
    
    stream->bytes_left = 0;
    
    return true;
}

bool encode_whistle_str(pb_ostream_t *stream, const pb_field_t *field, const void *encode_args)
{
    if (!pb_encode_tag_for_field(stream, field))
            return false;
    
    return pb_encode_string(stream, (uint8_t *) encode_args, strlen(encode_args));
}

bool encode_whistle_str_sized(pb_ostream_t *stream, const pb_field_t *field, const void *encode_args)
{
    if (!pb_encode_tag_for_field(stream, field))
            return false;
    
    return pb_encode_string(stream, (uint8_t *) ((WMP_sized_string_cb_args_t*) encode_args)->pPayload,
            ((WMP_sized_string_cb_args_t*) encode_args)->payloadMaxSize);
}

bool encode_submsg_with_tag(pb_ostream_t *stream, const pb_field_t *field, const void *encode_args)
{   
    encode_submsg_args_t *submsg_args = (encode_submsg_args_t*) encode_args;
    
    if (!pb_encode_tag_for_field(stream, field))
        return false;

    if (!pb_encode_submessage(stream, submsg_args->fields, submsg_args->pb_obj))
        return false;
    
    return true;
}
