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
#include <hif_internal.h>
#include "spi_hcd_if.h"
#include <targaddrs.h>
#include "AR6002/hw2.0/hw/mbox_host_reg.h"
 

#define CMD_ADDR_READ        (1 << 15)    
#define CMD_ADDRESS_INTERNAL (1 << 14)

#define ADJUST_WRBUF_SPACE(p,b) \
{                               \
    (p)->hcd.WriteBufferSpace = (p)->hcd.WriteBufferSpace - (b) - ATH_SPI_WRBUF_RSVD_BYTES; \
} 

/* macro to issue command/address phase */                    
#define OUT_TOKEN_CMD_ADDR_DMA_WRITE(pdev,addr) \
        Bus_InOutToken((pdev),(A_UINT32)A_CPU2BE16((addr)),ATH_TRANS_DS_16,NULL)

#define OUT_TOKEN_CMD_ADDR_DMA_READ(pdev,addr)  \
        OUT_TOKEN_CMD_ADDR_DMA_WRITE(pdev,((addr) | CMD_ADDR_READ))
                    
#define OUT_TOKEN_CMD_ADDR_INTERNAL_READ(pdev,addr) \
        OUT_TOKEN_CMD_ADDR_DMA_READ(pdev, ((addr) | CMD_ADDRESS_INTERNAL))
        
#define OUT_TOKEN_CMD_ADDR_INTERNAL_WRITE(pdev,addr) \
        OUT_TOKEN_CMD_ADDR_DMA_WRITE(pdev, ((addr) | CMD_ADDRESS_INTERNAL))             


/*****************************************************************************/
/*  DoPioReadInternal - Used to write a device internal register.
 *		A_VOID *pCxt - the driver context.
 *		A_UINT16 Addr - address of the register.
 *		A_UINT32 value - value to write to the register.
 *****************************************************************************/
static A_STATUS 
DoPioWriteInternal(A_VOID *pCxt, 
                                      A_UINT16        Addr,
                                      A_UINT32        Value) 
{   
    A_STATUS status;
    
    Addr |= CMD_ADDRESS_INTERNAL;  
          
        /* issue CMD/ADDR token */                                                 
    if(A_OK != (status = OUT_TOKEN_CMD_ADDR_INTERNAL_WRITE(pCxt,Addr))){
        return status;
    }    
    /* despite the UINT32 there are only 2 bytes of data for internal 
     * register.  These bytes must be set in Big Endian format for 
     * proper parsing by the chip. */
    Value = A_CPU2BE16(Value);          
        /* send out data */
    return Bus_InOutToken(pCxt, Value, ATH_TRANS_DS_16, NULL);
}   

/*****************************************************************************/
/*  DoPioReadInternal - Used to read a device internal register.
 *		A_VOID *pCxt - the driver context.
 *		A_UINT16 Addr - address of the register.
 *		A_UINT32 *pValue - pointer to the location to where the value should
 *		 be stored.
 *****************************************************************************/
static A_STATUS 
DoPioReadInternal(A_VOID *pCxt, 
                              A_UINT16        Addr, 
                              A_UINT32       *pValue) 
{   
    A_STATUS status;  
    
    do {               
        Addr |= CMD_ADDRESS_INTERNAL;  
                   
        if(A_OK != (status = OUT_TOKEN_CMD_ADDR_INTERNAL_READ(pCxt, Addr))){
            break;
        }        
              
        status = Bus_InOutToken(pCxt, 0x00000000, ATH_TRANS_DS_16, pValue);
        /* despite the UINT32 storage there are only 2 bytes of data from the 
         * internal register.  These bytes arrive in Big Endian format so  
	     * must be converted to CPU format for proper consumption. */
        *pValue = A_BE2CPU16(*pValue);
    } while (FALSE);
    
    return status;                                   
}   


/*****************************************************************************/
/*  EnableDisableSPIIRQHwDetect - Used to enable/disable SPI interrupt detection
 * 	 Ultimately this function calls HW_EnableDisableSPIIRQ to enable/disable the IRQ.
 *		A_VOID *pCxt - the driver context.
 *		A_BOOL Enable - if A_TRUE then enable SPI Interrupt if A_FALSE then 
 *		 disable SPI Interrupt.
 *****************************************************************************/
static A_VOID 
EnableDisableSPIIRQHwDetect(A_VOID *pCxt, A_BOOL Enable)
{    
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);

    if ((pDCxt->hcd.SpiHWCapabilitiesFlags & HW_SPI_INT_EDGE_DETECT) && Enable) {
        /* the SPI interrupt is edge triggered and has requirements for a good edge 
         * when we enable interrupt detection we disable all interrupts on the module and
           then re-enable them again to generate a good edge */
        
        if (A_OK != DoPioWriteInternal(pCxt, ATH_SPI_INTR_ENABLE_REG, 0)) {
            A_ASSERT(0);
        }    
            /* re-enable SPI interrupt detection at the host controller */ 
        HW_EnableDisableSPIIRQ(pCxt, A_TRUE, A_FALSE);   
        
            /* re-enable interrupt sources, if there are pending interrupts, this should
             * generate a nice clean edge */
        if (A_OK != DoPioWriteInternal(pCxt, ATH_SPI_INTR_ENABLE_REG, pDCxt->hcd.SpiIntEnableShadow)) {
            A_ASSERT(0);
        }     
        
    } else {
            /* the SPI interrupt logic is level triggered (or we are simply disabling) so we can simply tell
             * the hardware layer to enable/disable it */
        HW_EnableDisableSPIIRQ(pCxt, Enable, A_FALSE); 
    }
}

/*****************************************************************************/
/*  MaskSPIInterrupts - Used to mask specific device interrupts
 *	 function will write the SPI Interrupt Enable register with the resulting
 *   mask value.
 *		A_VOID *pCxt - the driver context.
 *		A_UINT16 Mask - bit mask indicating which interrupts should be 
 *		 masked.
 *****************************************************************************/
