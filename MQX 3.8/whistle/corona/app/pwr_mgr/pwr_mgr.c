/*
 * pwr_mgr.c
 *
 *  Created on: Apr 4, 2013
 *      Author: Ben McCrea
 */

#include <mqx.h>
#include <bsp.h>
#include "wassert.h"
#include "app_errors.h"
#include "time_util.h"
#include "sys_event.h"
#include "pwr_mgr.h"
#include "button.h"
#include "main.h"
#include "adc_driver.h"
#include "dis_mgr.h"
#include "cfg_mgr.h"
#include "accdrv_spi_dma.h" 
#include "led_hal.h"
#include "bootflags.h"
#include "button.h"
#include "wmp_datadump_encode.h"
#include "factory_mgr.h"
#include "rtc_alarm.h"
#include "persist_var.h"
#include "corona_console.h"
#include "corona_ext_flash_layout.h"
#include "ext_flash.h"

// This header is needed if Atheros SystemReset() is used
// to initiate a reboot
//#include "ar4100p_main.h"

#include "adc_kadc.h"

// -- Compile Switches

//#define DEBUG_LEDS
#ifdef DEBUG_LEDS
#define SET_LED(led, on) set_debug_led(led, on)
#else
#define SET_LED(led, on)
#endif

// Define this to turn on debug output
//#define PWR_DEBUG_FEATURES

#ifdef PWR_DEBUG_FEATURES
#define D(x)    x
#else
#define D(x)
#endif

// Define this to turn on idle state blinking on LED0
// An irratic blink rate indicates CPU overload
//#define BLINK_IDLE_STATE_ON_DEBUG_LED0

// -- Macros and Datatypes

#define PWRMGR_USB_UNPLUGGED_EVENT      0x00000001
#define PWRMGR_USB_PLUGGED_EVENT        0x00000002
#define PWRMGR_LOW_VBAT_EVENT           0x00000004
#define PWRMGR_BUTTON_WAKEUP_EVENT      0x00000008
#define PWRMGR_DISPLAY_CHARGE_EVENT     0x00000010
#define PWRMGR_CHG_B_RISE_EVENT         0x00000020
#define PWRMGR_CHG_B_FALL_EVENT         0x00000040
#define PWRMGR_LLW_WAKEUP_EVENT         0x00000080
#define PWRMGR_REBOOT_REQUESTED_EVENT   0x00000100
#define PWRMGR_REBOOT_READY_EVENT       0x00000200
#define PWRMGR_REBOOT_NOW_EVENT         0x00000400
#define PWRMGR_RTC_ALARM_EVENT          0x00000800
#define PWRMGR_PGOOD_CHANGE_EVENT       0x00001000
#define PWRMGR_CHG_B_FAULT_EVENT        0x00002000
#define PWRMGR_CHG_B_FAULT_CLEAR_EVENT  0x00004000
#define PWRMGR_SHIP_NOW_EVENT           0x00008000

// Shutdown Delay (# of seconds between low VBAT detect and shut down)
//#define PWRMGR_SHUTDOWN_DELAY       (120)
#define PWRMGR_SHUTDOWN_DELAY       (5000)
#define PWRMGR_SHIP_DELAY           (1000)
#define PWRMGR_REBOOT_DELAY         (250)
#define PWRMGR_REBOOT_GRACE_PERIOD  (10000)
#define PWRMGR_SHIP_GRACE_PERIOD	(3000)

// LLWU wakeup sources
#define PWR_LLWU_BUTTON             0x0001
#define PWR_LLWU_PGOOD_USB          0x0002
#define PWR_LLWU_ACC_INT1_WTM       0x0004
#define PWR_LLWU_ACC_INT2_WKUP      0x0008
#define PWR_LLWU_ME_RTC_ALARM       0x0010
#define PWR_LLWU_WIFI_INT           0x0020  // PTD2 = LLWU_P13

// CHG_B 2Hz filter parameters
#define CHG_B_FILTER_THRESHOLD      3      // # of periods to count
#define CHG_B_FILTER_PERIOD         (750)  // milliseconds

// PGOOD_B holdoff delay
// # of seconds to wait between processing of USB state changes
#define PGOOD_HOLDOFF_DELAY_SECS    2

#define BAT_PERCENT_FW_UPDATE 33
#define BAT_PERCENT_WARN 5

#define NUM_SECONDS_WDOG_ARM	20

typedef enum 
{
    /*
     *   SHIP MODE - All components are in deep sleep state.  Only button or charger interrupt can wake the system.
     */
    PWR_STATE_SHIP,
    
    /*
     *   SLEEP MODE - All components are in sleep state. Button interrupt, charger and accelerometer interrupts 
     *                can wake the system.
     */
    PWR_STATE_SLEEP,
    
    /*
     *   CHARGING MODE - System is charging off of USB, battery is not fully charged
     */
    PWR_STATE_CHARGING,
 
    /*
     * * CHARGED MODE - System is charging off of USB, battery is fully charged
     */
    PWR_STATE_CHARGED,
    
    /*
     *   NORMAL MODE - All components are enabled and system is not charging
     */
    PWR_STATE_NORMAL,
    
    /*
     * BOOTING - The system is booting
     */
    PWR_STATE_BOOTING,
    
    /*
     * REBOOT - The system is getting ready to reboot
     */
    PWR_STATE_REBOOT,
    
    /*
     * REBOOTING - The system is rebooting
     */
    PWR_STATE_REBOOTING
    
    
} PWRMGR_STATE_t;

typedef enum
{
    /*
     * RUNM Level - Fast clock speed and CPU RUNM mode
     */
    PWR_LEVEL_RUNM,
    
    /*
     * VLPR Level - Slow clock speed and CPU VLPR mode
     */
    PWR_LEVEL_VLPR
    
} PWRMGR_LEVEL_t;

// -- Global Private Data
static LWEVENT_STRUCT g_pwrmgr_evt;
static PWRMGR_STATE_t g_pwrmgr_state = PWR_STATE_BOOTING;
static PWRMGR_LEVEL_t g_pwrmgr_level = PWR_LEVEL_RUNM;
static UTIL_timer_t   g_shutdown_timer = 0;
static uint_32        g_runm_power_votes = 0;
static uint_32        g_awake_power_votes = 0;
static uint_32        g_active_power_votes = 0xffffffff;
static uint_32        g_stored_battery_counts = 0;
static uint_32        g_stored_battery_mv = 0;
static uint_32        g_stored_temp_counts = 0;
static LWGPIO_STRUCT  pgood_b_gpio;
static LWGPIO_STRUCT  chg_b_gpio;
static uint_8         g_ship_requested = 0;
static uint_8         g_is_rebooting = 0;
static uint_8         g_en_motion_wakeup = 0;
static uint_8         g_allow_wdog_refresh = 1;
static uint_8         g_wakeup_source = 0;
static uint_8         g_ship_allowed_on_usb_conn = 1;
static _task_id       burnin_id = MQX_NULL_TASK_ID;
static uint_8         g_en_fast_runm = 0;
static uint_8         g_chg_b_filter = 0;
static UTIL_timer_t   g_chg_b_filter_timer = 0;
static boolean        g_pgood_b_changed = FALSE;
static uint_8         g_pgood_holdoff = 0;
static uint_8         g_usb_plugged_state = 0;
static uint_8         g_ota_fwu_allowed = 0;
static uint_64        g_reboot_timestamp_ms = 0;
static uint_32        g_total_ms_to_reboot = 0;


// -- Private Prototypes
static uint_8 PWRMGR_PGOOD_B_Init( void );
static uint_8 PWRMGR_CHG_B_Init( void );
static void PWRMGR_TimerShip_Cbk(  void );
static void PWRMGR_CHG_B_Filter_Callback( void );
static void PWRMGR_ButtonShutdown_Callback( sys_event_t sys_event  );
static void PWRMGR_DisplayCharge_Callback( sys_event_t sys_event );
static void PWRMGR_Lowbatt_Callback( ADC_MemMapPtr adc_ptr );
static void PWRMGR_pgood_b_holdoff_Callback( void );
static void PWRMGR_PGOOD_B_int_action( void );
static void PWRMGR_usb_gpio_init( void );
static void PWRMGR_reboot_now(void);
static err_t pwrmgr_enter_runm( void );
static err_t pwrmgr_enter_vlpr( void );
static err_t pwrmgr_exit_vlpr( void );
static void pwrmgr_execute_reboot( void );
static void pwrmgr_config_llwu( uint_16 wakeup_source );
static uint_8 pwrmgr_debounce_pin( LWGPIO_STRUCT *pin );
static void pwrmgr_execute_usb_plugged( void );
static void pwrmgr_execute_usb_unplugged( void );
static void pwrmgr_init_low_power( void );
static void pwrmgr_set_26MHZ_BT( LWGPIO_VALUE value );
static int32_t PWRMGR_calc_batt_percent(uint32_t battery_counts);
static void pwrmgr_start_pgood_monitoring( void ); 
static void pwrmgr_breadcrumb(PWRMGR_STATE_t state);
static void pwrmgr_prep_for_lls( void );
static void pwrmgr_execute_ship( void );
void pwrmgr_enter_ship( void );

/*****************************************************************************
*
*   @name        PWRMGR_alpha_runtime_current_improve
*
*   @brief       Various runtime improvements for power, mainly targeted for Alpha release.
*
*   @param       void
*
*   @return      err_t code
**
*****************************************************************************/
void PWRMGR_alpha_runtime_current_improve( void )
{    
    /*
     *   Disable USB when USB_ENABLED == FALSE.
     *     Moving this logic to usb_mk60.c
     */
#ifndef USB_ENABLED
	wint_disable(); // SIM_* can be accessed in other drivers
    SIM_SCGC4 |= (SIM_SCGC4_USBOTG_MASK); // enable temporarily while you write to the module.
    SIM_SOPT1CFG |= SIM_SOPT1CFG_URWE_MASK /*| SIM_SOPT1CFG_UVSWE_MASK | SIM_SOPT1CFG_USSWE_MASK*/;
    SIM_SOPT1 &= ~(SIM_SOPT1_USBREGEN_MASK/* | SIM_SOPT1_USBSSTBY_MASK | SIM_SOPT1_USBVSTBY_MASK*/);
    USB0_INTEN = 0; // Disable all USB Interrupts
    USB0_CTL &= ~USB_CTL_USBENSOFEN_MASK; // disable USB module.
    SIM_SCGC4 &= ~(SIM_SCGC4_USBOTG_MASK); // clock gate USB.
    wint_enable();
#endif
    
    /*
     *   Ernie 4/31/2013
     *   Putting Atheros chip in power mode '1' saved about 45mA !  This is in ar4100p_main.c.
     */
    //SET_POWER_MODE("1\0");
    
    /*
     *   Ernie 4/31/2013
     *   MQX Idle loop (idletask.c), added going into wait when in idle loop. saves about 18mA
     */
    
    /*
     *   Ernie 4/31/2013 
     *   Changing SIMCLKDIV1:OUTDIV1 (and probably less so OUTDIV4) in bsp_cm.c, saved ~9mA for idle current.
     *   Also clock gated and reduced freq of FLEXBUS, which didn't seem to save a lot.
     */
    
    /*
     *   Ernie 5/1/2013
     *   CRC Clock gated except when in use.  Probably neglible, but still an improvement.
     */
    
    /*
     *   Ernie 5/1/2013
     *   Clock gate the VREF module (it was already disabled and not being used, since we supply ext reference for ADC).
     *   This is happening inside of adc_driver.c, idle current savings seemed neglible.
     *   //	wint_disable(); // SIM_* can be accessed in other drivers
     *   //SIM_SCGC4 &= ~( SIM_SCGC4_VREF_MASK );
     *   // wint_enable();
     */
    
    /*
     *    Ernie 5/1/2013
     *    CPU Clock = Bus Clock = FLASH Clock = 6MHz
     *    Atheros Clock (init_spi.c) = 6MHz
     *    These changes accounted for about 12mA of idle current.
     */
    
    /*
     *    Ernie 5/1/2013
     *    Ethernet - put it into sleep mode (this had no effect, and hardware put it back anyway).
     */
#if 0
	wint_disable(); // SIM_* can be accessed in other drivers
    SIM_SCGC2 |= (SIM_SCGC2_ENET_MASK);  // temporarily enable clock gate so we can put into sleep mode.
    ENET_ECR |= (ENET_ECR_STOPEN_MASK | ENET_ECR_DBGEN_MASK | ENET_ECR_SLEEP_MASK);
    SIM_SCGC2 &= ~(SIM_SCGC2_ENET_MASK); // clock gate again.
    wint_enable();
#endif
    
    /*
     *    Ernie 5/1/2013
     *    Using DMX32 field of MCG_C4 register in bsp_cm.c and coronak60n512.h to switch from
     *       96MHz --> 24MHz saved about 1.5mA at idle, and probably a lot more on average.
     */
    
}

// -- Callbacks and  Interrupt Handlers


