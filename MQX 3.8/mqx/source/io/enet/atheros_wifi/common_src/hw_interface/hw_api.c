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

A_VOID 
HW_EnableDisableSPIIRQ(A_VOID *pCxt, A_BOOL Enable, A_BOOL FromIrq)
{       
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
    if (A_FALSE == FromIrq) { 
        IRQEN_ACCESS_ACQUIRE(pCxt);
        CUSTOM_OS_INT_DISABLE((GET_DRIVER_CXT(pCxt)->sr_save));
    }
    
    do { 
        if (Enable) { 
            if (A_FALSE == pDCxt->hcd.IrqsEnabled) {
                pDCxt->hcd.IrqsEnabled = A_TRUE;  
                CUSTOM_HW_ENABLEDISABLESPIIRQ(pCxt, Enable);                              
            }
        } else {  
            if (A_TRUE == pDCxt->hcd.IrqsEnabled) {
                pDCxt->hcd.IrqsEnabled = A_FALSE;                
                CUSTOM_HW_ENABLEDISABLESPIIRQ(pCxt, Enable);
            }
        }     
    } while (0);
     
    if (A_FALSE == FromIrq) { 
    	CUSTOM_OS_INT_ENABLE(GET_DRIVER_CXT(pCxt)->sr_save);
        IRQEN_ACCESS_RELEASE(pCxt);
    }    
}



A_VOID 
HW_PowerUpDown(A_VOID *pCxt, A_UINT8 powerUp)
{
#if WLAN_CONFIG_ENABLE_CHIP_PWD_GPIO  		
	/* NOTE: when powering down it is necessary that the SPI MOSI on the MCU
	 * be configured high so that current does not leak from the AR6003.
	 * Hence because the SPI interface retains its last state we write a single
	 * 0xf out the SPI bus prior to powering the chip down.
	 */
	if(powerUp == 0){
		Bus_InOutToken( pCxt, (A_UINT32) 0xff,1,NULL); 
	}
	
    CUSTOM_HW_POWER_UP_DOWN(pCxt, powerUp);	
	
	if(powerUp){
		//delay to give chip time to come alive
		CUSTOM_HW_USEC_DELAY(pCxt, 2000);
	}
#endif
}

/*****************************************************************************/
/* BUS_InOut_DescriptorSet - Attempts to read OR write a set of buffers
 *  on the BUS (SPI or other).  The effort is required to occur synchronously 
 *  if the sync boolean is true.  Otherwise it may optionally occur 
 *  asynchronously. If the underlying bus cannot satisfy a synchronous
 *  request then the custom code must by written in such a way that 
 *  any asychronous activity is handled under the covers.
 *      A_VOID * pCxt - the driver context.
 *      A_TRANSPORT_OBJ *pObj - The object containing the buffers.
 *      A_BOOL sync - indication of synchronous or asynchronous completion.
 *****************************************************************************/
A_STATUS 
Bus_InOutDescriptorSet(A_VOID *pCxt, A_VOID *pReq)									
{	
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    A_STATUS status;
    A_UINT16 i, totalLength; 
    A_BOOL sync = (A_NETBUF_GET_ELEM(pReq, A_REQ_CALLBACK) != NULL)? A_FALSE : A_TRUE;
	A_UINT8 *pBuf;
	A_INT32 length;
	A_UINT8 doRead;
	
	doRead = (A_NETBUF_GET_ELEM(pReq, A_REQ_COMMAND) & ATH_TRANS_READ)? 1:0;
	
    do{
        /* Custom_Bus_Start_Transfer - allows the custom solution
         * to perform any pre transfer operations such as 
         * changing the SPI chip select if necessary. */
        if(A_OK != (status = CUSTOM_BUS_START_TRANSFER(pCxt, sync))){
            break;
        }
        
        i=0;
        totalLength = 0;

        while( NULL != (pBuf = A_NETBUF_GET_FRAGMENT(pReq, i, &length))){
	        if(length){
                /* Submit each buffer with a length to the underlying custom
                 * bus solution. whether the buffers actually transfer in 
                 * the context of this function call is up to the solution.
                 */
		        if(A_OK != (status = CUSTOM_BUS_INOUT_BUFFER(pCxt,
                                    pBuf,
							        length, 									
							        doRead, sync))){    			
        	        break;
		        }
	        }

            totalLength += length;
            i++;
        }	
        /* account for any padding bytes that were not part of the request length */
        if(totalLength < A_NETBUF_GET_ELEM(pReq, A_REQ_TRANSFER_LEN)){
            A_ASSERT(A_NETBUF_GET_ELEM(pReq, A_REQ_TRANSFER_LEN) - totalLength <= pDCxt->blockSize);

            status = CUSTOM_BUS_INOUT_BUFFER(pCxt,
                                    (A_UINT8*)pDCxt->padBuffer,
							        A_NETBUF_GET_ELEM(pReq, A_REQ_TRANSFER_LEN) - totalLength, 									
							        doRead, sync);  			
        }

        /* Custom_Bus_Complete_Transfer - allows the custom solution
         * to perform any post transfer operations such as 
         * changing the SPI chip select if necessary. */
        if(A_OK == status){
            status = CUSTOM_BUS_COMPLETE_TRANSFER(pCxt, sync);
        }
    }while(0);
    return status;
}


