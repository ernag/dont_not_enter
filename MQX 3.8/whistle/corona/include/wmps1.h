//
//  wmpparser.h
//  Barkley
//
//  Created by Justin Middleton on 5/20/13.
//
//
#ifdef MQX_ENV
#include <mqx.h>
#include <bsp.h>
#include <size_t.h>
#include "cormem.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#include <stdint.h>

#ifndef Barkley_wmpparser_h
#define Barkley_wmpparser_h

#define WMPS_VERSION 0x00

#ifdef MQX_ENV
#define WMPS_MAX_PB_SERIALIZED_MSG_SIZE 1536 // no fucking clue how big this should be

#define malloc walloc
#define free wfree
#else
#define malloc malloc
#define free free
#endif

typedef uint8_t wmps_version_t;
typedef uint8_t wmps_seqnum_t;
typedef uint32_t wmps_messagesize_t;

/* ========================================== */
/* Offsets                                    */

#define WMPS_VERSION_SZ       sizeof(wmps_version_t)
#define WMPS_SEQNUM_SZ        sizeof(wmps_seqnum_t)
#define WMPS_MESSAGESIZE_SZ   sizeof(wmps_messagesize_t)

#define WMPS_VERSION_OFFSET          0

#define WMPS_SEQNUM_OFFSET (WMPS_VERSION_OFFSET + WMPS_VERSION_SZ)

#define WMPS_MESSAGESIZE_OFFSET (WMPS_SEQNUM_OFFSET + WMPS_SEQNUM_SZ)

#define WMPS_HEADER_SZ (WMPS_VERSION_SZ + WMPS_SEQNUM_SZ + WMPS_MESSAGESIZE_SZ)

/* ========================================== */
/* error codes                                */

typedef enum {
    WMPS1ErrorOK = 0,
    WMPS1ErrorVersionMismatch,
    WMPS1ErrorMessageIncomplete,
    WMPS1ErrorMessagePackOverflow,
    WMPS1ErrorMessageUnpackOverflow,
    WMPS1ErrorMessageLengthTooSmall,
    WMPS1ErrorUnrecognizedMessageVersion
} wmps_error_t;

/* ========================================== */
/* nextHeader types                           */

typedef uint8_t wmps_next_header_payload_type_t;

typedef struct wmps_next_header_struct {
    wmps_next_header_payload_type_t payload_type; // only header defined in WMPS1
} wmps_next_header_struct;

typedef struct  wmps_msg_struct {
    wmps_seqnum_t seqnum;
    wmps_next_header_struct *next_header;
    uint8_t *payload;
    uint32_t payload_max_size;            // maximum alloc'd size of *payload
    uint32_t payload_size;                // actual size of payload
} wmps_msg_struct;

extern const wmps_next_header_payload_type_t wmps_next_header_payload_type_rawbytes;
extern const wmps_next_header_payload_type_t wmps_next_header_payload_type_wmp1;
extern const wmps_next_header_payload_type_t wmps_next_header_payload_type_wmp1_fragment;

/* ===========================================================
 * Convenience method; allocs a msg with a pre-allocated payload array
 * of the given size.  Should be freed with WMP_make_msg_struct_and_payload().
 * ===========================================================
 */

wmps_msg_struct * WMP_make_msg_struct_with_payload_alloc(uint32_t size);

/* ===========================================================
 * Free a message and its payload.
 * ===========================================================
 */

void WMP_free_msg_struct_and_payload(wmps_msg_struct *msg);

/* ===========================================================
 * Allocate and return an empty message with the given payload
 * buffer and buffer size. Automatically allocates
 * the next_header field as a pointer to a next_header instance.
 * All other values initialized to zero.  Should be freed with
 * WMP_free_msg_struct_with_payload.
 * ===========================================================
 */

wmps_msg_struct * WMP_make_msg_struct_with_payload(uint8_t *payload, uint32_t size);


/* ===========================================================
 * Allocate and return an empty message. Note that uint8_t *
 * referenced by struct->payload is NOT automatically allocated;
 * you'll have to do that yourself.
 *
 * You must free this with WMP_free_msg_struct() when you're done.
 * ===========================================================
 */

wmps_msg_struct * WMP_make_msg_struct();

/* ===========================================================
 * Free a message. This frees the memory allocated for the next_header,
 * as well as the message itself.  It does *not* free the memory
 * referenced by .payload -- that's your responsibility.
 * ===========================================================
 */

void WMP_free_msg_struct(wmps_msg_struct *message);

/* ===========================================================
   Alias for WMP_get_next_message_co() with copy=1
   =========================================================== */

wmps_error_t WMP_get_next_message(wmps_msg_struct *dest_message,
                            const uint8_t *src_buffer,                // source buffer
                            const size_t src_buffer_size,             // size of source buffer
                            size_t *pmessage_size);                     // number of bytes consumed in src_buffer, suitable for bumping an index


/* ===========================================================
   Alias for WMP_get_next_message_co() with copy=0
   ===========================================================*/
 
wmps_error_t WMP_get_next_message_inplace(wmps_msg_struct *dest_mesage,
                            const uint8_t *src_buffer,
			    const size_t src_buffer_size,
			    size_t *pmessage_size);

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
   =========================================================== */

wmps_error_t WMP_get_next_message_co(wmps_msg_struct *dest_message,
                            const uint8_t *src_buffer,
                            const size_t src_buffer_size,
                            size_t *pmessage_size,
			    int copy);

/* ===========================================================
 * Assembles a single encapsulates WMPS1 message, with header,
 * version, and payload sections, from src_buffer into dest_buffer.
 * Returns error code.
 * ===========================================================
 */

wmps_error_t WMP_pack_message(uint8_t *dest_buffer,                       // destination buffer
                        size_t dest_buffer_size,                    // max size of the destination buffer
                        const wmps_msg_struct *src_message,         // message source to pack
                        size_t *pmessage_size);                        // size of packed message

#endif
