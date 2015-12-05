/*
 * burnin.c
 *
 *  Created on: Aug 5, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "factory_mgr.h"
#include "corona_console.h"
#include "main.h"
#include "cfg_factory.h"
#include "ext_flash.h"
#include "wassert.h"
#include "corona_ext_flash_layout.h"
#include "app_errors.h"
#include "pwr_mgr.h"
#include "time_util.h"
#include "sys_event.h"
#include "wifi_mgr.h"
#include "pmem.h"
#include "led_hal.h"
#include "transport_mutex.h"
#include "prx_mgr.h"
#include "adc_driver.h"
#include "persist_var.h"
#include "factory_mgr.h"
#include "cfg_mgr.h"
#include "cormem.h"



////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define BURNIN_EVENT_PERIODIC_TESTS         0x00000001
#define BURNIN_EVENT_REGISTER_SOL           0x00000002
#define BURNIN_EVENT_SINGLE_PRESS           0x00000004
#define BURNIN_EVENT_DOUBLE_PRESS           0x00000008
#define BURNIN_EVENT_CHGB_FAULT             0x00000010
#define BURNIN_EVENT_PWR_DRAIN              0x00000020
#define BURNIN_EVENT_USB_UNPLUG				0x00000040			


#define WDOG_RESET_TIMER             (40) // seconds before a watchdog reset.
#define MAX_WDOG_STROBE_TIMEOUT      (5)  // seconds between WDOG strobes, need a bit of margin.
#define SIGN_OF_LIFE_TIMEOUT         (3)  // seconds between blinking

#define MAX_PERIODIC_TEST_TIMEOUT    (30 * 60) // seconds between wifi scans.

#define PWR_DRAIN_TIMEOUT            (30)           // seconds between battery voltage checks
#define PWR_DRAIN_TEST_DURATION      (80 * 60)     // 1:20
#define PWR_DRAIN_TEST_TICKS         (PWR_DRAIN_TEST_DURATION/PWR_DRAIN_TIMEOUT)
#define PWR_DRAIN_FULL_CHARGE        (4100)
#define PWR_DRAIN_LOW_LEVEL          (3700)   

#ifndef TRUE
#define TRUE    (1)
#endif

#ifndef FALSE
#define FALSE   (0)
#endif

/*
 *   WDOG Strobe Management
 *   
 *   We have some logic here to assure some feedback from the burn-in task.
 *   Otherwise we should WDOG reset if the burn-in task is hanging.
 *   
 *   If a non-zero value greater than 1 is set, then we allow strobing forever,
 *     and therefore do not reset it.
 */
#define DISALLOW_WDOG_STROBE        0
#define ALLOW_WDOG_STROBE_ONCE      1
#define ALLOW_WDOG_STROBE_FOREVER   2

typedef enum burnin_test_status_type
{
	BURNIN_TEST_FAILED = 0xDEADBEEF,
	BURNIN_TEST_PASSED = 0xFEEDFACE
}burnin_test_status_t;

////////////////////////////////////////////////////////////////////////////////
// Externs
////////////////////////////////////////////////////////////////////////////////
extern unsigned char BT_patch_is_valid(unsigned long patch_address);
extern void pwrmgr_enter_ship( void );

////////////////////////////////////////////////////////////////////////////////
// Private Declarations
////////////////////////////////////////////////////////////////////////////////
static void wdog_strobe_task(uint_32 dummy);

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static boolean g_burnin_is_running = FALSE;
static LWEVENT_STRUCT g_burnin_lwevent;
static TASK_TEMPLATE_STRUCT g_wdog_strobe_template = { (WHISTLE_NUM_TASKS+1), wdog_strobe_task, 1024, 4, "wdog_strobe", 0, 0, 0 };
static uint8_t g_strobe_ok = DISALLOW_WDOG_STROBE;   // a way to make sure our task is alive and allowing strobing.
static uint_16 adc_counts;

static char* g_pDefault_SSIDs[] = \
                                { "Sasha",
                                  "Sasha-TEST",
                                  NULL
                                };
//static char* g_pDefault_SSIDs[] = \
//                                { "JHome",
//                                  NULL
//                                };


static const char* burnin_errs[] = {    "",
                                        "SSID_UNSET",
                                        "SSID_NOT_FOUND",
                                        "WDOG_RESET",
                                        "FCFG",
                                        "TIMER",
                                        "DISPLAY_CMD",
                                        "TESTBRD_BT",
                                        "TESTBRD_MFI",
                                        "TESTBRD_ACCEL",
                                        "TESTBRD_WIFI",
                                        "TESTBRD_WSON",
                                        "MALLOC",
                                        "PATCHES",
                                        "TESTBRD_REBOOT",
                                        "CHGB_FAULT",
                                        "PWR_DRAIN"
                                    };

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Set a persistent state for burnin pass/fail status.
 */