static A_STATUS 
MaskSPIInterrupts(A_VOID *pCxt, A_UINT16 Mask)
{
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    pDCxt->hcd.SpiIntEnableShadow &= ~Mask;    
            
    if(Mask & pDCxt->hcd.SpiIntEnableShadow){
    	Hcd_UnmaskSPIInterrupts(pCxt, Mask);
    }

    return DoPioWriteInternal(pCxt, ATH_SPI_INTR_ENABLE_REG, (A_UINT32)pDCxt->hcd.SpiIntEnableShadow);
}


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  HcdTransferData - transfer data state machine
  Input:  pDevice - device object
          pReq    - transfer request
  Output: 
  Return: 
  Notes: This state machine handles the data phases of a DMA request or an external I/O access through
         the read and write ports.
         This state machine handles early termination of bytes and transfers bytes manually if
         the remaining bytes is small.
        
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
//FIXME remove this function later
static A_STATUS 
Hcd_TransferData(A_VOID *pCxt, A_VOID *pReq)
{    
	return Bus_InOutDescriptorSet(pCxt, pReq);  
}

/*****************************************************************************/
/*  Hcd_UnmaskSPIInterrupts - Used to un-mask specific device interrupts
 *	 function will write the SPI Interrupt Enable register with the resulting
 *   mask value.
 *		A_VOID *pCxt - the driver context.
 *		A_UINT16 Mask - bit mask indicating which interrupts should be 
 *		 unmasked.
 *****************************************************************************/
A_STATUS 
Hcd_UnmaskSPIInterrupts(A_VOID *pCxt, A_UINT16 Mask)
{
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    pDCxt->hcd.SpiIntEnableShadow |= Mask;    
 
    if((Mask & pDCxt->hcd.SpiIntEnableShadow) == 0){
    	MaskSPIInterrupts(pCxt, Mask);
    }
    
    return DoPioWriteInternal(pCxt, ATH_SPI_INTR_ENABLE_REG, (A_UINT32)pDCxt->hcd.SpiIntEnableShadow);
}

/*****************************************************************************/
/*  HandleExternalReadDone - Utlity to complete an External IO read operation.
 *		A_VOID *pCxt - the driver context.
 *		A_VOID *pReq - the transfer request object.
 *****************************************************************************/
static A_STATUS 
HandleExternalReadDone(A_VOID *pCxt, A_VOID *pReq)
{
    A_STATUS status;    
        /* send out cmd/addr token */    
    if(A_OK != (status = OUT_TOKEN_CMD_ADDR_INTERNAL_READ(pCxt, ATH_SPI_HOST_CTRL_RD_PORT_REG))){
        return status;
    }
        /* do the data frames to get the data */
    status = Hcd_TransferData(pCxt, pReq);
        /* this should never return pending, external I/O accesses do not invoke the DMA hardware */
    A_ASSERT(status != A_PENDING);
           
    return status;
}

#if 0
/*****************************************************************************/
/*  ReadSomeInternalReg - Utility function to read some device registers as 
 *   a Workaround for some chipsets.
 *		A_VOID *pCxt - the driver context.
 *****************************************************************************/
static A_VOID 
ReadSomeInternalReg(A_VOID *pCxt)
{
	A_UINT32 reg;
	
	do{			
        if(A_OK != DoPioReadInternal(pCxt, ATH_SPI_WRBUF_SPC_AVA_REG, &reg)){	
			break;
		}
				
        if(A_OK != DoPioReadInternal(pCxt, ATH_SPI_RDBUF_BYTE_AVA_REG, &reg)){	
			break;
		}

        if(A_OK != DoPioReadInternal(pCxt, ATH_SPI_CONFIG_REG, &reg)){	
			break;
		}
	}while(0);
}
#endif

A_UINT32
Hcd_GetMboxAddress(A_VOID *pCxt, A_UINT16 mbox, A_UINT32 length)
{
	A_UINT32 address;
	
	if(mbox == 0){
		address = GET_DRIVER_COMMON(pCxt)->mboxAddress + HIF_MBOX_WIDTH - length;
	}else{
		A_ASSERT(0);
	}
	
	return address;
}

A_STATUS
Hcd_DoPioInternalAccess(A_VOID *pCxt, A_UINT16 addr, A_UINT32 *pValue, A_BOOL isRead)
{
	A_STATUS status;
	
	if(isRead == A_TRUE){
		status = DoPioReadInternal(pCxt, addr, pValue);
	}else{
		status = DoPioWriteInternal(pCxt, addr, *pValue);
	}
	
	return status;
}

/*****************************************************************************/
/*  Hcd_DoPioExternalAccess - accessing external registers requires the use 
 *   of the SPI slave controller's internal proxy state machine
 *   in the event that the chip is asleep.  The internal proxy performs the operation 
 *   and will signal us via an interrupt or polling operation
 *		A_VOID *pCxt - the driver context.
 *		A_VOID *pReq - the transfer request object.
 *****************************************************************************/
