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
#include <wmi_api.h>
#include <netbuf.h>
#include <htc.h>
#include <spi_hcd_if.h>

// dump debug info before giving up the ghost
static void dump_error_info( A_DRIVER_CONTEXT *pDCxt, A_UINT32 sz, char *func )
{
    printf("ERR %s()\n", func );
    printf("drv_up=%d, chipDown=%d\n", pDCxt->driver_up, pDCxt->chipDown); 
    printf("pktSz=%d\n", sz);
    fflush(stdout);
}

/*
 *   This meaningless delay (previously in Custom_HW_InterruptHandler()),
 *     SEEMED to reduce the amount of CUSTOM_BLOCK's we were getting for some reason.
 *     
 *   The theory of why this would help?
 *     Maybe the Atheros interrupts the K60, and the K60 services the Atheros "too quickly".
 *     In other words, maybe the Atheros is not completely ready to be a SPI slave when it
 *       interrupts the K60?  That is the theory behind this delay.
 */
static void __ernie_meaningless_delay(void)
{
    int i;
    
    for(i = 0; i < 100; i++)
    {
        
    }
}

/*****************************************************************************/
/********** IMPLEMENTATION **********/
/*****************************************************************************/

/*****************************************************************************/
/*  Driver_Main - This is the top level entry point for the Atheros wifi driver
 *   This should be called from either a dedicated thread running in a loop or,
 *   in the case of single threaded systems it should be called periodically
 *   from the main loop.  Also, if in a single threaded system a API call
 *   requires synchronous completion then it may be necessary to call this 
 *   function from within the API call.
 *      A_VOID *pCxt - the driver context.
 *      A_UINT8 scope - Limits/allows what type of activity may be processed.
 *          Options are any combination of DRIVER_SCOPE_TX + DRIVER_SCOPE_RX.
 *      A_BOOL *pCanBlock - used to report to caller whether the driver is
 *          in a state where it is safe to block until further notice.
 *		A_UINT16 *pBlock_msec - used to report to caller how long the driver
 *			may block for. if zero then driver can block indefinitely.
 *****************************************************************************/  
