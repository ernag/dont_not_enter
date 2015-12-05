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
#include <htc.h>
#include <atheros_wifi_api.h>
#include <atheros_wifi_internal.h>
#include <htc_services.h>
#include <spi_hcd_if.h>
#include "AR6002/hw2.0/hw/mbox_host_reg.h"
//#include "htc_internal.h"

A_VOID 
Htc_PrepareRecvPacket(A_VOID *pCxt, A_VOID *pReq)
{
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	HTC_FRAME_HDR *pHTCHdr;
	A_UINT16 validLength, transLength;
	
	pHTCHdr = (HTC_FRAME_HDR *)&(pDCxt->lookAhead);
    /* Credits are only used in TX so init to 0 here. */
    A_NETBUF_SET_ELEM(pReq, A_REQ_CREDITS, 0);
    /* get the endpoint ID from the lookahead */
    A_NETBUF_SET_ELEM(pReq, A_REQ_EPID, pHTCHdr->EndpointID);    
    /* calc the full recv length */
    validLength = A_LE2CPU16(pHTCHdr->PayloadLen) + sizeof(HTC_FRAME_HDR);
    transLength = DEV_CALC_RECV_PADDED_LEN(pDCxt, validLength);
    /* init the packet length = payloadLen + sizeof(HTC_FRAME_HDR) + <padding> */
    /* NOTE: For receive packets the request transfer length and actual buffer length are the same
     *  this may not be true for transmit as the driver does not have control of the transmit
     *  buffers.
     */
    A_NETBUF_PUT(pReq, transLength);    
    A_NETBUF_SET_ELEM(pReq, A_REQ_TRANSFER_LEN, transLength);
    /* init the packet lookahead value */
    A_NETBUF_SET_ELEM(pReq, A_REQ_LOOKAHEAD, pDCxt->lookAhead);
}

/*****************************************************************************/
/*  HTC_RxComplete - Completes processing of received packets whose 
 *   endpoint ID == ENDPOINT_0. This is the special HTC control endpoint.
 *      A_VOID *pCxt - the driver context. 
 *      A_VOID *pReq - the packet.
 *****************************************************************************/
A_VOID 
Htc_RxComplete(A_VOID *pCxt, A_VOID *pReq)
{   
	A_UNUSED_ARGUMENT(pCxt);         
        /* the only control messages we are expecting are NULL messages (credit resports) */   
    if (A_NETBUF_LEN(pReq) > 0) {            
         A_ASSERT(0);
    }

    A_NETBUF_FREE(pReq);
}

A_UINT32 
Htc_ReadCreditCounter(A_VOID *pCxt, A_UINT32 index)
{
	A_NETBUF_DECLARE req;
	A_VOID *pReq = (A_VOID*)&req;
	A_UINT32 collector;
	
	A_NETBUF_CONFIGURE(pReq, &collector, 0, sizeof(A_UINT32), sizeof(A_UINT32));	
	/* read the target credit counter */
	ATH_SET_PIO_EXTERNAL_READ_OPERATION(pReq, COUNT_ADDRESS+4*index, A_TRUE, sizeof(A_UINT32));
    
    if(A_OK != Hcd_DoPioExternalAccess(pCxt, pReq)){
		A_ASSERT(0);
	}				

	collector = A_LE2CPU32(collector);
	
	return collector;
}

