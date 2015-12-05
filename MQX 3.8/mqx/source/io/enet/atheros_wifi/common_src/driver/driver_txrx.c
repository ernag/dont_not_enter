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
#include <driver_cxt.h>
#include <common_api.h>
#include <custom_api.h>
#include <spi_hcd_if.h>
#include <hif_internal.h>
#include <htc.h>


/*****************************************************************************/
/*  Driver_RxComplete - For completed RX packet requests this function routes
 *   the request to a handler based on endpoint ID.  Called when the Bus
 *   transfer operation has completed.
 *      A_VOID *pCxt - the driver context.
 *      A_VOID *pReq - the request object.
 *****************************************************************************/
A_VOID Driver_RxComplete(A_VOID *pCxt, A_VOID *pReq)
{
    A_UINT32 lookAheads[1];
    A_UINT32 NumLookAheads = 0;

    if(A_OK != Htc_ProcessRecvHeader(pCxt, pReq, lookAheads, &NumLookAheads)){
        A_ASSERT(0);
    }

    if(A_NETBUF_GET_ELEM(pReq, A_REQ_EPID) == ENDPOINT_0){
        Htc_RxComplete(pCxt, pReq);
    }else{
        Api_RxComplete(pCxt, pReq);
    }
}

/*****************************************************************************/
/*  Driver_TxComplete - For completed TX packet requests this function routes
 *   the request to a handler based on endpoint ID.  Called when the Bus
 *   transfer operation has completed.
 *      A_VOID *pCxt - the driver context.
 *      A_VOID *pReq - the request object.
 *****************************************************************************/
static A_VOID 
Driver_TxComplete(A_VOID *pCxt, A_VOID *pReq)
{
	Htc_ProcessTxComplete(pCxt, pReq);
    
    if(A_NETBUF_GET_ELEM(pReq, A_REQ_EPID) == ENDPOINT_0){
        /* this should never happen in practice as the driver
         * does not SEND messages on the HTC endpoint */
        A_ASSERT(0);
        //HTC_TxComplete(pCxt, pReq);
    }else{
        Api_TxComplete(pCxt, pReq);
    }
}

/*****************************************************************************/
/*  Driver_PostProcessRequest - Utility to post process requests that have 
 *	 completed Hcd_Request().
 *      A_VOID *pCxt - the driver context.
 *      A_VOID *pReq - the request object.
 *		A_STATUS reqStatus - the status of the transaction.
 *****************************************************************************/
static A_VOID
Driver_PostProcessRequest(A_VOID *pCxt, A_VOID *pReq, A_STATUS reqStatus)
{
	A_DRIVER_CONTEXT *pDCxt; 
	
	A_NETBUF_SET_ELEM(pReq, A_REQ_STATUS, reqStatus);
	
	if(A_NETBUF_GET_ELEM(pReq, A_REQ_CALLBACK) != NULL){
		pDCxt = GET_DRIVER_COMMON(pCxt);
		pDCxt->hcd.pCurrentRequest = pReq;
		
		if(reqStatus != A_PENDING){
    		pDCxt->hcd.booleans |= SDHD_BOOL_DMA_COMPLETE;
    	}
    }
}

/*****************************************************************************/
/*  Driver_SubmitTxRequest - Called from a User thread to submit a new
 *      Tx packet to the driver. The function will append the request to the
 *      drivers txqueue and wake up the driver thread.
 *      A_VOID *pCxt - the driver context.
 *      A_VOID *pReq - the request object.
 *****************************************************************************/
