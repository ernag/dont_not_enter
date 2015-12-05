/*
 * main.c
 *
 *  Created on: Mar 6, 2013
 *      Author: Chris
 */

/*
 ** MQX initialization information
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <timer.h>
#include <watchdog.h>
#include <klog.h>
#include "app_errors.h"
#include "ar4100p_main.h"
#include "button.h"
#include "virtual_com.h"
#include "corona_console.h"
#include "hardware_tests.h"
#include "unit_tests.h"
#include "cfg_mgr.h"
#include "sys_event.h"
#include "con_mgr.h"
#include "evt_mgr.h"
#include "acc_mgr.h"
#include "pwr_mgr.h"
#include "dis_mgr.h"
#include "fwu_mgr.h"
#include "min_act.h"
#include "pmem.h"
#include "persist_var.h"
#include "crc_util.h"
#include "bt_mgr.h"
#include "btn_mgr.h"
#include "per_mgr.h"
#include "rtcw_mgr.h"
#include "prx_mgr.h"
#include "user_config.h"
#include "fw_info.h"
#include "rf_switch.h"
#include "transport_mutex.h"
#include "main.h"
#include "wassert.h"
#include "wmp_datadump_encode.h"
#include "accdrv_spi_dma.h"
#include "factory_mgr.h"
#include "adc_driver.h"
#include "rtc_alarm.h"

#include "shaid.h"

#ifdef PROTO2
#error "PROTO2 config is OLD! Are you sure?"
#endif

#if !DRIVER_CONFIG_INCLUDE_BMI
////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Private Declarations
////////////////////////////////////////////////////////////////////////////////
static void WSTL_tsk(uint_32 dummy);
static void enable_kernel_logging(void);
static void handle_watchdog_expiry(pointer);
static void unexp_int_app_cbk(uint_32 task_id, uint_32 vector, uint_32 pc, uint_32 lr, uint_32 exp_frame[8], void * tsk_addr);

////////////////////////////////////////////////////////////////////////////////
// Externs
////////////////////////////////////////////////////////////////////////////////
extern void BTSampleAppTask(uint_32 initial_data); // TODO: Remove Me Later
extern void _install_unexpected_isr_app_cbk(void *pCbk);
extern void _persist_log_untimely_crumb( uint32_t crumb_mask );  // only main() is allowed to use this function externally.

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

// extra stack space when pwr breadcrumb enabled
#if PWRMGR_BREADCRUMB_ENABLED
#define P 256
#else
#define P 0
#endif

persistent_data_t *g_pPersist = &__SAVED_ACROSS_BOOT;  //useful for debugging persist.
persist_pending_action_t g_last_pending_action = PV_PENDING_NO_ACTION;

const TASK_TEMPLATE_STRUCT MQX_template_list[] =
{

/* 
 *         TASK INDEX                      TASK PTR       SIZE PRIORITY  NAME          ATTRIBUTE     PARAM  TIME SLICE
 */
    { TEMPLATE_IDX_MAIN,             WSTL_tsk,     2048+P, 1, MAIN_TASK_NAME,       MQX_AUTO_START_TASK, 0, 0 },
    { TEMPLATE_IDX_CORONA_CONSOLE,   CNSL_tsk,     2048+P,10, CONSOLE_TASK_NAME,    MQX_AUTO_START_TASK, 0, 0 },
    { TEMPLATE_IDX_BUTTON_DRV,       BTNDRV_tsk,   1280+P, 2, BUTTON_TASK_NAME,     0,                   0, 0 },
    { TEMPLATE_IDX_SYS_EVENT,        SYSEVT_tsk,   1280+P, 6, SYSEVENT_TASK_NAME,   0,                   0, 0 },
#ifdef USB_ENABLED
    { TEMPLATE_IDX_USBCDC,           USBCDC_Task,         768+P,10, CDC_TASK_NAME,        MQX_AUTO_START_TASK, 0, 0 },