A_VOID
Htc_GetCreditCounterUpdate(A_VOID *pCxt, A_UINT16 epId)
{
	A_ENDPOINT_T    *pEp;
	A_UINT32 collector;	
	A_UINT16 reg_index, array_index;
	A_UINT8 credits;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
	pEp = Util_GetEndpoint(pCxt, epId);
	
	if(pDCxt->htc_creditInit==0){
		/* never got interrupt from chip to indicate that this 
		 * credit scheme was supported */
		 pEp->intCreditCounter = 0;
		 return;
	}
	
	reg_index = pEp->epIdx>>2;
	array_index = pEp->epIdx & 0x03;	
	/* read the target credit counter */
	collector = Htc_ReadCreditCounter(pCxt, reg_index);
	
	credits = (collector>>(8*array_index)) & 0xff;
	pEp->intCreditCounter = credits;
	credits = pEp->intCreditCounter - pEp->rptCreditCounter;

	 /* The below condition occurs when we stop recieving in-band credits and are purely relying */
	 /* on out-of-band credits. In such cases it is possible that out-of-band credit can wrap around and this prevents the same */
        if(credits < pEp->intCredits)
	{
           return;  
	}	
	
	credits = credits - pEp->intCredits; //deduct any previous int credits
	pEp->intCredits += credits; //track num credits acquired this way.
	/* add the new credits to the EP credit count */
	pEp->credits += credits;	
}

static A_VOID 
Htc_DistributeCredits(A_VOID *pCxt)
{
	A_UINT32 numCredits;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
	numCredits = (A_UINT16)pDCxt->creditCount;
	pDCxt->ep[0].maxCredits = pDCxt->ep[0].credits = 1;
	pDCxt->ep[0].rptCreditCounter = 0;
	pDCxt->ep[0].intCreditCounter = 0;
	pDCxt->ep[0].intCredits = 0;
	numCredits--;
	pDCxt->ep[1].maxCredits = pDCxt->ep[1].credits = 1;
	pDCxt->ep[1].rptCreditCounter = 0;
	pDCxt->ep[1].intCreditCounter = 0;
	pDCxt->ep[1].intCredits = 0;
	numCredits--;
	pDCxt->ep[2].maxCredits = pDCxt->ep[2].credits = (A_UINT8)numCredits;
	pDCxt->ep[2].rptCreditCounter = 0;
	pDCxt->ep[2].intCreditCounter = 0;
	pDCxt->ep[2].intCredits = 0;
}

/* process credit reports and call distribution function */
static A_VOID 
Htc_ProcessCreditRpt(A_VOID *pCxt, HTC_CREDIT_REPORT *pRpt, A_INT32 NumEntries, HTC_ENDPOINT_ID FromEndpoint)
{
    int             i;
    A_ENDPOINT_T    *pEndpoint;
    A_UINT8	    credits;
	
	A_UNUSED_ARGUMENT(FromEndpoint);
   

    for (i = 0; i < NumEntries; i++) {
        if (pRpt[i].EndpointID >= ENDPOINT_MAX) {
            A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT            
            break;      
#endif          
        }

		pEndpoint = Util_GetEndpoint(pCxt, pRpt[i].EndpointID);
		/* this variable will and should wrap around. */
		pEndpoint->rptCreditCounter += pRpt[i].Credits;
		
		credits = pRpt[i].Credits;
		
		if(pEndpoint->intCredits){
			if(pEndpoint->intCredits <= pRpt[i].Credits){
				credits = pRpt[i].Credits - pEndpoint->intCredits;
				pEndpoint->intCredits = 0;
			}else{
				credits = 0;
				pEndpoint->intCredits -= pRpt[i].Credits;
			}
		}
		
		pEndpoint->credits += credits;
			
    }         
}

A_VOID 
HTC_ProcessCpuInterrupt(A_VOID *pCxt) 
{
	A_NETBUF_DECLARE req;
	A_VOID *pReq = (A_VOID*)&req;
	A_UINT32 collector;
        A_UINT8* ptr = (A_UINT8*)&collector;

	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);        
        
	/* if this is the first such interrupt then acquire the 
	 * address of the HTC comm memory. */
	A_NETBUF_CONFIGURE(pReq, &collector, 0, sizeof(A_UINT32), sizeof(A_UINT32));
	
	if(pDCxt->htc_creditInit == 0){	
		pDCxt->htc_creditInit = 1;
		ATH_SET_PIO_EXTERNAL_READ_OPERATION(pReq, COUNT_ADDRESS+4, A_TRUE, sizeof(A_UINT32));
				
		if(A_OK != Hcd_DoPioExternalAccess(pCxt, pReq)){
			A_ASSERT(0);
		}
		
		/* the last byte of the COUNT_ADDRESS contain the creditCount */
		pDCxt->creditCount = ptr[3];
	    /* distribute the credits among the endpoints */
	    Htc_DistributeCredits(pCxt);
	}
}

