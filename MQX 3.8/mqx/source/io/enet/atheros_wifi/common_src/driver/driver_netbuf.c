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
#include <a_config.h>
#include <a_types.h>
#include <a_osapi.h>
#include <netbuf.h>
#include <cust_netbuf.h>


A_VOID a_netbuf_enqueue(A_NETBUF_QUEUE_T *q, A_VOID *pReq)
{        
    if (q->head == NULL) {        
        q->head = pReq;            
    } else {  
    	A_ASSIGN_QUEUE_LINK(q->tail, pReq);                    
        //((A_NETBUF*)q->tail)->queueLink = (A_NETBUF*)pReq;   
    }

    q->tail = pReq;   
    A_CLEAR_QUEUE_LINK(pReq);            
    //((A_NETBUF*)pkt)->queueLink = NULL;
    q->count++;
}

A_VOID a_netbuf_prequeue(A_NETBUF_QUEUE_T *q, A_VOID *pReq)
{
    if(q->head == NULL){
        q->tail = pReq;
    }
    A_ASSIGN_QUEUE_LINK(pReq, q->head);
    //((A_NETBUF*)pkt)->queueLink = q->head;
    q->head = pReq;
    q->count++;
}

A_VOID *a_netbuf_dequeue(A_NETBUF_QUEUE_T *q)
{
    A_VOID* pReq;
    
    if(q->head == NULL) return (A_VOID*)NULL;
    
    pReq = q->head;
    
    if(q->tail == q->head){
        q->tail = q->head = NULL;       
    }else{
    	q->head = A_GET_QUEUE_LINK(pReq);
        //q->head = (A_VOID*)(curr->queueLink);
    }
    
    q->count--;
    A_CLEAR_QUEUE_LINK(pReq);
    //curr->queueLink = NULL;
    return (A_VOID*)pReq;
}

#if 0
A_VOID *a_netbuf_peek_queue(A_NETBUF_QUEUE_T *q)
{
    return q->head;
}


A_INT32 a_netbuf_queue_size(A_NETBUF_QUEUE_T *q)
{
    return q->count;
}

A_INT32 a_netbuf_queue_empty(A_NETBUF_QUEUE_T *q)
{
    return((q->count == 0)? 1:0);
}
#endif

A_VOID a_netbuf_queue_init(A_NETBUF_QUEUE_T *q)
{
    q->head = q->tail = NULL;
    q->count = 0;
}

/* EOF */