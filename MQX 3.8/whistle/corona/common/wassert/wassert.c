/*
 * wassert.c
 * 
 * Custom Whistle (w)assert.
 *
 *  Created on: Jul 8, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include "wassert.h"
#include "corona_console.h"
#include "pwr_mgr.h"
#include "sherlock.h"
#include "corona_ext_flash_layout.h"
#include "wmp_datadump_encode.h"
#include "persist_var.h"
#include "datamessage.pb.h"
#include "pmem.h"
#include "time_util.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define  WASSERT_REBOOT_DELAY   ( 10 * 1000 )

typedef enum non_graceful_type
{
    NON_GRACEFUL_IS_REBOOTING      = (uint8_t) 1,
    NON_GRACEFUL_RECURSION         = (uint8_t) 2,
    NON_GRACEFUL_ONE_SHOT_FAILED   = (uint8_t) 3,
    NON_GRACEFUL_DEVICE_NOT_BOOTED = (uint8_t) 4,
    NON_GRACEFUL_FAKE_JUST_A_TEST  = (uint8_t) 5,
    NON_GRACEFUL_ALLOC_FAILED      = (uint8_t) 6
    
} non_graceful_t;

////////////////////////////////////////////////////////////////////////////////
// Global Variables
////////////////////////////////////////////////////////////////////////////////
uint8_t g_device_has_booted = 0;

////////////////////////////////////////////////////////////////////////////////
// Externs
////////////////////////////////////////////////////////////////////////////////
extern int wson_read(uint32_t address, uint8_t *pData, uint32_t numBytes);
extern int wson_write(uint32_t      address,
                      uint8_t       *pData,
                      uint32_t      numBytes,
                      boolean       eraseSector);

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   This just sets the "device booted" flag, so we know how full-fledged the wassert() should be.
 */
void wassert_init(void)
{
    /*
     *   This flag tells wassert() that it is safe to use PWRMGR and corona console,
     *       otherwise, just reset no matter what.
     */
    g_device_has_booted = 1;
}

/*
 *   Reboot.
 */
void wassert_reboot(void)
{
	PWRMGR_reset(FALSE);
}

/*
 *   Function that records information for non-graceful wassert(0)'s.
 */
static void _wassert_non_graceful( non_graceful_t reason, uint32_t LR, int line )
{
	boolean thread_context = ( 0 == _int_get_isr_depth() );
	
    wint_disable();
    _task_stop_preemption();
    
    __SAVED_ACROSS_BOOT.non_graceful_assert_reason = (uint8_t) reason;
    
    // Overload the ARM exception link register value for this purpose.
    __SAVED_ACROSS_BOOT.arm_exception_LR = LR;
    
    // Overload the ARM exception's task id field for this purpose.
    __SAVED_ACROSS_BOOT.arm_exception_task_id = _task_get_id();
    
    // Overload the ARM exception's task address to be line number for non-graceful asserts.
    __SAVED_ACROSS_BOOT.arm_exception_task_addr = (uint32_t)line;
    
    // The following line will set the new checksum for the 
    persist_set_untimely_crumb(BreadCrumbEvent_NON_GRACEFUL_ASSERT);
    
    wint_enable();
    _task_start_preemption();
    
    if (thread_context)
    {
    	wassert_reboot();
    	_task_block();
    }
}

/*
 *   Custom Whistle (w)assert function.  
 *   
 *   - log data to console (if we can).
 *   - log data to EVTMGR (if we can).
 *   - reboot gracefully (if we can).
 */
