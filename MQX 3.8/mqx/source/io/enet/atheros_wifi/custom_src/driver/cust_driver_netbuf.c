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

#define AR6000_DATA_OFFSET    64


#include "common_stack_offload.h"
#include "custom_stack_offload.h"


A_VOID a_netbuf_set_tx_pool(A_VOID* buffptr);
A_VOID a_netbuf_set_rx_pool(A_VOID* buffptr);
A_VOID a_netbuf_free_tx_pool(A_VOID* buffptr);
A_VOID a_netbuf_free_rx_pool(A_VOID* buffptr);
A_VOID Driver_ReportRxBuffStatus(A_VOID *pCxt, A_BOOL status);


A_VOID default_native_free_fn(A_VOID* pcb_ptr)
{

    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)pcb_ptr;
    /* head is guaranteed to always point to the
     * start of the original buffer */
    if(a_netbuf_ptr->head){
        A_FREE(a_netbuf_ptr->head, MALLOC_ID_NETBUFF);
    }
    if(a_netbuf_ptr->native_orig){
        txpkt_free(a_netbuf_ptr->native_orig);
    }
    A_FREE(a_netbuf_ptr, MALLOC_ID_NETBUFF_OBJ);    
    
}

A_VOID* a_netbuf_dequeue_adv(A_NETBUF_QUEUE_T *q, A_VOID *pkt)
{        
    A_NETBUF* curr, *prev;
    
    if(q->head == NULL) return (A_VOID*)NULL;
    
    curr = (A_NETBUF*)q->head;
    
    while(curr != NULL)
    {
    	if((A_NETBUF*)curr->data == pkt)
    	{
    		/*Match found*/
    		if(curr == (A_NETBUF*)q->head)
    		{
    			/*Match found at head*/
    			q->head = curr->queueLink;
    			break;
    		}
    		else
    		{
    			/*Match found at intermediate node*/
    			prev->queueLink = curr->queueLink;
    			if(q->tail == curr)
    			{
    				/*Last node*/
    				q->tail = prev;
    			}
    			break;   			
    		}
    	}    	
    	prev = curr;
    	curr = curr->queueLink;     	
    }

	if(curr != NULL)
	{
		q->count--;
    	curr->queueLink = NULL;    	
	}
	return (A_VOID*)curr;    
}




A_VOID a_netbuf_purge_queue(A_UINT32 index)
{
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt;
	A_NETBUF_QUEUE_T *q;
	A_NETBUF* temp;
	
	pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]);
	
	q = &pcustctxt->rxqueue;
	
	while(q->count)
	{
		temp = q->head;
		q->head = A_GET_QUEUE_LINK(temp); 
		A_CLEAR_QUEUE_LINK(temp);
		A_NETBUF_FREE(temp);
		q->count--;	
	}
	q->head = q->tail = NULL;
}



A_VOID
a_netbuf_init(A_VOID* buffptr, A_VOID* freefn, A_VOID* priv)
{
	A_NETBUF* a_netbuf_ptr = (A_NETBUF*)buffptr;
	
//	a_netbuf_ptr->native.FREE = (_pcb_free_fn)freefn;
	a_netbuf_ptr->native.context = priv;
	a_netbuf_ptr->tail = a_netbuf_ptr->data = (pointer)((A_UINT32)a_netbuf_ptr->head + AR6000_DATA_OFFSET);
	a_netbuf_ptr->native.bufFragment[0].payload = a_netbuf_ptr->head;
	a_netbuf_ptr->native.bufFragment[0].length = (A_UINT32)a_netbuf_ptr->end - (A_UINT32)a_netbuf_ptr->head;
#if DRIVER_CONFIG_IMPLEMENT_RX_FREE_QUEUE 
	a_netbuf_ptr->rxPoolID = 1;
#endif	
}