static void _burnin_set_test_status(burnin_test_status_t status)
{
	wson_write(EXT_FLASH_BURNIN_ADDR, (uint8_t *)&status, sizeof(uint32_t), 1);
}

static burnin_test_status_t _burnin_get_test_status(void)
{
	burnin_test_status_t status = BURNIN_TEST_FAILED;
	
	wson_read(EXT_FLASH_BURNIN_ADDR, (uint8_t *)&status, sizeof(uint32_t));
	
	return status;
}

/*
 *   WIFI Scan timeout callback.
 */
static void _burnin_periodic_test_timeout_callback(void)
{
    _lwevent_set(&g_burnin_lwevent, BURNIN_EVENT_PERIODIC_TESTS);
}

/*
 *   WIFI Scan timer arming.
 */
static void _burnin_periodic_timer(UTIL_timer_t *pTimer)
{
    whistle_time_t time;
    
    //UTIL_time_stop_timer(*pTimer);
    RTC_cancel_alarm( pTimer );
    
    RTC_time_set_future(&time, MAX_PERIODIC_TEST_TIMEOUT);
    *pTimer = UTIL_time_start_oneshot_timer_at(TRUE, &time, _burnin_periodic_test_timeout_callback);
}

/*
 *   Button press callbacks.
 */
static void _burnin_sysevent_callback(sys_event_t sys_event)
{
    if(sys_event & SYS_EVENT_BUTTON_SINGLE_PUSH)
    {
        _lwevent_set(&g_burnin_lwevent, BURNIN_EVENT_SINGLE_PRESS);
    }
    
#if 0
    if(sys_event & SYS_EVENT_BUTTON_DOUBLE_PUSH)
    {
        _lwevent_set(&g_burnin_lwevent, BURNIN_EVENT_DOUBLE_PRESS);
    }
#endif
    
    if(sys_event & SYS_EVENT_USB_UNPLUGGED)
    {
        _lwevent_set(&g_burnin_lwevent, BURNIN_EVENT_USB_UNPLUG);
    }
}

/*
 *   Find an SSID in a scan.
 */
static err_t _burnin_wifi_scan_for_ssid(char *pBurninSSID)
{
    err_t err;
    int i;
    ATH_GET_SCAN ext_scan_list;
    
    corona_print("Search %s\n", pBurninSSID);
    
    /*
     *   Quick search first, since deep could take longer and be interrupted.
     */
    WIFIMGR_ext_scan(&ext_scan_list, FALSE);
    g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
    
    for (i = 0; i < ext_scan_list.num_entries; i++)
    {
        char temp_ssid[MAX_SSID_LENGTH];
    	memcpy(temp_ssid, ext_scan_list.scan_list[i].ssid, ext_scan_list.scan_list[i].ssid_len);
    	temp_ssid[ext_scan_list.scan_list[i].ssid_len] = 0;

        if( strcmp(pBurninSSID, temp_ssid) == 0 )
        {
            corona_print("RSSI %u\n", ext_scan_list.scan_list[i].rssi);
            return ERR_OK;
        }
    }
    
    /*
     *   Delay a bit to give time for the quick search to "finish up".
     */
    _time_delay( 1 * 1000 );
    
    /*
     *   Deep search.
     */
    WIFIMGR_ext_scan(&ext_scan_list, TRUE);
    
    for (i = 0; i < ext_scan_list.num_entries; i++)
    {
        char temp_ssid[MAX_SSID_LENGTH];
    	memcpy(temp_ssid, ext_scan_list.scan_list[i].ssid, ext_scan_list.scan_list[i].ssid_len);
    	temp_ssid[ext_scan_list.scan_list[i].ssid_len] = 0;

        if( strcmp(pBurninSSID, temp_ssid) == 0 )
        {
            corona_print("Found RSSI:%u\n", ext_scan_list.scan_list[i].rssi);
            return ERR_OK;
        }
    }
    
    /*
     *   Delay a bit to give time for the deep search for "finish up".
     */
    _time_delay( 2 * 1000 );
    
    return ERR_BURNIN_SSID_NOT_FOUND;
}

/*
 *    Scan for a the default convenience SSID's.
 */
static err_t _burnin_scan_default_ssids(void)
{
    uint8_t i = 0;
    err_t err = ERR_BURNIN_SSID_NOT_FOUND;
    
    while(1)
    {
        if( NULL == g_pDefault_SSIDs[i] )
        {
            break;
        }
        
        err = _burnin_wifi_scan_for_ssid(g_pDefault_SSIDs[i]);
        if(ERR_OK == err)
        {
            break;
        }
        i++;
    }
    
    return err;
}

/*
 *    Scan for a particular WIFI SSID, and report results.
 */
