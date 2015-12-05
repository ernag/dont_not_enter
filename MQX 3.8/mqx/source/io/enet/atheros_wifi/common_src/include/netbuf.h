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
#ifndef _NETBUF_H_
#define _NETBUF_H_

#include <a_types.h>
#include <a_config.h>
#include <athdefs.h>

/*
 * Network buffer queue support
 */
typedef struct {
    pointer head;
    pointer tail;
    A_UINT16 count;
} A_NETBUF_QUEUE_T;


typedef A_VOID (*A_TRANS_CB)(A_VOID*, A_VOID *);

/* A_TRANSPORT_OBJ - the structure used to encapsulate any/all bus requests */
typedef struct _transport_obj {   
    A_UINT32 lookahead; /* expected lookahead value for sanity checking */
    A_UINT16 transferLength; /* total transfer length including any pad bytes */
    A_UINT16 address; /* register address from which the transfer will begin */    
    A_UINT16 cmd; /* bit field containing command values */
    A_UINT8 credits; /* num credits consumed by this request (TX only) */
    A_UINT8 epid; /* endpoint ID */   
    A_STATUS status; /* transport status */    
    A_TRANS_CB cb; /* callback to be used upon completion <cannot be NULL> */
}A_TRANSPORT_OBJ;


#define A_NETBUF_QUEUE_INIT(q)  \
    a_netbuf_queue_init(q)

#define A_NETBUF_ENQUEUE(q, pReq) \
    a_netbuf_enqueue((q), (pReq))
#define A_NETBUF_PREQUEUE(q, pReq) \
    a_netbuf_prequeue((q), (pReq))
#define A_NETBUF_DEQUEUE(q) \
    (a_netbuf_dequeue((q)))
#define A_NETBUF_PEEK_QUEUE(q) ((A_NETBUF_QUEUE_T*)(q))->head            
#define A_NETBUF_QUEUE_SIZE(q)  ((A_NETBUF_QUEUE_T*)(q))->count
#define A_NETBUF_QUEUE_EMPTY(q) (!((A_NETBUF_QUEUE_T*)(q))->count)
   


A_VOID a_netbuf_enqueue(A_NETBUF_QUEUE_T *q, A_VOID *pReq);
A_VOID a_netbuf_prequeue(A_NETBUF_QUEUE_T *q, A_VOID *pReq);
A_VOID *a_netbuf_dequeue(A_NETBUF_QUEUE_T *q);
//A_INT32 a_netbuf_queue_size(A_NETBUF_QUEUE_T *q);
//A_INT32 a_netbuf_queue_empty(A_NETBUF_QUEUE_T *q);
A_VOID a_netbuf_queue_init(A_NETBUF_QUEUE_T *q);
//A_VOID *a_netbuf_peek_queue(A_NETBUF_QUEUE_T *q);


#endif /* _NETBUF_H_ */