/*****************************************************************************/
/*  Htc_ProcessTrailer - Trailers can be appended to the end of a received
 *   packet. This function processes those trailers prior to their removal
 *   from the packet. 
 *      A_VOID *pCxt - the driver context. 
 *      A_UINT8 *pBuffer - the buffer holding the trailers.
 *      A_INT32 Length - the length of the pBuffer in bytes.
 *      A_UINT32   *pNextLookAheads - storage for any lookaheads found in 
 *                  the trailer.
 *      A_INT32    *pNumLookAheads - storage for a count of valid lookaheads
 *                  stored in pNextLookAheads.
 *      HTC_ENDPOINT_ID FromEndpoint - the endpoint ID of the received packet.
 *****************************************************************************/
static A_STATUS 
Htc_ProcessTrailer(	 A_VOID *pCxt,
	                 A_UINT8    *pBuffer,
	                 A_INT32    Length,
	                 A_UINT32   *pNextLookAheads,
	                 A_UINT32    *pNumLookAheads,
	                 HTC_ENDPOINT_ID FromEndpoint)
{
    HTC_RECORD_HDR          *pRecord;
    A_UINT8                 *pRecordBuf;
    HTC_LOOKAHEAD_REPORT    *pLookAhead;
    A_STATUS                status;  


    status = A_OK;
    
    while (Length > 0) {

        if (Length < sizeof(HTC_RECORD_HDR)) {
            status = A_EPROTO;
            break;
        }
            /* these are byte aligned structs */
        pRecord = (HTC_RECORD_HDR *)pBuffer;
        Length -= sizeof(HTC_RECORD_HDR);
        pBuffer += sizeof(HTC_RECORD_HDR);

        if (pRecord->Length > Length) {
                /* no room left in buffer for record */            
            status = A_EPROTO;
            break;
        }
            /* start of record follows the header */
        pRecordBuf = pBuffer;

        switch (pRecord->RecordID) {
            case HTC_RECORD_CREDITS: /* the device is indicating that new TX credits are available */                
                Htc_ProcessCreditRpt(pCxt,
                                    (HTC_CREDIT_REPORT *)pRecordBuf,
                                    (A_INT32)(pRecord->Length / (sizeof(HTC_CREDIT_REPORT))),
                                    FromEndpoint);
                break;
            case HTC_RECORD_LOOKAHEAD: /* the device is providing the lookahead for the next packet */
                A_ASSERT(pRecord->Length >= sizeof(HTC_LOOKAHEAD_REPORT));
                pLookAhead = (HTC_LOOKAHEAD_REPORT *)pRecordBuf;
                if ((pLookAhead->PreValid == ((~pLookAhead->PostValid) & 0xFF)) &&
                    (pNextLookAheads != NULL)) {                    

                        /* look ahead bytes are valid, copy them over */
                    ((A_UINT8 *)(&pNextLookAheads[0]))[0] = pLookAhead->LookAhead[0];
                    ((A_UINT8 *)(&pNextLookAheads[0]))[1] = pLookAhead->LookAhead[1];
                    ((A_UINT8 *)(&pNextLookAheads[0]))[2] = pLookAhead->LookAhead[2];
                    ((A_UINT8 *)(&pNextLookAheads[0]))[3] = pLookAhead->LookAhead[3];
                    
                        /* just one normal lookahead */
                    *pNumLookAheads = 1;
                }
                break;
            case HTC_RECORD_LOOKAHEAD_BUNDLE:
#if 0 /* this feature is not currently supported by this driver */
                AR_DEBUG_ASSERT(pRecord->Length >= sizeof(HTC_BUNDLED_LOOKAHEAD_REPORT));
                if (pRecord->Length >= sizeof(HTC_BUNDLED_LOOKAHEAD_REPORT) &&
                    (pNextLookAheads != NULL)) {                   
                    HTC_BUNDLED_LOOKAHEAD_REPORT    *pBundledLookAheadRpt;
                    A_INT32                             i;
                    
                    pBundledLookAheadRpt = (HTC_BUNDLED_LOOKAHEAD_REPORT *)pRecordBuf;                                        
                    
                    if ((pRecord->Length / (sizeof(HTC_BUNDLED_LOOKAHEAD_REPORT))) >
                            HTC_HOST_MAX_MSG_PER_BUNDLE) {
                            /* this should never happen, the target restricts the number
                             * of messages per bundle configured by the host */        
                        A_ASSERT(FALSE);
                        status = A_EPROTO;
                        break;        
                    }
                                         
                    for (i = 0; i < (int)(pRecord->Length / (sizeof(HTC_BUNDLED_LOOKAHEAD_REPORT))); i++) {
                        ((A_UINT8 *)(&pNextLookAheads[i]))[0] = pBundledLookAheadRpt->LookAhead[0];
                        ((A_UINT8 *)(&pNextLookAheads[i]))[1] = pBundledLookAheadRpt->LookAhead[1];
                        ((A_UINT8 *)(&pNextLookAheads[i]))[2] = pBundledLookAheadRpt->LookAhead[2];
                        ((A_UINT8 *)(&pNextLookAheads[i]))[3] = pBundledLookAheadRpt->LookAhead[3];
                        pBundledLookAheadRpt++;
                    }
                    
                    *pNumLookAheads = i;
                } 
                break;              
#else
                A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT                
                break;
#endif                
#endif
                
            default:                
                break;
        }

        if (A_OK != status) {
            break;
        }

            /* advance buffer past this record for next time around */
        pBuffer += pRecord->Length;
        Length -= pRecord->Length;
    }

    if (A_OK != status) {
        A_ASSERT(0);
    }

    return status;
}

