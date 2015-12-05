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
#include <wmi_api.h>
#include <AR6K_version.h>
#include <hif_internal.h>
#include <targaddrs.h>
#include <atheros_wifi_api.h>
#include <atheros_wifi_internal.h>
#include <htc_services.h>
#include <bmi.h>
#include <htc.h>
#include <spi_hcd_if.h>
#include <aggr_recv_api.h>
#include "hw2.0/hw/apb_map.h"
#include "hw4.0/hw/rtc_reg.h"

 
A_UINT32 ar4XXX_boot_param = AR4XXX_PARAM_MODE_NORMAL;//AR4XXX_PARAM_RAW_QUAD;
A_UINT32 ar4xx_reg_domain = AR4XXX_PARAM_REG_DOMAIN_DEFAULT;
A_UINT32 last_driver_error;  //Used to store the last error encountered by the driver

/*****************************************************************************/
/*  ConnectService - Utility to set up a single endpoint service.
 * 		A_VOID *pCxt - the driver context.    
 *	 	HTC_SERVICE_CONNECT_REQ  *pConnect - pointer to connect request 
 *		 structure.
 *****************************************************************************/
static A_STATUS ConnectService(A_VOID *pCxt,
                               HTC_SERVICE_CONNECT_REQ  *pConnect)
{
    A_STATUS                 status;
    HTC_SERVICE_CONNECT_RESP response;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
    do {

        A_MEMZERO(&response,sizeof(response));

        if(A_OK != (status = HTC_ConnectService(pCxt, pConnect, &response))){
         	break;                          
       	}

        
        
        switch (pConnect->ServiceID) {
            case WMI_CONTROL_SVC :
                if (pDCxt->wmiEnabled == A_TRUE) {
                        /* set control endpoint for WMI use */
                    wmi_set_control_ep(pDCxt->pWmiCxt, response.Endpoint);
                }                                    
                break;
            case WMI_DATA_BE_SVC :
            	pDCxt->ac2EpMapping[WMM_AC_BE] = response.Endpoint;
            	pDCxt->ep2AcMapping[response.Endpoint] = WMM_AC_BE;                
                break;
            case WMI_DATA_BK_SVC :
                pDCxt->ac2EpMapping[WMM_AC_BK] = response.Endpoint;
            	pDCxt->ep2AcMapping[response.Endpoint] = WMM_AC_BK;      
                break;
            case WMI_DATA_VI_SVC :
                pDCxt->ac2EpMapping[WMM_AC_VI] = response.Endpoint;
            	pDCxt->ep2AcMapping[response.Endpoint] = WMM_AC_VI;
                break;
           case WMI_DATA_VO_SVC :
                pDCxt->ac2EpMapping[WMM_AC_VO] = response.Endpoint;
            	pDCxt->ep2AcMapping[response.Endpoint] = WMM_AC_VO;
                break;
           default:                
                status = A_EINVAL;
            break;
        }

    } while (0);

    return status;
}

/*****************************************************************************/
/*  SetupServices - Utility to set up endpoint services.
 * 		A_VOID *pCxt - the driver context.    
 *****************************************************************************/
