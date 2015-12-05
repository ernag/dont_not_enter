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
#include <aggr_recv_api.h>
#include <wmi_api.h>
#include <atheros_wifi_api.h>
#include <atheros_wifi_internal.h>
#include <atheros_stack_offload.h>

/*****************************************************************************/
/*  query_credit_deficit - Executed by the driver task to satisfy the 
 *	 Api_TxGetStatus() request.  It will query the wifi credit counters on 
 *	 the Wifi device as needed to get the most up-to-date tx credit status.	 
 *      A_VOID *pCxt - the driver context. 
 *****************************************************************************/
static A_STATUS query_credit_deficit(A_VOID *pCxt)
{
	A_UINT32 reg;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
	do{
		if((pDCxt->hcd.booleans & 
		   (SDHD_BOOL_FATAL_ERROR|SDHD_BOOL_DMA_IN_PROG|SDHD_BOOL_DMA_WRITE_WAIT_FOR_BUFFER)))
		{
	   		break;
		}
        
        reg = Htc_ReadCreditCounter(pCxt, 1);        		
		pDCxt->txQueryResult = (reg & 0x80000000)? 1:0;			
	}while(0);
	
	pDCxt->asynchRequest = NULL;
	pDCxt->txQueryInProgress = A_FALSE;
	CUSTOM_DRIVER_WAKE_USER(pCxt);
	return A_OK;
}

/*****************************************************************************/
/*  Api_TxGetStatus - Learns the status regarding any TX requests.  it will 
 *	 test the state of the driver first to learn if any TX requests are 
 *	 remaining on the host side.  if none then it will test the state of 
 *	 the HTC credits to learn if any TX requests are still being processed
 *	 by the WIFI device.  If none then the function will return the idle 
 *	 status.
 *      A_VOID *pCxt - the driver context. 
 *****************************************************************************/
A_UINT16 
Api_TxGetStatus(A_VOID* pCxt)
{
	A_UINT16 status = ATH_TX_STATUS_IDLE;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
		
	do{
	
		if(0 != A_NETBUF_QUEUE_SIZE(&(pDCxt->txQueue)) || 
			pDCxt->driver_state == DRIVER_STATE_TX_PROCESSING){
			status = ATH_TX_STATUS_HOST_PENDING;
			break;
		}
		/* Check the AR410X status */							
		status = ATH_TX_STATUS_WIFI_PENDING;
		/* set up driver operation to read credits from device */
		if(pDCxt->asynchRequest == NULL){
			pDCxt->txQueryInProgress = A_TRUE;
			pDCxt->asynchRequest = query_credit_deficit;
			CUSTOM_DRIVER_WAKE_DRIVER(pCxt);
			CUSTOM_DRIVER_WAIT_FOR_CONDITION(pCxt, &(pDCxt->txQueryInProgress), A_FALSE, 5000); 
			
			if(pDCxt->txQueryInProgress==A_FALSE){
					
				if(pDCxt->txQueryResult==0){
					status = ATH_TX_STATUS_IDLE;	
				}
			}
		}						
	}while(0);
	
	return status;
}

/*****************************************************************************/
/*  API_TxComplete - Brings a transmit packet to its final conclusion. The
 *   packet is either a data packet in which case it will be freed back to 
 *   the caller, or it is a wmi command in which case it will freed. 
 *      A_VOID *pCxt - the driver context. 
 *      A_VOID *pReq - the packet.
 *****************************************************************************/
A_VOID 
Api_TxComplete(A_VOID *pCxt, A_VOID *pReq)
{
    /* some user tasks may have blocked until a transmit complete occurs
     * these lines wake up these tasks. */
    GET_DRIVER_COMMON(pCxt)->tx_complete_pend = A_FALSE;
    CUSTOM_DRIVER_WAKE_USER(pCxt);

	if(ath_custom_init.Api_TxComplete != NULL){
		ath_custom_init.Api_TxComplete(pCxt,pReq);					
		return;
	} else {    
    	CUSTOM_API_TXCOMPLETE(pCxt, pReq);
    	A_NETBUF_FREE(pReq);
	}
}

/*****************************************************************************/
/*  API_RxComplete - Brings a received packet to its final conclusion. The
 *   packet is either a data packet in which case it will be processed and
 *   sent to the network layer, or it is a wmi event in which case it will
 *   be delivered to the wmi layer.
 *      A_VOID *pCxt - the driver context. 
 *      A_VOID *pReq - the packet.
 *****************************************************************************/