A_VOID *
a_netbuf_alloc(A_INT32 size)
{
    A_NETBUF* a_netbuf_ptr;
    
    do{    
	    a_netbuf_ptr = A_MALLOC(sizeof(A_NETBUF), MALLOC_ID_NETBUFF_OBJ);
	    
	    if(a_netbuf_ptr == NULL){
	    	break;
	    }
	    A_MEMZERO(a_netbuf_ptr, sizeof(A_NETBUF));	       
	    
	    a_netbuf_ptr->native.bufFragment[0].length = AR6000_DATA_OFFSET + size;
	    a_netbuf_ptr->native.bufFragment[0].payload = A_MALLOC((A_INT32)(a_netbuf_ptr->native.bufFragment[0].length), MALLOC_ID_NETBUFF);
	    
	    if(a_netbuf_ptr->native.bufFragment[0].payload == NULL){
	    	A_FREE(a_netbuf_ptr, MALLOC_ID_NETBUFF_OBJ);
	    	a_netbuf_ptr = NULL;
	    	break;
	    }
	    
	    a_netbuf_ptr->head = a_netbuf_ptr->native.bufFragment[0].payload;
	    a_netbuf_ptr->end = (pointer)((A_UINT32)a_netbuf_ptr->native.bufFragment[0].payload + a_netbuf_ptr->native.bufFragment[0].length);
	    // reserve head room
	    a_netbuf_ptr->tail = a_netbuf_ptr->data = (pointer)((A_UINT32)a_netbuf_ptr->head + AR6000_DATA_OFFSET);
	    A_MEMZERO(&a_netbuf_ptr->trans, sizeof(A_TRANSPORT_OBJ));
	    /*Init pool IDs*/
	    a_netbuf_ptr->txPoolID = 0;
	    a_netbuf_ptr->rxPoolID = 0;     
    }while(0);
    
    return ((A_VOID *)a_netbuf_ptr);
}



/*****************************************************************************/
/*  a_netbuf_reinit - called from custom free after TX over SPI is completed.
            It is only called in "Blocking TX" mode where we reuse the same 
            netbuf. 
            Tail, data pointers within netbuf may have moved during previous TX, 
            reinit the netbuf so it can be used again. No allocation is done here.

 * RETURNS: reinitialized netbuf pointer for success and NULL for failure 
 *****************************************************************************/
A_VOID* a_netbuf_reinit(A_VOID* netbuf_ptr, A_INT32 size)
{
    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)(netbuf_ptr);
    
    do{    	    
	    a_netbuf_ptr->native.bufFragment[0].length = AR6000_DATA_OFFSET + size;
	   	    
	    a_netbuf_ptr->head = a_netbuf_ptr->native.bufFragment[0].payload;
	    a_netbuf_ptr->end = (pointer)((A_UINT32)a_netbuf_ptr->native.bufFragment[0].payload + a_netbuf_ptr->native.bufFragment[0].length);
	    // reserve head room
	    a_netbuf_ptr->tail = a_netbuf_ptr->data = (pointer)((A_UINT32)a_netbuf_ptr->head + AR6000_DATA_OFFSET);
	    A_MEMZERO(&a_netbuf_ptr->trans, sizeof(A_TRANSPORT_OBJ));
	    /*Init pool IDs*/
	    a_netbuf_ptr->txPoolID = 0;
	    a_netbuf_ptr->rxPoolID = 0;     
    }while(0);
    
    return ((A_VOID *)a_netbuf_ptr);
}


A_VOID a_netbuf_set_rx_pool(A_VOID* buffptr)
{
	A_NETBUF* a_netbuf_ptr = (A_NETBUF*)buffptr;
	a_netbuf_ptr->rxPoolID = 1;
}


A_VOID a_netbuf_set_tx_pool(A_VOID* buffptr)
{
	A_NETBUF* a_netbuf_ptr = (A_NETBUF*)buffptr;
	a_netbuf_ptr->txPoolID = 1;
}

/*
 * Allocate an NETBUF w.o. any encapsulation requirement.
 */
