/*
 * dis_mgr_ut.c
 *
 *  Created on: Apr 11, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <mqx.h>
#include <bsp.h>
#include "dis_mgr.h"
#include "dis_mgr_ut.h"

void dis_mgr_ut(void)
{
#if 0
    corona_print("DISMGR_WIFI_CONNECTING...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_WIFI_CONNECTING_ONESHOT);
    
    _time_delay(1000);
    corona_print("DISMGR_LOW_BATTERY...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_LOW_BATTERY_RUN);
    
    _time_delay(1000);
    corona_print("DISMGR_WIFI_CONNECTING (should be ignored due to low battery)...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_WIFI_CONNECTING_ONESHOT);

    _time_delay(5000);
    corona_print("DISMGR_WIFI_CONNECTING (should be ignored due to low battery)...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_WIFI_CONNECTING_ONESHOT);

    _time_delay(15000);
    corona_print("DISMGR_WIFI_CONNECTING (should be ignored due to low battery)...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_WIFI_CONNECTING_ONESHOT);

    _time_delay(2000);
    corona_print("DISMGR_OFF...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_OFF);
    
    _time_delay(2000);
    corona_print("DISMGR_WIFI_CONNECTING...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_WIFI_CONNECTING_ONESHOT);

    _time_delay(3000);
    corona_print("DISMGR_WIFI_CONNECTED...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_WIFI_CONNECTED_ONESHOT);

    _time_delay(1000);
    corona_print("DISMGR_OFF...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_OFF);
    
    _time_delay(1000);
    corona_print("DISMGR_WIFI_CONNECTING...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_WIFI_CONNECTING_ONESHOT);
    
    _time_delay(3000);
    corona_print("DISMGR_WIFI_FAILED...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_WIFI_FAILED_ONESHOT);

    _time_delay(2000);
    corona_print("DISMGR_SHIP_MODE...\n"); fflush(stdout);
    DISMGR_run_pattern(DISMGR_PATTERN_SHIP_MODE_ONESHOT);
#endif
}
#endif