A_VOID 
Api_RxComplete(A_VOID *pCxt, A_VOID *pReq)
{    
	A_STATUS status;
    A_UINT16 minHdrLen;
    A_UINT16 packetLen;
    A_UINT8 containsDot11Hdr = 0;  
    WMI_DATA_HDR *dhdr;
    ATH_MAC_HDR *mac_hdr;
    A_UINT8 is_amsdu, is_acl_data_frame;
    A_UINT8 meta_type;      
    HTC_ENDPOINT_ID   ept = (HTC_ENDPOINT_ID)A_NETBUF_GET_ELEM(pReq, A_REQ_EPID);
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
        /* take lock to protect buffer counts
         * and adaptive power throughput state */        

	if(ath_custom_init.Api_RxComplete != NULL){
		ath_custom_init.Api_RxComplete(pCxt,pReq);					
		return;
	} else {  
    if (pDCxt->wmiEnabled == A_TRUE) {
        if (ept == ENDPOINT_1) {
            /*
             * this is a wmi control packet.
             */
            wmi_control_rx(pDCxt->pWmiCxt, pReq);
        }else if(pDCxt->promiscuous_mode){
    		wmi_data_hdr_remove(pDCxt->pWmiCxt, pReq);
    		CUSTOM_DELIVER_FRAME(pCxt, pReq);
            status = A_OK;
    	} else {                	
            /*
             * this is a data packet.
             */
            dhdr = (WMI_DATA_HDR *)A_NETBUF_DATA(pReq);                           
            packetLen = A_NETBUF_LEN(pReq);
            dhdr->info2 = A_LE2CPU16(dhdr->info2);
            is_acl_data_frame = (A_UINT8)(WMI_DATA_HDR_GET_DATA_TYPE(dhdr) == WMI_DATA_HDR_DATA_TYPE_ACL);
           
            minHdrLen = MIN_HDR_LEN;

                
            /* In the case of AP mode we may receive NULL data frames
             * that do not have LLC hdr. They are 16 bytes in size.
             * Allow these frames in the AP mode.
             * ACL data frames don't follow ethernet frame bounds for
             * min length
             */
            do{
	            if (pDCxt->networkType != AP_NETWORK &&
	                (packetLen < minHdrLen) ||
	                (packetLen > AR4100_MAX_RX_MESSAGE_SIZE))
	            {
	                /*
	                 * packet is too short or too long
	                 */                                
	                status = A_ERROR;
	                break;
	            } else {                                                      
	                is_amsdu = (A_UINT8)WMI_DATA_HDR_IS_AMSDU(dhdr);
	                meta_type = (A_UINT8)WMI_DATA_HDR_GET_META(dhdr);
	                containsDot11Hdr = (A_UINT8)WMI_DATA_HDR_GET_DOT11(dhdr);					
					/* NOTE: rssi is not currently managed on a per peer basis.
					 * 	therefore, if multiple peers are sending packets to 
					 *	this radio the rssi will be averaged across all 
					 * 	indiscriminately.
					 */
					/* simple averaging algorithm.  7/8 of original value is combined
					 * with 1/8 of new value unless original value is 0 in which case 
					 * just adopt the new value as its probably the first in the 
					 * connection. 
					 */                
	                pDCxt->rssi = (A_INT8)((pDCxt->rssi)? (pDCxt->rssi*7 + dhdr->rssi)>>3 : dhdr->rssi);									
	                wmi_data_hdr_remove(pDCxt->pWmiCxt, pReq);

	                if (meta_type) {
	                	/* this code does not accept per frame
	                	 * meta data */
	                    A_ASSERT(0);
	                }
	                /* NWF: print the 802.11 hdr bytes */
	                if(containsDot11Hdr) {
	                	/* this code does not accept dot11 headers */
	                    A_ASSERT(0);
	                } else if(!is_amsdu && !is_acl_data_frame) {
	                    status = WMI_DOT3_2_DIX(pReq);

	                }else{
	                	/* this code does not accept amsdu or acl data */
	                	A_ASSERT(0);
	                }

	                if (status != A_OK) {
	                    /* Drop frames that could not be processed (lack of memory, etc.) */
	                    break;
	                }                    
	                    
#if WLAN_CONFIG_11N_AGGR_SUPPORT 
#endif /* WLAN_CONFIG_11N_AGGR_SUPPORT */					
	                CUSTOM_DELIVER_FRAME(pCxt, pReq);
	                status = A_OK;
	            }	            	            
            }while(0);
            
            if(A_OK != status){
            	A_NETBUF_FREE(pReq);
            }
        }
    } else {
        A_NETBUF_FREE(pReq);		
    }

    return;
	}
}