A_VOID *
a_netbuf_alloc_raw(A_INT32 size)
{
    A_NETBUF* a_netbuf_ptr=NULL;
    
    do{
	    a_netbuf_ptr = A_MALLOC(sizeof(A_NETBUF), MALLOC_ID_NETBUFF_OBJ);
	    
	    if(a_netbuf_ptr == NULL){
	    	break;
	    }
	    
	    A_MEMZERO(a_netbuf_ptr, sizeof(A_NETBUF));
	        
	   // a_netbuf_ptr->native.FREE = default_native_free_fn;
	    a_netbuf_ptr->native.bufFragment[0].length = (A_UINT32)size;
	    a_netbuf_ptr->native.bufFragment[0].payload= 
	    	A_MALLOC((A_INT32)a_netbuf_ptr->native.bufFragment[0].length, MALLOC_ID_NETBUFF);
	    
	    
	    if(a_netbuf_ptr->native.bufFragment[0].payload == NULL){
	    	A_FREE(a_netbuf_ptr, MALLOC_ID_NETBUFF_OBJ);
	    	a_netbuf_ptr = NULL;
	    	break;
	    }
	    
	    a_netbuf_ptr->head = a_netbuf_ptr->tail = a_netbuf_ptr->data = a_netbuf_ptr->native.bufFragment[0].payload;
	    a_netbuf_ptr->end = (pointer)((A_UINT32)a_netbuf_ptr->native.bufFragment[0].payload + (A_UINT32)a_netbuf_ptr->native.bufFragment[0].length);
        A_MEMZERO(&a_netbuf_ptr->trans, sizeof(A_TRANSPORT_OBJ)); 
    }while(0);
    
    return ((A_VOID *)a_netbuf_ptr);
}

A_VOID
a_netbuf_free(A_VOID* buffptr)
{
   A_NETBUF* a_netbuf_ptr = (A_NETBUF*)buffptr;
   if(a_netbuf_ptr->rxPoolID != 0)
   {
   		a_netbuf_free_rx_pool(buffptr);
   		return;
   }
   else if(a_netbuf_ptr->txPoolID != 0)
   {
   		a_netbuf_free_tx_pool(buffptr);
   		return;
   }
   else
   {
     if(a_netbuf_ptr->native_orig){
          /*This was a TX packet, do not free netbuf here as it may be reused*/
	  txpkt_free(a_netbuf_ptr);
     }
     else{ 
     /*For all other packets, free the netbuf*/  
         if(a_netbuf_ptr->head){
              A_FREE(a_netbuf_ptr->head, MALLOC_ID_NETBUFF);
         }
     
	 A_FREE(a_netbuf_ptr, MALLOC_ID_NETBUFF_OBJ);    
     }
   }  
}

A_VOID
a_netbuf_free_rx_pool(A_VOID* buffptr)
{
	A_NETBUF* a_netbuf_ptr = (A_NETBUF*)buffptr;
	A_CUSTOM_DRIVER_CONTEXT *pCxt = (A_CUSTOM_DRIVER_CONTEXT *)a_netbuf_ptr->native.context;
    
    RXBUFFER_ACCESS_ACQUIRE((A_VOID*)pCxt);	    
    A_NETBUF_ENQUEUE(&(pCxt->rxFreeQueue), a_netbuf_ptr);			   
    Driver_ReportRxBuffStatus((A_VOID*)pCxt, A_TRUE);	    
    RXBUFFER_ACCESS_RELEASE((A_VOID*)pCxt);

}

A_VOID
a_netbuf_free_tx_pool(A_VOID* buffptr)
{
	
}
/* returns the length in bytes of the entire packet.  The packet may 
 * be composed of multiple fragments or it may be just a single buffer.
 */
