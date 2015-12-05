//
//  wmpparser.c
//  Barkley
//
//  Created by Justin Middleton on 5/20/13.
//
//

#include "wmps1.h"

/* =========================================== */
/* nextheader bits                             */
const size_t wmps_next_header_offset = WMPS_MESSAGESIZE_OFFSET + WMPS_MESSAGESIZE_SZ;

// type for header identifiers
typedef uint8_t wmps_next_header_id_t;                  

// sizes, in bytes, for various header types.
// indexes match ((wmps_next_header_id_t) id) >> 6.
// indices 1-3 are pretty arbitrary and up for discussion
const size_t wmps_next_header_sizes[] = { 1, 2, 4, 8 };

// The one required header, indicates the payload type (values haven't been defined)

#define WMPS_NEXT_HEADER_ID_PAYLOAD_TYPE 0x00

// Minimum size of a nextheader buffer.

#define WMPS_NEXT_HEADER_MIN_BUFFER_SIZE (sizeof(wmps_next_header_id_t) + sizeof(wmps_next_header_payload_type_t))

#define WMPS_MIN_MESSAGE_SIZE (WMPS_NEXT_HEADER_MIN_BUFFER_SIZE + WMPS_VERSION_SZ + WMPS_SEQNUM_SZ + WMPS_MESSAGESIZE_SZ)

// header type definitions
const wmps_next_header_payload_type_t wmps_next_header_payload_type_rawbytes = 0x00;
const wmps_next_header_payload_type_t wmps_next_header_payload_type_wmp1 = 0x01;
const wmps_next_header_payload_type_t wmps_next_header_payload_type_wmp1_fragment = 0x02;


#pragma mark - private

// Calculate the size of the next_header_value based upon the next_header_id
static inline size_t WMP_next_header_size(wmps_next_header_id_t next_header)
{
    return wmps_next_header_sizes[next_header >> 6];
}


/* ===========================================================
 * Slurp all next_header fields into the next_header_struct. 
 * unrecognized fields will be ignored.
 * ===========================================================
 */

static size_t WMP_extract_next_header(wmps_next_header_struct *next_header_struct, // ref to dest vers struct
                                      const wmps_next_header_id_t *src_buffer,     // source byte buffer
                                      const size_t src_buffer_size,                // size of header
                                      wmps_error_t *err)                         // pointer to error code
{
    size_t offset = 0;
    int done = 0;
    
    size_t current_value_size;
    // the buffer size must be at least as large as
    // an ID and the payload type qualifier.
    
    size_t min_size = WMPS_NEXT_HEADER_MIN_BUFFER_SIZE;
    if (src_buffer_size < min_size) {
        *err = WMPS1ErrorMessageIncomplete;
        return 0;
    }
    
    
    while (!done) {

        wmps_next_header_id_t next_header = (wmps_next_header_id_t)src_buffer[offset];
        current_value_size = WMP_next_header_size(next_header);
        
        switch ((unsigned int)next_header) {
            case WMPS_NEXT_HEADER_ID_PAYLOAD_TYPE:
                next_header_struct->payload_type = (wmps_next_header_payload_type_t) src_buffer[offset + sizeof(wmps_next_header_id_t)];
                done = 1;
                break;
                
            default:
                break;
        }
        
        offset += sizeof(wmps_next_header_id_t) + current_value_size;
    }
    
    return offset;
}

/* ===========================================================
 * Pack a wmps_next_header struct into a buffer.
 * ===========================================================
 */

static size_t WMP_pack_next_header(wmps_next_header_id_t *dest_buffer,              // where to throw the header
                            const wmps_next_header_struct *next_header  // source struct
                            )
{
    size_t offset = 0;

//    //#ifdef ADD_STINKER_HEADER
//    dest_buffer[offset] = (0x03 << 6) | wmps_next_header_id_payload_type; // should be an 8-byte wide header
//    offset += sizeof(wmps_next_header_id_t);
//    dest_buffer[offset + wmps_next_header_sizes[3]] = (uint64_t) 0x00;    // add 8 zeros
//    offset += wmps_next_header_sizes[3];
//    //#endif

    /* ---------------------------------------- */
    /* We only support the payload type now.    */
    /* !!This has to be the last thing added!!  */
    /* ---------------------------------------- */
    
    dest_buffer[offset] = WMPS_NEXT_HEADER_ID_PAYLOAD_TYPE;
    offset += sizeof(wmps_next_header_id_t);
    dest_buffer[offset] = (wmps_next_header_payload_type_t) next_header->payload_type;
    offset += sizeof(wmps_next_header_payload_type_t);
    
    return offset;
}

