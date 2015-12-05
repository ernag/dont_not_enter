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
#include <mqx.h>
#include <bsp.h>
#include <custom_api.h>
#include <common_api.h>
#include <netbuf.h>

#include "enet.h"
#include "enetprv.h"
#include "enet_wifi.h"
#include "atheros_wifi_api.h"
#include "atheros_wifi_internal.h"


ATH_CUSTOM_INIT_T ath_custom_init = 
{
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,	
	NULL,
	NULL,
	A_FALSE,
	A_FALSE,
};

ATH_CUSTOM_MEDIACTL_T ath_custom_mediactl = 
{
	NULL
};

ATH_CUSTOM_HTC_T ath_custom_htc = 
{
	NULL,
	NULL
};

volatile _task_id atheros_wifi_task_id = MQX_NULL_TASK_ID;

A_UINT32 g_totAlloc = 0;
A_UINT32 g_poolid = 0xffffffff;

static atheros_assert_cbk_t g_ath_assert_cbk = NULL;

#if DRIVER_CONFIG_MULTI_TASKING	

A_STATUS 
Custom_Driver_WaitForCondition(A_VOID *pCxt, volatile A_BOOL *pCond, A_BOOL value, A_UINT32 msec)
{                                                                      
	A_UINT32 ret = (msec)*BSP_ALARM_FREQUENCY/1000;   
	A_STATUS status= A_OK;                
	ret = (ret == 0)? 1:ret;                                  
	
	while(1){														
		if ((*pCond != value)){                             
		    if(MQX_OK != _lwevent_wait_ticks(&GET_DRIVER_CXT(pCxt)->userWakeEvent, 0x01, TRUE, ret)){  
		    	status = A_ERROR;
		        break;                                       
		    }else{                                                      
		        _lwevent_clear(&GET_DRIVER_CXT(pCxt)->userWakeEvent, 0x01);                            
		    }                  
                                            
		}else{  			                                                        
			break;														
		}																
	}							
										
	return status;                                                          
}   

static A_VOID 
Atheros_Driver_Task(A_UINT32 pContext)
{    
	A_VOID *pCxt = (A_VOID *)pContext;
	A_BOOL canBlock = A_FALSE;
	A_UINT16 block_msec;
	A_UINT32 timeout;
	
    do
	{			
		if(A_OK == Driver_Init(pCxt)){
		
			while (1) {    
				if( canBlock ){        	        
					GET_DRIVER_COMMON(pCxt)->driverSleep = A_TRUE;		
					timeout = (block_msec)*BSP_ALARM_FREQUENCY/1000;
					
					if(block_msec && timeout == 0){
						timeout = 1;
					}			
					
					if(MQX_OK != _lwevent_wait_ticks(&(GET_DRIVER_CXT(pCxt)->driverWakeEvent), 0x01, TRUE, timeout)){
						if(timeout == 0){
							break;
						}
					}
				}               
			
				GET_DRIVER_COMMON(pCxt)->driverSleep = A_FALSE;

				if(GET_DRIVER_CXT(pCxt)->driverShutdown == A_TRUE){
					break;
				}

				Driver_Main(pCxt, DRIVER_SCOPE_RX|DRIVER_SCOPE_TX, &canBlock, &block_msec);
			}
		
			Driver_DeInit(pCxt);	            
	        CUSTOM_DRIVER_WAKE_USER(pCxt);
		}
#if MQX_TASK_DESTRUCTION		
	}while(0);

    atheros_wifi_task_id = MQX_NULL_TASK_ID;
#else
		/* block on this event until a task wants to re-activate the driver thread */
		_task_block();
	}while(1);
#endif	

	
}



#define ATHEROS_TASK_PRIORITY 0
#define ATHEROS_TASK_STACKSIZE (2000)

A_STATUS 
Custom_Driver_CreateThread(A_VOID *pCxt)
{

	TASK_TEMPLATE_STRUCT task_template;
	A_STATUS status = A_ERROR; 
	//Create task 
	A_MEMZERO((uchar_ptr)&task_template, sizeof(task_template));
    task_template.TASK_NAME          = "Atheros_Wifi_Task"; 
    task_template.TASK_PRIORITY      = ATHEROS_TASK_PRIORITY;
    task_template.TASK_STACKSIZE     = ATHEROS_TASK_STACKSIZE;
    task_template.TASK_ADDRESS       = Atheros_Driver_Task;
    task_template.CREATION_PARAMETER = (uint_32)pCxt;
    /* if atheros_wifi_task_id != MQX_NULL_TASK_ID then it implies that the 
     * driver was already started and this call is to restart the driver 
     * from a previous call to DestroyDriverThread. in this case instead
     * of creating the task the code will just call _task_ready().
     */
    if(atheros_wifi_task_id == MQX_NULL_TASK_ID){
    	atheros_wifi_task_id = _task_create_blocked(0,0,(uint_32)&task_template);
    }
    
    do{
	    if(atheros_wifi_task_id == MQX_NULL_TASK_ID) {	                
	        break;            
	    }
	    //create event on which to wait for atheros task to init
	    
	    //start the atheros task
	    _task_ready(_task_get_td(atheros_wifi_task_id));
		//wait on completion or timeout
		
		status = A_OK;
	}while(FALSE);
	
	return status;
}