A_UINT32
a_netbuf_to_len(A_VOID *bufPtr)
{
	A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
    A_UINT32 len = (A_UINT32)a_netbuf_ptr->tail - (A_UINT32)a_netbuf_ptr->data;
    A_UINT16 i = 0;
    
    if(a_netbuf_ptr->num_frags) {
    	if(a_netbuf_ptr->native_orig != NULL){ 
	    	/* count up fragments */
	    	for(i=0 ; i<a_netbuf_ptr->num_frags ; i++){
	    		len += a_netbuf_ptr->native_orig->bufFragment[i].length;
	    	}
	    }else{
	    	/* used for special case native's from ATH_TX_RAW */
	    	for(i=1 ; i<=a_netbuf_ptr->num_frags ; i++){
	    		len += a_netbuf_ptr->native.bufFragment[i].length;	
	    	}
	    }	
    }
    
    return len;
}


/* returns a buffer fragment of a packet.  If the packet is not 
 * fragmented only index == 0 will return a buffer which will be 
 * the whole packet. pLen will hold the length of the buffer in 
 * bytes.
 */
A_VOID*
a_netbuf_get_fragment(A_VOID *bufPtr, A_UINT8 index, A_INT32 *pLen)
{
	void* pBuf = NULL;
	A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
	
	if(0==index){
		pBuf = a_netbuf_to_data(bufPtr);
		*pLen = (A_INT32)((A_UINT32)a_netbuf_ptr->tail - (A_UINT32)a_netbuf_ptr->data);
	}else if(a_netbuf_ptr->num_frags >= index) {
		if(a_netbuf_ptr->native_orig){
			/* additional fragments are held in native_orig. this is only
			 * used for tx packets received from the TCP stack. */
			pBuf = a_netbuf_ptr->native_orig->bufFragment[index-1].payload;
			*pLen = (A_INT32)a_netbuf_ptr->native_orig->bufFragment[index-1].length;	
		}else{
			/* used for special case native's from ATH_TX_RAW */
			pBuf = a_netbuf_ptr->native.bufFragment[index].payload;
			*pLen = (A_INT32)a_netbuf_ptr->native.bufFragment[index].length;
		}	
	}
	
	return pBuf;	
}

/* fills the spare bufFragment position for the net buffer */
A_VOID
a_netbuf_append_fragment(A_VOID *bufPtr, A_UINT8 *frag, A_INT32 len)
{
    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
    /* the first bufFragment is used to hold the built-in buffer. subsequent
     * entries if any can be used to append additional buffers. hence 
     * deduct 1 from MAX_BUF_FRAGS */
    if(a_netbuf_ptr->num_frags < MAX_BUF_FRAGS-1){
        a_netbuf_ptr->num_frags++;
        a_netbuf_ptr->native.bufFragment[a_netbuf_ptr->num_frags].payload = frag;
        a_netbuf_ptr->native.bufFragment[a_netbuf_ptr->num_frags].length = len;        
    }else{
        A_ASSERT(0);
    }   
}







A_VOID
a_netbuf_configure(A_VOID* buffptr, A_VOID *buffer, A_UINT16 headroom, A_UINT16 length, A_UINT16 size)
{
	A_NETBUF* a_netbuf_ptr = (A_NETBUF*)buffptr;
	
	A_MEMZERO(a_netbuf_ptr, sizeof(A_NETBUF));
	
	if(buffer != NULL){
	    a_netbuf_ptr->head = buffer;
        a_netbuf_ptr->data = &(((A_UINT8*)buffer)[headroom]);
	    a_netbuf_ptr->tail = &(((A_UINT8*)buffer)[headroom+length]);
        a_netbuf_ptr->end = &(((A_UINT8*)buffer)[size]);
    }
}




A_VOID *
a_netbuf_to_data(A_VOID *bufPtr)
{
    return (((A_NETBUF*)bufPtr)->data);
}

/*
 * Add len # of bytes to the beginning of the network buffer
 * pointed to by bufPtr
 */