#endif // USB_ENABLED
    { TEMPLATE_IDX_BTNMGR,           BTNMGR_tsk,   2048+P, 9, BTNMGR_TASK_NAME,     0,                   0, 0 },
    { TEMPLATE_IDX_BTMGR_DISC,       BTDISC_tsk,   1024+P, 8, BT_TASK_NAME,         0,                   0, 0 },
    { TEMPLATE_IDX_CONMGR,           CON_tsk,      2048+P, 5, CONMGR_TASK_NAME,     0,                   0, 0 },
    { TEMPLATE_IDX_EVTMGR_POST,      EVTPST_tsk,   1500+P, 5, POST_TASK_NAME,       0,                   0, 0 },
    { TEMPLATE_IDX_EVTMGR_DUMP,      EVTDMP_tsk,   1536+P, 5, DUMP_TASK_NAME,       0,                   0, 0 },
    { TEMPLATE_IDX_EVTMGR_UPLOAD,    EVTUPLD_tsk,  1792+P, 5, UPLOAD_TASK_NAME,     0,                   0, 0 },
    { TEMPLATE_IDX_ACCMGR,           ACC_tsk,      1536+P, 4, ACC_TASK_NAME,        0,                   0, 0 },
    { TEMPLATE_IDX_ACCMGR_OFFLOAD,   ACCOFFLD_tsk, 1536+P,10, ACCOFF_TASK_NAME,     0,                   0, 0 },
    { TEMPLATE_IDX_DISMGR,           DIS_tsk,      1536+P,10, DISPLAY_TASK_NAME,    0,                   0, 0 },
    { TEMPLATE_IDX_PWRMGR,           PWR_tsk,      1280+P, 9, PWR_TASK_NAME,        0,                   0, 0 },
    { TEMPLATE_IDX_PERMGR,           PER_tsk,      2048+P, 9, PER_TASK_NAME,        0,                   0, 0 },
    { TEMPLATE_IDX_RTCWMGR,          RTCW_tsk,     768+P,  3, RTCW_TASK_NAME,        0,                   0, 0 },
    { TEMPLATE_IDX_SHERLOCK,         SHLK_tsk,     2048+P,11, SHLK_TASK_NAME,       0,                   0, 0 },
    { TEMPLATE_IDX_FWUMGR,           FWU_tsk,      4096+P,15, FWU_TASK_NAME,        0,                   0, 0 },
    { TEMPLATE_IDX_BURNIN,           BI_tsk,       4096+P, 8, BURNIN_TASK_NAME,     0,                   0, 0 },
    { TEMPLATE_IDX_RTC_ALARM,        RTCA_tsk,     768+P, 3,  RTC_ALARM_TASK_NAME,  MQX_AUTO_START_TASK, 0, 0 },
    { 0 }
};

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

TASK_TEMPLATE_STRUCT *whistle_get_task_template_list(void)
{
	return (TASK_TEMPLATE_STRUCT *)MQX_template_list;
}

// MQX _watchdog component timeout handler. Called on interrupt context.
void handle_watchdog_expiry(pointer td_ptr)
{
	_mqx_uint reboot_thread;
	
	if (PWRMGR_is_rebooting())
	{
		return;
	}
	
	// Set the thread index ourselves since we are coming in on interrupt context
	reboot_thread = _task_get_index_from_id(_task_get_id_from_td(td_ptr));
	PMEM_VAR_SET(PV_REBOOT_THREAD, &reboot_thread);

	PWRMGR_Reboot(0, PV_REBOOT_REASON_WATCHDOG, FALSE);
}

/*
 *   This function will be called by the PSP if an exception interrupt (vector #3) OR unhandled interrupt (no default vector installed) occurs.
 *   NOTE:  We must do only bare bones stuff in this routine, no interrupt-dependency stuff!
 *   NOTE:  For exp_frame, [0]=R0, [1]=R1, [2]=R2, [3]=R3, [4]=R12, [5]=LR, [6]=PC, [7]=PSR
 */
static void unexp_int_app_cbk(uint_32 task_id, uint_32 vector, uint_32 pc, uint_32 lr, uint_32 exp_frame[8], void * tsk_addr)
{
    __SAVED_ACROSS_BOOT.reboot_reason = PV_REBOOT_REASON_UNHANDLED_INTERRUPT_EXCEPTION;
    __SAVED_ACROSS_BOOT.arm_exception_occured = EXCEPTION_MAGIC;
    __SAVED_ACROSS_BOOT.arm_exception_task_id = task_id;
    __SAVED_ACROSS_BOOT.arm_exception_task_addr = (uint_32)tsk_addr;
    __SAVED_ACROSS_BOOT.arm_interrupt_vector = vector;
    __SAVED_ACROSS_BOOT.arm_exception_PC = pc;
    __SAVED_ACROSS_BOOT.arm_exception_LR = lr;
    pmem_update_checksum();   // purposefully don't wait on mutex here.
    
    // Hard reboot without further delay.
    PWRMGR_reset( FALSE );
}

/*
 *   Check the previous boot's on power votes.  They should have been zero, but if 
 *      they are not, raise an error.
 */
