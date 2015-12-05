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
#include <custom_api.h>
#include <mqx.h>
#include <bsp.h>
#include <common_api.h>
#include <atheros_wifi.h>
#include "whistle_lwgpio_isr_mux.h"


#define POWER_UP_DELAY (1)

#define HW_SPI_CAPS (HW_SPI_FRAME_WIDTH_8| \
					 HW_SPI_NO_DMA | \
					 HW_SPI_INT_EDGE_DETECT)       


/*
 *   Ernie:  Experimented with different interrupt priorities here for the Atheros HW task.
 *           2 seems to help, while 0 and 1 cause almost an immediate Atheros assert(0).
 *           3 is what it was originally.
 */
#define ATHEROS_INTERRUPT_PRIORITY      (3)

A_VOID 
Custom_HW_SetClock(A_VOID *pCxt, A_UINT32 *pClockRate)
{
    A_UNUSED_ARGUMENT(pCxt);
	A_UNUSED_ARGUMENT(pClockRate);
}

A_VOID 
Custom_HW_EnableDisableSPIIRQ(A_VOID *pCxt, A_BOOL enable)
{
    lwgpio_int_enable(&GET_DRIVER_CXT(pCxt)->int_cxt, enable);
}

A_VOID Custom_HW_UsecDelay(A_VOID *pCxt, A_UINT32 uSeconds)
{
    //avoids division and modulo.
    A_INT32 time = uSeconds>>10; // divide by 1024
    // account for difference between 1024 and 1000 with 
    // acceptable margin for error
    time += (time * 24 > 500)? 1:0;
    // capture round off between usec and msec
    time += (uSeconds - (time * 1000) > 500)? 1:0;
    //input must be sub 500 so we simply wait 1msec
    if(time == 0) time += 1;
    //current implementation relys on msec sleep
    _time_delay(time);
    A_UNUSED_ARGUMENT(pCxt);
}

A_VOID 
Custom_HW_PowerUpDown(A_VOID *pCxt, A_UINT32 powerUp)
{   
#if NOT_WHISTLE
    A_UINT32 cmd = (powerUp)?  GPIO_IOCTL_WRITE_LOG1: GPIO_IOCTL_WRITE_LOG0;

    if (IO_OK != ioctl (GET_DRIVER_CXT(pCxt)->pwd_cxt, cmd, NULL))
	{
		A_ASSERT(0);
	}
#else
    LWGPIO_VALUE cmd = (powerUp)?  LWGPIO_VALUE_HIGH : LWGPIO_VALUE_LOW;

    lwgpio_set_value(&GET_DRIVER_CXT(pCxt)->pwd_cxt, cmd);
#endif /* NOT_WHISTLE */
}

/*****************************************************************************/
/* Custom_Bus_InOutBuffer - This is the platform specific solution to 
 *  transfer a buffer on the SPI bus.  This solution is always synchronous
 *  regardless of sync param. The function will use the MQX fread and fwrite
 *  as appropriate.
 *      A_VOID * pCxt - the driver context.
 *      A_UINT8 *pBuffer - The buffer to transfer.
 *      A_UINT16 length - the length of the transfer in bytes.
 *      A_UINT8 doRead - 1 if operation is a read else 0.
 *      A_BOOL sync - TRUE is synchronous transfer is required else FALSE.
 *****************************************************************************/
A_STATUS 
Custom_Bus_InOutBuffer(A_VOID *pCxt,
                                    A_UINT8 *pBuffer,
									A_UINT16 length, 									
									A_UINT8 doRead,
                                    A_BOOL sync)
{
	A_STATUS status = A_OK;    
    
    A_UNUSED_ARGUMENT(sync);
    /* this function takes advantage of the SPI turbo mode which does not toggle the chip select
     * during the transfer.  Rather the chip select is asserted at the beginning of the transfer
     * and de-asserted at the end of the entire transfer via fflush(). */     
    if(doRead){	    
	    if(length != fread(pBuffer, 1, length, GET_DRIVER_CXT(pCxt)->spi_cxt)){
		    status = A_HARDWARE;    	    
	    }	    	
	}else{		
		if(length != fwrite(pBuffer, 1, (A_INT32)length, GET_DRIVER_CXT(pCxt)->spi_cxt)){
			status = A_HARDWARE;        	
		}
	} 
	
    return status;
}				