static err_t _burnin_wifi_scan(void)
{
    char *pBurninSSID = NULL;
    fcfg_handle_t handle;
    err_t err = ERR_OK;
    uint8_t retries = 3;
    
    pBurninSSID = _lwmem_alloc_zero(128);
    if(NULL == pBurninSSID)
    {
        return PROCESS_NINFO( ERR_BURNIN_MEM_ALLOC, NULL );
    }
    
    /*
     *   Try to get the access point from the factory config.
     */
    if( FCFG_OK != FCFG_init(&handle,
                              EXT_FLASH_FACTORY_CFG_ADDR,
                              &wson_read,
                              &wson_write,
                              &_lwmem_alloc_system_zero,
                              &_lwmem_free) )
    {
        err = PROCESS_NINFO( ERR_BURNIN_FCFG_INIT, NULL );
        goto done_wifi_scan;
    }
    
    if( FCFG_OK != FCFG_get(&handle, CFG_FACTORY_BURNIN_SSID, pBurninSSID) )
    {
        err = PROCESS_NINFO( ERR_BURNIN_SSID_UNSET, NULL );
    }
    
    if( ERR_BURNIN_SSID_UNSET == err )
    {
        /*
         *   There is no SSID set in factory config.
         *   Before we just return that error, let's look for a couple of SSID's, out of convenience.
         */
        err = _burnin_scan_default_ssids();
        if( ERR_OK != err )
        {
            err = ERR_BURNIN_SSID_UNSET;
        }
        goto done_wifi_scan;
    }
    
    /*
     *   The fcfg SSID was set, let's try that first.
     */
    do
    {
        err = _burnin_wifi_scan_for_ssid(pBurninSSID);
        _time_delay(500);
    } while( (ERR_OK!=err) && (--retries) );
    
    if(ERR_OK != err)
    {
        /*
         *   Failed to find the SSID loaded in fcfg while scanning.
         *   Let's try the convenience SSID's before we fail out.
         */
        err = _burnin_scan_default_ssids();
        if( ERR_OK != err )
        {
            err = ERR_BURNIN_SSID_NOT_FOUND;
        }
        goto done_wifi_scan;
    }
    
done_wifi_scan:
    wfree(pBurninSSID);
    return err;
}

/*
 *   Strobe the WDOG, to prevent the system from resetting,
 *     then re-register so we keep strobing it.
 */
static void _burnin_sign_of_life(void)
{
    /*
     *   We want to register this callback on BURNIN's context,
     *   so we assure burn-in task is still OK.
     */
    _lwevent_set(&g_burnin_lwevent, BURNIN_EVENT_REGISTER_SOL);
}

/*
 *   WDOG Strobe timer arming.
 */
static void _burnin_sol_timer(UTIL_timer_t *pTimer)
{
    whistle_time_t time;
    
    //UTIL_time_stop_timer(*pTimer);
    RTC_cancel_alarm( pTimer );
    
    RTC_time_set_future(&time, SIGN_OF_LIFE_TIMEOUT);
    *pTimer = UTIL_time_start_oneshot_timer_at(TRUE, &time, _burnin_sign_of_life);
}

static void _burnin_pwr_drain(void)
{
    _lwevent_set(&g_burnin_lwevent, BURNIN_EVENT_PWR_DRAIN);
}

// Pwr drain test timer arming
//
static void _burnin_pwr_drain_timer(UTIL_timer_t *pTimer, int interval)
{
    whistle_time_t time;
    
    RTC_cancel_alarm( pTimer );
    
    RTC_time_set_future(&time, interval);
    *pTimer = UTIL_time_start_oneshot_timer_at(TRUE, &time, _burnin_pwr_drain);
}

static uint32_t get_bat_mv(void)
{
    adc_counts = (uint16_t) adc_sample_battery();
    return ((uint32_t) adc_getNatural(ADC0_Vsense_Corona_VBat, (uint16_t) adc_counts));
}

/*
 *   Initialize WDOG, check for evidence of a reset on previous boot.
 */
static err_t _burnin_wdog_init(void)
{
    uint16_t wdog_reset_count;

    wdog_reset_count = PWRMGR_watchdog_reset_count();
    PWRMGR_watchdog_clear_count();
    
    if(wdog_reset_count > 0)
    {
        corona_print("wdog rstcnt %u\n", wdog_reset_count);
        return ERR_BURNIN_WDOG_RESET_OCCURED;
    }

    PWRMGR_watchdog_arm( WDOG_RESET_TIMER );
    
    g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
    _task_create( 0, 0, (uint_32)&g_wdog_strobe_template );
    
    return ERR_OK;
}

/*
 *   Task for strobing the watchdog perodically.
 */