A_STATUS 
Hcd_DoPioExternalAccess(A_VOID *pCxt, A_VOID *pReq)
{
    A_STATUS status;
    A_UINT32 regValue;
    int retry;
    
    do {                                                       
            /* set incrementing or fixed addressing */
        regValue = (A_UINT32)(ATH_IS_TRANS_EXT_ADDR_FIXED(pReq) ? ATH_SPI_HOST_CTRL_NO_ADDR_INC : 0);
            /* set the length */
        regValue |= A_NETBUF_LEN(pReq);        
            /* write control reg */
        if(A_OK != (status = DoPioWriteInternal(pCxt, ATH_SPI_HOST_CTRL_BYTE_SIZE_REG, regValue))){       
            break;
        }                                
        
        if (ATH_IS_TRANS_WRITE(pReq)) {            
                /* write to the data port  */
                /* send out cmd/addr token */    
            if(A_OK != (status = OUT_TOKEN_CMD_ADDR_INTERNAL_WRITE(pCxt, ATH_SPI_HOST_CTRL_WR_PORT_REG))){            
                break;
            }    
                /* do the data frames */
            status = Hcd_TransferData(pCxt, pReq);
                /* these frames should go out without using interrupts */
            A_ASSERT(status != A_PENDING);
            if (A_OK != status) {            
                break;                                   
            } 
        }
        
            /* enable, set direction and set address range to do the operation on */
        regValue = ATH_SPI_HOST_CTRL_CONFIG_ENABLE;
        regValue |= ATH_IS_TRANS_READ(pReq) ? 0 : ATH_SPI_HOST_CTRL_CONFIG_DIR_WRITE;
        regValue |= A_NETBUF_GET_ELEM(pReq, A_REQ_ADDRESS);//ATH_GET_IO_ADDRESS(pReq);                 
            /* write config to start the operation */
        if(A_OK != (status = DoPioWriteInternal(pCxt, ATH_SPI_HOST_CTRL_CONFIG_REG, regValue))){
            break;
        }          
            /* poll the host_access_done bit in SPI_STATUS register, if the access takes longer
             * than the retry count, the core was probably asleep, we need to wait for an interrupt
             * for the operation to complete */
        //retry = EXTERNAL_ACCESS_DONE_RETRY_COUNT;
        retry = 200;
                 
        while (retry) {            
            if(A_OK != (status = DoPioReadInternal(pCxt, ATH_SPI_STATUS_REG, &regValue))){           
                break;
            }
        
            if (regValue & ATH_SPI_STATUS_HOST_ACCESS_DONE) {            
                break;    
            }
            
            retry--; 
            if(retry){            	
            	CUSTOM_HW_USEC_DELAY(pCxt, 1000);
            }  
        }

        if (0 == retry) {        
			status = A_ERROR;
            break;
        } 
        
        /* if we get here the chip was awake and the access finished within the polling interval */
        
        if (ATH_IS_TRANS_READ(pReq)) {
            /* for reads, empty the read port */
            status = HandleExternalReadDone(pCxt, pReq);
        }       
        
            /* clear the interrupt cause, each host access operation will set the RD or WR
             * cause bit */
        if(A_OK != (status = DoPioWriteInternal(pCxt,
                                    ATH_SPI_INTR_CAUSE_REG,
                                    ATH_SPI_INTR_HOST_CTRL_RD_DONE | ATH_SPI_INTR_HOST_CTRL_WR_DONE))){
            break;
        }                                                           
    } while (FALSE);
    
    return status;
}

/*****************************************************************************/
/*  ResetWriteBufferWaterMark - Used to reset the write buffer watermark on
 *   the device so as to prevent any further watermark interrupts from the
 *   device. 
 *		A_VOID *pCxt - the driver context.
 *****************************************************************************/
static A_STATUS 
ResetWriteBufferWaterMark(A_VOID *pCxt)
{
    A_STATUS status;
 
    if(A_OK != (status = DoPioWriteInternal(pCxt, ATH_SPI_WRBUF_WATERMARK_REG, 0))){
        return status;
    }
       /* the watermark interrupt status constantly latches, so after we
        * set to zero, we need to clear the cause register */
    return DoPioWriteInternal(pCxt,
                              ATH_SPI_INTR_CAUSE_REG,
                              ATH_SPI_INTR_WRBUF_BELOW_WMARK);            
}

/*****************************************************************************/
/*  ConfigureByteSwap - Configures the byte swapping if any to be applied
 *	 to any subsequent SPI transactions. This will write the SPI Config 
 *   register as necessary to configure the device for proper byte swapping.
 * 		A_VOID *pCxt - the driver context.
 *    	A_UINT8 TransferType - indicates the unit size for SPI transfer.
 *****************************************************************************/
static A_STATUS 
ConfigureByteSwap(A_VOID *pCxt, A_UINT8 TransferType)
{
    A_UINT16 swapSettings;
    
    swapSettings = GET_DRIVER_COMMON(pCxt)->hcd.SpiConfigShadow;
    
    /* based on the transfer type, figure out what mode settings we need */
                     
    if (TransferType == ATH_TRANS_DS_32) {
            /* make sure swap is turned on */
        swapSettings |= ATH_SPI_CONFIG_BYTE_SWAP;
            /* turn off 16 bit mode - which actually turns ON 32 bit swapping */
        swapSettings &= ~ATH_SPI_CONFIG_SWAP_16BIT; 
    } else if (TransferType == ATH_TRANS_DS_16) {
            /* make sure swap is turned on */
        swapSettings |= ATH_SPI_CONFIG_BYTE_SWAP;
            /* turn on 16 bit swapping mode */
        swapSettings |= ATH_SPI_CONFIG_SWAP_16BIT;   
    } else if (TransferType == ATH_TRANS_DS_8) {
            /* disable byte swapping entirely */
        swapSettings &= ~ATH_SPI_CONFIG_BYTE_SWAP;
        swapSettings &= ~ATH_SPI_CONFIG_SWAP_16BIT;    
    } else {
        A_ASSERT(0);    
    }       
    
        /* did any bits change? */
    if (swapSettings ^ GET_DRIVER_COMMON(pCxt)->hcd.SpiConfigShadow) {       
            /* save new value */
        GET_DRIVER_COMMON(pCxt)->hcd.SpiConfigShadow = swapSettings;
            /* write it out */
        return DoPioWriteInternal(pCxt, ATH_SPI_CONFIG_REG, swapSettings);   
    }
    
    return A_OK;
}

