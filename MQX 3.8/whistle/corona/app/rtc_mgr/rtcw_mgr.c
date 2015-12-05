/*
 * rtc_mgr.c
 *
 *  Created on: Oct 29, 2013
 *      Author: Chris
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include "rtc_alarm.h"
#include "rtcw_mgr.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define RTCW_CHECK_TIMEOUT 0x00000001
#define RTCW_CHECK_SECONDS 10

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static LWEVENT_STRUCT g_rtcwmgr_lwevent;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

static void _RTCWMGR_timeout_callback(void)
{
    _lwevent_set(&g_rtcwmgr_lwevent, RTCW_CHECK_TIMEOUT);
}

void RTCWMGR_init(void)
{
    _lwevent_create(&g_rtcwmgr_lwevent, LWEVENT_AUTO_CLEAR);
}

void RTCW_tsk(uint_32 dummy)
{	
	while(TRUE)
	{
	    // Start an RTC timer that the watchdog will keep an eye on to make sure RTC timers still work
	    // watchdog timers use sys-tick-based timers, so the sys-tick system will guard the RTC system.
		RTC_add_alarm_in_seconds(RTCW_CHECK_SECONDS, (rtc_alarm_cbk_t) _RTCWMGR_timeout_callback);
	    
	    // Let the watchdog out to watch the RTC system. Give the RTC a head start of 2000 ms.
		// Note, the first 1000 of the 3000 accounts for the fact that setting an RTC timeout of
		// X seconds into the future will actually expire X+1 seconds into the future.
	    _watchdog_start(RTCW_CHECK_SECONDS*1000 + 1000 + 2000);
	    
		_lwevent_wait_ticks(&g_rtcwmgr_lwevent, RTCW_CHECK_TIMEOUT, FALSE, 0);
		
		// Timers are working or we wouldn't get here, so we can call off the watchdog.
		_watchdog_stop();
	}
}