static void wdog_strobe_task(uint_32 dummy)
{
    while(1)
    {
        /*
         *   Wait for burn-in thread to chime in and allow us to strobe.
         *   This is to protect against the thread going bye-bye and we
         *   keep strobing away.
         */
        while(g_strobe_ok == DISALLOW_WDOG_STROBE)
        {
            _time_delay(200);
        }
        
        PWRMGR_watchdog_refresh();
        
        /*
         *   We have some logic here to assure some feedback from the burn-in task.
         *   Otherwise we should WDOG reset if the burn-in task is hanging.
         *   
         *   If a non-zero value greater than 1 is set, then we allow strobing forever,
         *     and therefore do not reset it.
         */
        if(ALLOW_WDOG_STROBE_ONCE == g_strobe_ok)
        {
            g_strobe_ok = DISALLOW_WDOG_STROBE;
        }
        
        _time_delay(MAX_WDOG_STROBE_TIMEOUT * 1000);
    }
}

/*
 *   Check that BT patches are valid in SPI Flash.
 */
static err_t _burnin_check_bt_patches(void)
{
    if( !BT_patch_is_valid( EXT_FLASH_LOWENERGY_ADDR ) )
    {
        return ERR_BURNIN_BAD_BT_PATCHES;
    }
    
    if( !BT_patch_is_valid( EXT_FLASH_BASEPATCH_ADDR ) )
    {
        return ERR_BURNIN_BAD_BT_PATCHES;
    }
    
    return ERR_OK;
}

static err_t _burnin_periodic_wifi_scan(void)
{
    uint8_t scan_retries = 3;
    err_t err;
    
    /*
     *   Start scanning for known WIFI access point, 
     *     and allow for some retries.
     *     
     *   We use the non-blocking mutex, so don't allow anyone
     *     to interrupt us, but we are allowed to interrupt others.
     */
    if( FALSE == transport_mutex_non_blocking_lock(__FILE__, __LINE__) )
    {
        /*
         *   This "should" never happen, as we try to eliminate the tasks/timers
         *     that use the connection mutex.
         *     
         *   However, if someone IS accidentally using it, just reboot, as opposed
         *     to hanging.
         */
        PWRMGR_reset(TRUE);
    }
    
    err = ERR_GENERIC;
    while(  1 )
    {
        err = _burnin_wifi_scan();
        g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
        
        if( (ERR_BURNIN_SSID_UNSET == err) || (ERR_OK == err) )
        {
            break;
        }
        
        if( 0 == scan_retries--)
        {
            break;
        }
        
        /*
         *   Give some time to allow the WIFI searches to finish up.
         *   But only if a retry is actually needed.
         */
        _time_delay( 2000 );
        g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
    }
    
    transport_mutex_unlock(__FILE__, __LINE__);
    
    if(ERR_OK != err)
    {
        return err;
    }
    
    burnin_display(BURNIN_DISPLAY_WIFI_OK);
    
    return ERR_OK;
}

static void _burnin_chgb_fault_cbk(sys_event_t unused)
{
    _lwevent_set(&g_burnin_lwevent, BURNIN_EVENT_CHGB_FAULT);
}

void burnin_ship_mode(void)
{
    /*
     *   COR-299
     *   
     *   Reset data.
     *   Restore factory defaults.
     *   Go into ship mode.
     */
	if( _burnin_get_test_status() == BURNIN_TEST_PASSED )
	{
	    g_strobe_ok = ALLOW_WDOG_STROBE_FOREVER;  // allow strobing forever, since we're going to ship.
	    
	    burnin_display(BURNIN_DISPLAY_DOUBLE_PRESS);
	    g_burnin_is_running = FALSE;
	    
	#if 0
	    /*
	     *   Now, we should rely on factory using "fle" command to erase the appropriate
	     *     SPI Flash sectors to achieve this, rather than doing it here.
	     */
	    EVTMGR_reset();
	    CFGMGR_load_factory_defaults( TRUE );
	#endif
	    
	    _time_delay(100);
	    led_run_pattern (LED_PATTERN_OFF);
	    
	    /*
	     *   Dunk ship mode directly, don't risk some stupid tasks getting in the way!
	     */
	    pwrmgr_enter_ship();
	    
	    _task_block();
	}
	else
	{
		corona_print("BI not done\n");
		burnin_display(BURNIN_DISPLAY_SOL);
	}
}

/*
 *   Burn-in entry point task.
 */
