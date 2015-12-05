/*
 * dis_mgr.c
 *
 *  Created on: Apr 11, 2013
 *      Author: Chris
 */

#include <mqx.h>
#include <bsp.h>
#include <watchdog.h>
#include "led_hal.h"
#include "app_errors.h"
#include "lwmsgq.h"
#include "wassert.h"
#include "time_util.h"
#include "dis_mgr_prv.h"
#include "min_act.h"
#include "pwr_mgr.h"
#include "factory_mgr.h"
#include "main.h"

static DISMGR_state_t g_dismgr_current_state = DISMGR_STATE_OFF;
static UTIL_timer_t g_dismgr_timer = 0;
static DISMGR_state_t g_dismgr_pushed_state = DISMGR_STATE_OFF;
static boolean g_dismgr_is_idle = TRUE;

uint_32 request_queue[sizeof(LWMSGQ_STRUCT)/sizeof(uint_32) + NUM_MESSAGES * MSG_SIZE];

// The idea here, which you are welcome to improve, is to translate a generic display pattern type
// into a set of LED patterns. For now, the translation is implemented with a case statement. At some point
// this may be table driven, where maybe the table points to a script to run.

static void DISMGR_timeout_callback( void )
{
    _mqx_uint          msg[MSG_SIZE];   
    
    msg[0] = (_mqx_uint)DISMGR_EVENT_TIMEOUT;
    msg[1] = 0;

    PWRMGR_VoteForAwake(PWRMGR_DISMGR_VOTE);
    PWRMGR_VoteForON(PWRMGR_DISMGR_VOTE);
    
    _lwmsgq_send((pointer)request_queue, msg, 0);
}

/*
 *   Harshly destroys the DISMGR as we know it.
 */
void DISMGR_destroy(void)
{
    whistle_task_destroy(DISPLAY_TASK_NAME);
}

void DISMGR_stop_timer(void)
{
    // cancel any pending
	UTIL_time_stop_timer(&g_dismgr_timer);
}

static void DISMGR_start_timer(_mqx_uint ms)
{
    DISMGR_stop_timer();
    g_dismgr_timer = UTIL_time_start_oneshot_ms_timer(ms, DISMGR_timeout_callback);
    wassert(g_dismgr_timer);
}

// The three following functions should not be called if minutes active is not being calculate
static led_pattern_t DISMGR_get_activity_colorAB(void)
{
	// Flash solid green if goal met, blue if not
	return MIN_ACT_met_daily_goal() ? LED_PATTERN_4 : LED_PATTERN_3;
}

static led_pattern_t DISMGR_get_activity_colorA(void)
{
	// Flash A-bank green if goal met, blue if not
	return MIN_ACT_met_daily_goal() ? LED_PATTERN_9 : LED_PATTERN_8;
}

static led_pattern_t DISMGR_get_activity_colorB(void)
{
	// Flash B-bank green if goal met, blue if not
	return MIN_ACT_met_daily_goal() ? LED_PATTERN_14 : LED_PATTERN_13;
}

static led_pattern_t DISMGR_get_battery_colorAB(void)
{
    // Flash solid red if battery low, green if not
    return PWRMGR_is_battery_low() ? LED_PATTERN_1 : LED_PATTERN_4;
}

static led_pattern_t DISMGR_get_battery_colorA(void)
{
    // Flash A-bank red if battery low, green if not
    return PWRMGR_is_battery_low() ? LED_PATTERN_6 : LED_PATTERN_9;
}

static led_pattern_t DISMGR_get_battery_colorB(void)
{
    // Flash B-bank red if battery low, green if not
    return PWRMGR_is_battery_low() ? LED_PATTERN_11 : LED_PATTERN_14;
}

static void DISMGR_execute_off(void)
{
    DISMGR_stop_timer();

    led_run_pattern(LED_PATTERN_OFF);

    g_dismgr_pushed_state = g_dismgr_current_state = DISMGR_STATE_OFF;
}

static void DISMGR_execute_power_on_oneshot(void)
{
    DISMGR_stop_timer();
    // led_run_pattern(LED_PATTERN_MW2S);   // Matrix white 2 seconds
    led_run_pattern(LED_PATTERN_BA3S);  // Battery status
    led_run_pattern(LED_PATTERN_OFF);
    
    g_dismgr_current_state = g_dismgr_pushed_state;
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
    	DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
}

