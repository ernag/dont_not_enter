/*
 * http.c
 *
 *  Created on: Apr 22, 2013
 *      Author: SpottedLabs
 */

#include <mqx.h>
#include <string.h>

#include "wassert.h"
#include "whttp.h"
#include "app_errors.h"


#define MAX_CONTENT_LEN_STR     16

int16_t WHTTP_parse_return_header(char* payload, uint32_t payload_size, int16_t *return_code, int16_t *is_chunked, uint32_t *header_size, uint32_t *content_length)
{
    *return_code = WHTTP_parse_return_code(payload, payload_size);
    if (0 > *return_code)
    {
    	return -1;
    }
    
    *is_chunked = WHTTP_is_a_chunked_transfer(payload, payload_size);
    if (0 > *is_chunked)
    {
    	return -1;
    }

    *header_size = WHTTP_return_header_size(payload, payload_size);
    if (0 >= *header_size)
    {
    	return -1;
    }
    
    *content_length = WHTTP_content_length(payload, payload_size);
    if (0 > *content_length)
    {
    	return -1;
    }
    
    return 0;
}

int WHTTP_is_HTTP_resp(char* payload, uint32_t payload_len)
{
    if (payload_len < WHTTP_HTTP_TAG_LEN )
    {
        PROCESS_NINFO(ERR_WIFI_WHTTP_GENERIC, "hdr shrt");
        return FALSE;
    }
    
    if ( ( 0 != strncmp(payload, WHTTP_HTTP_1_0_TAG, WHTTP_HTTP_1_0_TAG_LEN) ) &&
    		( 0 != strncmp(payload, WHTTP_HTTP_1_1_TAG, WHTTP_HTTP_1_1_TAG_LEN) ) )
    {
        PROCESS_NINFO(ERR_WIFI_WHTTP_GENERIC, "hdr tag");
        return FALSE;
    }

    return TRUE;
}

int16_t WHTTP_return_header_size(char* payload, uint32_t payload_len)
{
    char* res = NULL;
    if (payload_len < WHTTP_HTTP_RESP_TERMINATOR_LEN )
    {
        PROCESS_NINFO(ERR_WIFI_WHTTP_GENERIC, "whttp shrt");
        return -1;
    }
    res = strstr(payload, WHTTP_HTTP_RESP_TERMINATOR);
    if ( NULL == res )
    {
        PROCESS_NINFO(ERR_WIFI_WHTTP_GENERIC, "hdr len");
        return -1;
    }
    
    return (res - payload + WHTTP_HTTP_RESP_TERMINATOR_LEN);
}

int16_t  WHTTP_parse_return_code(char* response, uint32_t len)
{
    if ( WHTTP_is_HTTP_resp(response, len) == FALSE )
    {
        return -1;
    }
    
    return atol(response + WHTTP_HTTP_TAG_LEN*sizeof(char));
}
int16_t WHTTP_is_a_chunked_transfer_terminator(char* response, uint32_t len)
{
    if ( len != WHTTP_HTTP_TRANSFER_ENCODING_TERMINATOR_LEN )
    {
        corona_print("rx term\n");
        return -1;
    }
    
    if ( 0 != strncmp(WHTTP_HTTP_TRANSFER_ENCODING_TERMINATOR, response, WHTTP_HTTP_TRANSFER_ENCODING_TERMINATOR_LEN) )
    {
        return -1;
    }
    return 1;
}
int16_t WHTTP_is_a_chunked_transfer(char* response, uint32_t len)
{
    char* res;
    if (len < WHTTP_HTTP_TRANSFER_ENCODING_TAG_LEN )
    {
        corona_print("input short\n");
        return -1;
    }
    res = strstr(response, WHTTP_HTTP_TRANSFER_ENCODING_TAG);
    if ( NULL == res )
    {
        return 0;
    }
    return 1;
}

uint32_t WHTTP_content_length(char* response, uint32_t len)
{
    char* res;
    if (len < WHTTP_HTTP_CONTENT_LENGTH_TAG_LEN )
    {
        corona_print("input short\n");
        return -1;
    }
    res = strstr(response, WHTTP_HTTP_CONTENT_LENGTH_TAG);
    if ( NULL == res )
    {
        return -1;
    }

    res += WHTTP_HTTP_CONTENT_LENGTH_TAG_LEN;

    corona_print("len:%s\n", res);

    return atol(res);
}

/*
 *   This function will add the passed-in content length to the HTTP header as a string, and
 *     return the string and HTTP header length.
 *     
 *     pHttpHeader = output string (caller allocates memory).
 *     pHostAddr   = input string for host address.
 *     pLen        = output 
 *     content_len = length of the HTTP payload to insert into the header. 
 */
void WHTTP_build_http_header(char       *pHttpHeader, 
                               const char *serial_number,
                               const char *pHostAddr,
                               uint_16     port,
                               uint_16    *pLen, 
                               uint_32     content_len)
{
    char pContentLenStr[MAX_CONTENT_LEN_STR];
    char integer_string[7];

    sprintf(integer_string, "%d", port);

    pHttpHeader[0] = 0;
    strcat(pHttpHeader, HTTP_HEADER_BEFORE_SERIALNUMBER);
    strcat(pHttpHeader, serial_number);
    strcat(pHttpHeader, HTTP_HEADER_AFTER_SERIALNUMBER);
    strcat(pHttpHeader, pHostAddr);
    strcat(pHttpHeader, ":");
    strcat(pHttpHeader, integer_string);
    strcat(pHttpHeader, HTTP_HEADER_BEFORE_CONTENT_LEN);
    wassert( sprintf(pContentLenStr, "%d", content_len) < MAX_CONTENT_LEN_STR );
    strcat(pHttpHeader, pContentLenStr);
    strcat(pHttpHeader, HTTP_HEADER_AFTER_CONTENT_LEN);
    if (pLen)
    {
        *pLen = strlen(pHttpHeader); // TODO - can improve performance later by not strlen'ing every time the whole payload.
    }
}