void BI_tsk(uint_32 dummy)
{
    err_t err;
    UTIL_timer_t periodic_test_timer = 0;
    UTIL_timer_t sol_timer = 0;
    UTIL_timer_t pwr_drain_timer  = 0;
    uint32_t pwr_drain_dur;
    uint32_t pwr_drain_full;
    uint32_t pwr_drain_low;
    uint32_t bat_mv;
    int pwr_drain_ticks;
    int pwr_drain_max_ticks;
    
    /*
     *    Make sure we never go to sleep while in Burn-in.
     */
    PWRMGR_VoteForRUNM( PWRMGR_BURNIN_VOTE );
    
    g_burnin_is_running = TRUE;
    
    _lwevent_create(&g_burnin_lwevent, LWEVENT_AUTO_CLEAR);
    
    /*
     *   Give time for other tasks to get started, before we DESTROY THEM !
     */
    corona_print("\n *BURNIN\n\n");
    _time_delay( 500 );
    
    /*
     *   Initialize Watchdog features.
     */
    err = _burnin_wdog_init();
    if(ERR_OK != err)
    {
        goto end_burnin;
    }
    
    /*
     *   Destroy tasks that we do not care about for burn-in.
     *   Most or all of these should not be running.
     */
    whistle_task_destroy(ACC_TASK_NAME);
    whistle_task_destroy(ACCOFF_TASK_NAME);
    whistle_task_destroy(FWU_TASK_NAME);
    whistle_task_destroy(PER_TASK_NAME);
    whistle_task_destroy(UPLOAD_TASK_NAME);
    whistle_task_destroy(DUMP_TASK_NAME);
    whistle_task_destroy(BTNMGR_TASK_NAME);
    whistle_task_destroy(BT_TASK_NAME);
    
    /*
     *   Destroy timers/tasks that may cause unwanted activities, particularly,
     *     relating to syncing BT and WIFI.
     */
    PERMGR_stop_timer();
    DISMGR_stop_timer();
    CONMGR_shutdown();
    g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
    
    sys_event_register_cbk(_burnin_chgb_fault_cbk, SYS_EVENT_CHGB_FAULT);
    
    /*
     *   Make sure we didn't reboot in the middle of a testbrd run during the last boot.
     */
    if( pmem_flag_get( PV_FLAG_BURNIN_TESTBRD_IN_PROGRESS ) )
    {
        err = ERR_BURNIN_TESTBRD_REBOOT;
        goto end_burnin;
    }
    
    // Check if we had a pwr drain test failure
    if (pmem_flag_get(PV_FLAG_BURNIN_PWR_DRAIN_FAILURE))
    {
        err = ERR_BURNIN_PWR_DRAIN;
        goto end_burnin;
    }
    if (!PWRMGR_usb_is_inserted())  // if usb has not been reinserted
    {
        if (pmem_flag_get(PV_FLAG_BURNIN_PWR_DRAIN_RUNNING))    // if reset during the test
        {
            err = ERR_BURNIN_PWR_DRAIN;
            goto end_burnin;
        }
    }
    else    // usb is charging
    {
        // For the case where usb was asserted during the test
        PMEM_FLAG_SET(PV_FLAG_BURNIN_PWR_DRAIN_RUNNING, 0);
    }
    if (pmem_flag_get(PV_FLAG_BURNIN_PWR_DRAIN_SUCCESS))
    {
        goto pwr_drain;
    }
    
    /*
     *   BT Patches better be in SPI Flash.  THIS IS CHINA !
     */
    err = _burnin_check_bt_patches();
    if(ERR_OK != err)
    {
        goto end_burnin;
    }

    g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;

    /*
     *   Kick off a WIFI scan first.  This is needed for running testbrd's WIFI comm test.
     */
    err = _burnin_periodic_wifi_scan();
    if(ERR_OK != err)
    {
        goto end_burnin;
    }
    
    /*
     *   Run testbrd() now, to make sure all board-level is OK.
     */
    burnin_display(BURNIN_DISPLAY_TESTBRD_IN_PROGRESS);
    g_strobe_ok = ALLOW_WDOG_STROBE_FOREVER;     // if we hang on testbrd, allow the user to know.
    err = testbrd();
    if(ERR_OK != err)
    {
        goto end_burnin;
    }
    burnin_display(BURNIN_DISPLAY_TESTBRD_COMPLETE);

    ///////////////////////////////////////////////////////////////////
    // Do power drain test if rebooted without USB and
    // if battery voltage >= FULL_CHARGE and 
    // the pwr drain test has not been run
    if ((pwr_drain_dur = g_st_cfg.fac_st.pddur) == 0)
        pwr_drain_dur = PWR_DRAIN_TEST_DURATION;
    if ((pwr_drain_full = g_st_cfg.fac_st.pdfull) == 0)
        pwr_drain_full = PWR_DRAIN_FULL_CHARGE;
    if ((pwr_drain_low = g_st_cfg.fac_st.pdlow) == 0)
        pwr_drain_low = PWR_DRAIN_LOW_LEVEL;
    pwr_drain_max_ticks = pwr_drain_dur/PWR_DRAIN_TIMEOUT;
    
    if ((!PWRMGR_usb_is_inserted()) &&
        ((bat_mv = get_bat_mv()) >= pwr_drain_full) &&
        (!pmem_flag_get(PV_FLAG_BURNIN_PWR_DRAIN_SUCCESS)))
    {
        corona_print("Start BDDT\n");
        corona_print("pddur:%d pdfull:%d pdlow:%d pdticksmax: %d\n",
                      pwr_drain_dur, pwr_drain_full, pwr_drain_low, pwr_drain_max_ticks);
        corona_print("initial bat mv %d\n", bat_mv);
        
        burnin_display(BURNIN_DISPLAY_PWR_DRAIN_TEST);
        PMEM_FLAG_SET(PV_FLAG_BURNIN_PWR_DRAIN_RUNNING, 1);
        pwr_drain_ticks = 1;  // Count iterations for elapsed time
        
        while(1)
        {
            // Arm the timer for periodic events
            _burnin_pwr_drain_timer(&pwr_drain_timer, 30);
            if( 0 == pwr_drain_timer )
            {
                err = PROCESS_NINFO( ERR_BURNIN_INVALID_TIMER, NULL );
                goto end_burnin;
            }
            
            _lwevent_wait_ticks(&g_burnin_lwevent, BURNIN_EVENT_PWR_DRAIN, FALSE, 0);
            g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
            
            bat_mv = get_bat_mv();

            /*
             *   NOTE:  Do not change this string or format.  It is used by the factory.
             */
            corona_print("$BDDT,%d,%d,%d\n", pwr_drain_ticks*PWR_DRAIN_TIMEOUT, bat_mv, adc_counts);
            
            if (bat_mv < pwr_drain_low)
            {
                PMEM_FLAG_SET(PV_FLAG_BURNIN_PWR_DRAIN_RUNNING, 0);
                PMEM_FLAG_SET(PV_FLAG_BURNIN_PWR_DRAIN_FAILURE, 1);
                corona_print("bat mv < PWR_DRAIN_LOW_LEVEL: %d\n", pwr_drain_low);
                err = ERR_BURNIN_PWR_DRAIN;
                RTC_cancel_alarm( &pwr_drain_timer );
                goto end_burnin;
            }
            if (pwr_drain_ticks++ >= pwr_drain_max_ticks)
            {
                PMEM_FLAG_SET(PV_FLAG_BURNIN_PWR_DRAIN_RUNNING, 0);
                PMEM_FLAG_SET(PV_FLAG_BURNIN_PWR_DRAIN_SUCCESS, 1);
                corona_print("BDDT OK\n");
                RTC_cancel_alarm( &pwr_drain_timer );
                
                /*
                 *   OK, at this point all of the burn-in tests have passed,
                 *     so set this persistent state in SPI Flash.
                 */
                _burnin_set_test_status(BURNIN_TEST_PASSED);
                break;
            }
            burnin_display(BURNIN_DISPLAY_PWR_DRAIN_TEST);  // Blink the display each time
        }
    }
pwr_drain:
    if (pmem_flag_get(PV_FLAG_BURNIN_PWR_DRAIN_SUCCESS))
    {
        while(1)
        {
            _burnin_pwr_drain_timer(&pwr_drain_timer, 2);
            if( 0 == pwr_drain_timer )
            {
                err = PROCESS_NINFO( ERR_BURNIN_INVALID_TIMER, NULL );
                goto end_burnin;
            }
            
            _lwevent_wait_ticks(&g_burnin_lwevent, BURNIN_EVENT_PWR_DRAIN, FALSE, 0);
            
            g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
            // if no chg_b and have pgood_b, fully charged after test
            if (!PWRMGR_chgB_state() && PWRMGR_usb_is_inserted())
            {
                burnin_display(BURNIN_DISPLAY_PWR_DRAIN_TEST2); // It blinks momentarily each time it's called
            }
            else    // either charging or after successful test before charging is restarted
            {
                burnin_display(BURNIN_DISPLAY_PWR_DRAIN_TEST2); // Blink on every 2 seconds
                _time_delay(200);
                burnin_display(BURNIN_DISPLAY_OFF);
            }
        }
    }
    ///////////////////////////////////////////////////////////////////

    /*
     *   Register for button presses.
     */
    sys_event_register_cbk( _burnin_sysevent_callback, 
                            SYS_EVENT_BUTTON_SINGLE_PUSH | 
                            SYS_EVENT_BUTTON_DOUBLE_PUSH);

    /*
     *   Arm the one-shot timer for periodic/repeating scans.
     */
    _burnin_periodic_timer(&periodic_test_timer);
    if( 0 == periodic_test_timer )
    {
        err = PROCESS_NINFO( ERR_BURNIN_INVALID_TIMER, NULL );
        goto end_burnin;
    }
    
    /*
     *   Kick off sign of life.
     */
    _burnin_sign_of_life();
    g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
    
    /*
     *   Keep looping, periodically looking for a known SSID.
     */
    while(1)
    {
        _mqx_uint mask;
        
        _lwevent_wait_ticks(   &g_burnin_lwevent,
                               BURNIN_EVENT_PERIODIC_TESTS       | 
                               BURNIN_EVENT_REGISTER_SOL         | 
                               BURNIN_EVENT_SINGLE_PRESS         | 
                               BURNIN_EVENT_DOUBLE_PRESS         |
                               BURNIN_EVENT_USB_UNPLUG			 |
                               BURNIN_EVENT_CHGB_FAULT, 
                               FALSE, 
                               0);
        
        mask = _lwevent_get_signalled();
        g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
        
        if(mask & BURNIN_EVENT_CHGB_FAULT)
        {
            err = PROCESS_NINFO( ERR_BURNIN_CHGB_FAULT, NULL);
            goto end_burnin;
        }
        
        if(mask & BURNIN_EVENT_REGISTER_SOL)
        {
            _burnin_sol_timer(&sol_timer);
            if( 0 == sol_timer )
            {
                err = PROCESS_NINFO( ERR_BURNIN_INVALID_TIMER, NULL );
                goto end_burnin;
            }
            
            burnin_display(BURNIN_DISPLAY_SOL);
        }
        
        g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
        
        if(mask & BURNIN_EVENT_PERIODIC_TESTS)
        {
            /*
             *   Run testbrd() now, to make sure all board-level is OK.
             */
            burnin_display(BURNIN_DISPLAY_TESTBRD_IN_PROGRESS);
            err = testbrd();
            if(ERR_OK != err)
            {
                goto end_burnin;
            }
            burnin_display(BURNIN_DISPLAY_TESTBRD_COMPLETE);
            
            /*
             *   We will need to use WIFI again to scan for networks, so in the spirit of the app, reboot.
             */
            _time_delay(500);
            PWRMGR_reset(TRUE);
        }
        
        g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
        
        if(mask & BURNIN_EVENT_SINGLE_PRESS)
        {
            burnin_display(BURNIN_DISPLAY_SINGLE_PRESS);
        }
        
        g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
        
#if 0
        if(mask & BURNIN_EVENT_DOUBLE_PRESS)
#else
        if(mask & BURNIN_EVENT_USB_UNPLUG)
#endif
        {
        	/*
        	 *   NOTE:  This is actually handled in PWRMGR, b/c we want to be able
        	 *          to support going into ship BEFORE we even start waiting on the
        	 *          BURNIN_EVENT_USB_UNPLUG event, so...
        	 */
        	//burnin_ship_mode();
        }
        
        g_strobe_ok = ALLOW_WDOG_STROBE_ONCE;
    }
    
end_burnin:

    g_strobe_ok = ALLOW_WDOG_STROBE_FOREVER;   // allow strobing forever
    
    switch(err)
    {
        case ERR_OK:
            break;
            
        case ERR_BURNIN_SSID_UNSET:
            corona_print("SSID fcfg\n");
            break;
            
        case ERR_BURNIN_SSID_NOT_FOUND:
            corona_print("scan\n");
            break;
        
        case ERR_BURNIN_WDOG_RESET_OCCURED:
            corona_print("WDOG RST\n");
            break;
            
        default:
            PROCESS_NINFO( err, NULL );
            break;
    }
    
    _burnin_set_test_status(BURNIN_TEST_FAILED);
    burnin_display_err(err);
    
     _task_block();
}