A_STATUS 
Driver_Main(A_VOID *pCxt, A_UINT8 scope, A_BOOL *pCanBlock, A_UINT16 *pBlock_msec)
{
    A_VOID *pReq;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);    
    
    pDCxt->driver_state = DRIVER_STATE_PENDING_CONDITION_A;
    
    /* identify the interrupt and mask it */
    if(pDCxt->hcd.IrqDetected == A_TRUE)
    {
        __ernie_meaningless_delay();
    	pDCxt->driver_state = DRIVER_STATE_INTERRUPT_PROCESSING;
    	pDCxt->hcd.IrqDetected = A_FALSE;
    	
        if(A_FALSE == Hcd_SpiInterrupt(pCxt)){
            /* FIXME: HANDLE ERROR CONDITION */
        }             
    }   

    pDCxt->driver_state = DRIVER_STATE_PENDING_CONDITION_B;
    /* check to see if a deferred bus request has completed. if so 
     * process it. */
    if(pDCxt->hcd.booleans & SDHD_BOOL_DMA_COMPLETE){
        /* get the request */
        if(A_OK == Driver_CompleteRequest(pCxt, pDCxt->hcd.pCurrentRequest)){
	        /* clear the pending request and the boolean */
	        pDCxt->hcd.pCurrentRequest = NULL;
	        pDCxt->hcd.booleans &= ~SDHD_BOOL_DMA_COMPLETE;              
        }
    }
    
    if(pDCxt->hcd.CpuInterrupt){
    	pDCxt->driver_state = DRIVER_STATE_INTERRUPT_PROCESSING;
    	Hcd_ClearCPUInterrupt(pCxt);
        Hcd_UnmaskSPIInterrupts(pCxt, ATH_SPI_INTR_LOCAL_CPU_INTR);
        HTC_ProcessCpuInterrupt(pCxt);
    	pDCxt->hcd.CpuInterrupt = 0;    	
    }	

    pDCxt->driver_state = DRIVER_STATE_PENDING_CONDITION_C;
    /* This is where packets are received from the
     * wifi device. Because system buffers must be 
     * available to receive a packet we only call into
     * this function if we know that buffers are 
     * available to satisfy the operation.
     */
    if((scope & DRIVER_SCOPE_RX) && A_TRUE == Driver_RxReady(pCxt)){
    	pDCxt->driver_state = DRIVER_STATE_RX_PROCESSING;
        pReq = pDCxt->pRxReq;
        pDCxt->pRxReq = NULL;
        if(A_OK != Driver_RecvPacket(pCxt, pReq)){
            /* if this happens it is a critical error */
            A_ASSERT(0);
        }
        /* FIXME: as an optimization the next lookahead may have 
         * been acquired from the trailer of the previous rx packet.
         * in that case we should pre-load the lookahead so as to 
         * avoid reading it from the registers. */
        /* reset the lookahead for the next read operation */
        pDCxt->lookAhead = 0;
        /* check to see if a deferred bus request has completed. if so 
	     * process it. */
	    if(pDCxt->hcd.booleans & SDHD_BOOL_DMA_COMPLETE){
	        /* get the request */
	        if(A_OK == Driver_CompleteRequest(pCxt, pDCxt->hcd.pCurrentRequest)){
		        /* clear the pending request and the boolean */
		        pDCxt->hcd.pCurrentRequest = NULL;
		        pDCxt->hcd.booleans &= ~SDHD_BOOL_DMA_COMPLETE;              
	        }
	    }	    	    
    }

    pDCxt->driver_state = DRIVER_STATE_PENDING_CONDITION_D;

    if((scope & DRIVER_SCOPE_TX) && A_TRUE == Driver_TxReady(pCxt)){
    	pDCxt->driver_state = DRIVER_STATE_TX_PROCESSING;
        /* after processing any outstanding device interrupts
         * see if there is a packet that requires transmitting
         */    
    	if(pDCxt->txQueue.count){    
    		/* accesslock here to sync with threads calling 
    		 * submit TX 
    		 */   		 
    		TXQUEUE_ACCESS_ACQUIRE(pCxt);		
    		{
	    		pReq = A_NETBUF_DEQUEUE(&(pDCxt->txQueue));		    			    		
    		}
    		TXQUEUE_ACCESS_RELEASE(pCxt);	
            
            if(pReq != NULL){            	
    		    Driver_SendPacket(pCxt, pReq);    		    
            }
    	}        	    	
    }

    pDCxt->driver_state = DRIVER_STATE_PENDING_CONDITION_E;	
    
    /* execute any asynchronous/special request when possible */
    if(0 == (pDCxt->hcd.booleans & SDHD_BOOL_DMA_IN_PROG) && pDCxt->asynchRequest){
    	pDCxt->driver_state = DRIVER_STATE_ASYNC_PROCESSING;
    	pDCxt->asynchRequest(pCxt);
    	pDCxt->driver_state = DRIVER_STATE_PENDING_CONDITION_F;
    }
    
    pDCxt->driver_state = DRIVER_STATE_PENDING_CONDITION_G;
	/* allow caller to block if all necessary conditions are satisfied */
    if(pCanBlock){
        if( (pDCxt->hcd.PendingIrqAcks == 0 || pDCxt->rxBufferStatus == A_FALSE) &&
        	(pDCxt->hcd.IrqDetected == A_FALSE) && 
        	(pDCxt->txQueue.count == 0 || Driver_TxReady(pCxt)==A_FALSE) &&        	
        	(0 == (pDCxt->hcd.booleans & SDHD_BOOL_DMA_COMPLETE))){
        	*pCanBlock = A_TRUE;
        	
        	if(pDCxt->creditDeadlock == A_TRUE){
        		if(pBlock_msec){
        			*pBlock_msec = DEADLOCK_BLOCK_MSEC;        			
        		}
        		
        		pDCxt->creditDeadlockCounter++;
        	}
        	
        	pDCxt->driver_state = DRIVER_STATE_IDLE;	
        }else{
        	*pCanBlock = A_FALSE;        	
        }            
    }

    return A_OK;
}

/*****************************************************************************/
/*  Driver_ReportRxBuffStatus - Tracks availability of Rx Buffers for those 
 *	 systems that have a limited pool. The driver quearies this status before
 *	 initiating a RX packet transaction.
 *      A_VOID *pCxt - the driver context.
 *      A_BOOL status - new status A_TRUE - RX buffers are available, 
 *			A_FALSE - RX buffers are not available.
 *****************************************************************************/