/*****************************************************************************/
/* Custom_Bus_InOut_Token - This is the platform specific solution to 
 *  transfer 4 or less bytes in both directions. The transfer must be  
 *  synchronous. This solution uses the MQX spi ioctl to complete the request.
 *      A_VOID * pCxt - the driver context.
 *      A_UINT32 OutToken - the out going data.
 *      A_UINT8 DataSize - the length in bytes of the transfer.
 *      A_UINT32 *pInToken - A Buffer to hold the incoming bytes. 
 *****************************************************************************/
A_STATUS 
Custom_Bus_InOutToken(A_VOID *pCxt,
                           A_UINT32        OutToken,
                           A_UINT8         DataSize,
                           A_UINT32    *pInToken) 
{   
    SPI_READ_WRITE_STRUCT  spi_buf;        
    A_STATUS status = A_OK;    
    
    spi_buf.BUFFER_LENGTH = (A_UINT32)DataSize; // data size if really a enum that is 1 less than number of bytes
     
    _DCACHE_FLUSH_MBYTES((pointer)&OutToken, sizeof(A_UINT32));
    
    spi_buf.READ_BUFFER = (char_ptr)pInToken;
    spi_buf.WRITE_BUFFER = (char_ptr)&OutToken;
            
    do{        
        if (A_OK != Custom_Bus_StartTransfer(pCxt, A_TRUE))
        {        
            status = A_HARDWARE;
            break;
        }
        //IO_IOCTL_SPI_READ_WRITE requires read && write buffers        
        if (SPI_OK != ioctl(GET_DRIVER_CXT(pCxt)->spi_cxt, IO_IOCTL_SPI_READ_WRITE, &spi_buf)) 
        {        
            status = A_HARDWARE;
            break;
        }
        //waits until interrupt based operation completes            
        Custom_Bus_CompleteTransfer(pCxt, A_TRUE);
        _DCACHE_INVALIDATE_MBYTES((pointer)pInToken, sizeof(A_UINT32));
    }while(0);
     	
    return status;                                                                        
}


/*****************************************************************************/
/* Custom_Bus_Start_Transfer - This function is called by common layer prior
 *  to starting a new bus transfer. This solution merely sets up the SPI 
 *  mode as a precaution.
 *      A_VOID * pCxt - the driver context.     
 *      A_BOOL sync - TRUE is synchronous transfer is required else FALSE.
 *****************************************************************************/
A_STATUS 
Custom_Bus_StartTransfer(A_VOID *pCxt, A_BOOL sync)
{
#if 0
    /*
     *   ERNIE:  This is extremely wasteful.  We don't need to keep setting the SPI settings every time
     *             we want to TX/RX with the Atheros.  Instead, intialize it once and be done.
     */
    A_UINT32 param;
    A_STATUS status = A_OK;

    A_UNUSED_ARGUMENT(sync);
    //ensure SPI device is set to proper mode before issuing transfer
    param = SPI_CLK_POL_PHA_MODE3;
    /* set the mode in case the SPI is shared with another device. */
    if (SPI_OK != ioctl(GET_DRIVER_CXT(pCxt)->spi_cxt, IO_IOCTL_SPI_SET_MODE, &param)) 
    {        
        status = A_ERROR;        
    }

    return status;
#else
    return A_OK;
#endif
}


/*****************************************************************************/
/* Custom_Bus_Complete_Transfer - This function is called by common layer prior
 *  to completing a bus transfer. This solution calls fflush to de-assert 
 *  the chipselect.
 *      A_VOID * pCxt - the driver context.     
 *      A_BOOL sync - TRUE is synchronous transfer is required else FALSE.
 *****************************************************************************/