/*
 *   Displays a pattern for burnin.
 *   
 *   NOTE:  We are purposefully not using DISMGR, as not to couple
 *          DISMGR's state machine to burn-in.
 */
void burnin_display(burnin_display_t display)
{
    static uint32_t toggle = 0;
    int i;
    
    g_strobe_ok = ALLOW_WDOG_STROBE_FOREVER;  // allow as long as we are in the display function.
    
    switch(display)
    {
        case BURNIN_DISPLAY_CHARGING:
            led_run_pattern (LED_PATTERN_16);   // purple both banks
            break;
            
        case BURNIN_DISPLAY_FULLY_CHARGED:
    #ifdef WHISTLE_GREEN_IN_BURNIN
            led_run_pattern (LED_PATTERN_BA3S);    // WHiSTLE green both banks
    #else
            led_run_pattern (LED_PATTERN_4);       // Green both banks.
    #endif
            break;
            
        case BURNIN_DISPLAY_SINGLE_PRESS:
            for(i = 0; i < 5; i++)
            {
                led_run_pattern (LED_PATTERN_10); // yellow bank A
                _time_delay(40);
                led_run_pattern (LED_PATTERN_12); // white bank B
                _time_delay(40);
            }
            break;
            
        case BURNIN_DISPLAY_DOUBLE_PRESS:
            for(i = 0; i < 8; i++)
            {
                led_run_pattern (LED_PATTERN_9);  // green bank A
                _time_delay(250);
                led_run_pattern (LED_PATTERN_11); // red bank B
                _time_delay(250);
            }
            break;
            
        case BURNIN_DISPLAY_TESTBRD_IN_PROGRESS:
            led_run_pattern (LED_PATTERN_5);      // yellow both banks
            break;
            
        case BURNIN_DISPLAY_TESTBRD_COMPLETE:
            for(i = 0; i < 8; i++)
            {
                led_run_pattern (LED_PATTERN_5);  // green bank A
                _time_delay(50);
                led_run_pattern (LED_PATTERN_OFF); // green bank B
                _time_delay(50);
            }
            break;
            
        case BURNIN_DISPLAY_SOL:
            if(toggle++ % 2)
            {
                led_run_pattern (LED_PATTERN_OFF);
            }
            else
            {
                PWRMGR_charging_display();
            }
            break;
            
        case BURNIN_DISPLAY_WIFI_OK:
            for(i = 0; i < 8; i++)
            {
                led_run_pattern (LED_PATTERN_9);  // green bank A
                _time_delay(200);
                led_run_pattern (LED_PATTERN_14); // green bank B
                _time_delay(200);
            }
            break;
            
#if 0
        case BURNIN_DISPLAY_MUTEX_LOCKED_ERR:
            for(i = 0; i < 10; i++)
            {
                led_run_pattern (LED_PATTERN_11); // red bank b
                _time_delay(500);
                led_run_pattern (LED_PATTERN_5);  // yellow both banks
                _time_delay(500);
                led_run_pattern (LED_PATTERN_OFF);
                _time_delay(500);
            }
            break;
#endif
        case BURNIN_DISPLAY_PWR_DRAIN_TEST:
            led_run_pattern (LED_PATTERN_OFF);  // wink off
            _time_delay(100);
            led_run_pattern (LED_PATTERN_2);    // Solid white
            break;

        case BURNIN_DISPLAY_PWR_DRAIN_TEST2:
            led_run_pattern (LED_PATTERN_3);    // Solid blue
            break;

        case BURNIN_DISPLAY_OFF:
            led_run_pattern (LED_PATTERN_OFF);
            break;

        case BURNIN_DISPLAY_MUTEX_LOCKED_ERR:
        default:
            burnin_display_err(ERR_BURNIN_UNKNOWN_DISPLAY_CMD);
            break;
    }
    g_strobe_ok = 1;  // go back to the more cautious approach.
}