/*****************************************************************************
*
*   @name        PWRMGR_LLW_INT
*
*   @brief       Interrupt Handler for the LLWU module interrupt
*
*   @param       None
*
*   @return      None
**
*****************************************************************************/
static void PWRMGR_llw_int(void *unused)
{ 
    // Disable LLWU wakeup sources 
    LLWU_PE1 = LLWU_PE2 = LLWU_PE4 = LLWU_ME = 0; 
    
    g_wakeup_source = 0;
    
    // Sync MQX time to RTC time. It's best to do this right away so that
    // executed actions have the correct time
    wassert( _rtc_sync_with_mqx(TRUE) == MQX_OK );
    
    if ( LLWU_F1 & LLWU_F1_WUF0_MASK )
    {
        // Button pressed: handle the pin change 
        button_handler_action();      
        g_wakeup_source |= PWR_LLWU_BUTTON;  
    }
    
    if ( LLWU_F1 & LLWU_F1_WUF1_MASK )
    {
    	// PGOOD_B transition: handle the pin change
    	PWRMGR_PGOOD_B_int_action();
    	g_wakeup_source |= PWR_LLWU_PGOOD_USB;
    }

    if ( LLWU_F1 & LLWU_F1_WUF4_MASK )
    {
        // ACC_INT1 = WTM (Fifo Ready)
        
        // Action for WTM pin assertion
        // NOTE: normally this is called by the ACC_INT1 interrupt handler
        // but it apparently doesn't trigger when the pin assertion causes a 
        // wakeup from LLS
        accdrv_wtm_action();
        g_wakeup_source |= PWR_LLWU_ACC_INT1_WTM; 
    }

    if ( LLWU_F1 & LLWU_F1_WUF7_MASK)
    {
        // ACC_INT2 = WAKEUP (Motion Detect)
        
        // Action for WTM pin assertion
        accdrv_wakeup_action();
        g_wakeup_source |= PWR_LLWU_ACC_INT2_WKUP;
    }
    
    if ( LLWU_F3 & LLWU_F3_MWUF5_MASK )
    {    	
    	// RTC Alarm = LLWU_ME_WUME5
        
        // NOTE: this case is apparently never fired, so in the case of an RTC alarm
        // g_wakeup_source will be 0x00. 
        // When RTC alarms cause a LLWU wakeup, this (LLW_INT) interrupt fires but no flag
        // is set in LLWU_F3 are set. The RTC alarm interrupt handler fires on its own following 
        // the wakeup, which is distinctly different from how pin wakeups
        // are processed by the K60 (in the case of pin wakeups, the pin interrupt handler
        // doesn't execute and the flag corresponding to the wakeup source
        // is set in LLWU_F1 or LLWU_F2). 
    	
    	// Action for the RTC Alarm interrupt 
    	RTC_int_action();
    	g_wakeup_source |= PWR_LLWU_ME_RTC_ALARM;
    }

#if LLS_DURING_WIFI_USAGE
    if( LLWU_F2 & LLWU_F2_WUF13_MASK )
    {
        // WIFI INT -- poke the Atheros driver
        //wifi_high_pwr_cbk();
        Custom_HW_InterruptHandler(NULL);
        g_wakeup_source |= PWR_LLWU_WIFI_INT;
    }
    LLWU_F2 = 0xff;
#endif
    // Clear LLWU wakeup flags  
    LLWU_F1 = 0xff; // pin sources 0-7
    LLWU_F3 = 0xff; // module sources 0-7
        
    // Raise the LLW wakeup event
    _lwevent_set(&g_pwrmgr_evt, PWRMGR_LLW_WAKEUP_EVENT );
}

// NOTE: this event is used only for debug, to indicate that the RTC alarm has happened
void PWRMGR_rtc_alarm_event( void )
{
    _lwevent_set(&g_pwrmgr_evt, PWRMGR_RTC_ALARM_EVENT );
}

/*
 *   Set to zero if you don't want to allow going into ship mode
 *      when the charger is connected (supports FW-930).
 *      
 *   Set to non-zero if ship mode is OK when USB is connected.
 *      This case is more for the "pwr ship", "gto" scenario at the factory.
 */
void PWRMGR_ship_allowed_on_usb_conn( uint_8 ok )
{
    g_ship_allowed_on_usb_conn = ok;
}

// This callback is called when the CHG_B filter timer expires.
static void PWRMGR_CHG_B_Filter_Callback( void )
{
    if ( g_chg_b_filter >= CHG_B_FILTER_THRESHOLD )
    {
        // Timer Fault event was previously sent. Now send the Timer Fault Clear event
        // Send the Timer Fault event
        _lwevent_set(&g_pwrmgr_evt, PWRMGR_CHG_B_FAULT_CLEAR_EVENT );
    }
    g_chg_b_filter = 0;
    g_chg_b_filter_timer = 0;
}

/*
 * Callback for the PGOOD_B holdoff timer. Clears the holdoff flag and sends a PGOOD_B change
 * event to initiate processing of any PGOOD_B change that may have happened during the holdoff 
 * period. 
 */
static void PWRMGR_pgood_b_holdoff_Callback( void )
{
    PWRMGR_VoteForAwake( PWRMGR_PWRMGR_VOTE );

    // Reset the holdoff flag 
    g_pgood_holdoff=0;
            
    // Send the PGOOD Change event which will cause the PGOOD_B status to be checked     
    // and appropriate action taken
    _lwevent_set(&g_pwrmgr_evt, PWRMGR_PGOOD_CHANGE_EVENT );
}

/*
 * Action for the PGOOD_B interrupt. This action handler may be called by either
 * the PGOOD_B pin interrupt handler or by the LLWU interrupt handler upon wakeup due
 * to a PGOOD_B pin change.
 */
static void PWRMGR_PGOOD_B_int_action( void )
{   
	// Vote here since we don't always send PWRMGR_PGOOD_CHANGE_EVENT. We would otherwise vote in the main
	// task where we process PWRMGR_PGOOD_CHANGE_EVENT
    PWRMGR_VoteForAwake( PWRMGR_PWRMGR_VOTE );

    // Only send a change event if the holdoff timer isn't running
    if ( !g_pgood_holdoff )
    {
        _lwevent_set(&g_pwrmgr_evt, PWRMGR_PGOOD_CHANGE_EVENT );
    }
    
    // Set the PGOOD_B change flag to TRUE 
    g_pgood_b_changed = TRUE;    
}

/*****************************************************************************
*
*   @name        PWRMGR_PGOOD_B_int
*
*   @brief       Interrupt Handler for the PGOOD_B pin interrupt
*
*   @param       void *pin
*
*   @return      None
**
*****************************************************************************/
static void PWRMGR_PGOOD_B_int(void *pin)
{
    lwgpio_int_clear_flag(&pgood_b_gpio);
    
    // Execute PGOOD_B interrupt action
    PWRMGR_PGOOD_B_int_action();
}

/*****************************************************************************
*
*   @name        PWRMGR_CHG_B_int
*
*   @brief       Interrupt Handler for the CHG_B pin interrupt
*
*   @param       void *pin
*
*   @return      None
**
*****************************************************************************/
static void PWRMGR_CHG_B_int(void   *pin)
{
    lwgpio_int_clear_flag((LWGPIO_STRUCT_PTR)pin);
    
    // If USB is still plugged in, raise the local CHG_B de-assertion event
    // Note: PGOOD_B=0 --> USB plugged in
    if (0 == pwrmgr_debounce_pin(&pgood_b_gpio) )                    
    {
        if( 0 == lwgpio_get_value( &chg_b_gpio ) )
        {
            // CHG_B Falling Edge
            
            /*
             *   CHG_B signal was wasserted, so need to show "is charging".
             */
            _lwevent_set(&g_pwrmgr_evt, PWRMGR_CHG_B_FALL_EVENT );
        }
        else
        {
            // CHG_B Rising Edge 

            /*
             *   CHG_B signal was de-wasserted, so show "fully charged".
             */
            _lwevent_set(&g_pwrmgr_evt, PWRMGR_CHG_B_RISE_EVENT );
        }
    }
}

/*****************************************************************************
*
*   @name        PWRMGR_TimerShutdown_Callback
*
*   @brief       Callback routine executed to enter ship mode when the shutdown 
*                timer expires
*
*   @param       None
*
*   @return      None
**
*****************************************************************************/
static void PWRMGR_TimerShip_Cbk( void )
{
    g_shutdown_timer = 0;
    _lwevent_set(&g_pwrmgr_evt, PWRMGR_SHIP_NOW_EVENT );
}

/*****************************************************************************
*
*   @name        PWRMGR_TimerReboot_Callback
*
*   @brief       Callback routine executed to reboot when the shutdown 
*                timer expires
*
*   @param       None
*
*   @return      None
**
*****************************************************************************/
static void PWRMGR_TimerReboot_Callback( void )
{   
    _lwevent_set(&g_pwrmgr_evt, PWRMGR_REBOOT_NOW_EVENT );
}

/*****************************************************************************
*
*   @name        PWRMGR_AllButton_Callback
*
*   @brief       Response on system wake-up due to a button push 
*                NOTE: Not implemented in ALPHA
*
*   @param       sys_event
*
*   @return      None
**
*****************************************************************************/
void PWRMGR_WakeupButton_Callback( sys_event_t sys_event )
{
    // Raise the local All Button event
    _lwevent_set(&g_pwrmgr_evt, PWRMGR_BUTTON_WAKEUP_EVENT );  
}

// -- Private Functions

/*****************************************************************************
*
*   @name       PWRMGR_PGOOD_B_Init
*
*   @brief       This function initializes the PGOOD_B pin
*
*   @param       None
*
*   @return      None
**
*****************************************************************************/
static uint_8 PWRMGR_PGOOD_B_Init(void)
{
    _mqx_uint result;
    err_t status = ERR_OK;
    
    wint_disable(); // disable interrupts

    // initialise PGOOD_B for input
    if (!lwgpio_init(&pgood_b_gpio, BSP_GPIO_PGOOD_B_PIN, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE))
    {
        status = ERR_PWRMGR_INVALID_GPIO_INIT;
        goto PGOOD_INIT_DONE;
    }
    
    // Disable the pin interrupt, in case it's already enabled
    lwgpio_int_enable(&pgood_b_gpio, FALSE);
    
    // Configure as GPIO input pin
    lwgpio_set_functionality(&pgood_b_gpio, BSP_GPIO_PGOOD_B_MUX_GPIO);
    
    // Enable pin pull-up
    lwgpio_set_attribute(&pgood_b_gpio, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_ENABLE);
    
    // Enable pin low-pass filter
    lwgpio_set_attribute(&pgood_b_gpio, LWGPIO_ATTR_FILTER, LWGPIO_AVAL_ENABLE);  

    // Enable both rising and falling edge interrupts    
    if ( !lwgpio_int_init(&pgood_b_gpio, LWGPIO_INT_MODE_FALLING | LWGPIO_INT_MODE_RISING))
    {
        status = ERR_PWRMGR_INVALID_GPIO_INIT;
        goto PGOOD_INIT_DONE;
    }
    
    // install the PGOOD_B handler using GPIO port isr mux 
    whistle_lwgpio_isr_install(&pgood_b_gpio, BSP_PORTE_INT_LEVEL, PWRMGR_PGOOD_B_int);   
   
PGOOD_INIT_DONE:
    wint_enable(); // enable interrupts
    return status;
}

/*****************************************************************************
*
*   @name       PWRMGR_CHG_B_Init
*
*   @brief       This function initializes the CHG_B pin
*
*   @param       None
*
*   @return      None
**
*****************************************************************************/
static uint_8 PWRMGR_CHG_B_Init(void)
{
    _mqx_uint result;

    // initialize CHG_B for input
    if (!lwgpio_init(&chg_b_gpio, BSP_GPIO_CHG_B_PIN, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE))
    {
        return ERR_PWRMGR_INVALID_GPIO_INIT;
    }
    
    /* disable in case it's already enabled */
    lwgpio_int_enable(&chg_b_gpio, FALSE);
    
    lwgpio_set_functionality(&chg_b_gpio, BSP_GPIO_CHG_B_MUX_GPIO);
    lwgpio_set_attribute(&chg_b_gpio, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_ENABLE);

    // enable gpio functionality for given pin, react on rising edge
    if (!lwgpio_int_init(&chg_b_gpio, LWGPIO_INT_MODE_FALLING | LWGPIO_INT_MODE_RISING))
    {
        return ERR_PWRMGR_INVALID_GPIO_INIT;
    }
    
    // The CHG_B interrupt is disabled by default (enabled after USB is plugged in)
    lwgpio_int_enable(&chg_b_gpio, FALSE);
    
    // install the CHG_B handler using GPIO port isr mux 
    whistle_lwgpio_isr_install(&chg_b_gpio, BSP_PORTD_INT_LEVEL, PWRMGR_CHG_B_int);

    return ERR_OK;
}

static void pwrmgr_breadcrumb(PWRMGR_STATE_t state)
{
#if PWRMGR_BREADCRUMB_ENABLED
    TIME_STRUCT timestamp;
    uint32_t bat_counts = adc_sample_battery();
    extern err_t pwr_corona_print(const char *format, ...);
    
    RTC_get_time_ms( &timestamp );
    
	pwr_corona_print("%d %d %d %d %02x %02x %02x %d %d %d\n", timestamp.SECONDS % (24*60*60), timestamp.MILLISECONDS, state, g_pwrmgr_level,
			g_runm_power_votes, __SAVED_ACROSS_BOOT.on_power_votes, g_awake_power_votes, PWRMGR_calc_batt_percent(bat_counts), WIFIMGR_wifi_is_on(), BTMGR_bt_is_on());
#endif
}

/*
 * Turn UART5 by unmasking the transmit, receive and interrupt enables
 */
static void pwrmgr_enable_uart5( void )
{
    UART_MemMapPtr p_uart5 = _bsp_get_serial_base_address(5); // UART5
    // Enable the UART5 clock
	wint_disable(); // SIM_* can be accessed in other drivers
    SIM_SCGC1 |= SIM_SCGC1_UART5_MASK;
    wint_enable();
    // Turn on UART transmit, receive and interrupt enables
    p_uart5->C2 |= (UART_C2_TE_MASK | UART_C2_RE_MASK |
                    UART_C2_TIE_MASK | UART_C2_RIE_MASK );
   
}

/*
 * Turn off UART5 by masking off the transmit, receive and interrupt enables.
 * Returns a status indicating whether or not the UART was enabled.
 */