/*****************************************************************************/
/*  DoDMAOp - Utility function to initiate a DMA or other SPI transaction.
 *   Starts by writing the SPI DMA Size Register as required by the device.
 * 		A_VOID *pCxt - the driver context.
 *    	A_VOID *pReq - The request packet object.
 *****************************************************************************/
static A_STATUS 
DoDMAOp(A_VOID *pCxt, A_VOID *pReq)
{
    A_STATUS status;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    A_UINT16 mask = (A_UINT16)(pDCxt->hcd.SpiIntEnableShadow & ATH_SPI_INTR_PKT_AVAIL);   
    /* WAR during a data transfer/mbox operation it is necessary
     * to mask the interrupt from the wifi device as that has been shown
     * to corrupt SPI communication. */
	if(mask){       
		MaskSPIInterrupts(pCxt, mask);	
	}	                
    
    do {
            /* setup DMA size register */
        if(A_OK != (status = DoPioWriteInternal(pCxt, 
                                    ATH_SPI_DMA_SIZE_REG,
                                    A_NETBUF_GET_ELEM(pReq, A_REQ_TRANSFER_LEN)))){
            break;
        }        
        
        if (ATH_IS_TRANS_READ(pReq)) {           
                /* send out the read DMA token */
            status = OUT_TOKEN_CMD_ADDR_DMA_READ(pCxt, A_NETBUF_GET_ELEM(pReq, A_REQ_ADDRESS));
        } else {
                /* send out the write DMA token */
            status = OUT_TOKEN_CMD_ADDR_DMA_WRITE(pCxt, A_NETBUF_GET_ELEM(pReq, A_REQ_ADDRESS));
        }
        
        if (A_OK != status) {                     
           break;                                 
        }                                                
            /* call transfer state machine to handle data frames */                 
        status = Hcd_TransferData(pCxt, pReq);  
                
        if (A_PENDING == status) {            
                /* transfer routine requires DMA to start */    
            pDCxt->hcd.booleans |= SDHD_BOOL_DMA_IN_PROG;              
                /* we are transfering control to the SPI hardware's DMA engine, while it is
                 * running we cannot process SPI interrupts.  SPI interrupt processing requires
                 * CMD/Data frames on the SPI bus and requires ownership of the SPI hardware.
                 * this function is called with the driver lock held to hold off the SPI interrupt
                 * handler HcdSpiInterrupt() , the handler should wake up and check for a pending
                 * DMA hardware transfer and exit.  On completion of the DMA transfer, the SPI
                 * interrupt detection will be enabled */
                 /* disable SPI interrupt detection at the hardware layer */
            EnableDisableSPIIRQHwDetect(pCxt, A_FALSE);
            //HW_StartDMA(pCxt);         
        }
        
    } while (FALSE);
    
    if(mask){
        if(status == A_PENDING){
            pDCxt->hcd.irqMask = mask;
        }else{
		    Hcd_UnmaskSPIInterrupts(pCxt, mask);	
        }
	}
    
    return status;    
}


/*****************************************************************************/
/*  Hcd_ProgramWriteBufferWaterMark - Programs the device's write buffer 
 *   watermark level to trigger an interrupt when device FiFo space becomes 
 *   available.
 * 		A_VOID *pCxt - the driver context.
 *    	A_UINT32 length - the desired byte length to program watermark.
 *****************************************************************************/
A_STATUS 
Hcd_ProgramWriteBufferWaterMark(A_VOID *pCxt, A_UINT32 length)
{
    A_UINT32      waterMarkLevel,regValue;
    A_STATUS status = A_OK;
    A_BOOL        waitInterrupt = A_TRUE;
    A_UINT32      param;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);

    do {                                          
        /* calculate the water mark based on the number of bytes currently in the buffer 
         * and the number of bytes we need to fullfill the request, this is the level 
         * the buffer needs to drop below to trigger an interrupt */              
        waterMarkLevel = pDCxt->hcd.MaxWriteBufferSpace - length;
    
         
        if (waterMarkLevel < pDCxt->hcd.MaxWriteBufferSpace) {
                /* also... watermark level cannot be zero */
            waterMarkLevel = waterMarkLevel ? waterMarkLevel : 1;
        } else {
            A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT            
            waterMarkLevel = 1; 
#endif            
        }
          
            /* update watermark trigger */                  
        if(A_OK != (status = DoPioWriteInternal(pCxt,
                                    ATH_SPI_WRBUF_WATERMARK_REG,
                                    waterMarkLevel))){
            break;
        }                
            /* re-sample SPI_STATUS and space available, the operation to update the watermark could
             * miss the window when the core is still awake, if this happens the
             * watermark level will be reached but it will not generate an interrupt
             * as INTR_CAUSE is only updated while the core clock is active, so any updates to
             * WR_BUF_WATERMARK_REG are only valid while the core is awake */  
                               
        if(A_OK != (status = DoPioReadInternal(pCxt, ATH_SPI_STATUS_REG, &regValue))){
            break;
        }
                      
        if ((regValue & ATH_SPI_STATUS_RTC_STATE_MASK) == ATH_SPI_STATUS_RTC_STATE_SLEEP) {
            
                /* it can take up to 60 microseconds for the sleep state to change */
            CUSTOM_HW_USEC_DELAY(pCxt,60);
            
                /* re-read status and re-check the sleep state */
            if(A_OK != (status = DoPioReadInternal(pCxt, ATH_SPI_STATUS_REG, &regValue))){
                break;
            }
        
            if ((regValue & ATH_SPI_STATUS_RTC_STATE_MASK) != ATH_SPI_STATUS_RTC_STATE_SLEEP) {
                    /* core is awake or is just about to wakeup.  We can wait for an interrupt */
                break;    
            }
          
                /* re-read space available, the core went to sleep and we may never get an interrupt */            
            if(A_OK != (status = DoPioReadInternal(pCxt, ATH_SPI_WRBUF_SPC_AVA_REG, &param))){
                break;
            }

            pDCxt->hcd.WriteBufferSpace = param;                          
                /* no need to wait, we have buffer space to send this packet */
            waitInterrupt = FALSE;
            
                /* the core drained the write buffer after we sampled 
                 * SPC_AVAIL, we now have room and the core went to sleep.
                 * reset watermark since we won't be using it */
            if(A_OK != (status = ResetWriteBufferWaterMark(pCxt))){
                break;
            }        
            /* fall through and return SUCCESS */                                    
        } 
    
    } while (FALSE);
    
    if (A_OK == status && waitInterrupt) {        
            /*  no space, need to wait for interrupt  */
        pDCxt->hcd.booleans |= SDHD_BOOL_DMA_WRITE_WAIT_FOR_BUFFER;        
            /* wait for the buffer to empty below the water mark */
        Hcd_UnmaskSPIInterrupts(pCxt, ATH_SPI_INTR_WRBUF_BELOW_WMARK);                   
        status = A_PENDING;
    }
    
    return status;
}

