#ifdef MQX_ENV
#include <mqx.h>
#include <bsp.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#endif

#include "tests.h"
#include "wmps1.h"

#ifdef MQX_ENV
err_t OK = ERR_OK;
err_t ROUNDTRIP_FAIL    = ERR_GENERIC;
err_t MAKE_MESSAGE_FAIL = ERR_GENERIC;
err_t ERROR_CHECK_FAIL  = ERR_GENERIC;
#endif

void populate_msg(wmps_msg_struct *msg, uint8_t *payload, size_t len, uint8_t seqnum)
{
    msg->next_header->payload_type = wmps_next_header_payload_type_rawbytes;
    msg->seqnum = seqnum;
    msg->payload_size = len;
    msg->payload = payload;
}

err_t test_decode_errors()
{
    err_t result = OK;
    size_t bytes_read;

    // ===============================================
    // test decoding too-short message
    
    uint8_t stubby[] = {
        0x00, // wmps version
        0x02, // seqnum
        0x07,
        0x00,
        0x00,
        0x00, // size = 0x07 (uint32_t, little-endian)
        0x00,
        0x00,
    };
    
    wmps_error_t err = 0;
    wmps_msg_struct *decode_msg = WMP_make_msg_struct_with_payload_alloc(0x100);
    
    err = WMP_get_next_message(decode_msg, stubby, 8, &bytes_read);
    
    if (bytes_read) {
        printf("test_bad_msg_size: expect bytes_read to be 0; got %zd\n", bytes_read);
        result = ERROR_CHECK_FAIL;
    }
    
    if (err != WMPS1ErrorMessageLengthTooSmall) {
        printf("test_bad_msg_size: expected err to be %d; got %d.\n", WMPS1ErrorMessageLengthTooSmall, err);
        result = ERROR_CHECK_FAIL;
    }
    
    // ===============================================
    // test decoding message with unrecognized version
    
    stubby[2] = 0x08; // it will pass the size test now.
    stubby[0] = 0x01; // it will fail the version test now.

    err = WMP_get_next_message(decode_msg, stubby, 8, &bytes_read);
    
    if (bytes_read) {
        printf("test_bad_version: expect bytes_read to be 0; got %zd\n", bytes_read);
        result = ERROR_CHECK_FAIL;
    }
    
    if (err != WMPS1ErrorVersionMismatch) {
        printf("test_unrecognized_message_version: expected err to be %d; got %d.\n", WMPS1ErrorVersionMismatch, err);
        result = ERROR_CHECK_FAIL;
    }
        
    // ===============================================
    // test decode with a source buffer smaller than min message size

    stubby[0] = WMPS_VERSION; // Pass the version check again.
    
    err = WMP_get_next_message(decode_msg, stubby, 7, &bytes_read);
    
    if (bytes_read) {
        printf("test_small_srcbuf1: expect bytes_read to be 0; got %zd\n", bytes_read);
        result = ERROR_CHECK_FAIL;
    }
    
    if (!err) {
        printf("test_small_srcbuf1: expected err to be %d; got %d.\n", WMPS1ErrorMessageIncomplete, err);
        result = ERROR_CHECK_FAIL;
    }
    
    // ===============================================
    // test decode with a source buffer smaller than indicated message size
    
    stubby[2] = 0x0A;
    
    err = WMP_get_next_message(decode_msg, stubby, 0x09, &bytes_read);
    
    if (bytes_read) {
        printf("test_small_srcbuf2: expect bytes_read to be 0; got %zd\n", bytes_read);
        result = ERROR_CHECK_FAIL;
    }
    
    if (!err) {
        printf("test_small_srcbuf2: expected err to be %d; got %d.\n", WMPS1ErrorMessageIncomplete, err);
        result = ERROR_CHECK_FAIL;
    }

    // ===============================================
    // test decode with a payload buffer that's smaller than (msgsize - 8)
    
    decode_msg->payload_max_size = 0x01; // msglen is still 0x0A, so payload_max has to be at least 0x02.
    
    err = WMP_get_next_message(decode_msg, stubby, 0x0A, &bytes_read);
    
    if (bytes_read) {
        printf("test_small_max_payload: expect bytes_read to be 0; got %zd\n", bytes_read);
        result = ERROR_CHECK_FAIL;
    }
    
    if (!err) {
        printf("test_small_max_payload: expected err to be %d; got %d.\n", WMPS1ErrorMessageUnpackOverflow, err);
        result = ERROR_CHECK_FAIL;
    }
    
    WMP_free_msg_struct_and_payload(decode_msg);
    return result;
}