static uint8_t pwrmgr_disable_uart5( void )
{
    UART_MemMapPtr p_uart5 = _bsp_get_serial_base_address(5); // UART5
    uint8_t is_on = (p_uart5->C2 & UART_C2_TE_MASK) ? 1 : 0;
    // Enable the UART5 clock
	wint_disable(); // SIM_* can be accessed in other drivers
    SIM_SCGC1 |= SIM_SCGC1_UART5_MASK;
    wint_enable();
    // Only turn off the UART if it's not already off
    if (is_on)
    {
        // Turn off UART transmit, receive and interrupt enables
        p_uart5->C2 &= ~(UART_C2_TE_MASK | UART_C2_RE_MASK |
                         UART_C2_TIE_MASK | UART_C2_RIE_MASK );
    }
    return is_on;
}

// Enter VLPR power mode
static err_t pwrmgr_enter_vlpr( void )
{
    err_t err = ERR_OK;
    int i, tries;
    MQX_FILE_PTR fd = NULL;
    uint8_t uart5_on;
    
    // Raise a vote to stay awake while entering VLPR
    PWRMGR_VoteForAwake( PWRMGR_PWRMGR_VOTE );
    
#ifdef KERNEL_TIME_SANITY_CHECK
    whistle_time_t timestart, timeend;
    UTIL_time_get(&timestart);
    D( corona_print("VLPR START %d\n", timestart.TICKS[0]) );
#endif
    
    if ( PWR_LEVEL_VLPR == g_pwrmgr_level )
    {
        D( corona_print("VLPR %x\n", g_runm_power_votes) );
        // Allow LLS if it's enabled
        if ( g_st_cfg.pwr_st.enable_lls_deep_sleep )
        {
            PWRMGR_VoteForSleep( PWRMGR_PWRMGR_VOTE );
        }
        return ERR_OK;
    }
    
    // Wait for I2C to be free (obtain semaphore)
    I2C1_dibbs( &fd );
    
    // The following ensures that the clock switch won't happen while the UART is
    // in the middle of transmitting bytes
    _task_stop_preemption(); 
    fflush(stdout);

    wint_disable(); // disable interrupts
    
    // Disable UART5 transmit, receive and interrupt enables
    uart5_on = pwrmgr_disable_uart5();
   
    // Switch to clock config 2 (clock = 4mhz fast irc)
    _bsp_set_clock_configuration( CPU_CLOCK_CONFIG_2 );
 
    // Set The VLPR mode bit
    SMC_PMCTRL |= (SMC_PMCTRL_RUNM(2));   

    // Wait for the VLPR flag to be set in PMSTAT
    tries = 100;
    do 
    {
        // Wait for VLPR flag to be set
        for(i=0;i<30;i++)
        {
            asm("NOP");
        }
        if (!--tries)
        {
            // Error! Reenter RUNM and flag an error.
            // Don't bother restoring baudrates since they were never changed.
            pwrmgr_enter_runm();
            err = ERR_PWRMGR_VLPR_ERROR;
            goto ENTER_VLPR_DONE;
        }
    } while( (SMC_PMSTAT & SMC_PMSTAT_PMSTAT(4)) != SMC_PMSTAT_PMSTAT(4) );

    if ( uart5_on )
    {
        pwrmgr_enable_uart5();
        // Reinit UART5 baudrate
        _kuart_change_baudrate (_bsp_get_serial_base_address(5), VLPR_BUS_CLOCK, BSPCFG_SCI5_BAUD_RATE);
    }

    // Reinit external flash SPI baudrate
    ext_flash_set_baud( SPANSION_VLPR_BAUD_RATE ); 
        
    if (accdrv_spi_is_enabled())   
    {
        // Reinit accelerometer SPI baudrate
        accdrv_spidma_set_baud( ACCEL_BAUD_RATE ); 
    }
          
    g_pwrmgr_level = PWR_LEVEL_VLPR;
    
    // The following line is important and is legal because the system tick interrupt isn't
    // disabled by wint_disable().
    // It waits for the next system tick to expire so that the kernel time functions, 
    // which read the systick counter register to calculate time, aren't incorrect.
    // NOTE: The SYST_CSR register has an enable bit for the System Tick timer. It might be
    // a good idea to disable the system timer in _bsp_set_clock_configuration() above to
    // prevent a system tick interrupt from happening while the kernel timer parameters are
    // being re-initalized.
    _time_delay(1);
    
    wint_enable(); // re-enable interrupts
    
        
ENTER_VLPR_DONE:
    // Release I2C bus
    wassert(ERR_OK == I2C1_liberate( &fd ));
     
    // This delay may help to avoid UART5 garbage problem
    _time_delay(100);
    
    _task_start_preemption();

#ifdef KERNEL_TIME_SANITY_CHECK    
    UTIL_time_get(&timeend);
    D( corona_print("VLPR TIMESTART %d TIMEEND %d\n", timestart.TICKS[0], timeend.TICKS[0]) );
#endif

    wassert(ERR_OK == PROCESS_NINFO( err, NULL ));
    
    // All finished: Allow LLS if it's enabled
    if ( g_st_cfg.pwr_st.enable_lls_deep_sleep )
    {
        PWRMGR_VoteForSleep( PWRMGR_PWRMGR_VOTE );
    }
    
    pwrmgr_breadcrumb(g_pwrmgr_state);

    return err;   
}

// Exit VLPR to RUNM, without changing clock configuration
static err_t pwrmgr_exit_vlpr( void )
{
    int i, tries;
    
    // check to make sure in VLPR before exiting 
    if  ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4) 
    {
       // Clear RUNM
       SMC_PMCTRL &= ~(SMC_PMCTRL_RUNM(0x3));
    
       //Wait for normal RUN regulation mode to be confirmed 
       // 0 MCU is not in run regulation mode
       // 1 MCU is in run regulation mode
       tries = 500;
       while(!(PMC_REGSC & PMC_REGSC_REGONS_MASK)) 
       {
           for(i=0;i<30;i++)
           {
               asm("NOP");
           }
           if (!--tries)
           {
               return ERR_PWRMGR_RUNM_ERROR;
           }
       }
    }
    return ERR_OK;
}

// Enter RUNM mode
static err_t pwrmgr_enter_runm( void )
{ 
    err_t err = ERR_OK;
    uint_8 device_status;
    MQX_FILE_PTR fd = NULL;
    uint8_t uart5_on;
    uint8_t clk_config = g_en_fast_runm ? CPU_CLOCK_CONFIG_1 : CPU_CLOCK_CONFIG_0;

#ifdef KERNEL_TIME_SANITY_CHECK
    whistle_time_t timestart, timeend;
    UTIL_time_get(&timestart);
    D( corona_print("RUNM START %d\n", timestart.TICKS[0]) );
#endif
    
    if ( PWR_LEVEL_RUNM == g_pwrmgr_level )
    {
        D( corona_print("in RUNM %x\n", g_runm_power_votes) );
        return ERR_OK;
    }
    // Raise a vote to keep us awake until RUNM is successfully entered
    PWRMGR_VoteForAwake( PWRMGR_PWRMGR_VOTE );
    
    // Wait for I2C to be free (obtain semaphore)
    I2C1_dibbs( &fd );
    
    // The following ensures that the clock switch won't happen while the UART is
    // in the middle of transmitting bytes
    _task_stop_preemption(); 
    fflush(stdout);
    
    wint_disable(); // disable interrupts
    
    // Disable UART5 transmit, receive and interrupt enables
    uart5_on = pwrmgr_disable_uart5();
    
    // Clear the RUNM bit and wait for RUN mode regulation 
    err = pwrmgr_exit_vlpr();
    if (ERR_OK != err)
    {
        goto ENTER_RUNM_DONE;
    }
   
    // Restore RUNM clock configuration: 24MHz or 96MHz 
    _bsp_set_clock_configuration( clk_config );
        
        
    // Reinit external flash SPI baudrate
    ext_flash_set_baud( SPANSION_RUNM_BAUD_RATE ); 
        
    if (accdrv_spi_is_enabled())   
    {
        // Reinit accelerometer SPI baudrate
        accdrv_spidma_set_baud( ACCEL_BAUD_RATE ); 
    }

    if ( uart5_on )
    {
        pwrmgr_enable_uart5();
        // Reinit UART5 baudrate
        _kuart_change_baudrate (_bsp_get_serial_base_address(5), 
                                _bsp_get_clock( _bsp_get_clock_configuration(), CM_CLOCK_SOURCE_BUS ),
                                BSPCFG_SCI5_BAUD_RATE);
    }
    
    // The following line is important and is legal because the system tick interrupt isn't
    // disabled by wint_disable().
    // It waits for the next system tick to expire so that the kernel time functions, 
    // which read the systick counter register to calculate time, aren't incorrect.
    // NOTE: The SYST_CSR register has an enable bit for the System Tick timer. It might be
    // a good idea to disable the system timer in _bsp_set_clock_configuration() above to
    // prevent a system tick interrupt from happening while the kernel timer parameters are
    // being re-initalized.
    _time_delay(1);
   
    wint_enable(); // re-enable interrupts
    
ENTER_RUNM_DONE:
    // Release I2C bus
    wassert(ERR_OK == I2C1_liberate( &fd ));  
    
    _task_start_preemption();
        
    g_pwrmgr_level = PWR_LEVEL_RUNM;
    
    // Allow LLS if it's enabled
    if ( g_st_cfg.pwr_st.enable_lls_deep_sleep )
    {
        PWRMGR_VoteForSleep( PWRMGR_PWRMGR_VOTE );
    }
    
    if ( ERR_OK == err )
    {
        D( corona_print("RUNM %x\n", g_runm_power_votes) );
    }
    else
    {
        PROCESS_NINFO( err, "%x", g_runm_power_votes );
    }
    
#ifdef KERNEL_TIME_SANITY_CHECK
    UTIL_time_get(&timeend);
    D( corona_print("RUNM TIMESTART %d TIMEEND %d\n", timestart.TICKS[0], timeend.TICKS[0]) );
#endif
    
    wassert(ERR_OK == err);
    
    pwrmgr_breadcrumb(g_pwrmgr_state);
    
    return err;
}

void pwrmgr_enter_ship( void )
{       
	persist_reboot_reason_t reboot_reason = PV_REBOOT_REASON_SHIP_MODE;

	D( corona_print("Ship\n") );
    g_pwrmgr_state = PWR_STATE_SHIP;
    
    // Turn off calling task's watchdog
    _watchdog_stop();
    
    // Make sure there are no outstanding writes to SPI Flash.
    ext_flash_shutdown( 1000 );
    
    // Don't allow anyone else to run, since we've made the decision to enter ship mode.
    wint_disable();
    _task_stop_preemption();
    
    // Turn off WiFi and BT chips
    PWRMGR_radios_OFF();
    
    // Turn off user LEDs
    led_deinit();

    // Turn off debug LEDs
    set_debug_led(0,0);
    set_debug_led(1,0);
    set_debug_led(2,0);
    set_debug_led(3,0);    

    // Turn off UART0, UART3
    SIM_SCGC4 |= SIM_SCGC4_UART0_MASK | SIM_SCGC4_UART3_MASK;
    UART0_C2 &= ~(UART_C2_RE_MASK | UART_C2_TE_MASK);
    UART3_C2 &= ~(UART_C2_RE_MASK | UART_C2_TE_MASK);
    
    // Turn on UART5 clock, then deassert the transmit and receive enabled.
    SIM_SCGC1 |= SIM_SCGC1_UART5_MASK; 
    UART5_C2 &= ~(UART_C2_RE_MASK | UART_C2_TE_MASK);
    
    // Turn off clocks for all modules that aren't needed during ship mode
    SIM_SCGC1 = 0; // UART4, UART5
    SIM_SCGC2 = 0; // ENET, DAC0, DAC1
    SIM_SCGC3 = 0; // RNGA, FLEXCAN1, SPI2, SDHC, FTM2, ADC1, 
    
    // SIM_SCGC4: EWM, CMT, I2C0, I2C1, UART0, UART1, UART2, UART3, USBOTG, CMP, VREF, LLWU
    // Turn off all SIM_SCGC4 clocks except for LLWU and EWM (Watchdog module)
    SIM_SCGC4 = SIM_SCGC4_LLWU_MASK | SIM_SCGC4_EWM_MASK;
    
    // WARNING!! The following line (SIM_SCGC5) causes an unexpected interrupt to take place when VLLS3 is entered.
    // TODO: determine if some of these modules can be clock gated. In particular, watch out for GPIO ports that
    // might be needed for LLWU wakeup.   
    //SIM_SCGC5 &= ~(SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK |\
    //               SIM_SCGC5_PORTD_MASK | SIM_SCGC5_PORTE_MASK | SIM_SCGC5_LPTIMER_MASK);
    
    // TODO: verify if it's ok to turn off clock to DMAMUX and all peripherals at this time. Turn off heartbeat to avoid
    // write to I2C and use of ADC0. This is for ship so it's ok to disable the RTC. Verify that it's disabled & interrupt off.
    SIM_SCGC6 &= ~(SIM_SCGC6_RTC_MASK | SIM_SCGC6_ADC0_MASK | SIM_SCGC6_FTM0_MASK | SIM_SCGC6_FTM1_MASK |\
    SIM_SCGC6_PIT_MASK | SIM_SCGC6_PDB_MASK | SIM_SCGC6_USBDCD_MASK | SIM_SCGC6_SPI0_MASK | \
    SIM_SCGC6_CRC_MASK | SIM_SCGC6_I2S_MASK | SIM_SCGC6_SPI1_MASK | SIM_SCGC6_DMAMUX_MASK | \
    SIM_SCGC6_FLEXCAN0_MASK );
    
    // TODO: verify if it's safe to turn off MPU and DMA clocks at this time. ACCEL should be turned off. Is it?
    SIM_SCGC7 = 0; // MPU, DMA, FLEXBUS
        
    // In certain cases, the presence of USB charging is enough to not go into and ship,
    //    and instead reboot the system.
    if( lwgpio_get_value(&pgood_b_gpio) || g_ship_allowed_on_usb_conn )
    {
        
        /*
         *   This crumb will hopefully be retrieved/logged on the next boot up,
         *     unless we lose power.
         */
        persist_set_untimely_crumb( BreadCrumbEvent_EXITING_SHIP_MODE );
        PMEM_VAR_SET(PV_REBOOT_REASON, &reboot_reason);
        
        PWRMGR_enter_vlls3();
    }
    else
    {
        PWRMGR_reboot_now();
    }
    
    // NOTE:  We should never get here
    
    // Set 1010 pattern on debug LEDs to indicate we didn't make it to ship mode
    set_debug_led(0,1);
    set_debug_led(1,0);
    set_debug_led(2,1);
    set_debug_led(3,0);    
}