/*****************************************************************************/
/*  API_WmiTxStart - Function used by WMI via A_WMI_CONTROL_TX() to send 
 *   a WMI command.
 *      A_VOID *pCxt - the driver context. 
 *      A_VOID *pReq - the packet.
 *****************************************************************************/
A_STATUS
Api_WmiTxStart(A_VOID *pCxt, A_VOID *pReq, HTC_ENDPOINT_ID eid)
{
    A_STATUS         status = A_ERROR;    
    
    do {
        A_NETBUF_SET_ELEM(pReq, A_REQ_EPID, eid);        
               
        if(A_OK != Driver_SubmitTxRequest(pCxt, pReq)){        	    
            break;
        }                                       
	    
	    status = A_OK;  	    
	} while (0);

    return status;   
}

/*****************************************************************************/
/*  Api_DataTxStart - Starts the process of transmitting a data packet (pReq).
 *	 Various checks are performed to ensure the driver and device are ready
 *	 to open the data path.  A dot.3 header is pre-pended if necessary and a
 *	 WMI_DATA_HDR is pre-pended. In a multi-threaded system this function is
 * 	 expected to be executed on a non-driver thread.
 *      A_VOID *pCxt - the driver context.    
 *		A_VOID *pReq - the Transmit packet.
 *****************************************************************************/
A_STATUS 
Api_DataTxStart(A_VOID *pCxt, A_VOID *pReq)
{
#define AC_NOT_MAPPED   99
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    A_UINT8            ac = AC_NOT_MAPPED;
    HTC_ENDPOINT_ID    eid = ENDPOINT_UNUSED;    
    A_BOOL            bMoreData = A_FALSE;
    A_UINT8           dot11Hdr = WLAN_CONFIG_TX_DOT11_HDR;   
    A_STATUS    status = A_ERROR;
    WMI_TX_META_V5    ip_meta;
   
    do{
        /* If target is not associated */
        if( (A_FALSE == pDCxt->isConnected && A_FALSE == pDCxt->isConnectPending)) {
            break;
        }else if(A_FALSE == pDCxt->isConnected) {
    	    //block until connection completes.    
            CUSTOM_DRIVER_WAIT_FOR_CONDITION(pCxt, &(pDCxt->isConnected), A_TRUE, 5000);    	
                
		    if(A_FALSE == pDCxt->isConnected){			    
        	    break;
		    }
        }
        
        if(A_OK != Api_DriverAccessCheck(pCxt, 1, ACCESS_REQUEST_TX)){
            break;
        }

        if (pDCxt->wmiReady == A_FALSE) {
            break;
        }
       
        if (A_FALSE == pDCxt->wmiEnabled) {
            break;
        }              
        
        /* add wmi_data_hdr to front of frame */   
        if (wmi_data_hdr_add(pDCxt->pWmiCxt, pReq, DATA_MSGTYPE, bMoreData, 
        	(dot11Hdr)?WMI_DATA_HDR_DATA_TYPE_802_11:WMI_DATA_HDR_DATA_TYPE_802_3,
        	WMI_META_VERSION_5, (void*)&ip_meta) != A_OK) {                
            break;
        }          
                /* get the stream mapping */
        ac = wmi_implicit_create_pstream(pDCxt->pWmiCxt, pReq, 0, pDCxt->wmmEnabled);    
        eid = Util_AC2EndpointID (pCxt, ac);           

        if (eid && eid != ENDPOINT_UNUSED ){
            A_NETBUF_SET_ELEM(pReq, A_REQ_EPID, eid);        
			/* Submit for transmission to next layer */
            if(A_OK != Driver_SubmitTxRequest(pCxt, pReq)){        	    
                break;
            }                                 
        }else{    	    
            break;
        } 

        status = A_OK;
    }while(0);       

    return status;
}