static A_STATUS
SetupServices(A_VOID *pCxt)
{
	A_STATUS status;
	HTC_SERVICE_CONNECT_REQ connect;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
	do{
	    A_MEMZERO(&connect,sizeof(connect));
	        /* meta data is unused for now */
	    connect.pMetaData = NULL;
	    connect.MetaDataLength = 0;                      
	        /* connect to control service */
	    connect.ServiceID = WMI_CONTROL_SVC;

        if(A_OK != (status = ConnectService(pCxt, &connect))){
        	break;
        }
        
        connect.LocalConnectionFlags |= HTC_LOCAL_CONN_FLAGS_ENABLE_SEND_BUNDLE_PADDING;
            /* limit the HTC message size on the send path, although we can receive A-MSDU frames of
             * 4K, we will only send ethernet-sized (802.3) frames on the send path. */
        connect.MaxSendMsgSize = WMI_MAX_TX_DATA_FRAME_LENGTH;          

            /* for the remaining data services set the connection flag to reduce dribbling,
             * if configured to do so */
        if (WLAN_CONFIG_REDUCE_CREDIT_DRIBBLE) {
            connect.ConnectionFlags |= HTC_CONNECT_FLAGS_REDUCE_CREDIT_DRIBBLE;
            /* the credit dribble trigger threshold is (reduce_credit_dribble - 1) for a value
             * of 0-3 */
            connect.ConnectionFlags &= ~HTC_CONNECT_FLAGS_THRESHOLD_LEVEL_MASK;
            connect.ConnectionFlags |= HTC_CONNECT_FLAGS_THRESHOLD_LEVEL_ONE_HALF;                        
        }
            /* connect to best-effort service */
        connect.ServiceID = WMI_DATA_BE_SVC;

        if(A_OK != (status = ConnectService(pCxt, &connect))){
        	break;
        }

            /* connect to back-ground
             * map this to WMI LOW_PRI */
        connect.ServiceID = WMI_DATA_BK_SVC;
        
        if(A_OK != (status = ConnectService(pCxt, &connect))){
        	break;
        }

            /* connect to Video service, map this to
             * to HI PRI */
        connect.ServiceID = WMI_DATA_VI_SVC;
        
        if(A_OK != (status = ConnectService(pCxt, &connect))){
        	break;
        }

            /* connect to VO service, this is currently not
             * mapped to a WMI priority stream due to historical reasons.
             * WMI originally defined 3 priorities over 3 mailboxes
             * We can change this when WMI is reworked so that priorities are not
             * dependent on mailboxes */
        connect.ServiceID = WMI_DATA_VO_SVC;
        
        if(A_OK != (status = ConnectService(pCxt, &connect))){
        	break;
        }

		
        A_ASSERT(pDCxt->ac2EpMapping[WMM_AC_BE] != 0);
        A_ASSERT(pDCxt->ac2EpMapping[WMM_AC_BK] != 0);
        A_ASSERT(pDCxt->ac2EpMapping[WMM_AC_VI] != 0);
        A_ASSERT(pDCxt->ac2EpMapping[WMM_AC_VO] != 0);          
    } while (0);

    return status;

}

/*****************************************************************************/
/*  Driver_ContextInit - Initializes the drivers COMMON context. The allocation
 *	 for the context should be made by the caller. This function will call
 *	 the Custom_DRiverContextInit to perform any system specific initialization
 *	 Look to Driver_ContextDeInit() to undo what gets done by this function.
 *      A_VOID *pCxt - the driver context.
 *****************************************************************************/