/*****************************************************************************/
/* BUS_InOut_Token - Attempts to read AND write 4 or less bytes on the SPI bus.
 *  The effort is expected to occur synchronously so that when the function
 *  call returns the operation will have completed.  If the underlying 
 *  bus cannot satisfy this requirement then Custom_InOut_Token must be
 *  written in such a way that any asychronous activity is handled under
 *  the covers.
 *      A_VOID * pCxt - the driver context.
 *      A_UINT32 OutToken - 4 or less bytes of write data.
 *      A_UINT8 dataSize - length of data.
 *      A_UINT32 *pInToken - pointer to memory to store 4 or less bytes of
 *          read data.
 *****************************************************************************/
A_STATUS 
Bus_InOutToken(A_VOID *pCxt, A_UINT32 OutToken, A_UINT8 DataSize, A_UINT32 *pInToken) 
{          
    A_UINT32 dummy;    
    A_STATUS status;
    
    if(pInToken == NULL) {
        pInToken = &dummy;        
    }
    
    DataSize += 1;// data size if really a enum that is 1 less than number of bytes    
#if A_ENDIAN == A_BIG_ENDIAN   
    //because OutToken is passed in as a U32 the 
    //valid bytes will be the last bytes as this
    //is a big-endian cpu.  so we need to shift 
    //the valid bytes over to eliminate the invalid bytes.
    OutToken<<=(32-(DataSize<<3));
#endif    
        status = CUSTOM_BUS_INOUT_TOKEN(pCxt, OutToken, DataSize, pInToken);
        
        if(pInToken != &dummy){            
#if A_ENDIAN == A_BIG_ENDIAN
			//reading less than 4 bytes requires that the bytes
			//be shifted by the number of invalid bytes.
			*pInToken>>=(32-(DataSize<<3));
#endif              
        }            
     	
    return status;                                                                        
}

/*****************************************************************************/
/* BUS_TransferComplete - Callback for the Custom Bus solution to indicate 
 *  when an asynchronous operation has completed. This function is intended
 *  to be called from an interrupt level context if required by the custom
 *  solution.
 *      A_VOID *pCxt - the driver context.
 *      A_STATUS status - A_OK if transfer completed successfully. 
 *****************************************************************************/
A_VOID 
Bus_TransferComplete(A_VOID *pCxt, A_STATUS status)
{
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	A_UNUSED_ARGUMENT(status);
    if(0 == (pDCxt->hcd.booleans & SDHD_BOOL_DMA_IN_PROG)){
        /* error condition */
        return;
    }

    pDCxt->hcd.booleans |= SDHD_BOOL_DMA_COMPLETE;
    pDCxt->hcd.booleans &= ~SDHD_BOOL_DMA_IN_PROG;    
    /* QueueWork will kick start the driver thread if needed to perform any
     * any deferred work. */
    CUSTOM_DRIVER_WAKE_DRIVER(pCxt);
}

A_VOID 
HW_InterruptHandler(A_VOID *pCxt)
{	
	if(pCxt){
		GET_DRIVER_COMMON(pCxt)->hcd.IrqDetected = A_TRUE;
		    
	    /* disable IRQ while we process it */    
	    HW_EnableDisableSPIIRQ(pCxt, A_FALSE, A_TRUE);
	    /* startup work item to process IRQs */
	    CUSTOM_DRIVER_WAKE_DRIVER(pCxt);  
    }        
}