/*****************************************************************************/
/*  Hcd_RefreshWriteBufferSpace - Reads the device Write buffer space available
 *   register to learn the available byte length that the device can 
 *   currently accept for transmit operations.
 * 		A_VOID *pCxt - the driver context.
 *****************************************************************************/
A_VOID 
Hcd_RefreshWriteBufferSpace(A_VOID *pCxt)
{
	A_UINT32 param;
	if(A_OK == DoPioReadInternal(pCxt, ATH_SPI_WRBUF_SPC_AVA_REG, &param)){
		GET_DRIVER_COMMON(pCxt)->hcd.WriteBufferSpace = param;
    }   
}

/*****************************************************************************/
/*  Hcd_Request - Used to process a bus request. This could be a Tx or Rx 
 *   request.
 * 		A_VOID *pCxt - the driver context.
 * 		A_VOID *pReq - the packet object
 *****************************************************************************/
A_STATUS 
Hcd_Request(A_VOID *pCxt, A_VOID *pReq) 
{
    A_STATUS status = A_OK;    
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    A_UINT32 transLength = 0;
    
    do {   
        
        if (pDCxt->hcd.booleans & SDHD_BOOL_FATAL_ERROR) {
            status = A_HARDWARE;
            break;
        }
        
        if (pDCxt->hcd.booleans & SDHD_BOOL_SHUTDOWN) {            
            status = A_ECANCELED;
            break;
        }          
            /* check if we need to change byte swap logic
             * TODO : normally DMADataWidth does not change, however for testing
             * purposes we allow the width to change dynamically through a config request */
        if(A_OK != (status = ConfigureByteSwap(pCxt, pDCxt->hcd.DMADataWidth))){
            break;
        }
      
        if (ATH_IS_TRANS_READ(pReq)) {                	 
            status = DoDMAOp(pCxt, pReq); 
        } else {      
            transLength = A_NETBUF_GET_ELEM(pReq, A_REQ_TRANSFER_LEN);

                /* for writes we keep track of the write buffer space */
            ADJUST_WRBUF_SPACE(pDCxt, transLength);	             
                /* fire off the write operation */       
            status = DoDMAOp(pCxt, pReq);                   
        }
    } while (FALSE);           
     
    return status;
} 

A_VOID
Hcd_ClearCPUInterrupt(A_VOID *pCxt)
{
	A_NETBUF_DECLARE req;
	A_VOID *pReq = (A_VOID*)&req;
	A_UINT8 data;
	
	A_NETBUF_CONFIGURE(pReq, &data, 0, sizeof(A_UINT8), sizeof(A_UINT8));
	
	do{       			
		data = 0xff;
		
		ATH_SET_PIO_EXTERNAL_WRITE_OPERATION(pReq, CPU_INT_STATUS_ADDRESS, A_TRUE, sizeof(A_UINT8));
		
		if(A_OK != Hcd_DoPioExternalAccess(pCxt, pReq)){
			break;
		}
	}while(0);    
}


A_VOID 
Hcd_EnableCPUInterrupt(A_VOID *pCxt)
{
	A_NETBUF_DECLARE req;
	A_VOID *pReq = (A_VOID*)&req;
	A_UINT8 data;
	
	A_NETBUF_CONFIGURE(pReq, &data, 0, sizeof(A_UINT8), sizeof(A_UINT8));
	
	do{       	
       	data = 0xff;
		
		ATH_SET_PIO_EXTERNAL_WRITE_OPERATION(pReq, CPU_INT_STATUS_ENABLE_ADDRESS, A_TRUE, sizeof(A_UINT8));
		
		if(A_OK != Hcd_DoPioExternalAccess(pCxt, pReq)){
			break;
		}			
	}while(0); 			
}


/*****************************************************************************/
/*  HcdSpiInterrupt - Used to process an Interrupt from the device. It reads
 * 	 the SPI_INTR_CAUSE register to learn the reason(s) for the interrupt.
 *   Error causes are handled inside this function and other reasons are 
 *	 stored and used to notify other driver layers.
 * 		A_VOID *pCxt - the driver context.    
 *****************************************************************************/
