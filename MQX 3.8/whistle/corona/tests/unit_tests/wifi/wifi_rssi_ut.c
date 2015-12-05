/*
 * wifi_rssi_ut.c
 *
 *  Created on: May 15, 2013
 *      Author: Ernie
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "ar4100p_main.h"
#include "wifi_ut.h"
#include "wifi_mgr.h"
#include "wifi_mgr_ut.h"
#include "app_errors.h"
#include "sys_event.h"
#include "corona_console.h"
#include "drivers/led/bu26507/bu26507.h"

#if WIFI_RSSI_TEST_APP


#define     RSSI_STRONG_SIGNAL      (25)    // 25+
#define     RSSI_MEDIUM_SIGNAL      (15)    // 15:24
#define     RSSI_LOW_SIGNAL          (0)    // 0:14

#define     NUM_TIMES_TO_SCAN        (6)    // we don't always find our network, so scan several times.
#define     USE_BUTTON_FOR_RSSI      (0)    // either button, or periodically print

#if !USE_BUTTON_FOR_RSSI
    #define USE_PERIODIC_RSSI        (1)
    #define DELAY_BETWEEN_RSSI_PRINTS_MS    (1000) // don't delay much at all, scanning for SSID's is enough of a delay.
#endif


static uint8_t g_waiting_to_perodically_show_RSSI = 1;


#if USE_PERIODIC_RSSI

/*
 *   Callback to use, to wait for a button press before starting periodic printing.
 */
static void _WIFI_RSSI_start_periodic_printing(sys_event_t sys_event)
{
    uint8_t i;
    
    /*
     *   Give user a visual indicator we acknowledged their button press.
     */
    for(i = 0; i < 5; i++)
    {
        bu26507_run_pattern( BU26507_PATTERN18 );
        _time_delay( 50 );
        bu26507_run_pattern( BU26507_PATTERN2 );
        _time_delay( 50 );
        bu26507_run_pattern( BU26507_PATTERN1 );
        _time_delay( 50 );
        bu26507_run_pattern( BU26507_PATTERN_OFF );
    }
     
    /*
     *   Release the periodically printing hounds.
     */
    g_waiting_to_perodically_show_RSSI = 0;
}
#endif  // USE_PERIODIC_RSSI


/*
 *   Callback for handling button press, scan WIFI and report results.
 *      Whatever the number is for max_rssi, blink that many times.
 */
static void _WIFI_RSSI_callback(sys_event_t sys_event)
{
    uint8_t max_rssi = 0;
    uint8_t temp_rssi;
    uint8_t i;
    bu26507_pattern_t pattern;
    
#if USE_BUTTON_FOR_RSSI
    corona_print("\n *** WIFI RSSI Callback: Button Event: 0x%x ***\n", sys_event);
#else
    corona_print("\n *** WIFI RSSI Callback ***\n");
#endif
    
    for(i = 0; i < NUM_TIMES_TO_SCAN; i++)
    {
        LMMGR_printscan( &temp_rssi, 1 );
        if(temp_rssi > max_rssi)
        {
            max_rssi = temp_rssi;  // update to latest max.
        }
        _time_delay(100);
    }
    corona_print("\n *** Max RSSI: %d ***\n\n", max_rssi);
    
    if( max_rssi >= RSSI_STRONG_SIGNAL )
    {
        pattern = BU26507_PATTERN18; // use green for strong signal
    }
    else if( max_rssi >= RSSI_MEDIUM_SIGNAL )
    {
        pattern = BU26507_PATTERN2; // use blue for medium signal
    }
    else
    {
        pattern = BU26507_PATTERN1; // use red for weak signal
    }
    
    /*
     *   For any RSSI number/stength, show the corresponding display
     *     pattern for a bit to begin with.
     */
    bu26507_run_pattern( pattern );
    _time_delay( 1000 );
    
    /*
     *   Blink the lights quickly, corresponding to exact RSSI number.
     */
    for(i = 0; i < max_rssi; i++)
    {
        bu26507_run_pattern( pattern );
        _time_delay( 45 );
        bu26507_run_pattern( BU26507_PATTERN_OFF );
    }
    
    bu26507_run_pattern( BU26507_PATTERN_OFF );
}



/*
 *   COR-275:  Scan WIFI and report RSSI results on display.
 */
void wifi_rssi_ut(void)
{
    
#if USE_BUTTON_FOR_RSSI
    
    /*
     *   All button events result in calling this callback.
     */
    sys_event_register_cbk( _WIFI_RSSI_callback, 
                            SYS_EVENT_BUTTON_DOUBLE_PUSH |
                            SYS_EVENT_BUTTON_SINGLE_PUSH |
                            SYS_EVENT_BUTTON_LONG_PUSH );
#else
    
    /*
     *   Register for system event to tell us when to start periodically printing.
     */
    sys_event_register_cbk( _WIFI_RSSI_start_periodic_printing, 
                            SYS_EVENT_BUTTON_DOUBLE_PUSH |
                            SYS_EVENT_BUTTON_SINGLE_PUSH |
                            SYS_EVENT_BUTTON_LONG_PUSH );
    
    /*
     *   Wait until user presses a button, so we don't blast until user wants us to.
     */
    while( g_waiting_to_perodically_show_RSSI )
    {
        _time_delay( DELAY_BETWEEN_RSSI_PRINTS_MS );
    }
    
    /*
     *   Use periodic printing of RSSI instead of button-activated.
     */
    while(1)
    {
        _WIFI_RSSI_callback(0);
        _time_delay( DELAY_BETWEEN_RSSI_PRINTS_MS );
    }
    
#endif
    
}
#endif  //WIFI_RSSI_TEST_APP
#endif