err_t test_unpack_copy()
{
    err_t result = OK;
	int i;
    size_t readsz = 0;
    size_t totalsz = 0;
    size_t bufsz = 0x400;
    int j;

    /* set up the msgs that will be packed. */
    
    char *payloads[5] = {
        "bruno", 
        "francisco",
        "everything",
        "like everything",
        "foo foo foofeed doo do",
    };
    
    uint8_t *buffer = (uint8_t *)malloc(bufsz);
    size_t writesz = 0;
    wmps_msg_struct *msg_to_pack = WMP_make_msg_struct();
    
    wmps_error_t err = 0;
    wmps_msg_struct *decode_msg = WMP_make_msg_struct_with_payload_alloc(bufsz);

    
    for (i = 0; i < 5; i++) {
    	size_t temp_writesz;
        populate_msg(msg_to_pack, (uint8_t *)payloads[i], strlen(payloads[i]) + 1, i);
        err =  WMP_pack_message(buffer+writesz, bufsz-writesz, msg_to_pack, &temp_writesz);
        writesz += temp_writesz;
    }
    
    /* now unpack */

    i = 0;
    for (;;) {
        char errmsg[80];
        
        err = WMP_get_next_message(decode_msg, buffer + totalsz, bufsz - totalsz, &readsz);
        
        if (! readsz) {
            // we reached the end.
            if (i < 4) {
                printf("msg %d: reached end of decode prematurely (err %d)", i, err);
                result = ROUNDTRIP_FAIL;
            }
            break;
        }
        
        totalsz += readsz;
        
        if ((int) (decode_msg->seqnum) != i) {
            printf("msg %d seqnum: expected %d, got %d\n", i, i, (int)(decode_msg->seqnum));
            result = ROUNDTRIP_FAIL;
        }
        
        if (decode_msg->payload_size != strlen(payloads[i]) + 1) {
            printf("msg %d payload: expected %zd, got %zd\n", i, strlen(payloads[i]+1), decode_msg->payload_size);
            result = ROUNDTRIP_FAIL;
        }
        
        if (memcmp(payloads[i], decode_msg->payload, decode_msg->payload_size)) {
            printf("msg %d payload: expected '%s', got '%s' (payload_size %d)\n", i, payloads[i], (char *)decode_msg->payload, decode_msg->payload_size);
	    for (j = 0; j < decode_msg->payload_size; j++) {
		printf("i: %d %x %c\n", j, decode_msg->payload[j], decode_msg->payload[j]);
	    }
            result = ROUNDTRIP_FAIL;
        }
        
        if (result) {
            break;
        }
        ++i;
    }
    
    free(buffer);
    WMP_free_msg_struct(msg_to_pack);
    WMP_free_msg_struct_and_payload(decode_msg);

    return result;
}

