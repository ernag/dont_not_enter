/*
 * bark.c
 *
 *  Created on: Oct 28, 2013
 *      Author: Kevin Lloyd
 */
#include <mqx.h>

#include "con_mgr.h"
#include "whttp.h"
#include "cfg_mgr.h"
#include "bark.h"
#include "app_errors.h"
#include "wassert.h"
#include "whttp.h"

#define DEFAULT_HTTP_HEADER_SIZE 300

uint_32 g_btc_bytes_to_send;

/*
 * CAUTION: the pBuf buffer is actually used to receive the HTTP header
 *  as well so it may need to take 200+ bytes more than the expected payload
 * TODO: find a way to make this more elegant (e.g., maybe allocate a larger buffer or do zero-copy)
 * 
 * TODO: need to figure out what to do when more is received than the buffer has room for. Right now it's thrown away.
 */
bark_receive_res_t _bark_receive_wifi( void* conmgr_handle, void *pBuf, uint_32 buf_size, uint_32 timeout_ms, int_32 *pContentReceived )
{
	int_32 received;
	int_32 content_size = -1;
	uint_8 retries = 0, MAX_RETRIES = 3;
	err_t err;

	int_32 header_size = -1;
	int http_result_code;
	// buffer: decided to use the *pBuf as scratch pad for receiving the http header in addition to finally holding the payload.
	
	*pContentReceived = 0;
	
	err = CONMGR_receive(conmgr_handle, pBuf, buf_size, timeout_ms, &received, TRUE);
//    err = CONMGR_receive((void *) pResponseBuf, MAX_EVTMGR_RX_BUFFER_SIZE, EVTMGR_UPLOAD_TCP_RECEIVE_TIMEOUT, &received );
	if (ERR_CONMGR_CRITICAL == err)
	{
		PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "rx1 %x", err);
		return BARK_RECEIVE_CRITICAL;
	}
	if (received <= 0)
	{
		PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "rx1 %x", err);

		switch (err)
		{
            case ERR_CONMGR_NOT_CONNECTED:
                return BARK_RECEIVE_NOT_CONNECTED;
            case ERR_CONMGR_INVALID_HANDLE:
                return BARK_RECEIVE_SOCK_INVALID;
            case ERR_CONMGR_DNS_ERROR:
            case ERR_CONMGR_SOCK_CREATE_ERROR:
            default:
                return BARK_RECEIVE_GENERIC;
		}
	}

	if(!WHTTP_is_HTTP_resp(pBuf, received))
	{
	    PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "Not HTTP");
	    PROCESS_NINFO_BYTES(ERR_WHTTP_CONTENTS, pBuf, received);
	    
		return BARK_RECEIVE_RESP_PARSE_FAIL;
	}

	http_result_code = WHTTP_parse_return_code(pBuf, received);
	if ( HTTP_OK != http_result_code)
	{
	    PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "HTTP(%i)", http_result_code);
	    PROCESS_NINFO_BYTES(ERR_WHTTP_CONTENTS, pBuf, received);
	    
		return BARK_RECEIVE_RESP_PARSE_FAIL;
	}

	content_size = WHTTP_content_length(pBuf, received);
	if ( content_size < 0 )
	{
	    PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "HTTP len");
	    PROCESS_NINFO_BYTES(ERR_WHTTP_CONTENTS, pBuf, received);
	    
		return BARK_RECEIVE_RESP_PARSE_FAIL;
	}
	
	if ( 0 == content_size )
	{
		*pContentReceived = 0;
		return BARK_RECEIVE_OK;
	}
	
	header_size = WHTTP_return_header_size(pBuf, received);
	if (header_size <= 0)
	{
		PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "HTTP hdr len");
		PROCESS_NINFO_BYTES(ERR_WHTTP_CONTENTS, pBuf, received);
		
		return BARK_RECEIVE_RESP_PARSE_FAIL;
	}

	*pContentReceived = received - header_size;
	
	if (*pContentReceived > 0)
	{
		memmove(pBuf, ((char*) pBuf) + header_size, *pContentReceived);
	}
	
	err = ERR_OK;
	while (ERR_OK == err && *pContentReceived < content_size && retries < MAX_RETRIES)
	{
		received = 0;
		err = CONMGR_receive(conmgr_handle, ((char*) pBuf) + *pContentReceived, buf_size - *pContentReceived, timeout_ms, &received, FALSE);
		if (received > 0)
		{
			*pContentReceived += received;
		}
		else
		{
			retries++;
		}
	}
	
	if (ERR_CONMGR_CRITICAL == err)
	{
		PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "rx2 %x", err);
		return BARK_RECEIVE_CRITICAL;
	}
	
	// TODO: probably should have below equality be == ??
	if ( *pContentReceived >= content_size)
	{
		return BARK_RECEIVE_OK;
	}
	
	if (retries >= MAX_RETRIES)
	{
		PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "MAX RETRIES");
		return BARK_RECEIVE_GENERIC;
	}

	PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "rx2 %x", err);

	switch (err)
	{
        case ERR_CONMGR_NOT_CONNECTED:
            return BARK_RECEIVE_NOT_CONNECTED;
        case ERR_CONMGR_INVALID_HANDLE:
            return BARK_RECEIVE_SOCK_INVALID;
        case ERR_CONMGR_DNS_ERROR:
        case ERR_CONMGR_SOCK_CREATE_ERROR:
        default:
            return BARK_RECEIVE_GENERIC;
	}
}