A_VOID 
Driver_ReportRxBuffStatus(A_VOID *pCxt, A_BOOL status)
{    
	A_BOOL oldStatus;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);

    /* NOTE: Dont acquire the lock here instead the 
     * caller should acquire this lock if necessary prior to calling this function */
	//RXBUFFER_ACCESS_ACQUIRE(pCxt);
	{
		oldStatus = pDCxt->rxBufferStatus;
        pDCxt->rxBufferStatus = status;
	}
	//RXBUFFER_ACCESS_RELEASE(pCxt);
	/* the driver thread may have blocked on this 
	 * status so conditionally wake up the thread
	 * via QueueWork */
	if(oldStatus == A_FALSE && status == A_TRUE){
        if(pDCxt->hcd.PendingIrqAcks){
			CUSTOM_DRIVER_WAKE_DRIVER(pCxt);
        }
	}
}


/*****************************************************************************/
/*  Driver_CompleteRequest - Completes a deferred request. One that finished
 *      asynchronously. Such a request must have a non-null callback that will
 *      be called by this function to fullfill the operation.
 *      A_VOID *pCxt - the driver context.
 *      A_VOID *pReq - the bus request object.
 *****************************************************************************/
A_STATUS 
Driver_CompleteRequest(A_VOID *pCxt, A_VOID *pReq)
{
	A_STATUS status = A_ERROR;
    A_VOID (*cb)(A_VOID *, A_VOID *);
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    /* A hardware WAR exists whereby interrupts from the device are disabled
     * during an ongoing bus transaction.  This function is called when
     * the bus transaction completes.  if irqMask is set then it means that
     * interrupts should be re-enabled */
    if(pDCxt->hcd.irqMask){
        Hcd_UnmaskSPIInterrupts(pCxt, pDCxt->hcd.irqMask);	
        pDCxt->hcd.irqMask = 0;
    }
    
    if(pReq != NULL && NULL != (cb = A_NETBUF_GET_ELEM(pReq, A_REQ_CALLBACK))){
        cb(pCxt, pReq);
        status = A_OK;   
    }else{
        A_ASSERT(0);
    }
    
    return status;
}

/*****************************************************************************/
/*  Driver_TxReady - Determines whether it is safe to start a TX operation. If 
 *     a TX operation can be started this function returns A_TRUE else A_FALSE.
 *      A_VOID *pCxt - the driver context. 
 *****************************************************************************/