void _whistle_assertion_failed(char const *pFileName, char const *pFunctionName, int lineNumber)
{
    static uint8_t recursion = 0;
    uint32_t LR_u32 = 0;
    boolean thread_context  = ( 0 == _int_get_isr_depth() );
    uint32_t status;
    
    /*
     *   This will save the ~ address in Internal FLASH of the code that failed the assert.
     */
    __asm
    {
        mov LR_u32, LR
    }
    
    recursion++;
    
    if( recursion > 2 )
    {
    	/*
    	 *   Extra safety valve to bypass infinite recursion case.
    	 */
    	wassert_reboot();
    }
    
//#define TEST_NON_GRACEFUL
#ifdef  TEST_NON_GRACEFUL
    _wassert_non_graceful( NON_GRACEFUL_FAKE_JUST_A_TEST, LR_u32, lineNumber );
#endif
    
    if ( PWRMGR_is_rebooting() )
    {
        _wassert_non_graceful( NON_GRACEFUL_IS_REBOOTING, LR_u32, lineNumber );
        
        if (!thread_context)
        {
        	/* If we're an ISR, just bow out and let the previous reboot run its course */
        	set_debug_led_code(15);
        	return;
        }
    }
    
    if( recursion > 1 )
    {
        /*
         *   Oops, someone below may have called this and caused recursion, delay, then reboot.
         */
        _wassert_non_graceful( NON_GRACEFUL_RECURSION, LR_u32, lineNumber );
        
        if (!thread_context)
        {
        	/* If we're an ISR, just bow out and let the previous reboot run its course */
        	set_debug_led_code(15);
        	return;
        }
    }
    
	/*
	 * Start the hardware watchdog timer as a fail-safe reboot. If the assert code works,
	 * this will never fire.
	 */
	PWRMGR_watchdog_arm(20);
	
    /*
     *   First, we want to have a mechanism in place that will reboot if a certain amount of time goes by.
     *   This will help ensure that if the graceful reboot somehow fails, we'll be able to snap out of it and reboot.
     */
    if( 0 == UTIL_time_start_oneshot_ms_timer( WASSERT_REBOOT_DELAY, wassert_reboot ) )
    {
        /*
         *   Problem with oneshot, so just reboot immediately.
         */
        _wassert_non_graceful( NON_GRACEFUL_ONE_SHOT_FAILED, LR_u32, lineNumber );
        
        /* OK to continue and not bow out and return if we're in an ISR */
        if (!thread_context)
        {
        	/* If we're an ISR, just bow out and let the previous reboot run its course */
        	set_debug_led_code(15);
        }

    }
    
    if( g_device_has_booted )
    {
        /*
         *     Try to wassert as gracefully as possible, with nice prints and a "proper" reboot.
         */

    	if (thread_context)
    	{
    		char *buf;  // dunk it on the stack, don't risk getty fancy here with lw_malloc().
    		shlk_handle_t hdl;

    		buf = _lwmem_alloc_zero(256);

    		if( NULL != buf )
    		{
    			/*
    			 *   Log the error to the event manager.
    			 */
    			status = WMP_post_fw_info( ERR_WASSERT, 
    					"ASSERT",
    					pFileName,
    					pFunctionName,
    					(uint32_t)lineNumber);

    			/*
    			 *   Log error the sherlock.
    			 */
    			SHLK_init(  &hdl,
                            wson_read,
                            wson_write,
                            shlk_printf,
                            EXT_FLASH_SHLK_ASSDUMP_ADDR,
                            EXT_FLASH_SHLK_LOG_ADDR,
                            EXT_FLASH_SHLK_LOG_NUM_SECTORS );

    			sprintf(buf, "\n\t* wassert %s %s L%i 0x%08x *\n\n\0", (pFileName?pFileName:":"), 
    					(pFunctionName?pFunctionName:":"), 
    					lineNumber, status);

    			SHLK_asslog(&hdl, buf);   // log message permanently to SPI Flash.

    			/*
    			 *   Print assert to console.
    			 */
    			corona_print("%s", buf); // print msg to screen too if we can.
    			_lwmem_free(buf);
    		}
    		else
    		{
    			_wassert_non_graceful( NON_GRACEFUL_ALLOC_FAILED, LR_u32, lineNumber );
    		}
    	}
    	else
    	{
        	set_debug_led_code(15);
    	}    	
        
        /*
         *   Perform a proper reboot.
         */
        PWRMGR_Reboot( 500, PV_REBOOT_REASON_WHISTLE_ASSERT, thread_context );
    }
    else
    {
#if 0
        /*
         *   Too early to use typical Corona graceful shutdown methods.
         *     Instead just reset immediately.
         */
        wassert_reboot();
#else
        _wassert_non_graceful( NON_GRACEFUL_DEVICE_NOT_BOOTED, LR_u32, lineNumber );
        
        if (!thread_context)
        {
        	set_debug_led_code(15);
        	PWRMGR_reset(FALSE);
        }
#endif
    }
}