A_STATUS 
Driver_ContextInit(A_VOID *pCxt)
{   
	A_UINT8 i;	
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    /* most elements are initialized by setting to zero the rest are explicitly 
     * initialized.  A_BOOL are explicit as it is not obvious what A_FALSE might be */
    A_MEMZERO(pDCxt, sizeof(A_DRIVER_CONTEXT));
    pDCxt->driver_up = A_FALSE;   
    pDCxt->rxBufferStatus = A_FALSE;       
    pDCxt->hostVersion = AR6K_SW_VERSION; 
    pDCxt->wmiReady = A_FALSE;
    pDCxt->wmiEnabled = A_FALSE;    
    pDCxt->wmmEnabled = A_FALSE;    
    pDCxt->chipDown = A_TRUE;        
    pDCxt->strrclState = STRRCL_ST_DISABLED;
    pDCxt->strrclBlock = A_FALSE;
    pDCxt->wpsState = A_FALSE; 
    pDCxt->networkType = INFRA_NETWORK;
    pDCxt->dot11AuthMode = OPEN_AUTH;
    pDCxt->wpaAuthMode = NONE_AUTH;
    pDCxt->wpaPairwiseCrypto = NONE_CRYPT;
    pDCxt->wpaGroupCrypto = NONE_CRYPT;  
    pDCxt->wpaPmkValid = A_FALSE;        
    pDCxt->isConnected = A_FALSE;     
    pDCxt->isConnectPending = A_FALSE;     
    pDCxt->scanDone = A_FALSE;    
    pDCxt->userPwrMode = MAX_PERF_POWER;
    pDCxt->enabledSpiInts = ATH_SPI_INTR_PKT_AVAIL | ATH_SPI_INTR_LOCAL_CPU_INTR;
    pDCxt->mboxAddress = HIF_MBOX_START_ADDR(HIF_ACTIVE_MBOX_INDEX); /* the address for mailbox reads/writes. */
    pDCxt->blockSize = HIF_MBOX_BLOCK_SIZE; /* the mailbox block size */
    pDCxt->blockMask = pDCxt->blockSize-1; /* the mask derived from BlockSize */    
    pDCxt->padBuffer = A_MALLOC(pDCxt->blockSize, MALLOC_ID_CONTEXT);
    pDCxt->htcStart = A_FALSE;
    pDCxt->macProgramming = A_FALSE;
#if MANUFACTURING_SUPPORT
    pDCxt->testResp = A_FALSE;
#endif
    for(i=0 ; i<ENDPOINT_MANAGED_MAX ; i++){
    	pDCxt->ep[i].epIdx = i;
    }
    
    A_ASSERT(pDCxt->padBuffer != NULL);

    return CUSTOM_DRIVER_CXT_INIT(pCxt);    
}

/*****************************************************************************/
/*  Driver_ContextDeInit - Undoes any work performed by Driver_ContextInit.
 *	 Used as part of the shutdown process. Calls Custom_Driver_ContextDeInit()
 *	 to cleanup any work done by Custom_Driver_ContextInit().
 *      A_VOID *pCxt - the driver context.
 *****************************************************************************/
A_STATUS
Driver_ContextDeInit(A_VOID *pCxt)
{
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
	CUSTOM_DRIVER_CXT_DEINIT(pCxt);
	
	if(pDCxt->padBuffer != NULL){
		A_FREE(pDCxt->padBuffer, MALLOC_ID_CONTEXT);
		pDCxt->padBuffer = NULL;
	}
	
	return A_OK;
}

/*****************************************************************************/
/*  Driver_Init - inits any HW resources for HW access.
 *      Turns CHIP on via GPIO if necessary.
 *      Performs initial chip interaction. 
 *      If the system has a dedicated driver thread then
 *      this function should be called by the dedicated thread.
 *      If not then this function should be called between
 *      calls to api_InitStart and api_InitFinish
 *      A_VOID *pCxt - the driver context.
 *****************************************************************************/