A_BOOL 
Hcd_SpiInterrupt(A_VOID *pCxt)
{
    A_STATUS status;
    A_BOOL   notifyFunctionIRQs = A_FALSE;
    A_UINT32 interrupts;
    A_UINT32 hostIrqs;    
    A_UINT32 param;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
        /* check for pending DMA HW transfers */
    if (pDCxt->hcd.booleans & SDHD_BOOL_DMA_IN_PROG) {
         /* The DMA engine owns the hardware. We may not have been able to stop this task/thread
         * from running, so we need to abort processing interrupts immediately.
         * The SPI interrupts would have been disabled at this point so we can pick up the pending
         * interrupts later when the DMA completion routine turns interrupts back on */          
        /* exit immediately, we do not want to turn the interrupts back on */        
        return A_TRUE;
    }
        
    do {
        /* read the Interrupt Cause register to determine the reason(s) for the interrupt */
        if(A_OK != (status = DoPioReadInternal(pCxt, ATH_SPI_INTR_CAUSE_REG, &interrupts))){
            break;
        }         
        
        interrupts &= pDCxt->hcd.SpiIntEnableShadow;                      
            /* get the HCD-owned IRQs */
        hostIrqs = interrupts & (ATH_SPI_HCD_IRQ_SOURCES | ATH_SPI_INTR_HOST_CTRL_ACCESS_DONE);
        
            /* before processing HCD-owned IRQ sources we need to Ack them, some processing 
             * may cause additional bus requests which can generate more interrupts , we want to
             * avoid clearing an interrupt that might be valid */
             
        if (hostIrqs) {            
                /* ack interrupts that are HCD owned */
            if(A_OK != (status = DoPioWriteInternal(pCxt,
                                        ATH_SPI_INTR_CAUSE_REG,
                                        hostIrqs))){
                break;
            }          
        }
        
            /* process errors */ 
        if (interrupts & ATH_SPI_HCD_ERROR_IRQS) { 
            /* if an error has occurred the device will need to be reset before
             * any new communication can be attempted.  There is not graceful 
             * recovery from one of these errors. */
            if (interrupts & ATH_SPI_INTR_ADDRESS_ERROR) {
                //REL_PRINT(SDDBG_ERROR, ("ATH SPI - ADDRESS ERROR \n")); 
            }
            
            if (interrupts &  ATH_SPI_INTR_WRBUF_ERROR) {
                //REL_PRINT(SDDBG_ERROR, ("ATH SPI - WRBUF ERROR \n"));     
            }
            
            if (interrupts & ATH_SPI_INTR_RDBUF_ERROR) {
                //REL_PRINT(SDDBG_ERROR, ("ATH SPI - RDBUF ERROR \n"));     
            }            
                /* ack error interrupts */
            DoPioWriteInternal(pCxt,
                               ATH_SPI_INTR_CAUSE_REG,
                               interrupts & ATH_SPI_HCD_ERROR_IRQS);
                      
            interrupts &= ~ATH_SPI_HCD_ERROR_IRQS;
            pDCxt->hcd.booleans |= SDHD_BOOL_FATAL_ERROR;            
                /* mask all interrupts */
            DoPioWriteInternal(pCxt, ATH_SPI_INTR_ENABLE_REG, 0);
            break;            
        }
                       
        if (interrupts & ATH_SPI_INTR_HOST_CTRL_ACCESS_DONE) {
            /* this should not happen */
            interrupts &= ~ATH_SPI_INTR_HOST_CTRL_ACCESS_DONE;
            
                /* mask host access interrupts */
            MaskSPIInterrupts(pCxt, (ATH_SPI_INTR_HOST_CTRL_RD_DONE | ATH_SPI_INTR_HOST_CTRL_WR_DONE));            
        }
        
        if (interrupts & ATH_SPI_INTR_WRBUF_BELOW_WMARK) {                        
            interrupts &= ~ATH_SPI_INTR_WRBUF_BELOW_WMARK;                
                /* disable interrupt */
            MaskSPIInterrupts(pCxt, ATH_SPI_INTR_WRBUF_BELOW_WMARK);            
                /* check to see if we need to start any pending DMA waiting on more buffer space */      
            if (pDCxt->hcd.booleans & SDHD_BOOL_DMA_WRITE_WAIT_FOR_BUFFER) {                                               
                    /* re-cache the buffer space available register */    
                if(A_OK != (status = DoPioReadInternal(pCxt, ATH_SPI_WRBUF_SPC_AVA_REG, &param))){
                    break;
                }

                pDCxt->hcd.WriteBufferSpace = param;                
                pDCxt->hcd.booleans &= ~SDHD_BOOL_DMA_WRITE_WAIT_FOR_BUFFER;                                  
                    /* reset water mark */
                if(A_OK != (status = ResetWriteBufferWaterMark(pCxt))){
                    break;
                }
            }
        }               
            /* check to see if any other sources remain, these belong to the function driver */
            
        if (interrupts & ATH_SPI_INTR_PKT_AVAIL) {
                /* If there are pending interrupts of interest to the function driver 
                 * we disable all function-driver IRQ sources while the function driver processes
                 * the interrupt event. This allows the host driver to continue processing
                 * host access done and write buffer water mark interrupts.
                 * 
                 * The function driver will re-enable sources when it is done */            
            MaskSPIInterrupts(pCxt, ATH_SPI_INTR_PKT_AVAIL);    
            notifyFunctionIRQs = A_TRUE;    
        }     
        /* process the CPU interrupt */
        if(interrupts & ATH_SPI_INTR_LOCAL_CPU_INTR){
        	MaskSPIInterrupts(pCxt, ATH_SPI_INTR_CPU_INTR|ATH_SPI_INTR_LOCAL_CPU_INTR);
        	pDCxt->hcd.CpuInterrupt = 1;
        }   
    } while (0);


    if ((pDCxt->hcd.booleans & SDHD_BOOL_DMA_IN_PROG) == 0) {
        /* re-enable SPI interrupt detection at the hardware layer, note we always enable the
         * SPI interrupt so that the HCD can get host-access-done interrupts.
         * Note also that we do not enable interrupts if a DMA transfer is in progress */
        EnableDisableSPIIRQHwDetect(pCxt, A_TRUE);
    }        
                    
    if (notifyFunctionIRQs) {
        pDCxt->hcd.PendingIrqAcks = 1<<1;
    }             
    
    if (A_OK != status) {
        A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT        
        return A_FALSE;        
#endif        
    }
    
    return A_TRUE;
}