static void pwrmgr_execute_reboot( void )
{
    // Call the Atheros system reset
    PWRMGR_reset(TRUE);
}

// TODO: Remove me after Alpha
#include "time_util.h"
static void alpha_heartbeat_hack_timer(_timer_id id, pointer data_ptr, uint_32 seconds_not_used, uint_32 ms_not_used)
{
#if !FTA_WHISTLE
    _lwevent_set(&g_pwrmgr_evt, PWRMGR_DISPLAY_CHARGE_EVENT );  
#endif
}

static void pwrmgr_execute_usb_plugged( void )
{   
    g_pwrmgr_state = PWR_STATE_CHARGING;
    pwrmgr_breadcrumb(g_pwrmgr_state);
    
#ifdef USB_ENABLED    
    // Enter RUNM mode
    PWRMGR_VoteForRUNM( PWRMGR_USB_VOTE );   
    usb_allow(); // allow USB console to initialise and run
#endif
    
    // Shut down ACCMGR while charging (if it's not already shut down)
    if ( g_st_cfg.acc_st.shutdown_accel_when_charging && !ACCMGR_is_shutdown() )
    {
        ACCMGR_shut_down( false );
    }
    
    PWRMGR_charging_display();
            
    // Enable the CHG_B interrupt
    lwgpio_int_enable(&chg_b_gpio, TRUE);
}

static void pwrmgr_execute_usb_unplugged( void )
{    
    /*
     *   If we had previously shutdown the accelerometer on USB plug, start it back up on unplug.
     */
    if( ACCMGR_is_shutdown() )
    {
        wassert( ERR_OK == ACCMGR_start_up() );
    }
        
    // Disable the CHG_B interrupt
    lwgpio_int_enable(&chg_b_gpio, FALSE);
    
    // Turn off charging LEDs
    DISMGR_run_pattern( DISMGR_PATTERN_BATT_CHARGE_STOP );
    _time_delay(50);
    
    g_pwrmgr_state = PWR_STATE_NORMAL;
    pwrmgr_breadcrumb(g_pwrmgr_state);
    
#ifdef USB_ENABLED
    // TODO: Shut down USB task/driver
    
    // If no RUNM vote is outstanding, enter VLPR mode
    PWRMGR_VoteForVLPR( PWRMGR_USB_VOTE);
#endif
}

static void pwrmgr_config_llwu( uint_16 wakeup_source )
{
    uint_8 regval;
    
    if ( wakeup_source == 0)
    {
        LLWU_PE1 = LLWU_PE2 = LLWU_PE3 = LLWU_PE4 = 0; 
        return;
    }
    
    regval = LLWU_PE1; // get current settings
    if ( wakeup_source & PWR_LLWU_BUTTON )
    {
        // button (PTE1) maps to wakeup source LLWU_P0
        regval |= LLWU_PE1_WUPE0( 0x02 ); // falling edge
        LLWU_F1 |= LLWU_F1_WUF0_MASK;   // clear the flag for LLWU_P0
    }
    if ( wakeup_source & PWR_LLWU_PGOOD_USB )
    {
        // PGOOD (PTE2) maps to wakeup source LLWU_P1
        //regval |= LLWU_PE1_WUPE1( 0x03 ); // Wake on rising or falling edge
        regval |= LLWU_PE1_WUPE1( 0x02 ); // (insertion only, not an ejection).
        LLWU_F1 |= LLWU_F1_WUF1_MASK;   // clear the flag for LLWU_P1
    }
    LLWU_PE1 = regval;
    
    regval = LLWU_PE2;
    if ( wakeup_source & PWR_LLWU_ACC_INT1_WTM )
    {
        // ACC_INT1 = PTA13 = LLWU_P4
        regval |= LLWU_PE2_WUPE4( 0x01 ); // rising edge
        LLWU_F1 |= LLWU_F1_WUF4_MASK;   // clear the flag for LLWU_P4
    }  
    if ( wakeup_source & PWR_LLWU_ACC_INT2_WKUP )
    {
        // ACC_INT2 = PTC3 = LLWU_P7
        regval |= LLWU_PE2_WUPE7( 0x02 ); // falling edge
        LLWU_F1 |= LLWU_F1_WUF7_MASK;   // clear the flag for LLWU_P7
    }
    LLWU_PE2 = regval;

#if LLS_DURING_WIFI_USAGE
    regval = LLWU_PE4;
    if( wakeup_source & PWR_LLWU_WIFI_INT )
    {
        regval |= LLWU_PE4_WUPE13( 0x02 );  // falling edge
        LLWU_F2 |= LLWU_F2_WUF13_MASK;
    }
    LLWU_PE4 = regval;
#endif
    // Module Wakeups
    regval = LLWU_ME;
    if ( wakeup_source & PWR_LLWU_ME_RTC_ALARM )
    {
    	// RTC Alarm = LLWU_ME_WUME5
    	regval |= LLWU_ME_WUME5_MASK;
    	LLWU_F3 |= LLWU_F3_MWUF5_MASK; // clear the flag for module 5
    }
    LLWU_ME = regval;
}

static void pwrmgr_reboot_ready(uint_32 grace_period)
{
	UTIL_time_stop_timer(&g_shutdown_timer);
	
	RTC_get_ms( &g_reboot_timestamp_ms);
	
	if ( g_ship_requested )
	{
		g_total_ms_to_reboot = grace_period + PWRMGR_SHIP_DELAY;
		g_shutdown_timer = UTIL_time_start_oneshot_ms_timer( g_total_ms_to_reboot, PWRMGR_TimerShip_Cbk );
		wassert(g_shutdown_timer);
		D( corona_print("SHIP <= %dms\n", g_total_ms_to_reboot));
	}
	else
	{
		/*
		 *   "Normal" Reboot Path.
		 */
		if( grace_period )
		{
			g_total_ms_to_reboot = grace_period + PWRMGR_REBOOT_DELAY;
			g_shutdown_timer = UTIL_time_start_oneshot_ms_timer( g_total_ms_to_reboot, PWRMGR_TimerReboot_Callback );
			wassert(g_shutdown_timer);
			D( corona_print("REBOOT <= %dms\n", g_total_ms_to_reboot);)
		}
		else
		{
			D( corona_print("REBOOT NOW\n");)
			PWRMGR_TimerReboot_Callback();
		}
	}
}

static void pwrmgr_reboot_requested(void)
{
	WMP_post_reboot(__SAVED_ACROSS_BOOT.reboot_reason, __SAVED_ACROSS_BOOT.reboot_thread, 0);

    // LLS can't be entered when state is not PWR_STATE_NORMAL
    g_pwrmgr_state = PWR_STATE_REBOOT;
    
    pwrmgr_breadcrumb(g_pwrmgr_state);

	sys_event_post( SYS_EVENT_SYS_SHUTDOWN );
	_time_delay(500);  // give other tasks some time to vote for ON/Awake to finish their business, etc.

	if( g_ship_requested )
	{
		pwrmgr_reboot_ready( PWRMGR_SHIP_GRACE_PERIOD );	
	}
	else
	{
	    pwrmgr_reboot_ready( PWRMGR_REBOOT_GRACE_PERIOD );	
	}
}

static void pwrmgr_init_low_power( void )
{
    // Allow all low power modes: LLS, VLPx, VLLSx
    SMC_PMPROT = SMC_PMPROT_AVLP_MASK | SMC_PMPROT_ALLS_MASK | SMC_PMPROT_AVLLS_MASK;
    
    // Install the LLW interrupt service routine in the vector table
    _int_install_isr((uint_32) INT_LLW, PWRMGR_llw_int, (void *) NULL);
    
    // Initialize the LLWU BSP interrupt
    _bsp_int_init( INT_LLW, 3, 0, FALSE);

    // ensure LLWU clock is enabled
	wint_disable(); // SIM_* can be accessed in other drivers
    SIM_SCGC4 |= SIM_SCGC4_LLWU_MASK;
    wint_enable();
    
    // Clear previously configured wakeup sources
    LLWU_PE1 = LLWU_PE2 = LLWU_PE3 = LLWU_PE4 = 0; 
    
    // Clear LLWU wakeup flags  
    LLWU_F1 = LLWU_F2 = LLWU_F3 = 0xff;
}

// Warning! 32_KHZ_MCU glitches if we disable the 26_MHZ_BT in RUNM.
// This function should only be called by PWRMGR_Enable26MHz_BT()!
static void pwrmgr_set_26MHZ_BT( LWGPIO_VALUE value )
{
	static boolean configured = FALSE;
	static LWGPIO_STRUCT bt_pwdn_b;
	uint32_t max_wait_hci_rts_ms = 120;
	
	if (configured)
	{
		goto SET;
	}
	
	configured = TRUE;

    wassert( lwgpio_init(&bt_pwdn_b, BSP_GPIO_BT_PWDN_B_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE) );
    lwgpio_set_functionality(&bt_pwdn_b, BSP_GPIO_BT_PWDN_B_MUX_GPIO);
    
SET:
    lwgpio_set_value(&bt_pwdn_b, value);
    
    /*
     *   According to the CC2564 spec for power up sequencing, the HCI_RTS
     *     signal will transition from high to low at a max 100ms after the
     *     nSHUTD signal goes from low to high.  This means the CC256x is ready.
     */
    if( LWGPIO_VALUE_HIGH == value )
    {
    	while( max_wait_hci_rts_ms-- )
    	{
    		if( 0 == (GPIOD_PDIR & 0x20) )  // PTD5 is BT_CTS
    		{
    			//corona_print("num ms til BT rdy: %u\n", 120-(max_wait_hci_rts_ms+1));
    			return;
    		}
    		_time_delay(1);
    	}
    	
    	// If we get here, powering up the BT chip failed, which is fatal.
    	PROCESS_NINFO( ERR_BT_POWERUP_SEQ_FAILED, "BT pwr up" );
        PWRMGR_Reboot( 500, PV_REBOOT_REASON_BT_FAIL, FALSE );
    }
    else
    {
    	_time_delay(100);  // hold PWDN in reset state for at least this long.
    }
}  

#define MAX_DEBOUNCE_TRIES  16;
static uint_8 pwrmgr_debounce_pin( LWGPIO_STRUCT *pin )
{
    uint_8 pinval, lastval;
    uint_8 tries = MAX_DEBOUNCE_TRIES;

    // Disable context switching. This is necessary to avoid
    // extra long delays in handling the debounce due to other tasks
    // taking context. The debounce used to be done in the interrupt handler, but
    // the debounce delay was much smaller.
    _task_stop_preemption();
    
    // sample the pin for the first time
    pinval = lastval = (uint_8) lwgpio_get_value( pin );
   
    // Sample the pin until two consecutive samples have the same value
    do
    {
        // sample and compare to last pin value
        if ( (pinval = (uint_8) lwgpio_get_value( pin )) == lastval )
        {
            break;
        }
        lastval = pinval;
        
        // 1ms delay between tries
        _time_delay(1);
    } while (--tries);   
    
    // Re-enable context switching
    _task_start_preemption();
    
    // Assert if pin didn't become stable (may indicate a hardware problem)
    wassert(tries > 0);

    // Return the settled level (1=High, 0=Low)
    return (pinval);
}  

/*
 *   Try to go into ship mode.  If tests were not completed,
 *      then we will just reboot instead of going into ship mode.
 */
static void pwrmgr_ship_mode_if_burnin(void)
{
    if( MQX_NULL_TASK_ID != burnin_id )
    {
    	_task_destroy(burnin_id);
    	
		burnin_ship_mode();
		
		// Reboot if we were running the burn-in test and it didn't block
		//   in the ship mode call above (meaning not all burn-in tests have completed).
		D( corona_print("BI reboot\n") );
		_time_delay(500);  // give sherlock/printing time to go through before rebooting.
		
		// bypass normal reboot channels here and reboot immediately for burn-in.
		PWRMGR_reset(TRUE);
    }
}

static void pwrmgr_reboot_if_burnin(void)
{
    D( corona_print("Cable:"));
    
    switch( adc_get_cable( adc_sample_uid() ) )
    {
        case CABLE_BURNIN:
            D( corona_print("burnin REBOOT\n"));
            
            /*
             *   We want to reboot if the burnin cable was detected, to guarantee
             *     certain tasks never get a chance to run when we boot back up with the cable on.
             */
            _time_delay(500);
            PWRMGR_reset(TRUE);
            break;
            
        case CABLE_FACTORY_DBG:
            D( corona_print("factdbg\n"));
            break;
            
        case CABLE_CUSTOMER:
            D( corona_print("cust\n"));
            break;
            
        default:
            D( corona_print("ERR\n"));
            break;
    }
}

static void pwrmgr_start_pgood_monitoring( void ) 
{   
    PWRMGR_VoteForAwake( PWRMGR_PWRMGR_VOTE );

    // Enable PGOOD_B interrupts
    lwgpio_int_enable(&pgood_b_gpio, TRUE);   
    
    // Assert the PGOOD Changed event to ensure that USB state is sync'd to the PGOOD_B signal 
    _lwevent_set(&g_pwrmgr_evt, PWRMGR_PGOOD_CHANGE_EVENT );
}