A_STATUS
Htc_SendPacket(A_VOID *pCxt, A_VOID *pReq)
{
	A_STATUS status = A_ERROR;
	A_ENDPOINT_T *pEp;
    A_UINT8 htcFlags = 0;
    A_UINT8 SeqNo = 0;    
    A_UINT8 *pHdrBuf;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt); 
    
	do{
		/* adjust the Endpoint credit count */
	    if(NULL == (pEp = Util_GetEndpoint(pCxt, A_NETBUF_GET_ELEM(pReq, A_REQ_EPID)))){
	        break;
	    }
		if(pDCxt->htcStart == A_TRUE)
		{
			if(0 == pEp->credits){
	        	break;
		    }
		    
		    A_ASSERT(A_NETBUF_GET_ELEM(pReq, A_REQ_CREDITS));
		    pEp->credits -= A_NETBUF_GET_ELEM(pReq, A_REQ_CREDITS);		    		    		    
		    SeqNo = pEp->seqNo;
		    pEp->seqNo++;        

		    if(0 == pEp->credits){
		        htcFlags |= HTC_FLAGS_NEED_CREDIT_UPDATE;
	    	}
		}
	    
	    /* fill in elements of the HTC header. treat the header as an un-aligned struct for safety */
	    pHdrBuf = A_NETBUF_DATA(pReq);
	    A_SET_UINT16_FIELD(pHdrBuf, HTC_FRAME_HDR, PayloadLen, (A_UINT16)A_NETBUF_LEN(pReq) - HTC_HDR_LENGTH);  
	    A_SET_UINT8_FIELD(pHdrBuf, HTC_FRAME_HDR,Flags,(htcFlags));                         
	    A_SET_UINT8_FIELD(pHdrBuf, HTC_FRAME_HDR,EndpointID, (A_UINT8)A_NETBUF_GET_ELEM(pReq, A_REQ_EPID)); 
	    A_SET_UINT8_FIELD(pHdrBuf, HTC_FRAME_HDR,ControlBytes[0], (A_UINT8)(0));   
	    A_SET_UINT8_FIELD(pHdrBuf, HTC_FRAME_HDR,ControlBytes[1], (A_UINT8)(SeqNo));
    
    	status = A_OK;
    }while(0);
    
    return status;   
}