static void prev_on_vote_chk(uint32_t prev_on_pwr_votes)
{
    if( 0 != prev_on_pwr_votes )
    {
        PROCESS_NINFO( ERR_PWRMGR_ON_VOTES_INVALID, "%x", prev_on_pwr_votes );
    }
}

/*
 *   See if we just recovered from an unhandled interrupt.
 */
static void unexp_int_chk(void)
{
    if( EXCEPTION_MAGIC == __SAVED_ACROSS_BOOT.arm_exception_occured )
    {   
        /*
         *   Make sure this gets processed in EVTMGR now, since we could not do
         *     it previously in the context of the exception.
         */
        if( 3 == __SAVED_ACROSS_BOOT.arm_interrupt_vector )
        {
            PROCESS_NINFO( ERR_ARM_EXCEPTION, "ARM EXCEPTION\tvect %u tsk %x tskAddr %x PC %x LR %x",
                                                      __SAVED_ACROSS_BOOT.arm_interrupt_vector,
                                                      __SAVED_ACROSS_BOOT.arm_exception_task_id,
                                                      __SAVED_ACROSS_BOOT.arm_exception_task_addr,
                                                      __SAVED_ACROSS_BOOT.arm_exception_PC,
                                                      __SAVED_ACROSS_BOOT.arm_exception_LR );
        }
        else
        {
            PROCESS_NINFO( ERR_UNHANDLED_INT_EXCEPTION, "UNHANDLED INT\tvect %u tsk %x tskAddr %x PC %x LR %x",
                                                      __SAVED_ACROSS_BOOT.arm_interrupt_vector,
                                                      __SAVED_ACROSS_BOOT.arm_exception_task_id,
                                                      __SAVED_ACROSS_BOOT.arm_exception_task_addr,
                                                      __SAVED_ACROSS_BOOT.arm_exception_PC,
                                                      __SAVED_ACROSS_BOOT.arm_exception_LR );
        }

        /*
         *   Clear the unhandled int status.
         */
        wint_disable();
        __SAVED_ACROSS_BOOT.arm_exception_occured = 0;
        pmem_update_checksum();
        wint_enable();
    }
}

/*
 *   Standard function for destroying a task.
 *   
 *   returns task ID on success.
 *   MQX_NULL_TASK_ID for non-success.
 */
_task_id whistle_task_destroy(const char *pTaskName)
{
    _task_id id;
    
    id = _task_get_id_from_name((char *)pTaskName);
    
    if( (MQX_NULL_TASK_ID != id) && (_task_get_id() != id) )
    {
        if( MQX_INVALID_TASK_ID == _task_destroy(id) )
        {
            id = MQX_NULL_TASK_ID;
        }
    }
    
    return id;
}

/*
 *   Standard function for creating a Whistle task, and checking for errors.
 */
_task_id whistle_task_create(wtemplate_idx_t idx)
{
    _task_id id;
    
    /*
     *   The task ID must fall within valid bounds.
     */
    wassert( idx < WHISTLE_NUM_TASKS );
    wassert( idx != 0 );   
    
    /*
     *   NULL task_id points to trouble, print the task error, and wassert.
     */
    id = _task_create( 0, idx, 0 );
    if( MQX_NULL_TASK_ID == id )
    {
        wassert( ERR_OK == PROCESS_NINFO( ERR_TASK_GENERIC, "tsk %i %x", idx, _task_get_error() ) );
    }
    
#if MQX_USE_LOGS
    _klog_enable_logging_task(id);
#endif
    
    return id;
}

/*
 *   Destroys all the Whistle tasks.
 */
void whistle_kill_all_tasks(void)
{
    wtemplate_idx_t idx;

    for(idx = 0; 0 != MQX_template_list[idx].TASK_TEMPLATE_INDEX; idx++)
    {
        whistle_task_destroy( MQX_template_list[idx].TASK_NAME );
    }
}

/*
 *   Main Whistle Task
 */