static void DISMGR_execute_ship_mode_oneshot(void)
{
    // Blink red 4 times
    DISMGR_stop_timer();

    led_run_pattern(LED_PATTERN_1);
    _time_delay(100);
    led_run_pattern(LED_PATTERN_OFF);
    _time_delay(20);
    led_run_pattern(LED_PATTERN_1);
    _time_delay(100);
    led_run_pattern(LED_PATTERN_OFF);
    _time_delay(20);
    led_run_pattern(LED_PATTERN_1);
    _time_delay(100);
    led_run_pattern(LED_PATTERN_OFF);
    _time_delay(20);
    led_run_pattern(LED_PATTERN_1);
    _time_delay(100);
    led_run_pattern(LED_PATTERN_OFF);

    g_dismgr_pushed_state = g_dismgr_current_state = DISMGR_STATE_SHIP_MODE;
}

static void DISMGR_execute_batt_low_oneshot(void)
{
    // Blink red 3 times
    DISMGR_stop_timer();
    led_run_pattern(LED_PATTERN_MR1P);  // Matrix 1 red pulse
    led_run_pattern(LED_PATTERN_OFF);

    g_dismgr_current_state = g_dismgr_pushed_state;
    
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
        DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
}

static void DISMGR_execute_batt_low_repeat(void)
{
    // Start the persistent low battery red-flash toggle loop
    DISMGR_stop_timer();
    led_run_pattern(LED_PATTERN_1);
    DISMGR_start_timer(DISMGR_LOW_BATTERY_LOOP_ON_MS);
    
    g_dismgr_pushed_state = g_dismgr_current_state = DISMGR_STATE_BATT_LOW_LOOP_ON;
    
    //    DISMGR_execute_batt_low_oneshot();
    //    DISMGR_start_timer(DISMGR_LOW_BATTERY_REPEAT_LOOP);
    //    g_dismgr_pushed_state = g_dismgr_current_state = DISMGR_STATE_BATT_LOW_REPEAT;
}

static void DISMGR_execute_batt_charge_charging_repeat(void)
{    
    DISMGR_stop_timer();
    led_run_pattern(LED_PATTERN_CHGING);  // Matrix amber (Rg) continuous pulses, slow slope cycle
    
    g_dismgr_pushed_state = g_dismgr_current_state = DISMGR_STATE_BATT_CHARGE_CHARGING;
}


static void DISMGR_execute_batt_charge_charged_repeat(void)
{
    // Solid white
    DISMGR_stop_timer();

    led_run_pattern(LED_PATTERN_CHGD);
    
    g_dismgr_pushed_state = g_dismgr_current_state = DISMGR_STATE_BATT_CHARGE_CHARGED;
}

static void DISMGR_execute_batt_low_loop_toggle(void)
{
	// Toggle red
    if (DISMGR_STATE_BATT_LOW_LOOP_OFF == g_dismgr_current_state)
    {
        led_run_pattern(LED_PATTERN_1);
        DISMGR_start_timer(DISMGR_LOW_BATTERY_LOOP_ON_MS);

        g_dismgr_current_state = DISMGR_STATE_BATT_LOW_LOOP_ON;
    }
    else
    {
        led_run_pattern(LED_PATTERN_OFF);
        DISMGR_start_timer(DISMGR_LOW_BATTERY_LOOP_OFF_MS);
        
        g_dismgr_current_state = DISMGR_STATE_BATT_LOW_LOOP_OFF;
    }
}

static void DISMGR_execute_batt_charge_charging_loop_toggle(void)
{
#if 0
	// Toggle yellow
    if (DISMGR_STATE_BATT_CHARGE_CHARGING_LOOP_OFF == g_dismgr_current_state)
    {
        led_run_pattern(LED_PATTERN_5);
        DISMGR_start_timer(DISMGR_USB_CHARGING_LOOP_ON_MS);

        g_dismgr_current_state = DISMGR_STATE_BATT_CHARGE_CHARGING_LOOP_ON;
    }
    else
    {
        led_run_pattern(LED_PATTERN_OFF);
        DISMGR_start_timer(DISMGR_USB_CHARGING_LOOP_OFF_MS);
        
        g_dismgr_current_state = DISMGR_STATE_BATT_CHARGE_CHARGING_LOOP_OFF;
    }
#endif
}