A_STATUS 
Custom_Bus_CompleteTransfer(A_VOID *pCxt, A_BOOL sync)
{
    A_UNUSED_ARGUMENT(sync);
    fflush(GET_DRIVER_CXT(pCxt)->spi_cxt);
    return A_OK;
}

extern A_VOID *p_Global_Cxt;


/* interrupt service routine that gets mapped to the 
 * SPI INT GPIO */
A_VOID 
Custom_HW_InterruptHandler(A_VOID *pointer)
{
	LWGPIO_STRUCT_PTR int_cxt = &GET_DRIVER_CXT(p_Global_Cxt)->int_cxt;
	
#if 0
	/*
	 *   NOTE:  flag is already cleared in whistle_lwgpio_isr_mux().
	 */
	lwgpio_int_clear_flag(int_cxt);
#endif
	
    /* pass global context to common layer */
    HW_InterruptHandler(p_Global_Cxt);
    
    A_UNUSED_ARGUMENT(pointer);
}

A_STATUS 
Custom_HW_Init(A_VOID *pCxt)
{
    GPIO_PIN_STRUCT 	pins[2];
    A_CUSTOM_DRIVER_CONTEXT *pCustCxt = GET_DRIVER_CXT(pCxt);
    A_UINT32 baudrate, endian, input;
    ATHEROS_PARAM_WIFI_STRUCT *pParam;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
#ifdef ATH_SPI_DMA
    {
        extern int _bsp_dspi_dma_setup(void);
        static int bsp_dspi_dma_ready = 0;
        if (!bsp_dspi_dma_ready)
        {
            _bsp_dspi_dma_setup();
            bsp_dspi_dma_ready = 1;
        }
    }
#endif
    pParam = GET_DRIVER_CXT(pCxt)->pDriverParam;

    pCustCxt->spi_cxt = fopen(pParam->SPI_DEVICE, (pointer)SPI_FLAG_FULL_DUPLEX);
        
    if(pCustCxt->spi_cxt == NULL){
        A_ASSERT(0);
    }

    ioctl(pCustCxt->spi_cxt, IO_IOCTL_SPI_GET_BAUD, &baudrate);
  
#define MAX_ALLOWED_BAUD_RATE 25000000
    
	if(baudrate > MAX_ALLOWED_BAUD_RATE){
		baudrate = MAX_ALLOWED_BAUD_RATE;
		ioctl(pCustCxt->spi_cxt, IO_IOCTL_SPI_SET_BAUD, &baudrate);
	}		
	
    ioctl(pCustCxt->spi_cxt, IO_IOCTL_SPI_GET_ENDIAN, &endian);
#if PSP_MQX_CPU_IS_KINETIS
	input = SPI_PUSHR_PCS(1 << 0); 
#elif PSP_MQX_CPU_IS_MCF52
    input = MCF5XXX_QSPI_QDR_QSPI_CS0; 
#else
#error Atheros wifi: unsupported Freescale chip     
#endif    
    if (SPI_OK != ioctl (pCustCxt->spi_cxt, IO_IOCTL_SPI_SET_CS, &input)) 
    {        
        A_ASSERT(0);
    }
    //SPI_DEVICE_LITTLE_ENDIAN or SPI_DEVICE_BIG_ENDIAN
    //IO_IOCTL_SPI_SET_CS
    
#if NOT_WHISTLE
    /* fill the pins structure */
    pins[0] = pParam->INT_PIN;
    pins[1] = GPIO_LIST_END;        
    pCustCxt->int_cxt = fopen(pParam->GPIO_DEVICE, (char_ptr)&pins);

    if(NULL == pCustCxt->int_cxt){            
        A_ASSERT(0);
    }
        
    if (IO_OK != ioctl(pCustCxt->int_cxt, GPIO_IOCTL_SET_IRQ_FUNCTION, (pointer)&Custom_HW_InterruptHandler )){        
        A_ASSERT(0);
    }

#if WLAN_CONFIG_ENABLE_CHIP_PWD_GPIO
/* use gpio pin to reset the wifi chip as needed.
 * this service is required if during operation it
 * is desired to turn off the wifi chip. 
 */
	pins[0] = pParam->PWD_PIN;
    pins[1] = GPIO_LIST_END;     
	pCustCxt->pwd_cxt = fopen(pParam->PWD_DEVICE, (char_ptr)&pins);
	
	if(NULL == pCustCxt->pwd_cxt){            
        A_ASSERT(0);
    }
#endif
#else
	/* open WIFI_INT pin for input */
	A_ASSERT(lwgpio_init(&pCustCxt->int_cxt, pParam->INT_PIN, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE));

    /* disable in case it's already enabled */
    lwgpio_int_enable(&pCustCxt->int_cxt, FALSE);

	lwgpio_set_functionality(&pCustCxt->int_cxt, pParam->INT_MUX);

	/* enable gpio functionality, react on falling edge */
	A_ASSERT(lwgpio_int_init(&pCustCxt->int_cxt, LWGPIO_INT_MODE_FALLING));
	
    // Atheros Interrupt GPIO needs to be pulled up.
    lwgpio_set_attribute(&pCustCxt->int_cxt, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_ENABLE);
    
    whistle_lwgpio_isr_install(&pCustCxt->int_cxt, ATHEROS_INTERRUPT_PRIORITY, Custom_HW_InterruptHandler);
	
	/* open PWD pin for output */
	A_ASSERT(lwgpio_init(&pCustCxt->pwd_cxt, pParam->PWD_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE));

	/* swich pin functionality (MUX) to GPIO mode */
	lwgpio_set_functionality(&pCustCxt->pwd_cxt, pParam->PWD_MUX);

#endif /* NOT_WHISTLE */
        
    /* IT is necessary for the custom code to populate the following parameters
     * so that the common code knows what kind of hardware it is working with. */        
    pDCxt->hcd.OperationalClock = baudrate;
    pDCxt->hcd.PowerUpDelay = POWER_UP_DELAY;            
    pDCxt->hcd.SpiHWCapabilitiesFlags = HW_SPI_CAPS;
    pDCxt->hcd.MiscFlags |= MISC_FLAG_RESET_SPI_IF_SHUTDOWN;   
    
    return A_OK;
}