static void WSTL_tsk(uint_32 dummy)
{
    boolean  hard_boot = 0;
    cable_t  cable;
    uint32_t prev_on_pwr_votes = 0;
    uint8_t  go_to_ship_mode = 0;
    persist_pending_action_t pending_action_default;
    
    
    // GPIO initialisations need to occur early or the device will hang if the interrupt
    // happens before they init
    // Button GPIO
    button_init();
    
    /*
     *   Check to make sure pmem is valid before anyone else tries to use it 
     *     to validate memory that is corrupt.
     */
    hard_boot = ( !pmem_is_data_valid() || !RTC_IsInitialized() );
    
    if( !pmem_is_data_valid() )
    {
        persist_var_init();
        persist_set_untimely_crumb( BreadCrumbEvent_PMEM_RESET_CRC_INVALID );
    }
    else if( !persist_app_has_initialized() )
    {
        /*
         *   It is possible that boot2 "validated" pmem in order to set some flag(s),
         *   and since pmem has no knowledge (by design) of many of the app's pmem vars,
         *   it couldn't have known to init those vars. However, some vars are shared between boot2/app.
         *   
         *     As of 10/21/2013, the only var boot2 sets is the PV_FLAG_FO_TO_SHIP_MODE flag.
         *     If any others are added, this is the scope to handle them.
         */
        go_to_ship_mode = pmem_flag_get(PV_FLAG_GO_TO_SHIP_MODE);
        persist_var_init();
        persist_set_untimely_crumb( BreadCrumbEvent_PMEM_RESET_MAGIC_INVALID );
    }
    else
    {
        /*
         *   Grab the initial state of any variables now, before they get overwritten/changed.
         */
        prev_on_pwr_votes = __SAVED_ACROSS_BOOT.on_power_votes;
        go_to_ship_mode = pmem_flag_get(PV_FLAG_GO_TO_SHIP_MODE);
        
        /*
         *   We still need to re-initialize persist if the RTC is bogus, but
         *     for this case, we can grab the state of the vars first, since we've
         *     confirmed that pmem is valid above.
         */
        if( !RTC_IsInitialized() )
        {
            persist_var_init();
        }
    }
    
    g_last_pending_action = __SAVED_ACROSS_BOOT.pending_action;
    
    pending_action_default = PV_PENDING_NO_ACTION;
    PMEM_VAR_SET(PV_PENDING_ACTION, &pending_action_default);
    
    PWRMGR_VoteForAwake( PWRMGR_MAIN_TASK_VOTE );
    PWRMGR_ship_allowed_on_usb_conn( go_to_ship_mode );
    
    // Create the MQX timer component. If not created explicitly, MQX creates it the first time 
    // a timer is used. We need this component to be created before MQX timers are used.
    _timer_create_component(TIMER_DEFAULT_TASK_PRIORITY, TIMER_DEFAULT_STACK_SIZE);
    
    _watchdog_create_component(BSP_TIMER_INTERRUPT_VECTOR, handle_watchdog_expiry);
    
    _watchdog_start(60*1000);
    
    /*
     *   We intentionally do not use PMEM_VAR_SET() to init on_power_votes, b/c we don't want to validate
     *     a pmem group that could potentially not be valid.
     */
    __SAVED_ACROSS_BOOT.on_power_votes = 0;
    
    RTCWMGR_init();
    
    UTIL_crc_init();
    
    PERSIST_init();
    
    I2C1_init();
    
    sys_event_init();
    
    DISMGR_init();
    
    ext_flash_init(NULL);
    
    rf_switch_init();
    
    PWRMGR_init();
    
    // Init button driver ASAP to support actions out of ship mode.
    whistle_task_create( TEMPLATE_IDX_BUTTON_DRV ); 
    
    CONSOLE_init();

    CFGMGR_init();
    
    EVTMGR_init();
    
    wassert_init();  // always keep this as the last step, since it relies on PWRMGR, EVTMGR, and console.
          
    transport_mutex_init();

    whistle_task_create( TEMPLATE_IDX_SHERLOCK );

    enable_kernel_logging();
    _int_install_unexpected_isr(); // installs an "unexpected" ISR which gets called when an un-initialized ISR is invoked.
    _int_install_exception_isr();  // installs exception vector (#3) for hard faults, like data aborts.
    _install_unexpected_isr_app_cbk( unexp_int_app_cbk );  // tells the above ISR to call us back in that function so we can do cool stuff.
    
    print_header_info_short();
    
    PWRMGR_alpha_runtime_current_improve();
    
    BTMGR_init();
    
#ifdef WIFI_REBOOT_HACK
    // Set WiFi driver status to DRIVER_DOWN (needed to prevent hang if a thread later tries to disable it)
    driver_status(DRIVER_DOWN); 
#endif
    
    TPTMGR_init();

    CONMGR_init();
    
    /*
     *   NOTE:  Anything that needs to use EVTMGR must be after EVTMGR_init().
     */
    
    BTNMGR_init();
    #if (CALCULATE_MINUTES_ACTIVE) 
        MIN_ACT_init();
    #endif
    
    ACCMGR_init();
    
    PRXMGR_init();

    PERMGR_init();

    FWUMGR_init();
     
     // Initialize PWRMGR before possible entry into ship mode.
     whistle_task_create( TEMPLATE_IDX_PWRMGR );

     /* Need to check this (and possibly go to ship mode) before allowing tasks that could write to spi flash to startup */
     if ( PWRMGR_WakingFromShip() )
     {
    	 // Sample the battery one more time after giving it a sec to settle
    	 _time_delay( 500 );

    	 if ( !PWRMGR_process_batt_status( true ) )
    	 {
    		 // The battery is too low and system shutdown has been initiated
    	     goto WHISTLE_MAIN_TASK_DONE;
    	 }
     }
     
    /*
     *   NOTE:  The order you start these tasks matters, so be careful with 
     *          starting tasks that rely on other tasks that haven't been started.
     */
     
    cable = adc_get_cable( adc_sample_uid() );
    
    if( CABLE_BURNIN != cable )
    {
        /*
         *   Tasks needed for normal operation go here.
         */
        whistle_task_create( TEMPLATE_IDX_RTCWMGR );
        whistle_task_create( TEMPLATE_IDX_SYS_EVENT );
        whistle_task_create( TEMPLATE_IDX_DISMGR );  
        whistle_task_create( TEMPLATE_IDX_BTNMGR );  
        whistle_task_create( TEMPLATE_IDX_CONMGR );
        whistle_task_create( TEMPLATE_IDX_EVTMGR_POST );
        whistle_task_create( TEMPLATE_IDX_EVTMGR_DUMP );
        whistle_task_create( TEMPLATE_IDX_EVTMGR_UPLOAD );
        whistle_task_create( TEMPLATE_IDX_ACCMGR );
        whistle_task_create( TEMPLATE_IDX_ACCMGR_OFFLOAD );
        whistle_task_create( TEMPLATE_IDX_PERMGR );
    }
    else
    {
        /*
         *   Tasks needed for burn-in go here.
         *     In this way, we can avoid starting tasks we don't want in burn-in.
         */
        whistle_task_create( TEMPLATE_IDX_SYS_EVENT );
        whistle_task_create( TEMPLATE_IDX_DISMGR );  
        whistle_task_create( TEMPLATE_IDX_EVTMGR_POST );
        PWRMGR_start_burnin();
    }

    whistle_task_create( TEMPLATE_IDX_FWUMGR );
    
    print_persist_data();
    
    if (go_to_ship_mode)
    {
        corona_print("ship mode\n");
        PMEM_FLAG_SET(PV_FLAG_GO_TO_SHIP_MODE, FALSE);    
        PWRMGR_Ship( FALSE );
        goto WHISTLE_MAIN_TASK_DONE;
    }
    
    if (!pmem_flag_get(PV_FLAG_POWER_ON_RESET) && !PWRMGR_WakingFromShip())
    {
        /*
         *   We do NOT want to run this pattern when coming out of ship mode.
         *     Otherwise the user will be confused for the button-pressed case.
         */
    	DISMGR_run_pattern(DISMGR_PATTERN_POWER_ON_ONESHOT);
    	PMEM_FLAG_SET(PV_FLAG_POWER_ON_RESET, TRUE);    
    }
    
    PWRMGR_process_USB();
    
    if( !PWRMGR_usb_is_inserted() || !g_st_cfg.acc_st.shutdown_accel_when_charging )
    {
        // Start up the accelerometer manager
        ACCMGR_start_up();
        
        // Start logging right away, so we can capture a sleep event for the no-motion-on-boot case (COR-543).
        // accdrv_simulate_wakeup(); // No longer needed due to persistent sleep.
    }
    
    corona_print("Booted\n");
    corona_print("BldID %s\n", build_id);
    corona_print("BldTS %s\n", build_time);
    
    // Log why we rebooted to event manager.
    WMP_post_reboot( (persist_reboot_reason_t) __SAVED_ACROSS_BOOT.reboot_reason, __SAVED_ACROSS_BOOT.reboot_thread, 1 );
    
    /*
     *   Process/Log any new information we found out on this boot.
     */
    unexp_int_chk();
    prev_on_vote_chk(prev_on_pwr_votes);
    _persist_log_untimely_crumb( __SAVED_ACROSS_BOOT.untimely_crumb_mask );
    if( hard_boot )
    {
        WMP_post_breadcrumb( BreadCrumbEvent_PERSIST_MEM_RESET );
    }

    _watchdog_stop();
    
#ifdef ENABLE_WHISTLE_UNIT_TESTS
    hardware_tests();
    unit_tests();
#endif
    
    // Sample the battery one more time after giving it a sec to settle
    _time_delay( 1 * 1000 );
    
    // This call will initiate a system shutdown if the battery is too low
    PWRMGR_process_batt_status( false );
    
WHISTLE_MAIN_TASK_DONE:
    _watchdog_stop();
    PWRMGR_VoteExit( PWRMGR_MAIN_TASK_VOTE );
    _task_block(); // can't exit! The main task owns all the resources created during XXX_init()!
}