//Compliments CreateDriverThread
A_STATUS 
Custom_Driver_DestroyThread(A_VOID *pCxt)
{
	GET_DRIVER_CXT(pCxt)->driverShutdown = A_TRUE;
	CUSTOM_DRIVER_WAKE_DRIVER(pCxt);
	return A_OK;
}

#else /* DRIVER_CONFIG_MULTI_TASKING */

A_STATUS 
Custom_Driver_WaitForCondition(A_VOID *pCxt, volatile A_BOOL *pCond, A_BOOL value, A_UINT32 msec)
{      
	MQX_TICK_STRUCT start, current;                                                                
	A_STATUS status= A_OK;                
    A_UINT32 temp;               	
	_time_get_ticks(&start);
	
	while(1){														
		if ((*pCond != value)){                             
		    	Driver_Main(pCxt, DRIVER_SCOPE_RX|DRIVER_SCOPE_TX, NULL);
		    	_time_get_ticks(&current);
		    	if(_time_diff_milliseconds(&current,&start, &temp) >= msec){
		    		status = A_ERROR;
		    		break;
		    	}
		}else{  			                                                        
			break;														
		}																
	}							
										
	return status;                                                          
}   

#endif /* DRIVER_CONFIG_MULTI_TASKING */

static A_VOID 
Custom_FreeRxRequest(A_NATIVE_NETBUF *native_ptr)
{
    A_NETBUF* a_netbuf_ptr = (A_NETBUF*)native_ptr;
}


/*****************************************************************************/
/*  Custom_GetRxRequest - Attempts to get an RX Buffer + RX Request object 
 *	 for a pending RX packet.  If an Rx Request cannot be acquired then this
 *	 function calls Driver_ReportRxBuffStatus(pCxt, A_FALSE) to prevent the 
 *	 driver from repeating the request until a buffer becomes available. 
 *      A_VOID *pCxt - the driver context.
 *		A_UINT16 length - length of request in bytes.
 *****************************************************************************/
A_VOID *
Custom_GetRxRequest(A_VOID *pCxt, A_UINT16 length)
{    	
    A_VOID *pReq;
    
	do{						    	    									    	    	
	    RXBUFFER_ACCESS_ACQUIRE(pCxt);
        {
#if DRIVER_CONFIG_IMPLEMENT_RX_FREE_QUEUE        
	        pReq = A_NETBUF_DEQUEUE(&(GET_DRIVER_CXT(pCxt)->rxFreeQueue));	    		    		    	
    	    
		    if(A_NETBUF_QUEUE_EMPTY(&(GET_DRIVER_CXT(pCxt)->rxFreeQueue))){
			    Driver_ReportRxBuffStatus(pCxt, A_FALSE);
		    }
#else
			pReq = A_NETBUF_ALLOC(length);
			if(pReq == NULL){
				Driver_ReportRxBuffStatus(pCxt, A_FALSE);	
			}
#endif		    
        }		
	    RXBUFFER_ACCESS_RELEASE(pCxt); 
	    
	    if(pReq != NULL) {
	    	/* init pcb */
    		A_NETBUF_INIT(pReq, (A_VOID*)Custom_FreeRxRequest, pCxt);	     	
    		A_ASSERT(length <= A_NETBUF_TAILROOM(pReq)); 	    	
    	}else{
    		//should never happen thanks to HW_ReportRxBuffStatus
	    	A_ASSERT(0);	    	
    	}    	    		    	   	    
    }while(0);
    
    return pReq;        
}

#if DRIVER_CONFIG_MULTI_TASKING	
A_VOID 
Custom_Driver_WakeDriver(A_VOID *pCxt)
{
	_lwevent_set(&(GET_DRIVER_CXT(pCxt)->driverWakeEvent), 0x01);
}

A_VOID
Custom_Driver_WakeUser(A_VOID *pCxt)
{
	_lwevent_set(&(GET_DRIVER_CXT(pCxt)->userWakeEvent), 0x01);
}
#endif

/*****************************************************************************/
/*  Custom_Driver_ContextInit - Allocates and initializes driver context 
 *	 elements including pre-allocated rx buffers. This function will call
 *	 A_ASSERT for any failure. Look to Custom_Driver_ContextDeInit to undo
 *	 any allocations performed by this function.
 *      A_VOID *pCxt - the driver context.
 *****************************************************************************/