A_BOOL Driver_TxReady(A_VOID *pCxt)
{
    A_BOOL res = A_FALSE;
    A_VOID *pReq;
    A_ENDPOINT_T *pEp;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);

	pDCxt->creditDeadlock = A_FALSE;
	
    do{
    	/* there can be no errors */
        if(pDCxt->hcd.booleans & SDHD_BOOL_FATAL_ERROR){
            break;
        }
        /* there can be no in-complete requests */
        if((pDCxt->hcd.booleans & SDHD_BOOL_DMA_IN_PROG) || 
           (pDCxt->hcd.booleans & SDHD_BOOL_DMA_COMPLETE)|| 
           /* the write buffer watermark interrupt must not be configured */
           (pDCxt->hcd.booleans & SDHD_BOOL_DMA_WRITE_WAIT_FOR_BUFFER)){
            break;
        }
       
        /* there must be enough credits for the target endpoint */
        if(NULL == (pReq = A_NETBUF_PEEK_QUEUE(&(pDCxt->txQueue)))){
            break;
        }
        
        if(NULL == (pEp = Util_GetEndpoint(pCxt, A_NETBUF_GET_ELEM(pReq, A_REQ_EPID)))){
            break;
        }      
        /* ensure as sanity check that the request length is less than a credit length */
        if(pDCxt->creditSize < DEV_CALC_SEND_PADDED_LEN(pDCxt, A_NETBUF_LEN(pReq))){
            
// DEBUG
dump_error_info(pDCxt, (A_UINT32) DEV_CALC_SEND_PADDED_LEN(pDCxt, A_NETBUF_LEN(pReq )), (char*)__func__);
            
            // THIS IS AN ERROR AS REQUESTS SHOULD NEVER EXCEED THE CREDIT SIZE
            A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT            
            break;      
#endif          
        }                
        /* confirm there are enough credits for this transfer */
        if(0 == pEp->credits){
        	//if(pDCxt->rxBufferStatus == A_FALSE)
        	{
        		/* need to use alternate mode to acquire credits */
        		Htc_GetCreditCounterUpdate(pCxt, A_NETBUF_GET_ELEM(pReq, A_REQ_EPID));
        		
        		if(0 == pEp->credits){/* test again in case no new credits were acquired */
        			if(pDCxt->rxBufferStatus == A_FALSE){
        				/* with no rx buffers to receive a credit report and no interrupt
        				 * credits a condition exists where the driver may become deadlocked.
        				 * Hence the creditDeadlock bool is set to prevent the driver from 
        				 * sleeping.  this will cause the driver to poll for credits until
        				 * the condition is passed.
        				 */
        				pDCxt->creditDeadlock = A_TRUE;
        				
        				if(pDCxt->creditDeadlockCounter >= MAX_CREDIT_DEADLOCK_ATTEMPTS){
        					/* This is a serious condition that can be brought about by 
        					 * a flood of RX packets which generate TX responses and do not 
        					 * return the RX buffer to the driver until the TX response 
        					 * completes. To mitigate this situation purge the tx queue
        					 * of any packets on data endpoints.
        					 */
        					Driver_DropTxDataPackets(pCxt);
        					pDCxt->creditDeadlockCounter = 0;
        				}        				        				
        			}
        			
	        		break;
	        	}
        	}
        	//else{
        	//	break;/* wait for credit report from device */
        	//}        	        		        	
        }        
        
        pDCxt->creditDeadlockCounter = 0;
       
        /* there must be enough write buffer space in the target fifo */
        if(pDCxt->hcd.WriteBufferSpace < DEV_CALC_SEND_PADDED_LEN(pDCxt, A_NETBUF_LEN(pReq)) + ATH_SPI_WRBUF_RSVD_BYTES){
            /* this will read the internal register to get the latest value for write buffer space */
            Hcd_RefreshWriteBufferSpace(pCxt);

            if(pDCxt->hcd.WriteBufferSpace < DEV_CALC_SEND_PADDED_LEN(pDCxt, A_NETBUF_LEN(pReq)) + ATH_SPI_WRBUF_RSVD_BYTES){
                /* there currently isn't enough space in the target fifo to accept this packet. 
                 * Hence setup the write buffer watermark interrupt for future notification and exit. */
                Hcd_ProgramWriteBufferWaterMark(pCxt, DEV_CALC_SEND_PADDED_LEN(pDCxt, A_NETBUF_LEN(pReq)));
                /* wait for interrupt to indicate space available */
                break;
            }
        }
        /* all conditions are met to transmit a packet */
        res = A_TRUE;
    }while(0);

    return res;
}

/*****************************************************************************/
/*  Driver_RxReady - Determines whether it is safe to start a RX operation. If 
 *     a RX operation can be started this function returns A_TRUE else A_FALSE.
 *      A_VOID *pCxt - the driver context. 
 *****************************************************************************/