/*****************************************************************************/
/*  Hcd_ReinitTarget - Used to re-initialize the device. If the HCD layer is 
 *   already up but the device has been shutdown and needs to be re-started
 *   this function can be used to bring the chip out of reset and set the 
 *   write buffer watermark.
 * 		A_VOID *pCxt - the driver context.    
 *****************************************************************************/
A_STATUS 
Hcd_ReinitTarget(A_VOID *pCxt)
{
	A_STATUS status;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);

	do{
        if(A_OK != (status = DoPioWriteInternal(pCxt, ATH_SPI_CONFIG_REG, pDCxt->hcd.SpiConfigShadow))){
            break;
        }    	
            
        if(A_OK != (status = ResetWriteBufferWaterMark(pCxt))){
            break;
        }    	
            
	    Hcd_UnmaskSPIInterrupts(pCxt, ATH_SPI_HCD_ERROR_IRQS);
	    /* Enable the CPU interrupt so that the first interrupt
         * is not lost. */
 		Hcd_UnmaskSPIInterrupts(pCxt, ATH_SPI_INTR_LOCAL_CPU_INTR); 
	}while(0);
	
	return status;
}

/*****************************************************************************/
/*  Hcd_Deinitialize - Used to de-initialize the HCD layer. Conditionally 
 *	 write the SPI config register to reset the device. 
 * 		A_VOID *pCxt - the driver context.    
 *****************************************************************************/
A_VOID 
Hcd_Deinitialize(A_VOID *pCxt)
{            
    if (GET_DRIVER_COMMON(pCxt)->hcd.MiscFlags & MISC_FLAG_RESET_SPI_IF_SHUTDOWN) {        
            /* reset spi */
        DoPioWriteInternal(pCxt,
                           ATH_SPI_CONFIG_REG,
                           ATH_SPI_CONFIG_RESET);

    }                                     
}

/*****************************************************************************/
/*  Hcd_Init - Used to initialize the Host Controller Driver (HCD) layer. Uses
 *	 SPI API to write the SPI config register which will bring the device
 *   out of reset. reads the WRITE buffer space register to learn the default
 *	 size for this fifo.
 * 		A_VOID *pCxt - the driver context.    
 *****************************************************************************/
A_STATUS 
Hcd_Init(A_VOID *pCxt) 
{
    A_STATUS status;
    A_UINT32   startUpClock;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);

    do {                
        if (0 == (pDCxt->hcd.SpiHWCapabilitiesFlags & (HW_SPI_FRAME_WIDTH_8 | HW_SPI_FRAME_WIDTH_16))) {
                /* We minimally require 8 and 16 bit frame widths
                 * 16-bits - Command/Addr frames
                 * 8-bits - data frames including early termination */
            status = A_ERROR;
            A_ASSERT(0);
#if DRIVER_CONFIG_DISABLE_ASSERT            
            break;    
#endif            
        }
        
            /* the SPI module has byte swapping capability for DMA requests and we will be turning it on
             * set the DMA buffer copy mode to not swap  */
        pDCxt->hcd.booleans &= ~SDHD_BOOL_HOST_DMA_COPY_MODE;        
            /* for host access data frames, byte swaping is required */
        pDCxt->hcd.booleans |= SDHD_BOOL_HOST_ACCESS_COPY_MODE;        
        
        if (pDCxt->hcd.SpiHWCapabilitiesFlags & HW_SPI_FRAME_WIDTH_32) {
            pDCxt->hcd.HostAccessDataWidth = ATH_TRANS_DS_32;
            pDCxt->hcd.DMADataWidth = ATH_TRANS_DS_32;           
        } else if(pDCxt->hcd.SpiHWCapabilitiesFlags & HW_SPI_FRAME_WIDTH_8){
            pDCxt->hcd.HostAccessDataWidth = ATH_TRANS_DS_8;
            pDCxt->hcd.DMADataWidth = ATH_TRANS_DS_8;           
            pDCxt->hcd.booleans &= ~SDHD_BOOL_HOST_ACCESS_COPY_MODE;                    
        } else {
            pDCxt->hcd.HostAccessDataWidth = ATH_TRANS_DS_16;
            pDCxt->hcd.DMADataWidth = ATH_TRANS_DS_16;           
        }            
        pDCxt->hcd.SpiConfigShadow = 0;
        
        startUpClock = AR4XXX_NORMAL_CLOCK_RATE;
        
        if (startUpClock > pDCxt->hcd.OperationalClock) {
                /* the hardware layer wants a clock rate that is less than the normal rate */
            startUpClock = pDCxt->hcd.OperationalClock;
        }
                            
        if (pDCxt->hcd.OperationalClock > AR4XXX_NORMAL_CLOCK_RATE) {
                /* when operating above the normal clock rate, we need to enable some
                 * logic to improve timing margins.
                 * *** NOTE : setting the config register must be done at our startup clock rate.
                 *     We can switch to the higher clock rate once the SPI config register is written
                 * */
            pDCxt->hcd.SpiConfigShadow |= (0x1 << ATH_SPI_CONFIG_MISO_MUXSEL_MASK_SHIFT);            
        }
        
            /* set our startup clock mode, the config register must be written with the startup clock
             * rate */
        CUSTOM_HW_SET_CLOCK(pCxt, &startUpClock); 
        
            /* start off with the I/O enable bit and byte swapping turned off */
        pDCxt->hcd.SpiConfigShadow |= ATH_SPI_CONFIG_IO_ENABLE;        
                
        if (ATH_TRANS_DS_32 == pDCxt->hcd.DMADataWidth) {
                /* enable byte swapping logic, default is 32 bit mode */
            pDCxt->hcd.SpiConfigShadow |= ATH_SPI_CONFIG_BYTE_SWAP;           
        } else if (ATH_TRANS_DS_16 == pDCxt->hcd.DMADataWidth) {
                /* enable byte swapping logic for 16 bit mode */
            pDCxt->hcd.SpiConfigShadow |= (ATH_SPI_CONFIG_SWAP_16BIT | ATH_SPI_CONFIG_BYTE_SWAP);                       
        } else {
             /* for 8 bit mode, do not turn on byte swapping */      
        }
               
            /* delay a bit before our first SPI operation */       
        CUSTOM_HW_USEC_DELAY(pCxt, pDCxt->hcd.PowerUpDelay*1000);
//FIRST SPI OPERATION START!!!                    
        status = DoPioWriteInternal(pCxt, ATH_SPI_CONFIG_REG, pDCxt->hcd.SpiConfigShadow);

		Api_BootProfile(pCxt, 0x02);
		   
        if (A_OK != status) {
            break;    
        }       
            /* SPI configuration has been written, we can safely switch to the desired operating clock rate */       
        startUpClock = pDCxt->hcd.OperationalClock;
        CUSTOM_HW_SET_CLOCK(pCxt, &startUpClock); 
        
            /* read the buffer space available for this chip, at reset we should have an empty buffer */    
        status = DoPioReadInternal(pCxt, ATH_SPI_WRBUF_SPC_AVA_REG, &pDCxt->hcd.WriteBufferSpace);
        //DoPioReadInternal(pDevice,ATH_SPI_WRBUF_SPC_AVA_REG,&pDevice->WriteBufferSpace);
#if 0        
{
int i=0;
while(i<50000){
	DoPioReadInternal(pCxt, ATH_SPI_WRBUF_SPC_AVA_REG, (A_UINT32*)&test);
	if(test == EXPECTED_MAX_WRITE_BUFFER_SPACE){
		good++;
	}else{
		bad++;
	}
	test = 0;
	i++;
}
}
#endif


        
        if (A_OK != status) {
            break;    
        }
        
            /* save this off for watermark level calculations */
        pDCxt->hcd.MaxWriteBufferSpace = pDCxt->hcd.WriteBufferSpace;
        /* NOTE: if this assert fails it suggests that SPI communication with the WIFI device
         * 	is not functioning. Perhaps the 2 chips are not connected. */
        A_ASSERT(pDCxt->hcd.MaxWriteBufferSpace == EXPECTED_MAX_WRITE_BUFFER_SPACE);
            /* set write buffer watermark to zero, we only set it when we need an interrupt */       
        if(A_OK != (status = ResetWriteBufferWaterMark(pCxt))){
            break;
        }                                          
            /* Enable error IRQ sources */
        Hcd_UnmaskSPIInterrupts(pCxt, ATH_SPI_HCD_ERROR_IRQS);    
        	/* Enable the CPU interrupt so that the first interrupt
        	 * is not lost. */
 		Hcd_UnmaskSPIInterrupts(pCxt, ATH_SPI_INTR_LOCAL_CPU_INTR);    
            /* enable interrupts at the HW layer */
        EnableDisableSPIIRQHwDetect(pCxt, A_TRUE);
        /* this delay is necessary to prevent other driver code from accessing
         * the chip before it has had sufficient time to boot.  accessing 
         * prematurely will put the chip in a failed state.
        */
        A_MDELAY(2);
    } while (FALSE);
             
    return status;
}