A_STATUS 
Custom_Driver_ContextInit(A_VOID *pCxt)
{	
	ENET_CONTEXT_STRUCT_PTR enet_ptr = (ENET_CONTEXT_STRUCT_PTR)GET_DRIVER_CXT(pCxt)->pUpperCxt;
	A_VOID *pReq;
    A_UINT32 tempStorageLen = 0;
     A_DRIVER_CONTEXT* pDCxt = GET_DRIVER_COMMON(pCxt);
    
    /*Allocate the temporary storage buffer. This may be shared by multiple modules. 
      If store recall is enabled, it may use this buffer for storing target data.
      Will also be shared by scan module to store scan results*/
    tempStorageLen = max((STORE_RECALL_BUF_SIZE), (ATH_MAX_SCAN_BUFFERS*sizeof(ENET_SCAN_INFO)));
    tempStorageLen = max(tempStorageLen, (ATH_MAX_SCAN_BUFFERS*sizeof(ATH_SCAN_EXT)));
    
    if(tempStorageLen)
    {
       if(NULL == (pDCxt->tempStorage = A_MALLOC(tempStorageLen,0))){        
         return A_NO_MEMORY;
       }
       pDCxt->tempStorageLength = tempStorageLen;
    }
    else
    {
       pDCxt->tempStorage = NULL;
       pDCxt->tempStorageLength = 0;
    }
    
	/* Prepare buffer for the scan results, reuse tempStorage. */
    GET_DRIVER_CXT(pCxt)->pScanOut =  pDCxt->tempStorage;
    A_ASSERT(GET_DRIVER_CXT(pCxt)->pScanOut != NULL);
    GET_DRIVER_CXT(pCxt)->securityType = ENET_MEDIACTL_SECURITY_TYPE_NONE;  
#if DRIVER_CONFIG_IMPLEMENT_RX_FREE_QUEUE     
	{  
		A_UINT16 numRxBuffers, i;
		A_VOID *pReq;	  
		/* allocate rx objects and buffers */
		A_NETBUF_QUEUE_INIT(&GET_DRIVER_CXT(pCxt)->rxFreeQueue);
		/* NUM_RX_PCBS should be == BSP_CONFIG_ATHEROS_PCB which
		 * is defined in atheros_wifi.h
		 */	 
		numRxBuffers = enet_ptr->PARAM_PTR->NUM_RX_PCBS;
		//pre-allocate rx buffers
		for(i=0 ; i< numRxBuffers; i++){
		
			pReq = A_NETBUF_ALLOC_RAW(AR4100_MAX_RX_MESSAGE_SIZE);
	    		
			A_ASSERT(pReq != NULL);  
			/*Set rx pool ID so that the buffer is freed to the RX pool*/		
			A_NETBUF_SET_RX_POOL(pReq);
	    	/* mqx allows us to set a memtype to help identify buffers */	
	    	_mem_set_type(pReq, MEM_TYPE_ATHEROS_PERSIST_RX_PCB);
		    A_NETBUF_ENQUEUE(&GET_DRIVER_CXT(pCxt)->rxFreeQueue, pReq);
		    
	    }
	}
#endif    

    /* init the common->rxBufferStatus to true to indicate that rx buffers are
     * ready for use. */
    pDCxt->rxBufferStatus = A_TRUE;
    GET_DRIVER_CXT(pCxt)->driverShutdown = A_FALSE;
    
    return A_OK;
}

/*****************************************************************************/
/*  Custom_Driver_ContextDeInit - Frees any allocations performed by 
 *	 Custom_Driver_ContextInit.
 *      A_VOID *pCxt - the driver context.
 *****************************************************************************/
A_VOID 
Custom_Driver_ContextDeInit(A_VOID *pCxt)
{
	A_VOID *pReq;
	
	while(GET_DRIVER_COMMON(pCxt)->rxBufferStatus == A_TRUE){
		pReq = Custom_GetRxRequest(pCxt, 10);
		/* call default free function */
		default_native_free_fn((PCB_PTR)pReq);	
	}
	
	if(GET_DRIVER_COMMON(pCxt)->tempStorage != NULL){
		A_FREE(GET_DRIVER_COMMON(pCxt)->tempStorage,0);
		GET_DRIVER_COMMON(pCxt)->tempStorage = NULL;
	}
}

/*
 *   Register the callback to call in the event that the Atheros driver asserts.
 */
void ATHEROS_register_assert_cbk( atheros_assert_cbk_t pCbk )
{
    g_ath_assert_cbk = pCbk;
}

/*
 *   This function gets called upon an A_ASSERT().
 */
void assert_func(A_UINT32 line, char const *pFileName)
{
#ifdef COR_174_WORKAROUND
    uint_32 read_value;
#endif
    
    /*
     *   Check to see if the user registered a WIFI assert callback.
     */
    if( NULL != g_ath_assert_cbk )
    {
        char assert_info[50];
        
        sprintf(assert_info, "WIFI ATH assert\t%s\tL %d\0", pFileName, line);
        g_ath_assert_cbk(assert_info);
        return;
    }
    
    printf("driver assert at file %s line: %d\n", pFileName, line);
    
#ifdef COR_174_WORKAROUND
    read_value = SCB_AIRCR;
    printf("\n*** REBOOTING ***\n", line); fflush(stdout);
    _time_delay(1000);
    // Issue a Kinetis Software System Reset
    read_value &= ~SCB_AIRCR_VECTKEY_MASK;
    read_value |= SCB_AIRCR_VECTKEY(0x05FA);
    read_value |= SCB_AIRCR_SYSRESETREQ_MASK;
    wint_disable();
    SCB_AIRCR = read_value;
#endif

    while(1)
    {
        _time_delay(5000);  // don't hog a bunch of CPU resources on this.
    }
}
