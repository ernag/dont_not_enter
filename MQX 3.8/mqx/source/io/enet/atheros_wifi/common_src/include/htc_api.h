//------------------------------------------------------------------------------
// Copyright (c) 2011 Qualcomm Atheros, Inc.
// All Rights Reserved.
// Qualcomm Atheros Confidential and Proprietary.
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is
// hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================
#ifndef _HTC_API_H_
#define _HTC_API_H_

#include <a_config.h>


typedef A_UINT16 HTC_SERVICE_ID;

#define HTC_MAILBOX_NUM_MAX    4

/* ------ Endpoint IDS ------ */
typedef enum
{
    ENDPOINT_UNUSED = -1,
    ENDPOINT_0 = 0,    
    ENDPOINT_1,
    ENDPOINT_2,
    ENDPOINT_MANAGED_MAX,
    ENDPOINT_3 = ENDPOINT_2+1,
    ENDPOINT_4,
    ENDPOINT_5,
#if WLAN_CONFIG_TRUNCATED_ENDPOINTS
	ENDPOINT_MAX
#else	        
    ENDPOINT_6,
    ENDPOINT_7,
    ENDPOINT_8,
    ENDPOINT_MAX
#endif    
} HTC_ENDPOINT_ID;


/* service connection information */
typedef struct _HTC_SERVICE_CONNECT_REQ {
    HTC_SERVICE_ID   ServiceID;                 /* service ID to connect to */
    A_UINT16         ConnectionFlags;           /* connection flags, see htc protocol definition */
    A_UINT8         *pMetaData;                 /* ptr to optional service-specific meta-data */
    A_UINT8          MetaDataLength;            /* optional meta data length */
    A_INT32          MaxSendQueueDepth;         /* maximum depth of any send queue */
    A_UINT32         LocalConnectionFlags;      /* HTC flags for the host-side (local) connection */
    A_UINT32     	 MaxSendMsgSize;            /* override max message size in send direction */
} HTC_SERVICE_CONNECT_REQ;

#define HTC_LOCAL_CONN_FLAGS_ENABLE_SEND_BUNDLE_PADDING (1 << 0)  /* enable send bundle padding for this endpoint */

/* service connection response information */
typedef struct _HTC_SERVICE_CONNECT_RESP {
    A_UINT8         *pMetaData;         /* caller supplied buffer to optional meta-data */
    A_UINT8         BufferLength;       /* length of caller supplied buffer */
    A_UINT8         ActualLength;       /* actual length of meta data */
    HTC_ENDPOINT_ID Endpoint;           /* endpoint to communicate over */
    A_UINT32    	MaxMsgLength;       /* max length of all messages over this endpoint */
    A_UINT8         ConnectRespCode;    /* connect response code from target */
} HTC_SERVICE_CONNECT_RESP;




#endif /* _HTC_API_H_ */