/*****************************************************************************/
/*  Hcd_GetLookAhead - Interacts with device over SPI to acquire lookahead
 *	 structure. This structure provides details of a rx packet.
 * 		A_VOID *pCxt - the driver context.    
 *****************************************************************************/
A_STATUS 
Hcd_GetLookAhead(A_VOID *pCxt)
{
    A_STATUS        status;
    A_UINT16          lookAhead1, lookAhead2;
    A_UINT32          bytesAvail;    
    A_UINT32        assembledLookAhead;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);

    do {        
            /* get the bytes available in the SPI buffer */
        if(A_OK != (status = DoPioReadInternal(pCxt, 
                                 ATH_SPI_RDBUF_BYTE_AVA_REG,
                                 &bytesAvail))){
            break;
        }
        
        pDCxt->hcd.ReadBufferSpace = bytesAvail;        
            /* is there a header's worth ?*/
        if (bytesAvail < 4) {
                /* not enough bytes for the look-ahead  */
            break;    
        }
        
            /* peak into the SPI read buffer using the look aheads since we
             * established that there is at least a look-ahead number of bytes */
        if(A_OK != (status = DoPioReadInternal(pCxt, 
                                 ATH_SPI_RDBUF_LOOKAHEAD1_REG,
                                 &assembledLookAhead))){
            break;
        }
        
        lookAhead1 = (A_UINT16)assembledLookAhead;
         
        if(A_OK != (status = DoPioReadInternal(pCxt, 
                                 ATH_SPI_RDBUF_LOOKAHEAD2_REG,
                                 &assembledLookAhead))){
            break;
        }        
        
        lookAhead2 = (A_UINT16)assembledLookAhead;
        /* the lookaheads were already converted to native Endian by DoPioReadInternal
         * however for the assembledLookAhead code to work properly for both 
         * big E and little E it must be converted again for little E.
         */
        lookAhead1 = A_BE2CPU16(lookAhead1);
        lookAhead2 = A_BE2CPU16(lookAhead2);
           /* assemble look-ahead , first we need to swap bytes */
        ((A_UINT8*)&assembledLookAhead)[0] = ((A_UINT8*)&lookAhead1)[0];
        ((A_UINT8*)&assembledLookAhead)[1] = ((A_UINT8*)&lookAhead1)[1];
        ((A_UINT8*)&assembledLookAhead)[2] = ((A_UINT8*)&lookAhead2)[0];
        ((A_UINT8*)&assembledLookAhead)[3] = ((A_UINT8*)&lookAhead2)[1];
        
        A_ASSERT(assembledLookAhead != 0);
        pDCxt->lookAhead = assembledLookAhead;        
    } while (0);
    
    return status;
}