A_STATUS 
Driver_Init(A_VOID *pCxt)
{
    A_STATUS status;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
    do{
    	/* initialize various protection elements */
    	if(A_OK != (status = RXBUFFER_ACCESS_INIT(pCxt))){
    		break;
    	}
    	
    	if(A_OK != (status = TXQUEUE_ACCESS_INIT(pCxt))){
    		break;
    	}
    	
    	if(A_OK != (status = IRQEN_ACCESS_INIT(pCxt))){
    		break;
    	}
    	
    	if(A_OK != (status = DRIVER_SHARED_RESOURCE_ACCESS_INIT(pCxt))){
    		break;
    	}
        /* - init hw resources */
        if(A_OK != (status = CUSTOM_HW_INIT(pCxt))){
            break;
        }
        /* - Power up device */
        HW_PowerUpDown(pCxt, 1);
        
        /* - bring chip CPU out of reset */
        if(A_OK != (status = Hcd_Init(pCxt))){
            break;
        }
        
        /* - initiate communication with chip firmware */
        if(A_OK != (status = Driver_BootComm(pCxt))){
        	break;
        }
        
        if((ar4XXX_boot_param & AR4XXX_PARAM_MODE_MASK) == AR4XXX_PARAM_MODE_BMI){
        	BMIInit(pCxt);
        }
        
        /* - acquire target type */
        if(A_OK != (status = Driver_GetTargetInfo(pCxt))){
        	break;
        }
        /* - perform any BMI chip configuration */
        if(ath_custom_init.Driver_BMIConfig != NULL){
        	if(A_OK != (status = ath_custom_init.Driver_BMIConfig(pCxt))){
        		break;
        	}
        }
        /* - perform any target configuration */
        if(ath_custom_init.Driver_TargetConfig != NULL){
        	if(A_OK != (status = ath_custom_init.Driver_TargetConfig(pCxt))){
        		break;
        	}
        }
        /* - choose wmi or not wmi */
        if(ath_custom_init.skipWmi == A_FALSE){
        	pDCxt->pWmiCxt = wmi_init(pCxt);
        	pDCxt->wmiEnabled = A_TRUE;
#if WLAN_CONFIG_11N_AGGR_SUPPORT        	
        	pDCxt->pAggrCxt = aggr_init();
#endif        	
        }
                        
        /* - alternate boot mode - exit driver init with BMI active */
        if(ath_custom_init.exitAtBmi){
        	break;
        }
        
        if((ar4XXX_boot_param & AR4XXX_PARAM_MODE_MASK) == AR4XXX_PARAM_MODE_BMI){
        	/* - done with BMI; call BMIDone */
        	if(A_OK != (status = BMIDone(pCxt))){
        		break;
        	}
        }
        
        /* - wait for HTC setup complete */
        if(A_OK != (status = HTC_WaitTarget(pCxt))){
        	break;
        }
        /* - setup other non-htc endpoints */
        if(A_OK != (status = SetupServices(pCxt))){
        	break;
        }            
        
    	/* - start HTC */
    	if(A_OK != (status = HTC_Start(pCxt))){
        	break;
        }            
        
        
    }while(0);

	if(status == A_OK){
		pDCxt->chipDown = A_FALSE;
        pDCxt->driver_up = A_TRUE;
	}

    return status;
}

/*****************************************************************************/
/*  Driver_DeInit - Undoes any work performed by Driver_Init.
 *	 Used as part of the shutdown process. 
 *      A_VOID *pCxt - the driver context.
 *****************************************************************************/
A_STATUS 
Driver_DeInit(A_VOID *pCxt)
{
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	
	Hcd_Deinitialize(pCxt); /* not necessary if HW_PowerUpDown() actually 
								shuts down chip */
	HW_PowerUpDown(pCxt, 0);
	CUSTOM_HW_DEINIT(pCxt);
	
	if(pDCxt->pWmiCxt){
		wmi_shutdown(pDCxt->pWmiCxt);
		pDCxt->pWmiCxt = NULL;
	}
#if WLAN_CONFIG_11N_AGGR_SUPPORT        	
    if(pDCxt->pAggrCxt){
    	aggr_deinit(pDCxt->pAggrCxt);
    	pDCxt->pAggrCxt = NULL;
    }
#endif     		
	
	RXBUFFER_ACCESS_DESTROY(pCxt);
	TXQUEUE_ACCESS_DESTROY(pCxt);
	IRQEN_ACCESS_DESTROY(pCxt);
	DRIVER_SHARED_RESOURCE_ACCESS_DESTROY(pCxt);
	
	pDCxt->driver_up = A_FALSE;
	return A_OK;
}

/*****************************************************************************/
/*  Driver_GetTargetInfo - Used to get the device target info which includes
 *	 the target version and target type. the results are stored in the 
 *	 context.
 * 		A_VOID *pCxt - the driver context.    
 *****************************************************************************/