/*
 *   Display an error (forever).
 *   https://whistle.atlassian.net/wiki/display/COR/Burn-in+Guide#Burn-inGuide-ErrorChart
 */
void burnin_display_err(err_t err)
{
    uint32_t num_blinks = 0;
    
    if(ERR_OK == err)
    {
        return;
    }
    
    /*
     *   If the error is factory related, blink LED's that number of times.
     */
    if( (err > ERR_FACTORY_OFFSET) && (err < (ERR_FACTORY_OFFSET + 100)) )
    {
        num_blinks = err - ERR_FACTORY_OFFSET;

        corona_print("$$BURN-IN ERR: 0x%04X : %s\n", err, burnin_errs[num_blinks]);
        g_strobe_ok = ALLOW_WDOG_STROBE_FOREVER;
        _time_delay(3000);
    }
        
    _task_stop_preemption();                 // do not allow task preemption.
    whistle_task_destroy( PWR_TASK_NAME );   // make sure an unplug won't keep us from reporting the error.
    g_strobe_ok = ALLOW_WDOG_STROBE_FOREVER; // allow forever.
    
    while(1)
    {
        uint32_t i;
        
        if(num_blinks > 0)
        {
            /*
             *   We blink the LED's the number corresponding to the error code.
             */
            for(i = 0; i < num_blinks; i++)
            {
                led_run_pattern (LED_PATTERN_1); // red both banks
                _time_delay(400);
                led_run_pattern (LED_PATTERN_OFF);
                _time_delay(200);
            }
        
            led_run_pattern (LED_PATTERN_OFF);
            _time_delay(3000);
        }
        else
        {
            /*
             *   General errors will show up as fast non-stop blinking.
             */
            led_run_pattern (LED_PATTERN_11); // red bank b
            _time_delay(100);
            led_run_pattern (LED_PATTERN_6);  // red bank a
            _time_delay(100);
        }
    }
}

/*
 *   Return whether or not we are running the burn-in test.
 */
boolean burnin_running(void)
{
    return g_burnin_is_running;
}