A_BOOL Driver_RxReady(A_VOID *pCxt)
{
    A_BOOL res = A_FALSE;
    HTC_FRAME_HDR   *pHTCHdr;
    A_INT32 fullLength;
    A_UINT32 interrupts;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    
    do{
        /* there can be no errors */
        if(pDCxt->hcd.booleans & SDHD_BOOL_FATAL_ERROR){
            break;
        }
        /* there can be no in-complete requests */
        if( (pDCxt->hcd.booleans & SDHD_BOOL_DMA_IN_PROG) || 
            (pDCxt->hcd.booleans & SDHD_BOOL_DMA_COMPLETE)){
            break;
        }        
        /* the wifi device must have indicated that a packet is ready */
        if(0 == pDCxt->hcd.PendingIrqAcks){
            break;
        }      
        /* there must be rx buffers to process the request */
        if(A_FALSE == pDCxt->rxBufferStatus && NULL == pDCxt->pRxReq){
        	break;
        }   
        /* clear the pkt interrupt if its set. We do this here rather than in the 
         * interrupt handler because we will continue to process packets until we 
         * read a lookahead==0. only then will we unmask the interrupt */        
		if(A_OK != Hcd_DoPioInternalAccess(pCxt, ATH_SPI_INTR_CAUSE_REG, &interrupts, A_TRUE)){
			break;
		}
		
		if(interrupts & ATH_SPI_INTR_PKT_AVAIL){
			interrupts = ATH_SPI_INTR_PKT_AVAIL;
			
			if(A_OK != Hcd_DoPioInternalAccess(pCxt, ATH_SPI_INTR_CAUSE_REG, &interrupts, A_FALSE)){
				break;
			}
		}                       
        /* RX packet interrupt processing must be enabled */
        if(0 == (pDCxt->enabledSpiInts & ATH_SPI_INTR_PKT_AVAIL)){           
            break;
        }
        /* As a last step read the lookahead and the Receive buffer register in an 
         * effort to determine that a complete packet is ready for transfer. Do this
         * operation last as it does require several BUS transactions to complete. */        
        if(0 == pDCxt->lookAhead){
            /* need to refresh the lookahead */
            if(A_OK != Hcd_GetLookAhead(pCxt)){
                /* this is a major error */
                A_ASSERT(0);
            }
        }
        /* if lookahead is 0 then no need to proceed */
        if(0 == pDCxt->lookAhead){
        	if(0 != pDCxt->hcd.PendingIrqAcks){
	            /* this should always be true here */
	            pDCxt->hcd.PendingIrqAcks = 0;
	            Hcd_UnmaskSPIInterrupts(pCxt, ATH_SPI_INTR_PKT_AVAIL);
	        }	
	        
	        break;
        }                
        /* compare lookahead and recv buf ready value */
        pHTCHdr = (HTC_FRAME_HDR *)&(pDCxt->lookAhead);
        if(pDCxt->hcd.ReadBufferSpace < sizeof(HTC_FRAME_HDR) || 
            pDCxt->hcd.ReadBufferSpace < A_LE2CPU16(pHTCHdr->PayloadLen)){
            /* force a call to BUS_GetLookAhead on the next pass */
            pDCxt->lookAhead = 0;
            break;
        }
        /* there must be resources available to receive a packet */
        if(NULL == pDCxt->pRxReq){
            fullLength = (A_INT32)DEV_CALC_RECV_PADDED_LEN(pDCxt, A_LE2CPU16(pHTCHdr->PayloadLen) + sizeof(HTC_FRAME_HDR));
            
            if(pDCxt->rxBufferStatus){
            	/* try to get an RX buffer */
            	pDCxt->pRxReq = CUSTOM_DRIVER_GET_RX_REQ(pCxt, fullLength);
            }
            
            if(pDCxt->pRxReq == NULL){            	
               	break;                
            }   
            /* init the packet callback */
    		A_NETBUF_SET_ELEM(pDCxt->pRxReq, A_REQ_CALLBACK, Driver_RxComplete);    
                     
        }
        /* all conditions are met to receive a packet */
        res = A_TRUE;
    }while(0);

    return res;
}

A_VOID 
Driver_DropTxDataPackets(A_VOID *pCxt)
{
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	A_VOID *pReq;
    A_UINT16 i, count;
    
	if(pDCxt->txQueue.count){    
		count = pDCxt->txQueue.count;
		/* accesslock here to sync with threads calling 
		 * submit TX 
		 */   	
		for(i=0 ; i<count ; i++){	 
			TXQUEUE_ACCESS_ACQUIRE(pCxt);		
			{
	    		pReq = A_NETBUF_DEQUEUE(&(pDCxt->txQueue));		    			    		
			}
			TXQUEUE_ACCESS_RELEASE(pCxt);	
	        
	        if(pReq != NULL){        
	        	if(A_NETBUF_GET_ELEM(pReq, A_REQ_EPID) > ENDPOINT_1){
	        		Driver_CompleteRequest(pCxt, pReq);
	        	}else{  
	        		TXQUEUE_ACCESS_ACQUIRE(pCxt);	
			    	/* re-queue the packet as it is not one we can purge */
			    	A_NETBUF_ENQUEUE(&(pDCxt->txQueue), pReq);
			    	TXQUEUE_ACCESS_RELEASE(pCxt);
			    }    		    
	        }
	   	}
	}  
}