A_STATUS 
Htc_ProcessTxComplete(A_VOID *pCxt, A_VOID *pReq)
{
	A_UNUSED_ARGUMENT(pCxt);
	/* remove the HTC header from the front of the packet. */
    return A_NETBUF_PULL(pReq, HTC_HDR_LENGTH);
}


/*****************************************************************************/
/*  HTC_ProcessRecvHeader - Processes HTC headers and removes them from the 
 *   front of the packet.
 *      A_VOID *pCxt - the driver context. 
 *      A_VOID *pReq - the packet.
 *      A_UINT32   *pNextLookAheads - storage for any lookaheads found in
 *                  the trailer. 
 *      A_INT32    *pNumLookAheads - storage for a count of valid lookaheads
 *                  stored in pNextLookAheads. 
 *****************************************************************************/
A_STATUS 
Htc_ProcessRecvHeader(A_VOID *pCxt, A_VOID *pReq,
                                     A_UINT32   *pNextLookAheads, 
                                     A_UINT32        *pNumLookAheads)
{
    A_UINT8   temp;
    A_UINT8   *pBuf;
    A_STATUS  status = A_OK;
    A_UINT16  payloadLen, bufLength;
    A_UINT32  lookAhead;
    
    if (pNumLookAheads != NULL) {
        *pNumLookAheads = 0;
    }    

    do {
        bufLength = A_NETBUF_LEN(pReq);
        /* gets the first buffer which will/must contain the HTC header */
        if(NULL == (pBuf = A_NETBUF_DATA(pReq)) || bufLength < HTC_HDR_LENGTH){
            /* this is a critical error */
            A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT            
            status = A_EPROTO;
            break;
#endif            
        }
        /* note, we cannot assume the alignment of pBuffer, so we use the safe macros to
         * retrieve 16 bit fields */
        payloadLen = (A_UINT16)A_GET_UINT16_FIELD(pBuf, HTC_FRAME_HDR, PayloadLen);
        
        ((A_UINT8 *)&lookAhead)[0] = pBuf[0];
        ((A_UINT8 *)&lookAhead)[1] = pBuf[1];
        ((A_UINT8 *)&lookAhead)[2] = pBuf[2];
        ((A_UINT8 *)&lookAhead)[3] = pBuf[3];        
              
        if(lookAhead != A_NETBUF_GET_ELEM(pReq, A_REQ_LOOKAHEAD)){
            /* this is a critical error */
            A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT            
            status = A_EPROTO;
            break;
#endif            
        }        
			/* now that the payloadLen and the HTC header has been confirmed 
			 * remove any padding length that may have been added to the buffer
			 */
		if(payloadLen + HTC_HDR_LENGTH < A_NETBUF_LEN(pReq)){
			A_NETBUF_TRIM(pReq, A_NETBUF_LEN(pReq) - (payloadLen + HTC_HDR_LENGTH));
		}

            /* get flags */
        temp = A_GET_UINT8_FIELD(pBuf, HTC_FRAME_HDR, Flags);

        if (temp & HTC_FLAGS_RECV_TRAILER) {
            /* this packet has a trailer */

                /* extract the trailer length in control byte 0 */
            temp = A_GET_UINT8_FIELD(pBuf, HTC_FRAME_HDR, ControlBytes[0]);

            if ((temp < sizeof(HTC_RECORD_HDR)) || (temp > payloadLen)) {               
                /* this is a critical error */
                A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT                
                status = A_EPROTO;
                break;
#endif                
            }                
                /* process trailer data that follows HDR + application payload */
            if(A_OK != (status = Htc_ProcessTrailer(pCxt,
                                       (pBuf + HTC_HDR_LENGTH + payloadLen - temp),
                                       temp,
                                       pNextLookAheads,
                                       pNumLookAheads,
                                       (HTC_ENDPOINT_ID)A_NETBUF_GET_ELEM(pReq, A_REQ_EPID)))){
                break;
            }  
                /* remove the trailer bytess via buffer trim */
            A_NETBUF_TRIM(pReq, temp);            
        }
            /* if we get to this point, the packet is good */
            /* remove header and adjust length */
        A_NETBUF_PULL(pReq, HTC_HDR_LENGTH);

    } while (0);

    if (A_OK != status) {
    	A_ASSERT(0);            
    } 

    return status;
}
   
   
A_STATUS 
HTC_WaitTarget(A_VOID *pCxt)
{    
    A_STATUS                 status;   
    //HTC_READY_EX_MSG        *pRdyMsg;
    HTC_SERVICE_CONNECT_REQ  connect;
    HTC_SERVICE_CONNECT_RESP resp; 
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);       

    do {
		if(ath_custom_htc.HTCGetReady != NULL){
			if(A_OK != (status = ath_custom_htc.HTCGetReady(pCxt))){
				break;
			}
		}else{
			pDCxt->creditCount = AR4100_DEFAULT_CREDIT_COUNT;
	        pDCxt->creditSize = AR4100_BUFFER_SIZE;        
		}          
            /* setup our pseudo HTC control endpoint connection */
        A_MEMZERO(&connect,sizeof(connect));
        A_MEMZERO(&resp,sizeof(resp));
                
        connect.ServiceID = HTC_CTRL_RSVD_SVC;

            /* connect fake service */
        status = HTC_ConnectService(pCxt,
                                   &connect,
                                   &resp);

        if (!A_FAILED(status)) {
            break;
        }
    } while (FALSE);    

    return status;
}


