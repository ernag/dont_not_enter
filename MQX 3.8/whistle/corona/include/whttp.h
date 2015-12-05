/*
 * whttp.h
 *
 *  Created on: Apr 22, 2013
 *      Author: SpottedLabs
 */


#ifndef WHTTP_H
#define WHTTP_H

#include <mqx.h>
#include <bsp.h>

#define WHTTP_HTTP_1_0_TAG "HTTP/1.0 "
#define WHTTP_HTTP_1_0_TAG_LEN 9

#define WHTTP_HTTP_1_1_TAG "HTTP/1.1 "
#define WHTTP_HTTP_1_1_TAG_LEN 9

#define WHTTP_HTTP_TAG_LEN 9

#define WHTTP_HTTP_RESP_TERMINATOR "\r\n\r\n\0"
#define WHTTP_HTTP_RESP_TERMINATOR_LEN 4

#define WHTTP_HTTP_TRANSFER_ENCODING_TAG "transfer-encoding: chunked\0"
#define WHTTP_HTTP_TRANSFER_ENCODING_TAG_LEN 26
#define WHTTP_HTTP_TRANSFER_ENCODING_TERMINATOR "0\r\n\r\n\0"
#define WHTTP_HTTP_TRANSFER_ENCODING_TERMINATOR_LEN 5

#define WHTTP_HTTP_CONTENT_LENGTH_TAG "Content-Length:"
#define WHTTP_HTTP_CONTENT_LENGTH_TAG_LEN strlen(WHTTP_HTTP_CONTENT_LENGTH_TAG)


#define HTTP_ERROR_SERVER_CANT_PARSE 400
#define HTTP_OK 200

#define HTTP_HEADER_BEFORE_SERIALNUMBER "POST /devices/" 
#define HTTP_HEADER_AFTER_SERIALNUMBER "/messages HTTP/1.1\r\n\
User-Agent: whistle \r\nHost: "

#define HTTP_HEADER_BEFORE_CONTENT_LEN "\r\n\
Content-Type: whistle\r\n\
Accept: */*\r\n\
Connection: close\r\n\
Content-Length: "

#define HTTP_HEADER_AFTER_CONTENT_LEN "\r\n\r\n"


int16_t WHTTP_parse_return_header(char* payload, uint32_t payload_size, int16_t *return_code, int16_t *is_chunked, uint32_t *header_size, uint32_t *content_length);
int WHTTP_is_HTTP_resp(char* payload, uint32_t payload_len);
int16_t WHTTP_return_header_size(char* response, uint32_t len);
int16_t WHTTP_parse_return_code(char* response, uint32_t len);
int16_t WHTTP_is_a_chunked_transfer(char* response, uint32_t len);
int16_t WHTTP_is_a_chunked_transfer_terminator(char* response, uint32_t len);
uint32_t  WHTTP_content_length(char* response, uint32_t len);

void WHTTP_build_http_header(char       *pHttpHeader, 
                               const char *serial_number,
                               const char *pHostAddr,
                               uint_16     port,
                               uint_16    *pLen, 
                               uint_32     content_len);

#endif