#else
/* DRIVER_CONFIG_INCLUDE_BMI */
enum whistle_tasks_e
{
    TEMPLATE_IDX_MAIN = 1,
    TEMPLATE_IDX_FLASH_AGENT,
    TEMPLATE_IDX_USBCDC,
};

void WSTL_tsk(uint_32 dummy)
{
    usb_allow();
    
    // Wait for USB enabled
    while (!usb_is_enabled() )
    {
        _time_delay(100);
    }
    
    whistle_task_create( TEMPLATE_IDX_USBCDC );  
    whistle_task_create( TEMPLATE_IDX_FLASH_AGENT );  
}

#endif /* DRIVER_CONFIG_INCLUDE_BMI */


void enable_kernel_logging(void)
{
#if !MQX_USE_LOGS
	return;
#else
    
    /* create the log component */
    wassert( MQX_OK == _log_create_component() );
    
    /* create log number 0 */
    wassert( MQX_OK == _klog_create(5000, LOG_OVERWRITE) );
    
    _klog_control(0xFFFFFFFF, FALSE);
    _klog_control(  
                    KLOG_ENABLED |		
                    KLOG_FUNCTIONS_ENABLED |
                    KLOG_INTERRUPTS_ENABLED |
                    // KLOG_SYSTEM_CLOCKwint_enableD |
                    KLOG_CONTEXT_ENABLED |
                    KLOG_TASKING_FUNCTIONS |
                    KLOG_ERROR_FUNCTIONS |
                    KLOG_MESSAGE_FUNCTIONS |
                    KLOG_INTERRUPT_FUNCTIONS |
                    KLOG_MEMORY_FUNCTIONS |
                    KLOG_TIME_FUNCTIONS |
                    KLOG_EVENT_FUNCTIONS |
                    KLOG_NAME_FUNCTIONS |
                    KLOG_MUTEX_FUNCTIONS |
                    KLOG_SEMAPHORE_FUNCTIONS,
                    // KLOG_WATCHDOG_FUNCTIONS,
                    
                    TRUE
    );

        _klog_enable_logging_task(_task_get_id());

#endif
}

