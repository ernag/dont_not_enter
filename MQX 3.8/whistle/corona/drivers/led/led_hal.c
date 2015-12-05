/*
 * led_hal.c
 *
 *  Created on: Apr 11, 2013
 *      Author: Chris
 */

#include <mqx.h>
#include "wmutex.h"
#include "led_hal.h"
#include "bu26507/bu26507.h"
#include "wassert.h"

static MUTEX_STRUCT g_led_mutex;

err_t led_run_pattern(led_pattern_t pattern)
{
	// Don't grab the mutex for the fail pattern. This is an emergency!
	if (LED_PATTERN_FAIL == pattern)
	{
	    int num_blinks = 10;
	    
	    bu26507_run_pattern(BU26507_PATTERN_OFF);
	    
	    while(num_blinks--)
	    {
	        bu26507_run_pattern(BU26507_PATTERN_FAIL);
	        _time_delay(40);
	        
	        bu26507_run_pattern(BU26507_PATTERN_OFF);
	        _time_delay(40);
	    }
        
        return ERR_OK;
	}
	
    wmutex_lock(&g_led_mutex);
    
    // The idea here, which you are welcome to improve, is to translate a generic LED pattern type
    // into a set of commands that are specific to the underlying driver type. If we switch out
    // the underlying driver with a different one, you would re-implement this file. This file is
    // for the BU26507. For now, the translation is implemented with a case statement. At some point
    // this may be table driven, where maybe the table points to a script to run.
    
    bu26507_run_pattern(BU26507_PATTERN_OFF);   // Make sure a matrix pattern is cleared

    switch(pattern)
    {
        case LED_PATTERN_OFF:
            // bu26507_run_pattern(BU26507_PATTERN_OFF);
            break;
        case LED_PATTERN_1:
            bu26507_run_pattern(BU26507_PATTERN1);
            break;
        case LED_PATTERN_2:
            bu26507_run_pattern(BU26507_PATTERN2);
            break;      
        case LED_PATTERN_3:
            bu26507_run_pattern(BU26507_PATTERN3);
            break;      
        case LED_PATTERN_4:
            bu26507_run_pattern(BU26507_PATTERN18);
            break;
        case LED_PATTERN_5:
            bu26507_run_pattern(BU26507_PATTERN54);
            break;
        case LED_PATTERN_6:
            bu26507_run_pattern(BU26507_PATTERN101);
            break;
        case LED_PATTERN_7:
            bu26507_run_pattern(BU26507_PATTERN102);
            break;      
        case LED_PATTERN_8:
            bu26507_run_pattern(BU26507_PATTERN103);
            break;      
        case LED_PATTERN_9:
            bu26507_run_pattern(BU26507_PATTERN118);
            break;
        case LED_PATTERN_10:
            bu26507_run_pattern(BU26507_PATTERN154);
            break;
        case LED_PATTERN_11:
            bu26507_run_pattern(BU26507_PATTERN201);
            break;
        case LED_PATTERN_12:
            bu26507_run_pattern(BU26507_PATTERN202);
            break;      
        case LED_PATTERN_13:
            bu26507_run_pattern(BU26507_PATTERN203);
            break;      
        case LED_PATTERN_14:
            bu26507_run_pattern(BU26507_PATTERN218);
            break;
        case LED_PATTERN_15:
            bu26507_run_pattern(BU26507_PATTERN254);
            break;
        case LED_PATTERN_16:
            bu26507_run_pattern(BU26507_PATTERN55);
            break;
        case LED_PATTERN_17:
            bu26507_run_pattern(BU26507_PATTERN155);
            break;
        case LED_PATTERN_18:
            bu26507_run_pattern(BU26507_PATTERN255);
        case LED_PATTERN_MY3S:
            bu26507_run_pattern(BU26507_PATTERN_MY3S);
            break;
        case LED_PATTERN_MY3P:
            bu26507_run_pattern(BU26507_PATTERN_MY3P);
            break;
        case LED_PATTERN_MR1P:
            bu26507_run_pattern(BU26507_PATTERN_MR1P);
            break;
        case LED_PATTERN_CHGING:
            bu26507_run_pattern(BU26507_PATTERN_CHGING);
            break;
        case LED_PATTERN_CHGD:
            bu26507_run_pattern(BU26507_PATTERN_CHGD);
            break;
        case LED_PATTERN_BT_PAIR:
            bu26507_run_pattern(BU26507_PATTERN_BT_PAIR);
            break;
        case LED_PATTERN_MW3S:
            bu26507_run_pattern(BU26507_PATTERN_MW3S);
            break;
        case LED_PATTERN_MA3S:
            bu26507_run_pattern(BU26507_PATTERN_MA3S);
            break;
        case LED_PATTERN_BA3S:
            bu26507_run_pattern(BU26507_PATTERN_BA3S);
            break;
        case LED_PATTERN_MW2S:
            bu26507_run_pattern(BU26507_PATTERN_MW2S);
            break;
        case LED_PATTERN_RGB:
           bu26507_run_pattern(BU26507_PATTERN_RGB);
           break;
        case LED_PATTERN_FAIL:
           bu26507_run_pattern(BU26507_PATTERN_FAIL);
           break;
//        case LED_PATTERN_PWR_DRAIN_TEST:
//           bu26507_run_pattern(BU26507_PATTERN_PWR_DRAIN_TEST);
//           break;
        default:
            wassert(0);
    }
    
    wmutex_unlock(&g_led_mutex);
    
    return ERR_OK;
}

//err_t bu26507_run_matrix(matrix_setup_t* mxsp, boolean isScroll)
//{
//    wassert(MQX_OK == _mutex_lock(&g_led_mutex));
//    wassert(ERR_OK == xbu26507_run_matrix(mxsp, isScroll));
//    wassert(MQX_OK == _mutex_unlock(&g_led_mutex));
//    return ERR_OK;
//}

//err_t led_display_battery_level( uint_8 pct )
//{
//    int i;
//    led_info_t led_info;
//    wassert(MQX_OK == _mutex_lock(&g_led_mutex));    
//    
//    // Turn on LEDs 1-10 according to battery percentage
//    led_info.led_mask = 0;
//    for (i=0; i < 10; i++)
//    {
//        if ( pct >= (10 + 10*i) )
//        {
//            // turn on led in the mask
//            led_info.led_mask |= 1 << i; 
//        }
//        else
//        {
//            break; 
//        }
//    }
//    led_info.color_mask = CLR_GREEN;
//    led_info.brightness = DEFAULT_BRIGHTNESS;
//    
//    bu26507_run_select( &led_info );
//    
//    
//    wassert(MQX_OK == _mutex_unlock(&g_led_mutex));
//
//    return ERR_OK;
//}

void led_deinit(void)
{
	 _mutex_try_lock(&g_led_mutex); // Get it to lock it, but don't care if we don't get it
    bu26507_run_pattern(BU26507_PATTERN_OFF);   // Make sure a matrix pattern is cleared
}

void led_init(void)
{
    wmutex_init(&g_led_mutex);
    
    bu26507_run_pattern(BU26507_PATTERN_OFF);   // Make sure a matrix pattern is cleared
}