A_STATUS 
Custom_HW_DeInit(A_VOID *pCxt)
{
#if WLAN_CONFIG_ENABLE_CHIP_PWD_GPIO
	/* NOTE: if the gpio fails to retain its state after fclose
	 * then the wifi device will not remain in the off State.
	 */	
#if NOT_WHISTLE
    fclose(GET_DRIVER_CXT(pCxt)->pwd_cxt);
    GET_DRIVER_CXT(pCxt)->pwd_cxt = NULL;
#else
    lwgpio_set_direction(&GET_DRIVER_CXT(pCxt)->pwd_cxt, LWGPIO_DIR_INPUT); // just make it an input for low power
#endif /* NOT_WHISTLE */
#endif       
	/* clean up resources initialized in Probe() */
    fclose(GET_DRIVER_CXT(pCxt)->spi_cxt);
    GET_DRIVER_CXT(pCxt)->spi_cxt = NULL;
#if NOT_WHISTLE
    fclose(GET_DRIVER_CXT(pCxt)->int_cxt);
    GET_DRIVER_CXT(pCxt)->int_cxt = NULL;
#else
    /* enable interrupt on GPIO peripheral module*/
    lwgpio_int_enable(&GET_DRIVER_CXT(pCxt)->int_cxt, FALSE);

    /* Clear interrupt status register so you don't get a "fake" interrupt immediately. */
    lwgpio_int_clear_flag(&GET_DRIVER_CXT(pCxt)->int_cxt);
    
    /* Still need button interrupts, so leave the ISR installed */
#endif
    
    return A_OK;
}