#if FTA_WHISTLE

	// App Header
	// Pragma to place the App header field on correct location defined in linker file.
	#pragma define_section appheader ".appheader" far_abs R

	__declspec(appheader) uint8_t apph[0x20] = {
	    // Name [0,1,2]
	    'F', 'T', 'A',      // 0x46, 0x54, 0x41
	    // Version [3,4,5]
	    0x00, 0x04, 0x00,
	    // Checksum of Name and Version [6]
	    0x21U,
	    // Success, Update, 3 Attempt flags, BAD flag [7,8,9,A,B,C]
	    0x55U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
	    // SAVD [D], Reserved [E-F]
	    0xFFU, 0xFFU, 0xFFU,
	    // Version tag [10-1F]
	    'F','U','N','C','T','_','T','E','S','T','_','4', 0xFFU,0xFFU, 0xFFU, 0xFFU
	};

#else

	// App Header
	// Pragma to place the App header field on correct location defined in linker file.
	#pragma define_section appheader ".appheader" far_abs R

	__declspec(appheader) uint8_t apph[0x20] = {
	    // Name [0,1,2]
	    'A', 'P', 'P',      // 0x41, 0x50, 0x50
	    // Version [3,4,5]
	    0x00, 0x01, 0x00,
	    // Checksum of Name and Version [6]
	    0x1EU,
	    // Success, Update, 3 Attempt flags, BAD flag [7,8,9,A,B,C]
	    0x55U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
	    // SAVD [D], Reserved [E-F]
	    0xFFU, 0xFFU, 0xFFU,
	    // Version tag [10-1F]
	    'J','T','A','G',0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU,0xFFU
	};
	
#endif  // FTA_WHISTLE