// -- Public Functions

void PWRMGR_enter_vlps( void )
{
    volatile unsigned int dummyread;
    
    // Disable context switching
    _task_stop_preemption();
          
    // If we're in RUNM, switch to the fast IRC
    if ( g_pwrmgr_level == PWR_LEVEL_RUNM )
    {
        _bsp_set_clock_configuration( CPU_CLOCK_CONFIG_2 );
    }    
    
    // Entering sleep mode
    g_pwrmgr_state = PWR_STATE_SLEEP; 
    
    // Disable the clock monitor
    MCG_C6 &= ~MCG_C6_CME0_MASK;
    
    // Enable pull-up on TDO
    // Debugger will drop after this line     
    PORTA_PCR2 = PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
      
    // Set the STOPM field to 0b010 for VLPS mode
    SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK;
    SMC_PMCTRL |=  SMC_PMCTRL_STOPM(0x2);

    // wait for write to complete to SMC before stopping core
    dummyread = SMC_PMCTRL;
      
    // Set the SLEEPDEEP bit to enable deep sleep mode (STOP)
    SCB_SCR |= SCB_SCR_SLEEPDEEP_MASK;

    // WFI instruction will start entry into STOP mode
    asm("WFI");
    
    // Remove pull-up on TDO     
    PORTA_PCR2 &= ~(PORT_PCR_PE_MASK | PORT_PCR_PS_MASK);
    
    // Restore the clock configuration for RUNM mode
    if ( g_pwrmgr_level == PWR_LEVEL_RUNM )
    {
        // NOTE: See AN4503 sec. 9.1.4.
        // What clock mode is the device in when it wakes from VLPS if RUNM was active beforehand?
        _bsp_set_clock_configuration( CPU_CLOCK_CONFIG_0 );
    }    
    
    // Entering sleep mode
    g_pwrmgr_state = PWR_STATE_NORMAL;
          
    _task_start_preemption(); // re-enable context switching
      
    pwrmgr_breadcrumb(g_pwrmgr_state);

    LOG_WKUP("WK vlps\n");
}

static void pwrmgr_prep_for_lls( void )
{   
    volatile unsigned int dummyread;
    uint16_t pin_wakeups;
    
    pwrmgr_breadcrumb(g_pwrmgr_state);
    
    // Enable LLWU wake-up on the following pin transitions:
    // Button 
    // PGOOD (USB plug/unplug)
    // ACC_INT1: Accel FIFO Ready
    // ACC_INT2: Motion Detect (only when Accel isn't already logging)
    // PTD2 = WIFI_INT on Rev B / P4.
    pin_wakeups =  PWR_LLWU_BUTTON | PWR_LLWU_PGOOD_USB | PWR_LLWU_ACC_INT1_WTM | PWR_LLWU_WIFI_INT;
    
    // Only enable motion wakeup if the interrupt is enabled
    if ( g_en_motion_wakeup )
    {
        pin_wakeups |= PWR_LLWU_ACC_INT2_WKUP;
    }   
    
    // Configure wakeup sources
    pwrmgr_config_llwu(pin_wakeups | PWR_LLWU_ME_RTC_ALARM );     
    
    // enable the LLWU interrupt
    _bsp_int_enable( INT_LLW );
    
    // Disable the clock monitor: MCG_C6: CME0=0
    MCG_C6 &= (uint8_t)~(uint8_t)0x20U;    
    
    /* Set the STOPM field to 0b011 for LLS mode  */
    SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK;
    SMC_PMCTRL |=  SMC_PMCTRL_STOPM(0x3);
    /*wait for write to complete to SMC before stopping core */
    dummyread = SMC_PMCTRL;
    
    // Set the SLEEPDEEP bit to enable deep sleep mode 
    SCB_SCR |= SCB_SCR_SLEEPDEEP_MASK;
    
    SET_LED(0,0);
    SET_LED(1,0);
    
    // NOTE: interrupts must be enabled to wake from LLS!
    
    // The registers are now set for entering LLS when a WFI instruction is executed.
}

/*
 * Enter LLS. This function provides an external API to enter LLS via a console command.
 * 
 * NOTE! Do not call this function from PWRMGR! The idle task is responsible for calling WFI 
 * and PWRMGR_VoteForSleep() will call pwrmgr_prep_for_lls() if conditions are right for
 * allowing LLS mode
 * 
 */
void PWRMGR_enter_lls( void )
{
    pwrmgr_prep_for_lls();
    
    // Use WFI to enter LLS
    asm("WFI");    
}

void PWRMGR_enter_vlls3(void)
{
    volatile unsigned int dummyread;
    
    // Disable interrupts
    wint_disable();
    
    // Configure wakeup sources
    // Wake on button press or USB plugged in
    // NOTE: Until it's fixed, bootloader will see PGOOD_B when USB is plugged in
    // and won't boot the app. This will be changed..
    pwrmgr_config_llwu( PWR_LLWU_BUTTON | PWR_LLWU_PGOOD_USB );
    
    // NOTE: We can't wake up from VLLS3 reliably if it was entered from VLPR, so
    // enter RUNM now (for power considerations the clock configuration is not changed)
    pwrmgr_exit_vlpr();
    
    // Disable the clock monitor
    MCG_C6 &= ~MCG_C6_CME0_MASK;
   
    // Enable pull-up on TDO   
    PORTA_PCR2 = PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    
    /* Set the STOPM field to 0b100 for VLLS3 mode */
    SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK; 
    SMC_PMCTRL |=  SMC_PMCTRL_STOPM(0x4); 
    
    /* set VLLSM = 0b11 */
    SMC_VLLSCTRL =  SMC_VLLSCTRL_VLLSM(3);    
    
    /*wait for write to complete to SMC before stopping core */  
    dummyread = SMC_VLLSCTRL;
    
    /* Set the SLEEPDEEP bit to enable deep sleep mode (STOP) */
    SCB_SCR |= SCB_SCR_SLEEPDEEP_MASK;  

    /* WFI instruction will start entry into STOP mode */
    asm("WFI");
}

void PWRMGR_assert_radio_shutdowns( void )
{
    LWGPIO_STRUCT clk32K, wifipwrdn;
    // NOTE: Atheros driver asserts WIFI_PWDN_B when the driver is shut down
    
    // WIFI: Drive the TXD signal high.
    PORTC_PCR17 = PORT_PCR_MUX(0x01);
    GPIOC_PDDR |= 0x20000;    // pin 17 is a GPIO output now
    GPIOC_PDOR &= ~(0x20000); // pin 17 is WIFI_UART_TXD     
    
    // TODO: For BETA, disable BT here and re-enable it when needed
    // for a connection or when RUNM is entered
    
    // Bluetooth: Make sure RTS and TX are driven high.
    PORTD_PCR4 = PORT_PCR_MUX(0x01);
    GPIOD_PDDR |= 0x10; // pin 4 is an output now
    GPIOD_PDOR |= 0x10; // pin 4 is BT_RTS
    
    PORTD_PCR7 = PORT_PCR_MUX(0x01);
    GPIOD_PDDR |= 0x80; // pin 7 is an output now
    GPIOD_PDOR |= 0x80; // pin 7 is BT_TXD
    
    
    // Assert BT_PWDN_B pin: Drops about 1.5mA
    PWRMGR_Enable26MHz_BT(FALSE);
    
    // Disable Bluetooth 32KHz clock: Drops about 0.4mA
    wassert( lwgpio_init(&clk32K, BSP_GPIO_BT_32KHz_EN_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE) );
    lwgpio_set_functionality(&clk32K, BSP_GPIO_BT_32KHz_EN_MUX_GPIO);
    lwgpio_set_value(&clk32K, LWGPIO_VALUE_LOW);     
    
    
#if 0
// had no affect on current
    // Disable 26MHz clock (note: should not ever be enabled) 
    wassert( lwgpio_init(&clk26M, BSP_GPIO_MCU_26MHz_EN_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE) );
    lwgpio_set_functionality(&clk26M, BSP_GPIO_MCU_26MHz_EN_MUX_GPIO);
    lwgpio_set_value(&clk26M, LWGPIO_VALUE_LOW);    
#endif    
 
     // wassert WIFI_PWDN_B pin
     wassert( lwgpio_init(&wifipwrdn, BSP_GPIO_WIFI_PWDN_B_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE) );
     lwgpio_set_functionality(&wifipwrdn, BSP_GPIO_WIFI_PWDN_B_MUX_GPIO);
     lwgpio_set_value(&wifipwrdn, LWGPIO_VALUE_LOW);   
}

void PWRMGR_radios_OFF( void )
{
    volatile unsigned int dummyread;
    
    corona_print("Radios OFF\n");
    
    // Shut down the WIFI driver safely: drops ~0.5mA
    whistle_ShutDownWIFI(); 
    
    PWRMGR_assert_radio_shutdowns();  
     
	wint_disable(); // SIM_* can be accessed in other drivers
// The Disable USB code below seems to actually raise current draw by about 0.3mA! 
#if 0
     // Disable USB
     SIM_SCGC6 |= SIM_SCGC6_USBDCD_MASK;
     SIM_SCGC4 |= SIM_SCGC4_USBOTG_MASK;    
     USB0_CTL |= USB_CTL_USBENSOFEN_MASK; // enable USB module to cause a reset
     dummyread = USB0_CTL; // dummy read to allow USB reset to complete
     USB0_CTL &= ~USB_CTL_USBENSOFEN_MASK; // disable USB module.
     
     // Disable the USB regulator and module
     SIM_SOPT1CFG |= SIM_SOPT1CFG_URWE_MASK; // Enable write to the USB voltage regulator enable bit
     SIM_SOPT1 &= ~SIM_SOPT1_USBREGEN_MASK;  // Turn off the USB voltage regulator enable bit
#endif
     
     
 #if 0
     // trying to disable USB actually increases current. commenting out for now
   
     // Disable USB
     SIM_SCGC6 |= SIM_SCGC6_USBDCD_MASK;
     SIM_SCGC4 |= SIM_SCGC4_USBOTG_MASK;    
     USB0_CTL |= USB_CTL_USBENSOFEN_MASK; // enable USB module to cause a reset
     dummyread = USB0_CTL; // dummy read to allow USB reset to complete
     USB0_CTL &= ~USB_CTL_USBENSOFEN_MASK; // disable USB module.
     
     // Disable the USB regulator and module
     SIM_SOPT1CFG |= SIM_SOPT1CFG_URWE_MASK; // Enable write to the USB voltage regulator enable bit
     SIM_SOPT1 &= ~SIM_SOPT1_USBREGEN_MASK;  // Turn off the USB voltage regulator enable bit
 #endif
     
     // Turn off ADC1
     SIM_SCGC3 |= SIM_SCGC3_ADC1_MASK;
     ADC1_SC1A |= 0x1f;
     ADC1_SC1B |= 0x1f;
     
     // Turn off ADC0
     SIM_SCGC6 |= SIM_SCGC6_ADC0_MASK;
     ADC0_SC1A |= 0x1f;
     ADC0_SC1B |= 0x1f;  
     wint_enable();
}

void PWRMGR_radios_ON( void )
{
    // TODO: bring up WIFI and BT after they've been turned off 
}

/*****************************************************************************
*
*   @name        PWRMGR_Ship
*
*   @brief       Public API for entering ship mode. Can be safely called before
*                the PWRMGR task is running if urgent=TRUE
*
*   @param       urgent: if TRUE, skip clean shutdown of other threads
*
*   @return      None
**
*****************************************************************************/
void PWRMGR_Ship( boolean urgent )
{
	// don't let the calling task's watchdog expire
    _watchdog_stop();
    
    g_ship_requested = 1;    

    // get this off the caller's thread
    if ( !urgent )
    {
        // Flash the LEDs to indicate impending shutdown doom
        DISMGR_run_pattern( DISMGR_PATTERN_SHIP_MODE_ONESHOT );
        
        _lwevent_set(&g_pwrmgr_evt, PWRMGR_REBOOT_REQUESTED_EVENT );
    }
    else
    {
        // Shut down quickly without broadcasting the shutdown event to other threads
        ACCMGR_shut_down( true ); // force the accel to shut down quickly 
        
        g_pwrmgr_state = PWR_STATE_REBOOT; // LLS can't be entered when state is not PWR_STATE_NORMAL
        pwrmgr_reboot_ready( 0 );
    }
}

/*
 *   Return how many milliseconds remain before reboot.
 *     If we are not currently about to reboot, we return 0x7FFFFFFE
 */
uint32_t PWRMGR_ms_til_reboot( void )
{
	uint_64 ms_now;
	uint32_t surpassed;
	
	if( !PWRMGR_is_rebooting() || (0 == g_total_ms_to_reboot) )
	{
		return 0x7FFFFFFE; // not rebooting, so return something very large (but also positive for int32_t case).
	}
	
	RTC_get_ms( &ms_now);
	
	if( ms_now <= g_reboot_timestamp_ms )
	{
		return g_total_ms_to_reboot;
	}
	
	surpassed = (uint32_t)( ms_now - g_reboot_timestamp_ms );
	
	if(surpassed > g_total_ms_to_reboot)
	{
		return 0;
	}
	
	return ( g_total_ms_to_reboot - surpassed );
}


/*****************************************************************************
*
*   @name        PWRMGR_is_rebooting
*
*   @brief       Public API for telling whether the system is powering down
*
*   @param       None
*
*   @return      TRUE: is rebooting
*                FALSE: not rebooting
**
*****************************************************************************/
boolean PWRMGR_is_rebooting( void )
{
	return (1==g_is_rebooting);
}