A_STATUS
Driver_GetTargetInfo(A_VOID *pCxt)
{
	A_STATUS status = A_OK;
	A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	struct bmi_target_info targ_info;

   	if((ar4XXX_boot_param & AR4XXX_PARAM_MODE_MASK) == AR4XXX_PARAM_MODE_BMI){
    	status = BMIGetTargetInfo(pCxt, &targ_info);
    	pDCxt->targetVersion = targ_info.target_ver;
    	pDCxt->targetType = targ_info.target_type;
    } else {    
		/* hardcoded for boottime speedup */
		pDCxt->targetVersion = ATH_FIRMWARE_TARGET;
		pDCxt->targetType = TARGET_TYPE;
    }	
    
    return status;
}



/*****************************************************************************/
/*  Driver_TargReset - This function performs a warm reset operation on the
 *   wifi CPU after setting up firmware so that it can discover the 
 *   nvram config structure.   
 * 		A_VOID *pCxt - the driver context.    
 *****************************************************************************/
static A_UINT16
Driver_TargReset(A_VOID *pCxt)
{    
#if ATH_FIRMWARE_TARGET == TARGET_AR4100_REV2
    volatile A_UINT32 param;
            
	if((ar4XXX_boot_param & AR4XXX_PARAM_MODE_MASK) == AR4XXX_PARAM_MODE_NORMAL){
	    do{
	        if (Driver_ReadDataDiag(pCxt, 								
	                TARG_VTOP(AR4100_NVRAM_SAMPLE_ADDR),																		
	                                 (A_UCHAR *)&param,
	                                 4) != A_OK)	
	        {	         	         	        
	             A_ASSERT(0);
	        }
	    }while(param != A_CPU2LE32(AR4100_NVRAM_SAMPLE_VAL));

	    if (Driver_ReadDataDiag(pCxt, 								
	            TARG_VTOP(AR4100_CONFIGFOUND_ADDR),																		
	                             (A_UCHAR *)&param,
	                             4) != A_OK)	
	    {	         	         	        
	         A_ASSERT(0);
	    }

	    if(param == A_CPU2LE32(AR4100_CONFIGFOUND_VAL)){
	        /* force config_found value */
	        param = 0xffffffff;        
	        if (Driver_WriteDataDiag(pCxt, 
	                        TARG_VTOP(AR4100_CONFIGFOUND_ADDR),
	                         (A_UCHAR *)&param,
	                         4) != A_OK)	
	        {         
	             A_ASSERT(0);
	        }                  
	        /* instruct ROM to preserve nvram values */       
	        param = A_CPU2LE32(HI_RESET_FLAG_PRESERVE_NVRAM_STATE);
	        if (Driver_WriteDataDiag(pCxt, 
	                        TARG_VTOP(HOST_INTEREST_ITEM_ADDRESS(hi_reset_flag)),
	                         (A_UCHAR *)&param,
	                         4) != A_OK)	
	        {         
	             A_ASSERT(0);
	        } 
	        /* instruct ROM to parse hi_reset_flag */
	        param = A_CPU2LE32(HI_RESET_FLAG_IS_VALID);
	        if (Driver_WriteDataDiag(pCxt, 
	                        TARG_VTOP(HOST_INTEREST_ITEM_ADDRESS(hi_reset_flag_valid)),
	                         (A_UCHAR *)&param,
	                         4) != A_OK)	
	        {         
	             A_ASSERT(0);
	        } 
	        /* clear hi_refclk_hz to identify reset point */
	        param = A_CPU2LE32(0x00);
	        if (Driver_WriteDataDiag(pCxt, 
	                        TARG_VTOP(HOST_INTEREST_ITEM_ADDRESS(hi_refclk_hz)),
	                         (A_UCHAR *)&param,
	                         4) != A_OK)	
	        {         
	             A_ASSERT(0);
	        } 
	        /* reset CPU */            
	        param = A_CPU2LE32(WLAN_RESET_CONTROL_WARM_RST_MASK);
	        if (Driver_WriteDataDiag(pCxt, 
	                        TARG_VTOP(RTC_BASE_ADDRESS|RESET_CONTROL_ADDRESS),
	                         (A_UCHAR *)&param,
	                         4) != A_OK)	
	        {         
	             A_ASSERT(0);
	        }   
	        /* force delay before further chip access after reset */
	        CUSTOM_HW_USEC_DELAY(pCxt, 1000); 
	               
	        return 1;
	    }
	}
#else
    A_UNUSED_ARGUMENT(pCxt);   
#endif    
    return 0;
}