err_t test_unpack_inplace()
{
    err_t result = OK;
	int i;
    size_t readsz = 0;
    size_t totalsz = 0;
    size_t bufsz = 0x1400;

    /* set up the msgs that will be packed. */
    
    char *payloads[8] = {
        "bruno", 
        "francisco",
        "everything",
        "like everything",
        "foo foo foofeed doo do",
	"Of course you’re writing tests for your code. You’re writing unit tests for the smaller components and integration tests to make sure that the components get along amicably. You may even be the sort of person who writes the unit tests first and then builds the program to pass the tests."
	"Now you've got the problem of keeping all those tests organized, which is where a test harness comes in. A test harness is a system that sets up a small environment for every test, runs the test, and reports whether the result is as expected. Like the debugger, I expect that some of you are wondering who it is that doesn’t use a test harness, and to others, it's something you never really considered."
	"There are abundant choices. It's easy to write a macro or two to call each test function and compare its return value to the expected result, and more than enough authors have let that simple basis turn into yet another implementation of a full test harness. From [Page 2008]: 'Microsoft’s internal repository for shared tools includes more than 40 entries under test harness.' For consistency with the rest of the book, I’ll show you"
	"Excerpt From: Ben Klemens. 21st Century C. iBooks.",
	"",
	"Moo.",
    };
    
    uint8_t *buffer = (uint8_t *)malloc(bufsz);
    size_t writesz = 0;
    wmps_msg_struct *msg_to_pack = WMP_make_msg_struct();
    
    wmps_error_t err = 0;
    wmps_msg_struct *decode_msg = WMP_make_msg_struct();

    
    for (i = 0; i < 8; i++) {
    	size_t temp_writesz;
        populate_msg(msg_to_pack, (uint8_t *)payloads[i], strlen(payloads[i]) + 1, i);
        err =  WMP_pack_message(buffer+writesz, bufsz-writesz, msg_to_pack, &temp_writesz);
        writesz += temp_writesz;
    }
    
    /* now unpack */

    i = 0;
    for (;;) {
        char errmsg[80];
        
        err = WMP_get_next_message_inplace(decode_msg, buffer + totalsz, bufsz - totalsz, &readsz);
        
        if (! readsz) {
            // we reached the end.
            if (i < 7) {
                printf("msg %d: reached end of decode prematurely (err %d) ", i, err);
                result = ROUNDTRIP_FAIL;
            }
            break;
        }
        
        totalsz += readsz;
        
        if ((int) (decode_msg->seqnum) != i) {
            printf("msg %d seqnum: expected %d, got %d\n", i, i, (int)(decode_msg->seqnum));
            result = ROUNDTRIP_FAIL;
        }
        
        if (decode_msg->payload_size != strlen(payloads[i]) + 1) {
            printf("msg %d payload: expected %zd, got %zd\n", i, strlen(payloads[i]+1), decode_msg->payload_size);
            result = ROUNDTRIP_FAIL;
        }
        
        if (memcmp(payloads[i], decode_msg->payload, decode_msg->payload_size)) {
            printf("msg %d payload: expected %s, got %s\n", i, payloads[i], (char *)decode_msg->payload);
            result = ROUNDTRIP_FAIL;
        }
        
        if (result) {
            break;
        }
        ++i;
    }
    
    free(buffer);
    WMP_free_msg_struct(msg_to_pack);
    WMP_free_msg_struct(decode_msg);

    return result;
}