static void DISMGR_execute_bt_pairing_repeat(void)
{
    // Start the persistent BT pairing white toggle loop (pulsating slowly)
    DISMGR_stop_timer();
    led_run_pattern(LED_PATTERN_BT_PAIR); // Matrix white continuous slow slope cycle

    g_dismgr_current_state = DISMGR_STATE_BT_PAIRING;
}

static void DISMGR_execute_bt_pairing_loop_toggle(void)
{
#if 0
	// Toggle white/off
	if (DISMGR_STATE_BT_PAIRING_LOOP_OFF == g_dismgr_current_state)
    {
        led_run_pattern(LED_PATTERN_7);
        DISMGR_start_timer(DISMGR_BT_PAIRING_LOOP_ON_MS);

        g_dismgr_current_state = DISMGR_STATE_BT_PAIRING_LOOP_ON;
    }
    else if (DISMGR_STATE_BT_PAIRING_LOOP_ON == g_dismgr_current_state)
    {
        led_run_pattern(LED_PATTERN_12);
        DISMGR_start_timer(DISMGR_BT_PAIRING_LOOP_OFF_MS);
        
        g_dismgr_current_state = DISMGR_STATE_BT_PAIRING_LOOP_OFF;
    }
#endif
}

static void DISMGR_execute_bt_pairing_succeeded(void)
{
    // Solid white 3 sec.
    DISMGR_stop_timer();
#if 0
    led_run_pattern(LED_PATTERN_MW3S);  // Matrix white 3 second pulse
#endif
    led_run_pattern(LED_PATTERN_OFF);
    
    g_dismgr_current_state = g_dismgr_pushed_state;
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
    	DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
}

static void DISMGR_execute_bt_pairing_failed(void)
{
    // Blink yellow 3 times
    DISMGR_stop_timer();
#if 0
    led_run_pattern(LED_PATTERN_MY3P);  // Matrix 3 pulses of yelllow
#endif
    led_run_pattern(LED_PATTERN_OFF);
    
    g_dismgr_current_state = g_dismgr_pushed_state;
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
    	DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
}

static void DISMGR_execute_bt_connecting_repeat(void)
{
    // Start the persistent BT connecting continuous white (previously purple toggle loop)
#if 0
    led_run_pattern(LED_PATTERN_16);
    DISMGR_start_timer(DISMGR_BT_CONNECTING_LOOP_ON_MS);

	g_dismgr_current_state = DISMGR_STATE_BT_CONNECTING_LOOP_ON;
#endif
	
#if 0
	DISMGR_stop_timer();
    led_run_pattern(LED_PATTERN_2);  // Solid White
    
    g_dismgr_pushed_state = g_dismgr_current_state = DISMGR_STATE_BT_CONNECTING;
#endif
}

static void DISMGR_execute_bt_connecting_loop_toggle(void)
{
#if 0
	// Toggle purple on/off
	if (DISMGR_STATE_BT_CONNECTING_LOOP_OFF == g_dismgr_current_state)
    {
        led_run_pattern(LED_PATTERN_17);
        DISMGR_start_timer(DISMGR_BT_PAIRING_LOOP_ON_MS);

        g_dismgr_current_state = DISMGR_STATE_BT_CONNECTING_LOOP_ON;
    }
    else if (DISMGR_STATE_BT_CONNECTING_LOOP_ON == g_dismgr_current_state)
    {
        led_run_pattern(LED_PATTERN_18);
        DISMGR_start_timer(DISMGR_BT_PAIRING_LOOP_OFF_MS);
        
        g_dismgr_current_state = DISMGR_STATE_BT_CONNECTING_LOOP_OFF;
    }
#endif
}

static void DISMGR_execute_bt_connecting_succeeded(void)
{
#if 0
    // Solid white 3 sec.
    DISMGR_stop_timer();
    led_run_pattern(LED_PATTERN_MW3S);
    led_run_pattern(LED_PATTERN_OFF);
    
    g_dismgr_current_state = g_dismgr_pushed_state;
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
    	DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
#endif
}

static void DISMGR_execute_bt_connecting_failed(void)
{
#if 0
    // Blink yellow 3 times
    DISMGR_stop_timer();
    led_run_pattern(LED_PATTERN_MY3P);
    led_run_pattern(LED_PATTERN_OFF);
    
    g_dismgr_current_state = g_dismgr_pushed_state;
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
    	DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
#endif
}