/*****************************************************************************
 *
 *   @name        PWRMGR_Reboot
 *
 *   @brief       Public API for shutting down and rebooting
 *
 *   @param       None
 *
 *   @return      None
 **
 *****************************************************************************/
void PWRMGR_Reboot( uint_32 millisecs_until_reboot, persist_reboot_reason_t reboot_reason, boolean should_block )
{   
	boolean thread_context;
	
	if (g_is_rebooting)
	{
		goto CHECK_BLOCK;
	}
	
	/*
	 *   Make sure we can't go into LLS after a Reboot was requested.
	 */
	PWRMGR_VoteForAwake( PWRMGR_PWRMGR_VOTE );

	g_is_rebooting = 1;
	
	/*
	 *   Arm the Watchdog to assure we have a fallback in case we get hung up on the reboot.
	 */
	g_allow_wdog_refresh = 0;
	
	PWRMGR_watchdog_arm( NUM_SECONDS_WDOG_ARM );
	
	thread_context = ( 0 == _int_get_isr_depth() );
	
	PMEM_VAR_SET(PV_REBOOT_REASON, &reboot_reason);
	
	if (thread_context)
	{
		uint_32 reboot_thread = _task_get_template_ptr(_task_get_id())->TASK_TEMPLATE_INDEX;
		
		PMEM_VAR_SET(PV_REBOOT_THREAD, &reboot_thread);
		
    	// don't let the calling task's watchdog expire, since we'll have plenty of checks now.
        _watchdog_stop();
        
		// delay desired amount
		_time_delay(millisecs_until_reboot);
	}

    // get this off the caller's thread/isr
    _lwevent_set(&g_pwrmgr_evt, PWRMGR_REBOOT_REQUESTED_EVENT );
    
    if (PV_REBOOT_REASON_WIFI_FAIL == reboot_reason)
    {
    	WIFIMGR_notify_disconnect();
    }
    
    BTMGR_force_disconnect(); // just in case it was connected
    
CHECK_BLOCK:
    if (should_block && thread_context)
    {
    	_task_block();
    }
}

uint_8 PWRMGR_WakingFromShip( void )
{
    return (PV_REBOOT_REASON_SHIP_MODE == __SAVED_ACROSS_BOOT.reboot_reason);
}

uint_8 PWRMGR_WakingFromBoot( void )
{
    return ( PWR_STATE_BOOTING == g_pwrmgr_state ? 1 : 0 );
}

void PWRMGR_GotoSleep(void)
{
	g_pwrmgr_state = PWR_STATE_SLEEP;

	pwrmgr_breadcrumb(g_pwrmgr_state);

	PWRMGR_enter_lls();
}

err_t PWRMGR_VoteForVLPR( uint_32 my_vote )
{
    err_t err = ERR_OK;
    uint_32 runm_votes; 
    
    corona_print("VLPR %x\n", my_vote);
    
    wint_disable();
    
    if ( PWRMGR_CONSOLE_VOTE == my_vote )
    {
        // Console trumps all other votes
        g_runm_power_votes = 0;
    }
    else
    {
        // Clear the caller's vote for RUNM mode
        g_runm_power_votes &= ~(my_vote);
    }
    
    runm_votes = g_runm_power_votes;
    
    // Switch to VLPR if no more votes for RUNM are outstanding
    if ( g_runm_power_votes == 0 )
    {
        err = pwrmgr_enter_vlpr();
    }
    
    wint_enable();

    // do the corona_print outside the int_disable
    if ( runm_votes != 0 )
    {
        corona_print("RUNM need %x\n", runm_votes);
    }

    return err;
}

/*
 * Set the core clock speed for RUNM mode.
 * speed: 0=24MHz, 1=96MHz
 */
void PWRMGR_SetRUNMClockSpeed( uint8_t speed )
{
    g_en_fast_runm = speed;
}

err_t PWRMGR_VoteForRUNM( uint_32 my_vote )
{   
    err_t err = ERR_OK;
    
    wint_disable();
    
    if (0 == (g_active_power_votes & my_vote))
    {
    	goto DONE;
    }
    
    // Cast the caller's vote for RUNM mode
    g_runm_power_votes |= my_vote;
    err = pwrmgr_enter_runm();
    
DONE:
    wint_enable();
    
    if (ERR_OK == err)
    {
        corona_print("RUNM %x\n", my_vote);
    }
    
    return err;
}

err_t PWRMGR_VoteForOFF( uint_32 my_vote )
{   
    static uint32_t votes_at_last_print = 0;
    uint32_t vote_var;
    boolean print_on_msg = FALSE;
    uint32_t votes_to_print;
    
	wint_disable();
    
    // Don't bother processing if your vote isn't even on.
    if( 0 == (my_vote & __SAVED_ACROSS_BOOT.on_power_votes) )
    {
    	goto VOTE_OFF_DONE;
    }
    
    // Don't let the caller's thread's watchdog expire
    _watchdog_stop();
    
    if ( PWRMGR_CONSOLE_VOTE == my_vote )
    {
        // Console trumps all other votes
        vote_var = 0;
    }
    else
    {
        // Clear the caller's vote for ON mode
        vote_var = __SAVED_ACROSS_BOOT.on_power_votes;
        vote_var &= ~(my_vote);
    }
    
    PMEM_VAR_SET( PV_ON_PWR_VOTES, &vote_var );
    
    // Switch to OFF if no more votes for ON are outstanding
    if ( PWR_STATE_REBOOT == g_pwrmgr_state) 
    {
    	if ( 0 == __SAVED_ACROSS_BOOT.on_power_votes )
    	{
    		// Handle the actual powering off on PWRMGR's thread
    	    g_pwrmgr_state = PWR_STATE_REBOOTING;
    		_lwevent_set(&g_pwrmgr_evt, PWRMGR_REBOOT_READY_EVENT );
    		
            /*
             * NOTE: We used to return here without unlocking the mutex. That was causing a deadlock
             * with some of the threads that would keep PWRMGR from ever running again. Now we test
             * reboot_ready_event_sent in PWRMGR_VoteForOn() instead.
            */    		
    		goto VOTE_OFF_DONE;
    	}
    	else
    	{
    	    if( __SAVED_ACROSS_BOOT.on_power_votes != votes_at_last_print )
    	    {
    	        /*
    	         *   Don't spam the console with a bunch of "ON" prints unless the info (vote mask) has changed.
    	         */
                votes_at_last_print = __SAVED_ACROSS_BOOT.on_power_votes;
                votes_to_print = votes_at_last_print; // just for corona_print
                print_on_msg = TRUE;
    	    }
    	}
    } 

VOTE_OFF_DONE:    
    wint_enable();
    
    // print outside the int_disable
    if (print_on_msg)
    {
    	corona_print("ON %x\n", votes_to_print);
    }

    return ERR_OK;
}

err_t PWRMGR_VoteForON( uint_32 my_vote )
{
	wint_disable();
	
    if (0 == (g_active_power_votes & my_vote))
    {
    	goto DONE;
    }
	
	// Don't allow new ON votes once we've started to reboot
	if ( PWR_STATE_REBOOTING != g_pwrmgr_state )
	{
	    // Cast the caller's vote for ON mode
	    my_vote |= __SAVED_ACROSS_BOOT.on_power_votes;
	    PMEM_VAR_SET( PV_ON_PWR_VOTES, &my_vote );
	}
	
DONE:    
    wint_enable();
    
    return ERR_OK;
}

/*
 * Raise a vote to prevent system from going into LLS mode.
 * This routine may be called from interrupt handlers.
 */
void PWRMGR_VoteForAwake( uint_32 my_vote )
{
    wint_disable();
    
    if (0 == (g_active_power_votes & my_vote))
    {
    	goto DONE;
    }
    
    // Cast the caller's vote for staying awake
    g_awake_power_votes |= my_vote;
        
    // Clear the SLEEPDEEP flag to cause an abort of LLS f entering it was already in progress   
    SCB_SCR &= ~SCB_SCR_SLEEPDEEP_MASK;
    
    SET_LED(1,1);
    
DONE:
    wint_enable();
}

/*
 * Raise a vote to allow the system to enter LLS mode.
 * This routine may be called from interrupt handlers.
 */
void PWRMGR_VoteForSleep( uint_32 my_vote )
{
    // Cast the caller's vote for going to sleep (LLS)
    wint_disable();
    g_awake_power_votes &= ~(my_vote);
    wint_enable();
}

void PWRMGR_VoteExit( uint_32 my_vote )
{
    wint_disable();
    PWRMGR_VoteForVLPR(my_vote);
    PWRMGR_VoteForOFF(my_vote);
    PWRMGR_VoteForSleep(my_vote);
    
    g_active_power_votes &= ~my_vote;
    wint_enable();
}

err_t PWRMGR_Enable26MHz_BT( boolean enable )
{
    err_t err = ERR_OK;
    PWRMGR_LEVEL_t cur_pwrmgr_level = g_pwrmgr_level;
    
    // Vote to stay awake even in VLPR mode
    PWRMGR_VoteForAwake( PWRMGR_PWRMGR_VOTE );

    wint_disable();
    
    // 32_KHZ_MCU glitches if we disable the 26_MHZ_BT in RUNM.
    // Even though we are enabling (not disabling) here, let's run off internal 4 MHZ (VLPR)
    // just to be safe.
    err = pwrmgr_enter_vlpr();
    
    if (ERR_OK != err)
    {
    	goto DONE;
    }
    
    pwrmgr_set_26MHZ_BT(enable ? LWGPIO_VALUE_HIGH : LWGPIO_VALUE_LOW);

    // Restore RUNM level, if it was active upon entry to this function
    if ( PWR_LEVEL_RUNM == cur_pwrmgr_level )
    {
        err = pwrmgr_enter_runm();
    }
    
    if (ERR_OK != err)
    {
    	goto DONE;
    }
    
DONE:
    wint_enable();
    
    // Allow LLS if it's enabled
    if ( g_st_cfg.pwr_st.enable_lls_deep_sleep )
    {
        PWRMGR_VoteForSleep( PWRMGR_PWRMGR_VOTE );
    }
       
    return err;
}

void PWRMGR_enable_motion_wakeup( uint_8 en )
{
    g_en_motion_wakeup = en;
}

void PWRMGR_start_burnin(void)
{
    burnin_id = (MQX_NULL_TASK_ID == burnin_id)?  whistle_task_create( TEMPLATE_IDX_BURNIN ) : burnin_id;
}

static void PWRMGR_reboot_now(void)
{
    uint32_t read_value = SCB_AIRCR;
    
    _task_stop_preemption();

    // Issue a Kinetis Software System Reset
    read_value &= ~SCB_AIRCR_VECTKEY_MASK;
    read_value |= SCB_AIRCR_VECTKEY(0x05FA);
    read_value |= SCB_AIRCR_SYSRESETREQ_MASK;

    wint_disable();
    SCB_AIRCR = read_value;
    while (1)
    {
    }
}

void PWRMGR_reset(boolean wait_for_wifi)
{
    boolean thread_context  = ( 0 == _int_get_isr_depth() );
	
	// don't let the calling task's watchdog expire
    if (thread_context)
    {
    	_watchdog_stop();

    	if (wait_for_wifi)
    	{
    		WIFIMGR_shut_down();
    	}

    	ext_flash_shutdown( 1000 );   // 1 second is a good estimate for max time, though we would rarely take that long.
    }
    
    PWRMGR_reboot_now();
}

/*
 *   Show charging LED's based on CHG_B signal from charger chip.
 */
void PWRMGR_charging_display(void)
{
    cable_t cable = cable = adc_get_cable( adc_sample_uid() );
    
    if( 0 == lwgpio_get_value( &chg_b_gpio ) )
    {
        /*
         *   CHG_B signal was asserted, show "is charging".
         */
        if( CABLE_BURNIN == cable)
        {
            // display a special pattern to indicate to the user this is a burnin cable.
            burnin_display(BURNIN_DISPLAY_CHARGING);
        }
        else
        {
            corona_print("PM Chg'g\n");
            DISMGR_run_pattern( DISMGR_PATTERN_BATT_CHARGE_CHARGING_REPEAT );
        }   
    }
    else
    {
        /*
         *   CHG_B signal was de-asserted, show "fully charged".
         */
        if( CABLE_BURNIN == cable)
        {
            // display a special pattern to indicate to the user this is a burnin cable.
            burnin_display(BURNIN_DISPLAY_FULLY_CHARGED);
        }
        else
        {
            corona_print("PM chg'd\n");
            DISMGR_run_pattern( DISMGR_PATTERN_BATT_CHARGE_CHARGED_REPEAT );
        }
    }
}

//
// Return chg_b state
//
boolean PWRMGR_chgB_state(void)
{
    return ( 0 == lwgpio_get_value( &chg_b_gpio ) ); // true if charging
}

/*
 *   If USB is connected, call the USB handler.
 */
void PWRMGR_process_USB(void)
{
    // Sample PGOOD to determine if USB is already plugged in and store the state
    g_usb_plugged_state = (0 == pwrmgr_debounce_pin(&pgood_b_gpio) );                    
    
    if ( 1 == g_usb_plugged_state )  
    {
        // Init for USB plugged in
        pwrmgr_execute_usb_plugged();
    }
    else
    {
        g_pwrmgr_state = PWR_STATE_NORMAL;
        pwrmgr_breadcrumb(g_pwrmgr_state);
    }
}

/*
 * Return the current USB state
 *  1 = Plugged in
 *  0 = Unplugged
 */
boolean PWRMGR_usb_is_inserted(void)
{
#if 0
    return ( 1 == g_usb_plugged_state ); 
#endif
    return ( 0 == lwgpio_get_value( &pgood_b_gpio ) );
}

static void PWRMGR_usb_gpio_init()
{
    wassert( ERR_OK == PWRMGR_PGOOD_B_Init() );
    wassert( ERR_OK == PWRMGR_CHG_B_Init() );
}

