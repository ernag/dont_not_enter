/*
 * bark.h
 *
 *  Created on: Oct 28, 2013
 *      Author: Kevin Lloyd
 */

#ifndef BARK_H_
#define BARK_H_

#include <mqx.h>

#define MAX_HTTP_HEADER_LEN     300  // This will probably be around 250 most of the time, give us some margin here.

typedef enum bark_open_res
{
    BARK_OPEN_OK            = 0,
    BARK_OPEN_GENERIC,		
    BARK_OPEN_CRITICAL,			// Restarted needed, bail out fast!
    BARK_OPEN_SERVER_NOT_FOUND,
    BARK_OPEN_NOT_CONNECTED,		// Not connected on a transport that supports server comm
    BARK_OPEN_TIMEOUT,				// Server did not respond in time.
    BARK_MAX_CONNS					// Too many connections open
} bark_open_res_t;
bark_open_res_t bark_open ( void* conmgr_handle, uint_32 bytes_to_send );

typedef enum bark_send_res
{
    BARK_SEND_OK            = 0,
    BARK_SEND_GENERIC,		
    BARK_SEND_CRITICAL,			// Restarted needed, bail out fast!
    BARK_SEND_SOCK_INVALID,		// Communication socket is no longer valid
    BARK_SEND_NOT_CONNECTED,		// Not connected on a transport that supports server comm   
    BARK_SEND_TIMEOUT				// Server did not respond in time.
} bark_send_res_t;
bark_send_res_t bark_send ( void* conmgr_handle, uint_8 *data, uint_32 data_len, int_32 *bytes_sent );


typedef enum bark_receive_res
{
    BARK_RECEIVE_OK            = 0,
    BARK_RECEIVE_GENERIC,		
    BARK_RECEIVE_CRITICAL,			// Restarted needed, bail out fast!
    BARK_RECEIVE_SOCK_INVALID,		// Communication socket is no longer valid
    BARK_RECEIVE_NOT_CONNECTED,		// Not connected on a transport that supports server comm   
    BARK_RECEIVE_TIMEOUT,				// Server did not respond in time.
    BARK_RECEIVE_RESP_PARSE_FAIL	// Could not parse response message properly (e.g., HTTP -1 error)
} bark_receive_res_t;
bark_receive_res_t bark_receive( void* conmgr_handle, void *pBuf, uint_32 buf_size, uint_32 timeout_ms, int_32 *pBytesReceived );


typedef enum bark_close_res
{
    BARK_CLOSE_OK            = 0,
    BARK_CLOSE_GENERIC,		
    BARK_CLOSE_CRITICAL,			// Restarted needed, bail out fast!
    BARK_CLOSE_SOCK_INVALID,		// Communication socket is no longer valid
    BARK_CLOSE_NOT_CONNECTED,		// Not connected on a transport that supports server comm   
    BARK_CLOSE_TIMEOUT				// Server did not respond in time.
} bark_close_res_t;
bark_close_res_t bark_close ( void* conmgr_handle );

union bark_res
{
	bark_open_res_t open_res;
	bark_send_res_t send_res;
	bark_receive_res_t receive_res;
	bark_close_res_t close_res;
};

#endif /* BARK_H_ */