static void DISMGR_execute_man_sync_repeat(void)
{
	// Start the persistent manual sync flash, starting with current activity color
    //led_run_pattern(DISMGR_get_activity_colorAB());
    led_run_pattern(DISMGR_get_battery_colorAB());
	
    DISMGR_start_timer(DISMGR_MAN_SYNC_LOOP_ON_MS);

	g_dismgr_current_state = DISMGR_STATE_MAN_SYNC_LOOP_ON;
}

static void DISMGR_execute_man_sync_loop_toggle(void)
{
	// Toggle current activity/off
	if (DISMGR_STATE_MAN_SYNC_LOOP_OFF == g_dismgr_current_state)
    {
        //led_run_pattern(DISMGR_get_activity_colorA());
	    led_run_pattern(DISMGR_get_battery_colorA());
        DISMGR_start_timer(DISMGR_MAN_SYNC_LOOP_ON_MS);

        g_dismgr_current_state = DISMGR_STATE_MAN_SYNC_LOOP_ON;
    }
    else if (DISMGR_STATE_MAN_SYNC_LOOP_ON == g_dismgr_current_state)
    {
        //led_run_pattern(DISMGR_get_activity_colorB());
        led_run_pattern(DISMGR_get_battery_colorB());
        DISMGR_start_timer(DISMGR_MAN_SYNC_LOOP_OFF_MS);
        
        g_dismgr_current_state = DISMGR_STATE_MAN_SYNC_LOOP_OFF;
    }
}

static void DISMGR_execute_man_sync_succeeded(void)
{
    // Current activity color 3 sec.
#if 0
	DISMGR_stop_timer();

    led_run_pattern(DISMGR_get_activity_colorAB());
    _time_delay(3000);
    led_run_pattern(LED_PATTERN_OFF);
#endif
    
    DISMGR_stop_timer();
#if 0
    //led_run_pattern(LED_PATTERN_MA3S);  // Matrix activity color 3 seconds
    led_run_pattern(LED_PATTERN_BA3S);    // Battery Status
#endif
    led_run_pattern(LED_PATTERN_OFF);
    
    g_dismgr_current_state = g_dismgr_pushed_state;
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
    	DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
}

static void DISMGR_execute_man_sync_failed(void)
{
    // Blink yellow 3 times
    DISMGR_stop_timer();
#if 0
    led_run_pattern(LED_PATTERN_MY3P);  // Matrix yellow 3 pulses
#endif
    led_run_pattern(LED_PATTERN_OFF);
    
    g_dismgr_current_state = g_dismgr_pushed_state;
    
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
    	DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
}

static void DISMGR_execute_man_sync_start_oneshot(void)
{
	// Start the persistent manual sync flash, starting with current activity color
//    led_run_pattern(DISMGR_get_activity_colorAB());	
//    _time_delay(3000);
    
    DISMGR_stop_timer();
    //led_run_pattern(LED_PATTERN_MA3S);
    led_run_pattern(LED_PATTERN_BA3S);  // Battery status
    led_run_pattern(LED_PATTERN_OFF);
    
    g_dismgr_current_state = g_dismgr_pushed_state;
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
    	DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
}

///////////////////////////////////////////////////////
// RGB command display
///////////////////////////////////////////////////////
static void DISMGR_execute_rgb_display(void)
{
    DISMGR_stop_timer();
    led_run_pattern(LED_PATTERN_RGB);
    led_run_pattern(LED_PATTERN_OFF);
    
    g_dismgr_current_state = g_dismgr_pushed_state;
    if (DISMGR_STATE_OFF != g_dismgr_current_state)
    {
        DISMGR_start_timer(DISMGR_STATE_RETURN_MS);
    }
}