/*****************************************************************************
*
*   @name        PWRMGR_is_battery_low
*
*   @brief       API to report if battery is low
*
*   @param       None
*
*   @return      1: Battery is low. 0: Battery is not low
**
*****************************************************************************/
uint_8 PWRMGR_is_battery_low(void)
{
	if ( g_pwrmgr_state == PWR_STATE_NORMAL )
	{
	    PWRMGR_process_batt_status( false );
	    // If the battery is below the alert threshold drop a bread crumb
	    // As of 20130925 this was only done on a user button press
	    if (g_stored_battery_mv < LOW_VBAT_ALERT_THRESHOLD)
	    {
	        WMP_post_breadcrumb( BreadCrumbEvent_BATTERY_LOW );
	        return 1;
	    }
	            
	}
    return 0;
}

// Turn on the Fail LED pattern and go into a coma.
void PWRMGR_Fail(err_t err, const char *pErr)
{
    char buf[32];
    shlk_handle_t hdl;

    PWRMGR_VoteForVLPR(PWRMGR_PWRMGR_VOTE);
    
    whistle_kill_all_tasks();  // nothing deserves to live!
    
    if( ERR_EFLASH_HW_BROKEN != err )
    {
        SHLK_init(  &hdl,
                    wson_read,
                    wson_write,
                    shlk_printf,
                    EXT_FLASH_SHLK_ASSDUMP_ADDR,
                    EXT_FLASH_SHLK_LOG_ADDR,
                    EXT_FLASH_SHLK_LOG_NUM_SECTORS );

        sprintf(buf, "\n%s\t%s HW\t0x%x\n\0", __func__, pErr?pErr:"", err);

        SHLK_asslog(&hdl, buf);   // log message permanently to SPI Flash.
    }
    
    I2C1_take_over();  // screw anyone who wants to use I2C at this point.
	led_run_pattern(LED_PATTERN_FAIL);
	wint_disable();
	
	while(1)
	{
	    if( BUTTON_DOWN == button_get_val() )
	    {
	        _time_delay(800);
	        
	        led_run_pattern(LED_PATTERN_FAIL);
	    }
	    
	    _time_delay(10);
	}
}

void PWRMGR_breadcrumb(void)
{
	pwrmgr_breadcrumb(g_pwrmgr_state);
}

void PWR_tsk( uint_32 dummy )
{       
    _mqx_uint mask;

    // Vote to enter VLPR mode if no other task requires RUNM mode.
    PROCESS_NINFO( PWRMGR_VoteForVLPR(PWRMGR_PWRMGR_VOTE), NULL );
    
    // NOTE: saw baud corruption when _rtc_init() was called. trying in VLPR to see if it makes a differnce..
    // there might be an issue due RTC running off the same clock that sources the FEE for system clock
    //test_rtc_timer_queue();
    
    // Just for Alpha hack
    _timer_start_periodic_every(alpha_heartbeat_hack_timer, 0, TIMER_ELAPSED_TIME_MODE, 60*1000);
    
    while (1)
    {   
        // Remove the vote we cast in the interrupt handler.
    	PWRMGR_VoteForSleep(PWRMGR_PWRMGR_VOTE);

        pwrmgr_breadcrumb(g_pwrmgr_state);

        _watchdog_stop();

    	// Wait for an event
        _lwevent_wait_ticks( &g_pwrmgr_evt, 
                       //     PWRMGR_USB_UNPLUGGED_EVENT |
                       //     PWRMGR_USB_PLUGGED_EVENT |
                       //     PWRMGR_LOW_VBAT_EVENT |      // deprecated
                            PWRMGR_BUTTON_WAKEUP_EVENT |
                            PWRMGR_CHG_B_RISE_EVENT | 
                            PWRMGR_CHG_B_FALL_EVENT | 
                            PWRMGR_LLW_WAKEUP_EVENT |
                            PWRMGR_REBOOT_REQUESTED_EVENT |
                            PWRMGR_REBOOT_READY_EVENT | 
                            PWRMGR_REBOOT_NOW_EVENT |
                            PWRMGR_RTC_ALARM_EVENT |
                            PWRMGR_PGOOD_CHANGE_EVENT |
                            PWRMGR_CHG_B_FAULT_EVENT | 
                            PWRMGR_CHG_B_FAULT_CLEAR_EVENT |
                            PWRMGR_SHIP_NOW_EVENT,
                            FALSE, 0);
        
        _watchdog_start(60*1000);
        
        pwrmgr_breadcrumb(g_pwrmgr_state);
                                    
        // Get the event mask
        mask = _lwevent_get_signalled();

        if ( mask & PWRMGR_RTC_ALARM_EVENT )
        {
            corona_print("RTC Alrm: a:x%x o:x%x r:x%x\n", g_awake_power_votes, __SAVED_ACROSS_BOOT.on_power_votes, g_runm_power_votes);
        }
        
        if ( mask & PWRMGR_CHG_B_FAULT_EVENT )
        {            
            if( MQX_NULL_TASK_ID == burnin_id )
            {
                /*
                 *   If we are running the customer app, solid yellow time since there is a bad battery.
                 */
                PWRMGR_Fail(ERR_PWRMGR_CHGB_2Hz_FAULT, "CHG_B");
            }
            else
            {
                PROCESS_NINFO( ERR_PWRMGR_CHGB_2Hz_FAULT, "CHGB ERR\n" );
                sys_event_post( SYS_EVENT_CHGB_FAULT );
            }
        }
        
        if ( mask & PWRMGR_CHG_B_FAULT_CLEAR_EVENT )
        {
            corona_print("CHGB Fault CLR\n");
        }  
        
        if ( mask & PWRMGR_LLW_WAKEUP_EVENT )
        {          
            switch ( g_pwrmgr_level )
            {
            case PWR_LEVEL_RUNM:
                // Re-enter full RUNM level 
                pwrmgr_enter_runm();
                break;
            case PWR_LEVEL_VLPR:               
                // Re-enable VLPR: According to RM, we should still be in BLPI after exiting LLS mode
                // if LLS was entered while in VLPR. Therefore should only need to restore VLPR flag
                // here instead of going through the whole VLPR sequence.
                SMC_PMCTRL |= (SMC_PMCTRL_RUNM(2));   // Set The VLPR mode bit
                break;
            default:
                break;
            }

            LOG_WKUP("WK %x\n", g_wakeup_source);
        }

        if ( mask & PWRMGR_BUTTON_WAKEUP_EVENT )
        {
            switch ( g_pwrmgr_state )
            {
                case PWR_STATE_NORMAL:
                    corona_print("PM Btn\n");
                    break;

                case PWR_STATE_SHIP:
                    // A button press has woken us from ship mode
                    ACCMGR_start_up();
                    g_pwrmgr_state = PWR_STATE_NORMAL;
                    break;
                    
                default: 
                    break;
            }
        }
               
        if ( mask & PWRMGR_PGOOD_CHANGE_EVENT )
        {
            uint8_t new_usb_plugged_state = g_usb_plugged_state;
            
            // Keep resampling until the pin stops changing
            while ( g_pgood_b_changed )
            {
                g_pgood_b_changed = FALSE;
                new_usb_plugged_state = ( 0 == pwrmgr_debounce_pin( &pgood_b_gpio ) );
            } 
                        
            // Has the state changed?
            if ( new_usb_plugged_state != g_usb_plugged_state )
            {            
                // Process new USB state: 0=unplugged, 1=plugged
                
                // If USB is newly plugged in, process the new state immediately
                if ( new_usb_plugged_state )
                {                  
                    #if 0
                    /*
                     *   EA:  FW-930 was causing the device to stay in a permanently
                     *        funky state when it was on its way into ship mode.
                     *        Don't cancel the shutdown timer in the middle of going into ship mode!
                     */
                    // If the shutdown timer is running, cancel it
                	UTIL_time_stop_timer( &g_shutdown_timer );
                    #endif
                    D( corona_print("USB in\n") );
                    pwrmgr_execute_usb_plugged();
                    pwrmgr_reboot_if_burnin();
                    WMP_post_breadcrumb( BreadCrumbEvent_USB_CONNECTED );
    
                    // Broadcast the event for others
                    sys_event_post( SYS_EVENT_USB_PLUGGED_IN );
                }
                else                    
                {
                    // USB Unplugged
                	
                	pwrmgr_ship_mode_if_burnin();
                    
                    D( corona_print("USB out\n") );
                    pwrmgr_execute_usb_unplugged();
                    WMP_post_breadcrumb( BreadCrumbEvent_USB_DISCONNECTED );
    
                    // Broadcast the event for others
                    sys_event_post( SYS_EVENT_USB_UNPLUGGED );
                }                                
                
                // Remember the new state
                g_usb_plugged_state = new_usb_plugged_state;

                // Start holdoff timer to ignore all changes for the next 2 seconds
                // NOTE: interrupt handler is still operative and any change will be processed when the timer expires.
                wassert(RTC_add_alarm_in_seconds( PGOOD_HOLDOFF_DELAY_SECS, (rtc_alarm_cbk_t) PWRMGR_pgood_b_holdoff_Callback ));                    
                g_pgood_holdoff = 1;
            }
        }
                
        // Charger chip safety timer detection:
        // The charger chip has a safety time that expires if the battery hasn't reached
        // the fully charged state after charging for 6.25hrs. When the safety timer has 
        // expired the charger chip will begin flashing the CHG_B signal at a 2Hz rate. 
        // The CHG_B 2Hz waveform (250ms high, 250ms low) is detected using a 300ms timer
        // that is used as follows:
        //  - When a CHG_B falling edge takes place after the battery is already charged,
        //    the 600ms timer is started.
        //  - If a CHG_B falling edges takes place before the timer is expired, a counter is
        //    incremented and the timer is restarted If the timer reaches 3, a timer fault event 
        //    is generated.
        //  - If the timer expires before the next CHG_B falling edge takes place, the counter 
        //    is cleared.
        
        if (mask & PWRMGR_CHG_B_FALL_EVENT)
        {
            D(corona_print("chg'g %i\n", g_pwrmgr_state));
            if ( PWR_STATE_CHARGED == g_pwrmgr_state )
            {
                // Look for CHG_B 2Hz change which indicates that the charger chip safety timers have expired
                if ( ++g_chg_b_filter == CHG_B_FILTER_THRESHOLD )
                {
                    // Send the Timer Fault event
                    _lwevent_set(&g_pwrmgr_evt, PWRMGR_CHG_B_FAULT_EVENT );
                }
                
                UTIL_time_stop_timer(&g_chg_b_filter_timer);
                g_chg_b_filter_timer = UTIL_time_start_oneshot_ms_timer( CHG_B_FILTER_PERIOD, PWRMGR_CHG_B_Filter_Callback );
                wassert( g_chg_b_filter_timer );
            }
        }
        
        if (mask & PWRMGR_CHG_B_RISE_EVENT)
        {
            D(corona_print("chg'd %i\n", g_pwrmgr_state));
            if ( PWR_STATE_CHARGING == g_pwrmgr_state )
            {
                g_pwrmgr_state = PWR_STATE_CHARGED;
                DISMGR_run_pattern( DISMGR_PATTERN_BATT_CHARGE_CHARGED_REPEAT );
                
                // Broadcast the event for others
                sys_event_post( SYS_EVENT_CHARGED );
            }
        }
#if 0
        // depcrecated event
        if (mask & PWRMGR_LOW_VBAT_EVENT)
        {
            if ( g_pwrmgr_state == PWR_STATE_NORMAL )
            {               
                // Display the Low Battery pattern on LEDs
                DISMGR_run_pattern( DISMGR_PATTERN_BATT_LOW_REPEAT );

                // TODO: test this breadcrumb call when we start shutting down on low battery.
                WMP_post_breadcrumb( BreadCrumbEvent_BATTERY_LOW );
                
                // NOTE: The VBAT_LOW system event has been broadcast. PWRMGR expects WIFI, EVTMGR,
                // ACCMGR & others will spend the next few seconds shutting down.
                // The timer below executes the shutdown callback to enter shipmode when it expires              
                
                D( corona_print("PM:shut'g down\n"));
                g_shutdown_timer = UTIL_time_start_oneshot_ms_timer( PWRMGR_SHUTDOWN_DELAY, PWRMGR_TimerShip_Cbk ); 
                wassert(g_shutdown_timer);
            }   
        }  
#endif
        
        if (mask & PWRMGR_REBOOT_REQUESTED_EVENT)
        {       
        	PWRMGR_VoteForAwake(PWRMGR_PWRMGR_VOTE);
        	pwrmgr_reboot_requested();
        	
        	// This will cause a reboot now if there are no other ON votes. 
        	// If there are ON votes, the reboot will happen after the last thread is done
        	// shutting down, when it calls VoteForOff() with its own vote
        	PWRMGR_VoteForOFF(PWRMGR_PWRMGR_VOTE);
        }
        
        if (mask & PWRMGR_REBOOT_READY_EVENT)
        {     
            // !!! we are already going to reboot. don't try to grab power mutex - will cause us to hang!!
        	//PWRMGR_VoteForAwake(PWRMGR_PWRMGR_VOTE);
        	if( !g_ship_requested )
        	{
        		pwrmgr_reboot_ready(0);
        	}
        	else
        	{
        		// For ship, just call the ship mode callback directly.
        		PWRMGR_TimerShip_Cbk();
        	}
        }
        
        if (mask & PWRMGR_REBOOT_NOW_EVENT)
        {
        	PWRMGR_VoteForAwake(PWRMGR_PWRMGR_VOTE);
            g_shutdown_timer = 0; // too late to cancel the shutdown!   
            pwrmgr_execute_reboot();
        }
        
        if (mask & PWRMGR_SHIP_NOW_EVENT)
        {
            pwrmgr_enter_ship();
        }
    }
}