A_STATUS 
HTC_ConnectService(A_VOID *pCxt,
                   HTC_SERVICE_CONNECT_REQ  *pConnectReq,
                   HTC_SERVICE_CONNECT_RESP *pConnectResp)
{
    A_STATUS 		status = A_OK;    
    HTC_ENDPOINT_ID	assignedEndpoint = ENDPOINT_MAX;
    A_ENDPOINT_T	*pEndpoint;
    A_UINT32		maxMsgSize = 0;    

    do {
        
        if (HTC_CTRL_RSVD_SVC == pConnectReq->ServiceID) {
                /* special case for pseudo control service */
            assignedEndpoint = ENDPOINT_0;
            maxMsgSize = HTC_MAX_CONTROL_MESSAGE_LENGTH;
        } else {
        
        	if(ath_custom_htc.HTCConnectServiceExch != NULL){
        		status = ath_custom_htc.HTCConnectServiceExch(pCxt, pConnectReq, pConnectResp,
        						&assignedEndpoint, &maxMsgSize);
        		
        		if(status != A_OK){        		
        			break;
        		}
        	}else{
        		pConnectResp->ConnectRespCode = HTC_SERVICE_SUCCESS;
			
				switch(pConnectReq->ServiceID)
				{
				case WMI_CONTROL_SVC:
					assignedEndpoint = ENDPOINT_1;
					maxMsgSize = 1542;
					break;
				case WMI_DATA_BE_SVC:
					assignedEndpoint = ENDPOINT_2;
					maxMsgSize = 4144;
					break;
				case WMI_DATA_BK_SVC:
					assignedEndpoint = ENDPOINT_3;
					maxMsgSize = 4144;
					break;
				case WMI_DATA_VI_SVC:
					assignedEndpoint = ENDPOINT_4;
					maxMsgSize = 4144;
					break;
				case WMI_DATA_VO_SVC:
					assignedEndpoint = ENDPOINT_5;
					maxMsgSize = 4144;
					break;
				}					
        	}

        }

            /* the rest of these are parameter checks so set the error status */
        status = A_EPROTO;       

        if (0 == maxMsgSize) {
            A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT            
            break;
#endif            
        }

		pEndpoint = Util_GetEndpoint(pCxt, assignedEndpoint);		
		/* this marks the endpoint in use */
		pEndpoint->serviceID = pConnectReq->ServiceID;     
            /* return assigned endpoint to caller */
        pConnectResp->Endpoint = assignedEndpoint;
        pConnectResp->MaxMsgLength = maxMsgSize;
        
        pEndpoint->maxMsgLength = (A_INT32)maxMsgSize;        
                        
        status = A_OK;

    } while (0);    

    return status;
}

