//------------------------------------------------------------------------------
// <copyright file="custom_stack_offload.h" company="Atheros">
//    Copyright (c) 2004-2007 Atheros Corporation.  All rights reserved.
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

#ifndef _CUSTOM_STACK_OFFLOAD_H_
#define _CUSTOM_STACK_OFFLOAD_H_

#include <atheros_stack_offload.h>
#include "common_stack_offload.h"


/* Wifi device not responding error */
#define A_WHISTLE_REBOOT   -3
#define HALF_CLOSE         100

#define CUSTOM_BLOCK(pCxt,ctxt,msec, direction)  \
		blockForResponse((pCxt), (ctxt), (msec), (direction))
		
#define CUSTOM_UNBLOCK(ctxt, direction)  \
		unblock((ctxt),(direction))		

#define CUSTOM_SOCKET_CONTEXT_INIT()  \
		custom_socket_context_init()

#define CUSTOM_SOCKET_CONTEXT_DEINIT()  \
		custom_socket_context_deinit()

#define GET_CUSTOM_SOCKET_CONTEXT(ctxt)  \
  		((SOCKET_CUSTOM_CONTEXT_PTR)(((ATH_SOCKET_CONTEXT_PTR)ctxt)->sock_custom_context))		 

#define IS_SOCKET_BLOCKED(index)  \
        isSocketBlocked((index))

#define CUSTOM_QUEUE_EMPTY(index) \
	  		custom_queue_empty((index))

/* Headroom definitions*/
#define UDP_HEADROOM 44 
#define TCP_HEADROOM 64
#define UDP6_HEADROOM 64
#define TCP6_HEADROOM 88
#define TCP6_HEADROOM_WITH_NO_OPTION 84

#define TX_PKT_OVERHEAD (TCP6_HEADROOM + sizeof(DRV_BUFFER) + sizeof(A_UINT32))

/***Custom socket context. This must be adjusted based on the underlying OS ***/
typedef struct socket_custom_context {
    LWEVENT_STRUCT     sockWakeEvent;	//Event to unblock waiting socket
    A_NETBUF_QUEUE_T   rxqueue;     	//Queue to hold incoming packets	
    A_UINT8		   blockFlag;
    A_BOOL             respAvailable;   //Flag to indicate a response from target is available
    A_BOOL             txUnblocked;
    A_BOOL             txBlock;
    A_BOOL             rxBlock;
    
#if NON_BLOCKING_TX
    A_NETBUF_QUEUE_T   non_block_queue;   //Queue to hold packets to be freed later
    A_MUTEX_T          nb_tx_mutex;       //Mutex to synchronize access to non blocking queue
#endif        
    
    /*
     * Added by Whistle
     */
    union 
    {
        SOCKADDR_T addr4;
        SOCKADDR_6_T addr6;    
    }addr;
} SOCKET_CUSTOM_CONTEXT, *SOCKET_CUSTOM_CONTEXT_PTR;


#if ZERO_COPY
extern A_NETBUF_QUEUE_T zero_copy_free_queue;
#endif


/******* Function Declarations *******************/
A_STATUS unblock(A_VOID* ctxt, A_UINT8 direction);
A_STATUS blockForResponse(A_VOID* pCxt, A_VOID* ctxt, A_UINT32 msec, A_UINT8 direction);
uint_32 isSocketBlocked(A_VOID* ctxt);
A_STATUS custom_socket_context_init();
A_STATUS custom_socket_context_deinit();
A_UINT32 custom_receive_tcpip(A_VOID *pCxt, A_VOID *pReq);
void txpkt_free(A_VOID* buffPtr);
uint_32 custom_send_tcpip
   (
      A_VOID*  			   pCxt,
         /* [IN] the Ethernet state structure */
      DRV_BUFFER_PTR              db_ptr,
         /* [IN] the packet to send */
      uint_32              size,
         /* [IN] total size of the packet */
      uint_32              frags,
         /* [IN] total fragments in the packet */
      uint_8*              header,   
      uint_32              header_size   
     
   );

A_VOID custom_free(A_VOID* buf);
A_VOID* custom_alloc(A_UINT32 size);
A_UINT32 get_total_pkts_buffered();
A_INT32 t_ipconfig(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 mode,A_UINT32* ipv4_addr, A_UINT32* subnetMask, A_UINT32* gateway4);
A_INT32 t_ip6config(ENET_CONTEXT_STRUCT_PTR enet_ptr, A_UINT32 mode,IP6_ADDR_T *v6Global,IP6_ADDR_T *v6Local,
		    IP6_ADDR_T *v6DefGw,IP6_ADDR_T *v6GlobalExtd,A_INT32 *LinkPrefix, A_INT32 *GlbPrefix, A_INT32 *DefGwPrefix, A_INT32 *GlbPrefixExtd);
A_UINT32 custom_queue_empty(A_UINT32 index);
ATH_SOCKET_CONTEXT* get_ath_sock_context(A_UINT32 handle);

#endif