bark_receive_res_t _bark_receive_btc( void* conmgr_handle, void *pBuf, uint_32 buf_size, uint_32 timeout_ms, int_32 *pBytesReceived )
{
	err_t err;
	
	err = CONMGR_receive(conmgr_handle, pBuf, buf_size, timeout_ms, pBytesReceived, TRUE);
	
	switch (err)
	{
	case ERR_OK:
		return BARK_RECEIVE_OK;
	case ERR_CONMGR_NOT_CONNECTED:
		PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "%x", err);
		return BARK_RECEIVE_NOT_CONNECTED;
	case ERR_CONMGR_ERROR:
	default:
		PROCESS_NINFO(ERR_BARK_RECEIVE_ERROR, "%x", err);
		return BARK_RECEIVE_GENERIC;
	}
}

bark_send_res_t _bark_btc_wmps_send_post( void* conmgr_handle, int_32 *bytes_sent )
{
	int_32 dummy_bytes_sent;
	err_t err;
	
	// contract with iOS is to always send an empty message to indicate no more data
	
	g_btc_bytes_to_send -= *bytes_sent;
	
	wassert(0 <= g_btc_bytes_to_send);
	
	if (0 == g_btc_bytes_to_send)
	{
		 err = CONMGR_send(conmgr_handle, NULL, 0,  &dummy_bytes_sent);
	}
	
	switch (err)
	{
    case ERR_OK:
    	return BARK_SEND_OK;
        
	case ERR_CONMGR_INVALID_HANDLE:
		return BARK_SEND_SOCK_INVALID;
		
	case ERR_CONMGR_NOT_CONNECTED:
		return BARK_SEND_NOT_CONNECTED;
		
    case ERR_CONMGR_ERROR:
        return BARK_SEND_GENERIC;

    default:
    	return BARK_SEND_CRITICAL;
	}
}

void _bark_wifi_http_open_pre (void* conmgr_handle)
{
	return;
}

bark_open_res_t _bark_wifi_http_open_post (void* conmgr_handle, uint_32 bytes_to_send)
{
	uint8_t pHttpHeader[DEFAULT_HTTP_HEADER_SIZE];
	uint_16 http_header_len;
	err_t err;
	bark_open_res_t res = BARK_OPEN_OK;
	int_32 send_result;
	
    /*
     *   Build an HTTP header for this particular packet.
     */
    WHTTP_build_http_header( (char*) pHttpHeader,
                             g_st_cfg.fac_st.serial_number,
                             HOST_TO_UPLOAD_DATA_TO,
                             g_st_cfg.con_st.port,
                             &http_header_len,
                             bytes_to_send );
    /*
     *   Send just the HTTP header first.
     */
    err = CONMGR_send(conmgr_handle, (uint8_t *)pHttpHeader, http_header_len, &send_result);

    if (err == ERR_OK)
    {
    	if (send_result != http_header_len)
    	{
    		res = BARK_OPEN_GENERIC;
    		PROCESS_NINFO(ERR_BARK_OPEN_ERROR, "x%x %d httphdr", err, send_result);
    	}
    	else
    	{
    		res = BARK_OPEN_OK;
    	}	
    }
    else
    {
		PROCESS_NINFO(ERR_BARK_OPEN_ERROR, "%x", err);

		if (ERR_CONMGR_NOT_CONNECTED == err)
    	{
    		res = BARK_OPEN_NOT_CONNECTED;
    	}
    	else if ( ERR_CONMGR_CRITICAL == err)
    	{
    		res = BARK_OPEN_CRITICAL;
    	}
    	else
    	{
    		res = BARK_OPEN_GENERIC;    	
    	}
    }

    return res;
}