/* Start HTC, enable interrupts and let the target know host has finished setup */
A_STATUS 
HTC_Start(A_VOID *pCxt)
{    
    A_STATUS   status = A_OK;

    do {
    	Htc_DistributeCredits(pCxt);
		
	
		if(ath_custom_htc.HTCSendSetupComplete != NULL){
			/* the caller is done connecting to services, so we can indicate to the
            * target that the setup phase is complete */
	        status = ath_custom_htc.HTCSendSetupComplete(pCxt);

	        if (A_FAILED(status)) {
	            break;
	        }
		}
		
		/* unmask the packet interrupt on-chip */
		Hcd_UnmaskSPIInterrupts(pCxt, ATH_SPI_INTR_PKT_AVAIL);	 
		GET_DRIVER_COMMON(pCxt)->htcStart = A_TRUE;        
    } while (0);
    
    return status;
}


/* synchronously wait for a message from the target,
 */
#if 0
A_STATUS 
Htc_WaitforMessage(HTC_HANDLE HTCHandle, HTC_PACKET **ppControlPacket, A_UINT8 EndpointID)
{
    A_STATUS        status;
    A_UINT32        lookAhead;
    HTC_PACKET      *pPacket = NULL;
    HTC_FRAME_HDR   *pHdr;
	HTC_TARGET      *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);

    do  {       

            /* call the polling function to see if we have a message */
        status = DevPollMboxMsgRecv(&target->Device,
                                    &lookAhead,
                                    HTC_TARGET_RESPONSE_TIMEOUT);

        if (A_FAILED(status)) {
            break;
        }

            /* check the lookahead */
        pHdr = (HTC_FRAME_HDR *)&lookAhead;

        if (pHdr->EndpointID != EndpointID) {
                /* unexpected endpoint number, should be zero */
            status = A_EPROTO;
            break;
        }

        if (A_FAILED(status)) {
                /* bad message */
            status = A_EPROTO;
            break;
        }

		if(*ppControlPacket != NULL){
        	pPacket = *ppControlPacket;
        }else{
        	pPacket = HTC_ALLOC_CONTROL_RX(target);

	        if (pPacket == NULL) {
	            status = A_NO_MEMORY;
	            break;
	        }
        }
        
        pPacket->PktInfo.AsRx.HTCRxFlags = 0;
        pPacket->PktInfo.AsRx.ExpectedHdr = lookAhead;
        pPacket->ActualLength = A_LE2CPU16(pHdr->PayloadLen) + HTC_HDR_LENGTH;

        if (pPacket->ActualLength > pPacket->BufferLength) {
            status = A_EPROTO;
            break;
        }

            /* we want synchronous operation */
        pPacket->Completion = NULL;

            /* get the message from the device, this will block */
        status = HTCIssueRecv(target, pPacket);

        if (A_FAILED(status)) {
            break;
        }

            /* process receive header */
        status = HTCProcessRecvHeader(target,pPacket,NULL,NULL);

        pPacket->Status = status;

        if (A_FAILED(status)) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                    ("HTCWaitforMessage, HTCProcessRecvHeader failed (status = %d) \n",
                     status));
            break;
        }

            /* give the caller this control message packet, they are responsible to free */
        *ppControlPacket = pPacket;

    } while (FALSE);

    if (A_FAILED(status)) {
        if (pPacket != NULL && *ppControlPacket == NULL) {
                /* cleanup buffer on error */
            HTC_FREE_CONTROL_RX(target,pPacket);
        }
    }

    return status;
}

#endif