static void DISMGR_process_pattern_request( DISMGR_request_PTR_t pReq )
{
    DISMGR_state_t old_state = g_dismgr_current_state;
    
    corona_print("DIS\tST:%i\tEVT:%i\n", old_state, (NULL != pReq)?pReq->event:0);
    
    switch(g_dismgr_current_state)
    {
    case DISMGR_STATE_OFF:
        DISMGR_stop_timer(); // just in case
    case DISMGR_STATE_BATT_CHARGE_CHARGING_LOOP_ON:
    case DISMGR_STATE_BATT_CHARGE_CHARGING_LOOP_OFF:
    case DISMGR_STATE_BATT_CHARGE_CHARGING:
    case DISMGR_STATE_BATT_CHARGE_CHARGED:
    case DISMGR_STATE_BATT_LOW_LOOP_ON:            
    case DISMGR_STATE_BATT_LOW_LOOP_OFF:            
    	switch (pReq->event)
    	{
    	case DISMGR_EVENT_OFF:
        case DISMGR_EVENT_BATT_LOW_STOP:
        case DISMGR_EVENT_BATT_CHARGE_STOP:
    		DISMGR_execute_off();
    		break;
        case DISMGR_PATTERN_BATT_LOW_ONESHOT:
            DISMGR_execute_batt_low_oneshot();
        	break;
        case DISMGR_EVENT_BATT_LOW_REPEAT:
            DISMGR_execute_batt_low_repeat();
            break;
        case DISMGR_EVENT_BATT_LEVEL_ONESHOT:
    	case DISMGR_EVENT_POWER_ON_ONESHOT:
    		DISMGR_execute_power_on_oneshot();
    		break;
        case DISMGR_EVENT_BATT_CHARGE_CHARGING_REPEAT:
            DISMGR_execute_batt_charge_charging_repeat();
            break;
        case DISMGR_EVENT_BATT_CHARGE_CHARGED_REPEAT:
            DISMGR_execute_batt_charge_charged_repeat();
            break;
    	case DISMGR_EVENT_SHIP_MODE_ONESHOT:
    		DISMGR_execute_ship_mode_oneshot();
    		break;
        case DISMGR_EVENT_BT_PAIRING_REPEAT:
            //        	if (PWRMGR_is_battery_low())
            //        	{
            //        		DISMGR_execute_batt_low_oneshot();
            //        	}
        	DISMGR_execute_bt_pairing_repeat();
        	break;
        case DISMGR_EVENT_BT_PAIRING_SUCCEEDED_STOP:
        	DISMGR_execute_bt_pairing_succeeded();
        	break;
        case DISMGR_EVENT_BT_PAIRING_FAILED_STOP:
        	DISMGR_execute_bt_pairing_failed();
        	break;
        case DISMGR_EVENT_BT_CONNECTING_REPEAT:
            //        	if (PWRMGR_is_battery_low())
            //        	{
            //        		DISMGR_execute_batt_low_oneshot();
            //        	}
        	DISMGR_execute_bt_connecting_repeat();
        	break;
        case DISMGR_EVENT_BT_CONNECTING_SUCCEEDED_STOP:
        	DISMGR_execute_bt_connecting_succeeded();
        	break;
        case DISMGR_EVENT_BT_CONNECTING_FAILED_STOP:
        	DISMGR_execute_bt_connecting_failed();
        	break;
        case DISMGR_EVENT_MAN_SYNC_REPEAT:
            // Currently done in the man sync callback
            if (0 && PWRMGR_is_battery_low())
        	{
        		DISMGR_execute_batt_low_oneshot();
        	}

        	DISMGR_execute_man_sync_repeat();
        	break;
        case DISMGR_EVENT_MAN_SYNC_START_ONESHOT:
            // Currently done in the man sync callback
        	if (0 && PWRMGR_is_battery_low())
        	{
        		DISMGR_execute_batt_low_oneshot();
        	}

        	DISMGR_execute_man_sync_start_oneshot();
        	break;
        case DISMGR_EVENT_MAN_SYNC_SUCCEEDED_STOP:
        case DISMGR_EVENT_MAN_SYNC_SUCCEEDED_ONESHOT:
        	DISMGR_execute_man_sync_succeeded();
        	break;
        case DISMGR_EVENT_MAN_SYNC_FAILED_STOP:
        case DISMGR_EVENT_MAN_SYNC_FAILED_ONESHOT:
        	DISMGR_execute_man_sync_failed();
        	break;
        case DISMGR_EVENT_RGB_DISPLAY:
           DISMGR_execute_rgb_display();
           break;
    	case DISMGR_EVENT_TIMEOUT:
    		switch(g_dismgr_current_state)
    		{
            case DISMGR_STATE_BATT_CHARGE_CHARGING_LOOP_ON:
            case DISMGR_STATE_BATT_CHARGE_CHARGING_LOOP_OFF:
            	DISMGR_execute_batt_charge_charging_loop_toggle();
            	break;
            case DISMGR_STATE_BATT_CHARGE_CHARGING:
            	DISMGR_execute_batt_charge_charging_repeat();
            	break;
            case DISMGR_STATE_BATT_CHARGE_CHARGED:
            	DISMGR_execute_batt_charge_charged_repeat();
            	break;
            case DISMGR_STATE_BATT_LOW_LOOP_ON:            
            case DISMGR_STATE_BATT_LOW_LOOP_OFF:            
                DISMGR_execute_batt_low_loop_toggle();
                break;
        	case DISMGR_STATE_BT_PAIRING_LOOP_ON:
        	case DISMGR_STATE_BT_PAIRING_LOOP_OFF:
        		DISMGR_execute_bt_pairing_loop_toggle();
        		break;
        	case DISMGR_STATE_BT_CONNECTING:
        	    DISMGR_execute_bt_connecting_repeat();
        	    break;
        	case DISMGR_STATE_BT_CONNECTING_LOOP_ON:
        	case DISMGR_STATE_BT_CONNECTING_LOOP_OFF:
        		DISMGR_execute_bt_connecting_loop_toggle();
        		break;
        	case DISMGR_STATE_MAN_SYNC_LOOP_ON:
        	case DISMGR_STATE_MAN_SYNC_LOOP_OFF:
        		DISMGR_execute_man_sync_loop_toggle();
        		break;
        	default:
        		break;
    		}
    		break;
    	default:
    		break;
    	}
    	break;                         
              
    case DISMGR_STATE_BT_CONNECTING:
    case DISMGR_STATE_BT_PAIRING:
    case DISMGR_STATE_BT_PAIRING_LOOP_ON:
    case DISMGR_STATE_BT_PAIRING_LOOP_OFF:
    case DISMGR_STATE_BT_CONNECTING_LOOP_ON:
    case DISMGR_STATE_BT_CONNECTING_LOOP_OFF:
    case DISMGR_STATE_MAN_SYNC_LOOP_ON:
    case DISMGR_STATE_MAN_SYNC_LOOP_OFF:
    	switch (pReq->event)
    	{
    	case DISMGR_EVENT_OFF:
    		DISMGR_execute_off();
    		break;
        case DISMGR_EVENT_BATT_LOW_STOP:
        case DISMGR_EVENT_BATT_CHARGE_STOP:
        	g_dismgr_pushed_state = DISMGR_STATE_OFF;
        	break;
    	case DISMGR_EVENT_BATT_LOW_REPEAT:
    		DISMGR_execute_batt_low_repeat();
    		break;
    	case DISMGR_EVENT_BATT_LEVEL_ONESHOT:
    	case DISMGR_EVENT_POWER_ON_ONESHOT:
    		DISMGR_execute_power_on_oneshot();
    		break;
        case DISMGR_EVENT_BATT_CHARGE_CHARGING_REPEAT:
        	g_dismgr_pushed_state = DISMGR_STATE_BATT_CHARGE_CHARGING;
            break;
        case DISMGR_EVENT_BATT_CHARGE_CHARGED_REPEAT:
        	g_dismgr_pushed_state = DISMGR_STATE_BATT_CHARGE_CHARGED;
        	break;
    	case DISMGR_EVENT_SHIP_MODE_ONESHOT:
    		DISMGR_execute_ship_mode_oneshot();
    		break;
    	case DISMGR_EVENT_BT_PAIRING_REPEAT:
    		DISMGR_execute_bt_pairing_repeat();
    		break;
    	case DISMGR_EVENT_BT_PAIRING_SUCCEEDED_STOP:
    		DISMGR_execute_bt_pairing_succeeded();
    		break;
    	case DISMGR_EVENT_BT_PAIRING_FAILED_STOP:
    		DISMGR_execute_bt_pairing_failed();
    		break;
    	case DISMGR_EVENT_BT_CONNECTING_REPEAT:
    		DISMGR_execute_bt_connecting_repeat();
    		break;
    	case DISMGR_EVENT_BT_CONNECTING_SUCCEEDED_STOP:
    		DISMGR_execute_bt_connecting_succeeded();
    		break;
    	case DISMGR_EVENT_BT_CONNECTING_FAILED_STOP:
    		DISMGR_execute_bt_connecting_failed();
    		break;
    	case DISMGR_EVENT_MAN_SYNC_REPEAT:
    		DISMGR_execute_man_sync_repeat();
    		break;
    	case DISMGR_EVENT_MAN_SYNC_SUCCEEDED_STOP:
        case DISMGR_EVENT_MAN_SYNC_SUCCEEDED_ONESHOT:
    		DISMGR_execute_man_sync_succeeded();
    		break;
    	case DISMGR_EVENT_MAN_SYNC_FAILED_STOP:
        case DISMGR_EVENT_MAN_SYNC_FAILED_ONESHOT:
    		DISMGR_execute_man_sync_failed();
    		break;
        case DISMGR_EVENT_MAN_SYNC_START_ONESHOT:
        	DISMGR_execute_man_sync_start_oneshot();
        	break;
    	case DISMGR_EVENT_TIMEOUT:
    		switch(g_dismgr_current_state)
    		{
    		case DISMGR_STATE_BT_PAIRING_LOOP_ON:
    		case DISMGR_STATE_BT_PAIRING_LOOP_OFF:
    			DISMGR_execute_bt_pairing_loop_toggle();
    			break;
    		case DISMGR_STATE_BT_CONNECTING_LOOP_ON:
    		case DISMGR_STATE_BT_CONNECTING_LOOP_OFF:
    			DISMGR_execute_bt_connecting_loop_toggle();
    			break;
    		case DISMGR_STATE_MAN_SYNC_LOOP_ON:
    		case DISMGR_STATE_MAN_SYNC_LOOP_OFF:
    			DISMGR_execute_man_sync_loop_toggle();
    			break;
    		default:
    			break;
    		}
    		break;
    		default:
    			break;
    	}
    	break;                         

    case DISMGR_STATE_SHIP_MODE:
    	// No way out of ship mode
    	break;

    default:
    	switch(pReq->event)
    	{
    	case DISMGR_EVENT_OFF:
    		DISMGR_execute_off();
    		break;
    	case DISMGR_EVENT_BATT_LOW_REPEAT:
    		DISMGR_execute_batt_low_repeat();
    		break;
    	case DISMGR_EVENT_SHIP_MODE_ONESHOT:
    		DISMGR_execute_ship_mode_oneshot();
    		break;
    	default:
    		break;
    	}

    	break;
    }
}