A_STATUS
a_netbuf_push(A_VOID *bufPtr, A_INT32 len)
{
    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
    
    if((A_UINT32)a_netbuf_ptr->data - (A_UINT32)a_netbuf_ptr->head < len) 
    {
    	A_ASSERT(0);
    }
    
    a_netbuf_ptr->data = (pointer)(((A_UINT32)a_netbuf_ptr->data) - len);    

    return A_OK;
}

/*
 * Add len # of bytes to the beginning of the network buffer
 * pointed to by bufPtr and also fill with data
 */
A_STATUS
a_netbuf_push_data(A_VOID *bufPtr, A_UINT8 *srcPtr, A_INT32 len)
{
    a_netbuf_push(bufPtr, len);
    A_MEMCPY(((A_NETBUF*)bufPtr)->data, srcPtr, (A_UINT32)len);

    return A_OK;
}

/*
 * Add len # of bytes to the end of the network buffer
 * pointed to by bufPtr
 */
A_STATUS
a_netbuf_put(A_VOID *bufPtr, A_INT32 len)
{
    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
    
    if((A_UINT32)a_netbuf_ptr->end - (A_UINT32)a_netbuf_ptr->tail < len) {
    	A_ASSERT(0);
    }
    
    a_netbuf_ptr->tail = (pointer)(((A_UINT32)a_netbuf_ptr->tail) + len);
    
    return A_OK;
}

/*
 * Add len # of bytes to the end of the network buffer
 * pointed to by bufPtr and also fill with data
 */
A_STATUS
a_netbuf_put_data(A_VOID *bufPtr, A_UINT8 *srcPtr, A_INT32 len)
{
    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
    void *start = a_netbuf_ptr->tail;
    
    a_netbuf_put(bufPtr, len);
    A_MEMCPY(start, srcPtr, (A_UINT32)len);

    return A_OK;
}





/*
 * Returns the number of bytes available to a a_netbuf_push()
 */
A_INT32
a_netbuf_headroom(A_VOID *bufPtr)
{
    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
    
    return (A_INT32)((A_UINT32)a_netbuf_ptr->data - (A_UINT32)a_netbuf_ptr->head);
}

A_INT32
a_netbuf_tailroom(A_VOID *bufPtr)
{
	A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
    
    return (A_INT32)((A_UINT32)a_netbuf_ptr->end - (A_UINT32)a_netbuf_ptr->tail);	
}

/*
 * Removes specified number of bytes from the beginning of the buffer
 */
A_STATUS
a_netbuf_pull(A_VOID *bufPtr, A_INT32 len)
{
    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
    
    if((A_UINT32)a_netbuf_ptr->tail - (A_UINT32)a_netbuf_ptr->data < len) {
    	A_ASSERT(0);
    }

    a_netbuf_ptr->data = (pointer)((A_UINT32)a_netbuf_ptr->data + len);

    return A_OK;
}

/*
 * Removes specified number of bytes from the end of the buffer
 */
A_STATUS
a_netbuf_trim(A_VOID *bufPtr, A_INT32 len)
{
    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)bufPtr;
    
    if((A_UINT32)a_netbuf_ptr->tail - (A_UINT32)a_netbuf_ptr->data < len) {
    	A_ASSERT(0);
    }

    a_netbuf_ptr->tail = (pointer)((A_UINT32)a_netbuf_ptr->tail - len);

    return A_OK;
}


A_VOID* a_malloc(A_INT32 size, A_UINT8 id)
{
	A_VOID* addr;
        int len;
	addr =  _mem_alloc_system_zero((A_UINT32)size);
	if(addr != NULL)
        {
          len = _get_size(addr);
          g_totAlloc += len;
        }
	/* in this implementation malloc ID is not used */
	A_UNUSED_ARGUMENT(id);
	return addr;
}

A_VOID a_free(A_VOID* addr, A_UINT8 id)
{
	int len = _get_size(addr);
	g_totAlloc -= len;
	/* in this implementation malloc ID is not used */
	A_UNUSED_ARGUMENT(id);
	_mem_free(addr);
}