err_t test_make_msg_noalloc()
{
    err_t result = OK;
    uint8_t *dest_msg;
    wmps_error_t err = WMPS1ErrorOK;
    size_t writesz;
    uint8_t coded_version;
    uint8_t coded_seqnum;
    uint8_t coded_next_header_id;
    uint8_t coded_next_header_value;
    size_t coded_msgsize;
    uint8_t *coded_bytes;
    
    const size_t expected_writesz =
        1 + // version
        1 + // seqnum
        4 + // message size
        2 + // next_header
        7;  // actual payload size
    
    char errmsg[80];

    /* set up the msg that will be packed. */
    
    uint8_t bytes[7] = {'d', 'u', 'n', 'k', 'i', 't', '\0'};
    
    /* here is where we make the message. */
    wmps_msg_struct *msg = WMP_make_msg_struct_with_payload(bytes, 7);
    msg->next_header->payload_type = wmps_next_header_payload_type_rawbytes;
    msg->seqnum = 0x10;
    
    /* check message construction */
    
    if (! msg) {
        printf("msg should not be null.\n");
        result = MAKE_MESSAGE_FAIL;
    }
    
    if (!msg->next_header) {
        printf("Msg missing next_header.\n");
        result = MAKE_MESSAGE_FAIL;
    }
    
    if (memcmp(bytes, msg->payload, 7)) {
        printf("payload not assigned as expected.");
        result = MAKE_MESSAGE_FAIL;
    }
    
    if (7 != msg->payload_size) {
        printf("payload_size not assigned as expected");
        return MAKE_MESSAGE_FAIL;
    }

    if (7 != msg->payload_max_size) {
        printf("payload_max_size not assigned as expected.");
        result = MAKE_MESSAGE_FAIL;
    }
    
    dest_msg = malloc(0x400);
    if (! dest_msg) {
        printf("Dest message alloc failed.\n");
        result = MAKE_MESSAGE_FAIL;
    }
    
    /* set up and pack message into dest buffer */
    
    err = WMP_pack_message(dest_msg, 0x400, msg, &writesz);
    
    
    // Make sure no error was reported
    if (err != WMPS1ErrorOK) {
        printf("WMP_pack_message() failed with code %d", err);
        result = MAKE_MESSAGE_FAIL;
    }
    
    // Check sizes.
    if (expected_writesz != writesz) {
        printf("WMP_pack_message() failed: wrote %zd bytes, expected %zd",
            writesz, expected_writesz);
        result = MAKE_MESSAGE_FAIL;
    }

    // Check WMPS version
    coded_version = *dest_msg;
    if (coded_version != 0x00) {
        printf("msg_version (byte 0) incorrect: got %d, expected %d",
            coded_version, 0x00);
        result = MAKE_MESSAGE_FAIL;
    }
    
    // Check sequence number
    coded_seqnum = (uint8_t)*(dest_msg + 1);
    if (coded_seqnum != msg->seqnum) {
        printf("msg_seqnum (byte 1) incorrect: got %d, expected %d",
            coded_seqnum, msg->seqnum);
        result = MAKE_MESSAGE_FAIL;
    }
    
    // Check message size
    coded_msgsize = (size_t)*(dest_msg + 2);
    if (coded_msgsize != expected_writesz) {
        printf("msg_size (LE uint32_t at bytes 2-5) incorrect: got %zd, expected %zd",
            coded_msgsize,
            expected_writesz);
        result = MAKE_MESSAGE_FAIL;
    }
    
    // Check nextheader
    coded_next_header_id = *(dest_msg + 6);
    if (coded_next_header_id != 0x00) {
        printf("next_header_id (byte 6) incorrect: got %d, expected %d",
            coded_next_header_id,
            0x00);
        result = MAKE_MESSAGE_FAIL;
    }

    coded_next_header_value = *(dest_msg + 7);
    if (coded_next_header_value != 0x00) {
        printf("next_header_value (byte 7) incorrect: got %d, expected %d",
            coded_next_header_value,
            0x00);
        result = MAKE_MESSAGE_FAIL;
    }
    
    // Finally, check the actual data.
    coded_bytes = dest_msg + 8;
    if (memcmp(coded_bytes, bytes, 7)) {
        printf("coded_bytes (starting at byte 8) != bytesn\n");
        result = MAKE_MESSAGE_FAIL;
    }
    
    free(dest_msg);
    WMP_free_msg_struct(msg);
    return result;
}


#ifndef MQX_ENV
int main(int argc, char* argv[])
{
    result_t result = OK;
    result |= test_unpack_copy();
    result |= test_unpack_inplace();
    result |= test_make_msg_noalloc();
    result |= test_decode_errors();
    return result;
}
#endif