bark_open_res_t _bark_btc_wmps_open_post (void* conmgr_handle, uint_32 bytes_to_send)
{
	g_btc_bytes_to_send = bytes_to_send;
	
	return BARK_OPEN_OK;
}


bark_open_res_t bark_open ( void* conmgr_handle, uint_32 bytes_to_send )
{
	bark_open_res_t res;
	err_t w_rc;
	
	// Prep
    switch (CONMGR_get_current_actual_connection_type())
    {
        case CONMGR_CONNECTION_TYPE_WIFI:
        	_bark_wifi_http_open_pre(conmgr_handle); // blocking
            break;
            
        case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
            break;
            
        case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
            break;
            
        default:
            // We can legitimately get here when a client opens after
            // a connection is closed.
            break;
    }
    
	w_rc = CONMGR_open(conmgr_handle, HOST_TO_UPLOAD_DATA_TO, g_st_cfg.con_st.port);
	
	if (ERR_OK == w_rc)
	{
		// Post open work (e.g., send http header)
		switch (CONMGR_get_current_actual_connection_type())
		{
			case CONMGR_CONNECTION_TYPE_WIFI:
				res = _bark_wifi_http_open_post(conmgr_handle, bytes_to_send); // blocking
				break;
				
			case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
				res = _bark_btc_wmps_open_post(conmgr_handle, bytes_to_send);
				break;
				
			case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
				break;
				
			default:
				// We can legitimately get here when a client opens after
				// a connection is closed.
				break;
		}
		return res;
	}
	else if (ERR_CONMGR_CRITICAL == w_rc)
	{
		return BARK_OPEN_CRITICAL;
	}
	else if (ERR_CONMGR_NOT_CONNECTED == w_rc)
	{
		return BARK_OPEN_NOT_CONNECTED;
	}
	// TODO: need to have more specific error handling here
	else
	{
		return BARK_OPEN_GENERIC;
	}
}


bark_send_res_t bark_send ( void* conmgr_handle, uint_8 *data, uint_32 data_len, int_32 *bytes_sent )
{
	err_t err;
	
	err = CONMGR_send(conmgr_handle, data, data_len, bytes_sent );
	
	if (ERR_OK == err)
	{
	   	switch (CONMGR_get_current_actual_connection_type())
	   	{
	   	case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
	   		return _bark_btc_wmps_send_post ( conmgr_handle, bytes_sent );

	   	default:
	   		break;
	   	}
	}
	
	switch (err)
	{
    case ERR_OK:
    	return BARK_SEND_OK;
        
	case ERR_CONMGR_INVALID_HANDLE:
		return BARK_SEND_SOCK_INVALID;
		
	case ERR_CONMGR_NOT_CONNECTED:
		return BARK_SEND_NOT_CONNECTED;
		
    case ERR_CONMGR_ERROR:
        return BARK_SEND_GENERIC;

    default:
    	return BARK_SEND_CRITICAL;
	}
}

bark_receive_res_t bark_receive( void* conmgr_handle, void *pBuf, uint_32 buf_size, uint_32 timeout_ms, int_32 *pBytesReceived )
{
	switch (CONMGR_get_current_actual_connection_type())
	{
		case CONMGR_CONNECTION_TYPE_WIFI:
			return _bark_receive_wifi ( conmgr_handle, pBuf, buf_size, timeout_ms, pBytesReceived );
			
		case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
			return _bark_receive_btc ( conmgr_handle, pBuf, buf_size, timeout_ms, pBytesReceived );
			
		case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
		default:
			wassert(0);
	}
	
	return BARK_RECEIVE_CRITICAL;
}

bark_close_res_t bark_close ( void* conmgr_handle )
{
	err_t res = CONMGR_close(conmgr_handle);
	switch (res)
	{
    case ERR_OK:
    	return BARK_CLOSE_OK;
    
    case ERR_CONMGR_CRITICAL:
    	return BARK_CLOSE_CRITICAL;
    	
	case ERR_CONMGR_INVALID_HANDLE:
		return BARK_CLOSE_SOCK_INVALID;
		
	case ERR_CONMGR_NOT_CONNECTED:
		return BARK_CLOSE_NOT_CONNECTED;
		
    case ERR_CONMGR_ERROR:
    default:
        return BARK_CLOSE_GENERIC;

	}
}