void DISMGR_init(void)
{
    led_init();
    _lwmsgq_init((pointer)request_queue, NUM_MESSAGES, MSG_SIZE);
}

void DIS_tsk(uint_32 dummy)
{
    _mqx_uint msg[MSG_SIZE];  

    while (1)
    {
        _watchdog_stop();

        _lwmsgq_receive((pointer)request_queue, msg, LWMSGQ_RECEIVE_BLOCK_ON_EMPTY, 0, 0);
        
        _watchdog_start(60*1000);
        
        /*
         *   When we are in factory burn-in mode, we hi-jack the display system, and it cannot be used 
         *      for normal display purposes.
         */
        if(burnin_running())
        {
            continue;
        }

        DISMGR_process_pattern_request( (DISMGR_request_PTR_t) msg );
        
        if (LWMSGQ_IS_EMPTY((pointer)request_queue))
        {
            PWRMGR_VoteForOFF(PWRMGR_DISMGR_VOTE);
            PWRMGR_VoteForSleep(PWRMGR_DISMGR_VOTE);
        }
    }
}

err_t DISMGR_run_pattern(DISMGR_pattern_t pattern)
{
    _mqx_uint          msg[MSG_SIZE];   
    
    msg[0] = (_mqx_uint)pattern; // pattern as event
    msg[1] = 0;
    
    /*
     *   We vote for Awake/ON here instead of the low priority Display task, 
     *      to minimize risk of thinking its OK to sleep/reboot/ship mode prematurely.
     */
    PWRMGR_VoteForAwake(PWRMGR_DISMGR_VOTE);
    PWRMGR_VoteForON(PWRMGR_DISMGR_VOTE);

    _lwmsgq_send((pointer)request_queue, msg, 0);

    return ERR_OK;
}

err_t DISMGR_display_battery(uint_8 pct)
{
    _mqx_uint          msg[MSG_SIZE];   
    
    msg[0] = (_mqx_uint)DISMGR_EVENT_BATT_LEVEL_ONESHOT;
    msg[1] = (_mqx_uint)pct;
    
    PWRMGR_VoteForAwake(PWRMGR_DISMGR_VOTE);
    PWRMGR_VoteForON(PWRMGR_DISMGR_VOTE);

    _lwmsgq_send((pointer)request_queue, msg, 0);

    return ERR_OK;
}