void PWRMGR_get_pwr_votes(uint32_t *awake, uint32_t *on, uint32_t *runm)
{
	*awake = g_awake_power_votes;
	*on = __SAVED_ACROSS_BOOT.on_power_votes;
	*runm = g_runm_power_votes;
}

void PWRMGR_init ( void )
{   
    _lwevent_create(&g_pwrmgr_evt, LWEVENT_AUTO_CLEAR);
    
    // Allow all low power modes: LLS, VLPx, VLLSx
    SMC_PMPROT = SMC_PMPROT_AVLP_MASK | SMC_PMPROT_ALLS_MASK | SMC_PMPROT_AVLLS_MASK;
    
    // Install the LLW interrupt service routine in the vector table
    _int_install_isr((uint_32) INT_LLW, PWRMGR_llw_int, (void *) NULL);
    
    // Initialize the LLWU BSP interrupt
    _bsp_int_init( INT_LLW, 3, 0, FALSE);
    
    // Initialize the RTC and RTC alarm queue
    RTC_init();

#if 1
    // Turn off the debug LEDs
    PORTD_PCR12 = PORT_PCR_MUX(0x01);
    PORTD_PCR13 = PORT_PCR_MUX(0x01);
    PORTD_PCR14 = PORT_PCR_MUX(0x01);
    PORTD_PCR15 = PORT_PCR_MUX(0x01); 
    GPIOD_PDDR |= (0xF << 12); // dir=out
    GPIOD_PDOR |= (0xF << 12); // turn them all off
#endif
    
    // Initialize the LLWU and low power modes
    pwrmgr_init_low_power();
    
    // TODO: remove ADC0 sampling and replace w/ comparator+dac
    
    // Initialise ADC0 for default sampling
    adc_default_init();
    //PWRMGR_get_batt_percent();  // nice to see battery but it is in-accurate at boot, so just creates confusion.
        
    // USB GPIO
    PWRMGR_usb_gpio_init();
    
    pwrmgr_start_pgood_monitoring();
    
    // Turn off the WiFi and BT chips
    PWRMGR_assert_radio_shutdowns();
}

#define IDLE_COUNTER_ROLLOVER 	(50)
//#define LLS_STARVATION_METRIC   // provides a way to be notified if we go a long time without entering LLS.
        
void _mqx_idle_task
   ( 
      /* [IN] parameter passed to the task when created */
      uint_32 parameter
   )
{
    volatile unsigned int dummyread;
    static uint_32 idle_counter = 0;
    
#ifdef LLS_STARVATION_METRIC
    static uint_32 lls_starvation_counter = 0;
#endif
    
     while (1) 
    {
#ifdef BLINK_IDLE_STATE_ON_DEBUG_LED0
        /*
         * Increment idle counter and toggle debug LED, if enabled, to indicate CPU load 
         */
        if (++idle_counter > IDLE_COUNTER_ROLLOVER)
        {
            idle_counter = 0;
            if ( GPIOD_PDOR & (0x8 << 12))
            {
                GPIOD_PDOR &= ~(0x8 << 12);
            }
            else
            {
                GPIOD_PDOR |= (0x8 << 12);
            }
        }
#endif
        
#ifdef LLS_STARVATION_METRIC
        
        #define LLS_STARVATION_THRESHOLD  100   // number of counts before we have "LLS Starvation".
        
        if( lls_starvation_counter >= LLS_STARVATION_THRESHOLD )
        {
            corona_print("\n** LLS Starvation **\n");
            corona_print("\tlevel: %u\n\tstate: %u\n\tawake: 0x%x\n\n", g_pwrmgr_level, g_pwrmgr_state, g_awake_power_votes);
            
            lls_starvation_counter = 0;
        }
        
#endif
    	
        /*
         * Go into a sleep (WAIT) or deep sleep (LLS)
         * Which one is entered depends on the state of the SLEEPDEEP bit in
         * the SCB_SCR register.
         * The SLEEPDEEP bit state is cleared by PWRMGR_VoteForAwake() and by interrupts that can't tolerate LLS mode
         */
        
        wint_disable();
        if (    (PWR_LEVEL_VLPR == g_pwrmgr_level) && 
                (PWR_STATE_NORMAL == g_pwrmgr_state) && 
                (0 == g_awake_power_votes)      )
        {
#ifdef LLS_STARVATION_METRIC
                lls_starvation_counter = 0;
#endif
                pwrmgr_prep_for_lls();
        }
        wint_enable();

        asm("WFI");
        
        SET_LED(0,1);
        SET_LED(2, g_awake_power_votes != 0);
        SET_LED(3, g_pwrmgr_level == PWR_LEVEL_RUNM);
        
    } /* Endwhile */
}

void PWRMGR_watchdog_arm(uint32_t sec)
{
	static boolean is_armed = 0;
	
	// Per K60 TRM: "these register bits can be modified only once after unlocking them for editing, even after reset."
    wint_disable();
	if (!is_armed)
	{
	    uint32_t cycles = 200*sec;
	    	    
	    WDOG_UNLOCK = 0xc520;
	    WDOG_UNLOCK = 0xd928;
	    
	    WDOG_STCTRLH = WDOG_STCTRLH_WDOGEN_MASK      | 
	                   //WDOG_STCTRLH_CLKSRC_MASK      |
	                   WDOG_STCTRLH_ALLOWUPDATE_MASK |
	                   //WDOG_STCTRLH_DBGEN_MASK |
	                   WDOG_STCTRLH_WAITEN_MASK;
	    
	    /*
	     *   WDOG value is defined in terms of cycles of WDOG clock.
	     */
		
		WDOG_TOVALH = ((cycles >> 16) & 0xFFFF);
		WDOG_TOVALL = ((cycles) & 0xFFFF);
		
		// enable
		//WDOG_STCTRLH |= WDOG_STCTRLH_WDOGEN_MASK | WDOG_STCTRLH_CLKSRC_MASK;
		
		// clear the reset counter
		PWRMGR_watchdog_clear_count();
		
		WDOG_STCTRLH &= ~(WDOG_STCTRLH_ALLOWUPDATE_MASK) ;
		
		is_armed = TRUE;
	}
	wint_enable();
}

uint16_t PWRMGR_watchdog_reset_count(void)
{
    return WDOG_RSTCNT;
}

void PWRMGR_watchdog_clear_count(void)
{
    // clear the wdog reset counter.
    WDOG_RSTCNT = 0xFFFF;
}

void PWRMGR_watchdog_refresh(void)
{
    if( g_allow_wdog_refresh )
    {
        wint_disable();
        WDOG_REFRESH = 0xA602;
        WDOG_REFRESH = 0xB480;
        wint_enable();
    }
}

void PWRMGR_watchdog_rearm(void)
{
	wint_disable();
    WDOG_UNLOCK = 0xc520;
    WDOG_UNLOCK = 0xd928;
	WDOG_STCTRLH |= WDOG_STCTRLH_WDOGEN_MASK;
	wint_enable();
}

void PWRMGR_watchdog_disarm(void)
{
	wint_disable();
    WDOG_UNLOCK = 0xc520;
    WDOG_UNLOCK = 0xd928;
	WDOG_STCTRLH = (WDOG_STCTRLH & ~WDOG_STCTRLH_WDOGEN_MASK) | WDOG_STCTRLH_ALLOWUPDATE_MASK;
	wint_enable();
}

/*
 *   Let a caller know if we are currently allowing OTA Firmware Update.
 */
uint_8 PWRMGR_ota_fwu_is_allowed(void)
{
    return (g_ota_fwu_allowed > 0)? 1:0;
}

boolean PWRMGR_is_batt_level_safe(void)
{
    g_stored_battery_counts = adc_sample_battery();
    g_stored_battery_mv = (uint32_t) adc_getNatural(ADC0_Vsense_Corona_VBat, (uint16_t) g_stored_battery_counts);

    return g_stored_battery_mv > LOW_VBAT_THRESHOLD;
}

/*
 * Allows other parts of the app to store the current value of the battery in counts
 * to the variable global private variable so it can be retrieved later.
 * 
 * Returns 1 if the battery is good, 0 if battery is low enough to trigger a system shutdown
 */
uint8_t PWRMGR_process_batt_status( boolean fast_shutdown ) 
{
    PWRMGR_VoteForAwake( PWRMGR_PWRMGR_VOTE );
    
    /*
     *     Check various action-able thresholds.
     */
    if( !PWRMGR_is_batt_level_safe() && !PWRMGR_usb_is_inserted() )
    {
    	/* Let's give it a few secs, then check again */
    	_time_delay(1500);
    	
    	if( !PWRMGR_is_batt_level_safe() )
    	{
    		/* 
    		 *   OK. I believe you. Go into Ship Mode quick-like. Will be documented by ship mode crumb.
    		 */
    	    if( fast_shutdown )
    	    {
    	    	// In this case, we are entering ship programatically.
    	    	//  Make sure we don't allow it if USB should become connected.
    	        PWRMGR_ship_allowed_on_usb_conn(0);
    	    }

    	    PWRMGR_Ship( fast_shutdown );

    		return 0;
    	}
    }
    
    /*
     *    See if we're OK for OTA FWU
     */
    if( g_stored_battery_mv < LOW_VBAT_FWU_THRESH)
    {
        g_ota_fwu_allowed = 0;
    }
    else
    {
        g_ota_fwu_allowed = 1;
    }
    
    /*
     *   Do the temperature last, since we may not even want/need to read it for the low battery cases.
     */
    g_stored_temp_counts = adc_sample_mcu_temp();
    
    corona_print("\n\tBATT:%umv\tTEMP:%u\tOTA allow:%u\n\n", g_stored_battery_mv, g_stored_temp_counts, PWRMGR_ota_fwu_is_allowed());
    PWRMGR_VoteForSleep( PWRMGR_PWRMGR_VOTE );
    
    return 1;
}


/*
 *   This function stores the battery status in EVTMGR, so that it can be uploaded later.
 *     In addition, it will track the important battery thresholds and store their state.
 */
void PWRMGR_encode_batt_status(void)
{
    //uint32_t bat_counts = adc_sample_battery();
    
    WMP_post_battery_status(g_stored_battery_counts, 
                            PWRMGR_calc_batt_percent(g_stored_battery_counts), 
                            adc_sample_mcu_temp(), g_stored_battery_mv,
                            LOW_VBAT_ALERT_THRESHOLD,
                            WIFIMGR_wifi_is_on(), 
                            BTMGR_bt_is_on());
}

uint32_t PWRMGR_get_stored_batt_counts( void )
{
    return g_stored_battery_counts;
}

uint32_t PWRMGR_get_stored_temp_counts( void )
{
    return g_stored_temp_counts;
}

static int32_t PWRMGR_calc_batt_percent(uint32_t battery_counts)
{
    uint32_t bat_volt;
    uint32_t bat_no_offset;
    int64_t res = -137879437212LL; // Start with c0=-2.56820465e+02 for polynomial
    int64_t temp;
      
    /*
     * The goal here is to use a polynomial to fit the upper part of the curve and a
     * linear fit to fit the steep drop off at the end.  These values were calculated
     * using data from 4 units tested using 0.2-BATCAL
     */
    // Linear parameters
    const int32_t poly_cut_off_volt = 3700;
    const int32_t min_poly_percent = 7; // Calculated from the polynomial to be continuous
    
    // Polynomial parameters all shifted by by final_scale to keep resolution
    const int32_t c3 = 519; // 9.66042260e-07
    const int32_t c2 = -1023913; // -1.90718698e-03
    const int32_t c1 = 730981320; // 1.36155881e+00
    const int32_t final_scale = 29;
    
    /*
     * Min & Max voltage are used to contain the values, avoid unneccessary computations
     * and ensure that we do not cross any inflection points.
     */
    const uint32_t max_volt = 4154;
    const uint32_t min_volt = LOW_VBAT_THRESHOLD; // Defines the 0 battery level below which we go into ship mode
    
    if ( (PWR_STATE_CHARGING == g_pwrmgr_state) || (PWR_STATE_CHARGED == g_pwrmgr_state) )
    {
        corona_print("bat chg'g\n");
        if( 0 == lwgpio_get_value( &chg_b_gpio ) )
        {
            /*
             *   CHG_B signal was asserted, return "is charging".
             */
            return 101;
        }
        
        return 102;
    }
    
    bat_volt = (uint32_t) adc_getNatural(ADC0_Vsense_Corona_VBat, (uint16_t) battery_counts);    
    
    if (bat_volt >= max_volt)
    {
        res = 100;
        goto done_batt_perc;
    }
    else if (bat_volt <= min_volt)
    {
        res = 0;
        goto done_batt_perc;
    }  
    
    bat_no_offset = bat_volt - min_volt; // Cannot be negative due to conditionals above
    
    if (bat_volt < poly_cut_off_volt) // linear
    {
        res = (bat_no_offset * min_poly_percent) / (poly_cut_off_volt - min_volt);
    } else // polynomial
    {
        temp = bat_no_offset;
        res += (c1 * temp);
        temp *= bat_no_offset;
        res += (c2 * temp);
        temp *= bat_no_offset;
        res += (c3 * temp);
        res /= (1 << final_scale); // Remove scale factor
    }
    
    if (res < 0)
    {
        res = 0;
        goto done_batt_perc;
    }
    else if (res > 100)
    {
        res = 100;
        goto done_batt_perc;
    }
    
done_batt_perc:
    corona_print("BATT: %i%%\t", res);
    corona_print("%u cnt\n", bat_volt);
    return (int8_t) res;
}

int32_t PWRMGR_get_batt_percent( void )
{
    return PWRMGR_calc_batt_percent(adc_sample_battery());
}

int32_t PWRMGR_get_stored_batt_percent( void )
{
    return PWRMGR_calc_batt_percent(g_stored_battery_counts);
}