#pragma mark - public 

/* ===========================================================
 * Convenience method; allocs a msg with a pre-allocated payload array
 * of the given size.  Should be freed with WMP_make_msg_struct_and_payload().
 * ===========================================================
 */

wmps_msg_struct * WMP_make_msg_struct_with_payload_alloc(uint32_t size)
{
    uint8_t *payload = malloc(size);
    
#ifndef MQX_ENV
    if (!payload) {
        return 0;
    }
#endif
    
    return WMP_make_msg_struct_with_payload(payload, size);
}

/* ===========================================================
 * Free a message and its payload.
 * ===========================================================
 */

void WMP_free_msg_struct_and_payload(wmps_msg_struct *msg)
{
    free(msg->payload);
    WMP_free_msg_struct(msg);
}

/* ===========================================================
 * Allocate and return an empty message with the given payload
 * buffer and buffer size. Automatically allocates
 * the next_header field as a pointer to a next_header instance.
 * All other values initialized to zero.  Should be freed with
 * WMP_free_msg_struct.
 * ===========================================================
 */

wmps_msg_struct * WMP_make_msg_struct_with_payload(uint8_t *payload, uint32_t size)
{
    wmps_msg_struct *msg = WMP_make_msg_struct();
    msg->payload = payload;
    msg->payload_max_size = size;
    msg->payload_size     = size;
    return msg;
}

/* ===========================================================
 * Allocate and return an empty message. Automatically allocates
 * the next_header field as a pointer to a next_header instance.
 * All values initialized to zero.
 * ===========================================================
 */

wmps_msg_struct * WMP_make_msg_struct()
{
    wmps_msg_struct *message = malloc(sizeof(wmps_msg_struct));

    // next header block
    wmps_next_header_struct *next_header = malloc(sizeof(wmps_next_header_struct));
    next_header->payload_type = 0;
    
    // assemble the message
    message->next_header = next_header;
    message->payload = 0;
    message->payload_size = 0;
    message->seqnum = 0;
    
    return message;
}

/* ===========================================================
 * Free a message. This frees the memory allocated for the next_header,
 * as well as the message itself.  It does *not* free the memory
 * referenced by .payload -- that's your responsibility.
 * ===========================================================
 */

void WMP_free_msg_struct(wmps_msg_struct *message)
{
    free(message->next_header);
    free(message);
}


/* ===========================================================
   Decode a single message from src_buffer into the given wmp_msg_struct.
   Params:
    dest_message    : a pointer to the wmp_msg_struct into which to decode

    src_buffer      : the buffer which contains the bytes to decode.  Index 0 must be the
                      beginning of a WMPS message.

    src_buffer_size : The size of the src buffer

    pmessage_size   : A pointer to a size_t, which will receive the number of bytes
                      (including all WMPS header and payload fields) decoded from
              src_buffer

    copy            : If nonzero, will copy payload bytes from src_buffer into the
                      memory pointed to by dest_message->payload.
              If zero, will set dest_message=->payload to be a pointer to the
              start of the payload bytes in src_buffer.

    To avoid leaks:
        copy=0 should be paired with WMP_make_msg_struct() or 
        WMP_make_msg_struct_with_payload(), and WMP_free_msg_struct().

        copy=1 should be paired with WMP_make_msg_struct_alloc()
        and WMP_free_msg_struct_and_payload().

   Returns error code.
 * ===========================================================
 */