/*****************************************************************************/
/*  Driver_BootComm - Communicates with the device using the diagnostic
 * 	 window to perform various interactions during the boot process.
 * 		A_VOID *pCxt - the driver context.    
 *****************************************************************************/
A_STATUS 
Driver_BootComm(A_VOID *pCxt)
{
	volatile A_UINT32 param;	
    A_UINT8 resetpass=0;    

	if(ath_custom_init.Driver_BootComm != NULL){
		return ath_custom_init.Driver_BootComm(pCxt);
	}

#if (ATH_FIRMWARE_TARGET == TARGET_AR4100_REV2 || ATH_FIRMWARE_TARGET == TARGET_AR400X_REV1)
	/* interact with host_proxy via HOST_INTEREST to control BMI active */	
	
    do{  
        Api_BootProfile(pCxt, 0x02);        

        do{	
            if (Driver_ReadDataDiag(pCxt, 								
                    TARG_VTOP(HOST_INTEREST_ITEM_ADDRESS(hi_refclk_hz)),																		
                                     (A_UCHAR *)&param,
                                     4) != A_OK)	
            {	         	         	        
                 A_ASSERT(0);
            }	   
        }while(param != A_CPU2LE32(EXPECTED_REF_CLK_AR4100) && 
               param != A_CPU2LE32(EXPECTED_REF_CLK_AR400X));  
        
        Api_BootProfile(pCxt, 0x01);
	
	
#if 0 //Enabling firmware UART print and setting baud rate to 115200
        param = A_CPU2LE32(1);
        if (Driver_WriteDataDiag(pCxt, 
                                TARG_VTOP(HOST_INTEREST_ITEM_ADDRESS(hi_serial_enable)),
                                 (A_UCHAR *)&param,
                                 4) != A_OK)	
        {         
             A_ASSERT(0);
        }
        
        param = A_CPU2LE32(115200);
        if (Driver_WriteDataDiag(pCxt, 
                                TARG_VTOP(HOST_INTEREST_ITEM_ADDRESS(hi_desired_baud_rate)),
                                 (A_UCHAR *)&param,
                                 4) != A_OK)	
        {         
             A_ASSERT(0);
        }
#endif
#if 0//(ATH_FIRMWARE_TARGET == TARGET_AR400X_REV1)  //Todo to enable lpl for KF
       	param = A_CPU2LE32(1);
        if (Driver_WriteDataDiag(pCxt,
                                TARG_VTOP(HOST_INTEREST_ITEM_ADDRESS(hi_pwr_save_flags)),
                                 (A_UCHAR *)&param,
                                 4) != A_OK)
        {
             A_ASSERT(0);
        }
#endif
        
        /* this code is called prior to BMIGetTargInfo so we must assume AR6003 */
        param = A_CPU2LE32(ar4XXX_boot_param | ar4xx_reg_domain);
            
        if (Driver_WriteDataDiag(pCxt, 
                                TARG_VTOP(HOST_INTEREST_ITEM_ADDRESS(hi_flash_is_present)),
                                 (A_UCHAR *)&param,
                                 4) != A_OK)	
        {         
             A_ASSERT(0);
        }  
        
        Api_BootProfile(pCxt, 0x02);       
    }while(resetpass++ < 1 && Driver_TargReset(pCxt));
    
#else
	A_UNUSED_ARGUMENT(param);
	A_UNUSED_ARGUMENT(loop);     
#endif   
    return A_OK;
}