A_STATUS 
Driver_SubmitTxRequest(A_VOID *pCxt, A_VOID *pReq)
{
    A_STATUS status = A_ERROR;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	A_UINT32 transferLength, creditsRequired;
	
	do{
	    A_NETBUF_SET_ELEM(pReq, A_REQ_CALLBACK, Driver_TxComplete);
	    /* reserve space for HTC header to allow for proper calculation
	     * of transmit length. The header will be populated when the 
	     * request is transmitted. */
	    if (A_NETBUF_PUSH(pReq, HTC_HDR_LENGTH) != A_OK) {
	        break;
	    }
	    /* The length of the request is now complete so 
	     * calculate and assign the number of credits required by this
	     * request. */
	    transferLength = DEV_CALC_SEND_PADDED_LEN(pDCxt, A_NETBUF_LEN(pReq));
	    
		if (transferLength <= pDCxt->creditSize) {
            creditsRequired = 1;    
        } else {
                /* figure out how many credits this message requires */
            creditsRequired = (A_UINT32)(transferLength / pDCxt->creditSize);
            
            if(creditsRequired * pDCxt->creditSize < transferLength){
            	creditsRequired++;
            }            
        }
		
		A_NETBUF_SET_ELEM(pReq, A_REQ_CREDITS, creditsRequired);
		
		TXQUEUE_ACCESS_ACQUIRE(pCxt);
		{
			A_NETBUF_ENQUEUE(&(pDCxt->txQueue), pReq);		
			status = A_OK;	
		}
		TXQUEUE_ACCESS_RELEASE(pCxt);
		
		if(status == A_OK){
			CUSTOM_DRIVER_WAKE_DRIVER(pCxt);
		}		
	}while(0);
	
	return status;
}

/*****************************************************************************/
/*  Driver_SendPacket - Entry point to send a packet from the device. Sets
 *   up request params address and other and calls HCD_Request to perform the
 *   work.  If a callback is provided in the request then it will be used to 
 *   complete the request.  If a callback is not provided the lower layers 
 *	 must complete the request synchronously.
 *      A_VOID *pCxt - the driver context.
 *      A_VOID *pReq - the request object.
 *****************************************************************************/
A_STATUS Driver_SendPacket(A_VOID *pCxt, A_VOID *pReq)
{    
    A_STATUS status = A_ERROR;    
    A_UINT32 address;
    A_UINT16 transLength;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);

    do{        
    	if(A_OK != (status = Htc_SendPacket(pCxt, pReq))){
    		Driver_PostProcessRequest(pCxt, pReq, status);    		    		    	
    		break;
        }
        /* calc the padding and mbox address */
        /* NOTE: for Transmit the request length may not equal the transfer length due to 
         *  padding requirements. */
        transLength = DEV_CALC_RECV_PADDED_LEN(pDCxt, A_NETBUF_LEN(pReq));
        address = Hcd_GetMboxAddress(pCxt, HIF_ACTIVE_MBOX_INDEX, transLength);    
        address &= ATH_TRANS_ADDR_MASK;
        A_NETBUF_SET_ELEM(pReq, A_REQ_ADDRESS, address);
        A_NETBUF_SET_ELEM(pReq, A_REQ_TRANSFER_LEN, transLength);
        /* init the packet read params/cmd incremental vs fixed address etc. */
        A_NETBUF_SET_ELEM(pReq, A_REQ_COMMAND, (ATH_TRANS_WRITE | ATH_TRANS_DMA));
        status = Hcd_Request(pCxt, pReq); 		
        Driver_PostProcessRequest(pCxt, pReq, status);  
    }while(0);

    return status;
}

/*****************************************************************************/
/*  Driver_RecvPacket - Entry point to receive a packet from the device. Sets
 *   up request params address and other and calls HCD_Request to perform the
 *   work.  This path always provides a request callback for asynchronous 
 *   completion.  This is because DMA if available will always be used for 
 *   packet requests.
 *      A_VOID *pCxt - the driver context.
 *      A_VOID *pReq - the request object.
 *****************************************************************************/
A_STATUS Driver_RecvPacket(A_VOID *pCxt, A_VOID *pReq)
{
    //A_UINT16 validLength, transLength, hdrLength;    
    
    A_STATUS status;
    A_UINT32 address;

    Htc_PrepareRecvPacket(pCxt, pReq);
        
    /* init the packet mailbox address - begin with mailbox end address and then subtract the request transfer length */
    address = Hcd_GetMboxAddress(pCxt, HIF_ACTIVE_MBOX_INDEX, A_NETBUF_LEN(pReq));    
    address &= ATH_TRANS_ADDR_MASK;
    A_NETBUF_SET_ELEM(pReq, A_REQ_ADDRESS, address);
    /* init the packet read params/cmd incremental vs fixed address etc. */
    A_NETBUF_SET_ELEM(pReq, A_REQ_COMMAND, (ATH_TRANS_READ | ATH_TRANS_DMA));

    status = Hcd_Request(pCxt, pReq); 
    Driver_PostProcessRequest(pCxt, pReq, status);  

    return status;
}