wmps_error_t WMP_get_next_message_co(wmps_msg_struct *dest_message,
                                     const uint8_t *src_buffer,                // source buffer
                                     const size_t src_buffer_size,             // size of source buffer
                                     size_t *pmessage_size,                    // number of bytes consumed in src_buffer, suitable for bumping an index
                                     int copy)                                 // If nonzero, will memcpy() data from src_buffer to
                                                                               // dest_message->payload.  If zero, will set dest_message->payload
                                                                               // to a pointer to the payload location inside src_buffer.
{
    size_t next_header_sz;
    size_t payload_offset;
    size_t payload_size;
    wmps_error_t err = WMPS1ErrorOK;

    wmps_version_t message_version   = (wmps_version_t)     *src_buffer;
    wmps_messagesize_t message_size  = *((wmps_messagesize_t *)(src_buffer + WMPS_MESSAGESIZE_OFFSET));
    if (message_size > src_buffer_size) {
        // Handle a bug in older codecs—they only put the message size in the first byte
        // Note that this trips a splint warning (expectedly... that is, after all, the bug.)
        message_size  = (wmps_messagesize_t) *(src_buffer + WMPS_MESSAGESIZE_OFFSET);
    }

    // Start out with nothing until we succeed
    *pmessage_size = 0;

    // If we don't have enough data for a header, stop
    if (src_buffer_size < WMPS_HEADER_SZ) {
        return WMPS1ErrorMessageIncomplete;
    }
    
    // get basic packet info
    
    // Version must match WMPS_VERSION, or we give up.
    if (message_version != WMPS_VERSION) {
        return WMPS1ErrorVersionMismatch;
    }
    
    dest_message->seqnum = *((wmps_seqnum_t *)(src_buffer + WMPS_SEQNUM_OFFSET));
    
    // message must be at least as large as the minimum header + nextHeader.
    if (message_size < WMPS_MIN_MESSAGE_SIZE) {
        return WMPS1ErrorMessageLengthTooSmall;
    }

    // src_buffer_size must be >= message_size.
    if (src_buffer_size < message_size) {
        return WMPS1ErrorMessageIncomplete;
    }
    
    
    // Copy the version into the dst_version structure,
    // advancing the buffer to the end of the nextHeader set.
    
    next_header_sz = WMP_extract_next_header(dest_message->next_header,
                                             (wmps_next_header_id_t *) (src_buffer + wmps_next_header_offset),
                                             (src_buffer_size - wmps_next_header_offset),
                                             &err);

    if (err) {
        return err;
    }
    
    // WMP_extract_version pulls all of the nextheader info.  We're now at the beginning of the payload.
    payload_offset = WMPS_HEADER_SZ + next_header_sz;
    payload_size = message_size - payload_offset;
    
    if (copy) {
        if (payload_size > dest_message->payload_max_size) {
            return WMPS1ErrorMessageUnpackOverflow;
        }
        memcpy(dest_message->payload, src_buffer + payload_offset, payload_size);
    } else {
        dest_message->payload = ((uint8_t *)src_buffer) + payload_offset;
    }

    dest_message->payload_size = payload_size;
    
    *pmessage_size = message_size;
    
    return err;
}

wmps_error_t WMP_get_next_message(wmps_msg_struct *dest_message,
                                  const uint8_t *src_buffer,                // source buffer
                                  const size_t src_buffer_size,             // size of source buffer
                                  size_t *pmessage_size)                    // number of bytes consumed in src_buffer, suitable for bumping an index

{
    return WMP_get_next_message_co(dest_message, src_buffer, src_buffer_size, pmessage_size, 1);
}

wmps_error_t WMP_get_next_message_inplace(wmps_msg_struct *dest_message,
                                          const uint8_t *src_buffer,
                                          const size_t src_buffer_size,
                                          size_t *pmessage_size)

{
    return WMP_get_next_message_co(dest_message, src_buffer, src_buffer_size, pmessage_size, 0);
}

/* ===========================================================
 * Assembles a single encapsulates WMPS1 message, with header,
 * version, and payload sections, from src_buffer into dest_buffer.
 * Returns error code.
 * ===========================================================
 */

wmps_error_t WMP_pack_message(uint8_t *dest_buffer,                       // destination buffer
                        size_t dest_buffer_size,                    // max size of the destination buffer
                        const wmps_msg_struct *src_message,         // message source to pack
                        size_t *pmessage_size)                        // size of packed message
{
    wmps_version_t message_version = WMPS_VERSION;
    wmps_messagesize_t message_size;
    size_t next_header_size;

    // Start out with nothing until we succeed
    *pmessage_size = 0;
    
    // assemble the header (except for message size)
    *dest_buffer = message_version;
    *((wmps_seqnum_t *)(dest_buffer + WMPS_SEQNUM_OFFSET)) = src_message->seqnum;

    message_size = WMPS_HEADER_SZ;

    // pack and copy in the next header struct.
    next_header_size = WMP_pack_next_header((wmps_next_header_id_t *)(dest_buffer + wmps_next_header_offset),
                                            src_message->next_header);
    message_size += next_header_size;

    // check that we still have room for the src buffer
    if (message_size + src_message->payload_size > dest_buffer_size) {
        return WMPS1ErrorMessagePackOverflow;
    }
    
    // copy in the src buffer and incr messagesize
    memcpy(dest_buffer + message_size,
           src_message->payload,
           src_message->payload_size);

    message_size += src_message->payload_size;
    
    // pop the message size into the buffer
    *((wmps_messagesize_t *)(dest_buffer + WMPS_MESSAGESIZE_OFFSET)) = message_size;
    
    *pmessage_size = message_size;
    return WMPS1ErrorOK;
}
