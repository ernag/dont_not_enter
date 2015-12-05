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
#include <common_api.h>
#include <custom_api.h>
#include <htc.h>
#include "mqx.h"
#include "bsp.h"
#include "enet.h"
#include "enetprv.h"
#include <atheros_wifi_api.h>
#include "cust_netbuf.h"

#include "atheros_stack_offload.h"
#include "common_stack_offload.h"
#include "custom_stack_offload.h"
#include "atheros_wifi.h"


A_UINT32 Custom_Api_Send(ENET_CONTEXT_STRUCT_PTR enet_ptr, PCB_PTR pcb_ptr, uint_32 size, uint_32 frags, uint_32 flags);


A_UINT32 Custom_Api_Send(ENET_CONTEXT_STRUCT_PTR enet_ptr, PCB_PTR pcb_ptr, uint_32 size, uint_32 frags, uint_32 flags)
{
	return A_OK;
}


/*****************************************************************************/
/*  custom_receive_tcpip - Called by Custom_DeliverFrameToNetworkStack to 
 *   deliver a data frame to the application thread. It unblocks a waiting 
 *   application thread and queues the packet in the queue of the corresponding 
 *   socket. If socket is not valid, the packet is dropped.
 *      A_VOID *pCxt - the driver context. 
 *      A_VOID *pReq - the packet.
 *****************************************************************************/
uint_32 custom_receive_tcpip(A_VOID *pCxt, A_VOID *pReq)
{
	A_NETBUF* a_netbuf_ptr = (A_NETBUF*)pReq;	
	A_INT32 index = 0, handle=0;
	A_UINT8* data;
	
	SOCKET_CUSTOM_CONTEXT_PTR pcustctxt = NULL;
	
    if(a_netbuf_ptr) {        
  	
		data = (A_UINT8*)A_NETBUF_DATA(a_netbuf_ptr);
	
    	/*Extract socket handle from custom header*/
    	handle = A_CPU2LE32(*((A_UINT32*)data));
    	
    	/*Get index*/
    	index = find_socket_context(handle, TRUE);
		if(index < 0 || index > MAX_SOCKETS_SUPPORTED)
		{
			last_driver_error = A_SOCKCXT_NOT_FOUND;
			A_NETBUF_FREE(pReq);
			return A_ERROR;
		}

        if(ath_sock_context[index]->sock_st_mask){
            /* socket is pending a socket event so it cannot 
             * receive data packets */
            last_driver_error = A_SOCK_UNAVAILABLE;
            A_NETBUF_FREE(pReq);
            return A_ERROR;
        }
						
		pcustctxt = GET_CUSTOM_SOCKET_CONTEXT(ath_sock_context[index]);

		//if(!IS_SOCKET_BLOCKED(ath_sock_context[index]))
		/*To-Do- Remove this once drop ligic is implemented in firmware*/ 
		if(get_total_pkts_buffered() >= BSP_CONFIG_ATHEROS_PCB /*- 1*/)
		{
			printf("%s dropping pkts for %x\n", __FUNCTION__, handle);
			A_NETBUF_FREE(pReq);
		}
		else
		{
			A_NETBUF_ENQUEUE(&(pcustctxt->rxqueue), a_netbuf_ptr);
			CUSTOM_UNBLOCK(ath_sock_context[index], RX_DIRECTION);  
		}

    }
    return A_OK;
}

/*****************************************************************************/
/*  Custom_DeliverFrameToNetworkStack - Called by API_RxComplete to 
 *   deliver a data frame to the network stack. This code will perform 
 *   platform specific operations.
 *      A_VOID *pCxt - the driver context. 
 *      A_VOID *pReq - the packet.
 *****************************************************************************/
A_VOID 
Custom_DeliverFrameToNetworkStack(A_VOID *pCxt, A_VOID *pReq)
{
	ATH_PROMISCUOUS_CB prom_cb = (ATH_PROMISCUOUS_CB)(GET_DRIVER_CXT(pCxt)->promiscuous_cb);
    		
    if(GET_DRIVER_COMMON(pCxt)->promiscuous_mode){
        prom_cb(pReq);
    }else{
    	custom_receive_tcpip(pCxt,pReq);
	}     
}





/*****************************************************************************/
/*  Custom_Api_Send - Entry point for MQX driver interface to send a packet.
 *	 This is specific to MQX. This function is NOT called from within the 
 *	 driver.
 *      ENET_CONTEXT_STRUCT_PTR  enet_ptr - the MQX driver context.
 *		PCB_PTR              pcb_ptr - the MQX packet object.
 *		uint_32              size - the length in bytes of pcb_ptr.
 *		uint_32              frags - the number of fragments in pcb_ptr.
 *		uint_32              flags - optional flags for transmit.      
 *****************************************************************************/
uint_32 custom_send_tcpip
   (  
   	  A_VOID*              pCxt,    
      DRV_BUFFER_PTR       db_ptr,
         /* [IN] the packet to send */
      uint_32              size,
         /* [IN] total size of the packet */
      uint_32              frags,
         /* [IN] total fragments in the packet */
      A_UINT8*              header,   
      uint_32              header_size   
      
   )
{ 
    uint_32 error = A_OK;
    A_NETBUF* a_netbuf_ptr; 
    uint_32 total_hdr_size =  header_size;
    A_UINT8* payloadPtr = db_ptr->bufFragment[0].payload;
    
    /* create an atheros pcb and continue or fail. */
    do{    	
    	
      /*Get the netbuf pointer from TX packet*/
        a_netbuf_ptr = ((TX_PACKET_PTR)(payloadPtr - TX_PKT_OVERHEAD))->a_netbuf_ptr;

        a_netbuf_ptr->num_frags = (A_UINT8)frags;   

        A_NETBUF_PUT_DATA(a_netbuf_ptr, (A_UINT8*)header, total_hdr_size);
    	//ensure there is enough headroom to complete the tx operation
    	if (A_NETBUF_HEADROOM(a_netbuf_ptr) < sizeof(ATH_MAC_HDR) + sizeof(ATH_LLC_SNAP_HDR) +
            		sizeof(WMI_DATA_HDR) + HTC_HDR_LENGTH + WMI_MAX_TX_META_SZ) {
            error = A_NO_MEMORY;
            A_NETBUF_FREE(a_netbuf_ptr);
            break;   		
        }
    	//carry the original around until completion.
    	//it is freed by A_NETBUF_FREE  
    	a_netbuf_ptr->native_orig = db_ptr;       	
        
        if(A_OK != Api_DataTxStart(pCxt, (A_VOID*)a_netbuf_ptr)){
            error = A_ERROR;
            break;
    	} 	    
   	}while(0);
    
    if(error != A_OK){
		/* in the event of api failure, the original pcb should be returned to the caller un-touched
		 * and the a_netbuf_ptr should be freed */
		if(error != A_NO_MEMORY) 
			A_NETBUF_FREE((A_VOID*)a_netbuf_ptr);
	}          
    return error;
}

