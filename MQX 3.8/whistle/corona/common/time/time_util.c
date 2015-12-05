/*
 * time_util.c
 *
 *  Created on: Mar 27, 2013
 *      Author: Chris
 *      
 *      
 *  Utilities for using time.
 */

#include <mqx.h>
#include <mutex.h>
#include "time_util.h"
#include "wassert.h"
#include "pmem.h"
#include "cfg_mgr.h"
#include "rtc_alarm.h"

// Private Functions

static void util_one_shot_timeout_at(_timer_id id, pointer data_ptr, MQX_TICK_STRUCT_PTR pticks_not_used)
{
    ((UTIL_timer_expired_cbk_t)data_ptr)();
}

static void util_one_shot_timeout(_timer_id id, pointer data_ptr, uint_32 seconds_not_used, uint_32 ms_not_used)
{
    ((UTIL_timer_expired_cbk_t)data_ptr)();
}

// Public Functions

/*
 * Compare 'this_time' and 'that_time' with a resolution of 1 OS tick.
 * Returns true if 'this_time' is sooner than or equal to 'that_time'.
 */
boolean UTIL_time_is_sooner_than( whistle_time_t this_time, whistle_time_t that_time ) 
{
	if ( this_time.TICKS[1] == that_time.TICKS[1])
	{
		return (this_time.TICKS[0] <= that_time.TICKS[0]);
	}
	return (this_time.TICKS[1] < that_time.TICKS[1]);
}

void UTIL_time_print_mqx_time
(
        DATE_STRUCT_PTR     time_date,
        TIME_STRUCT_PTR     time_sec_ms
)
{
    corona_print ("MQX : %d s, %d ms (%02d.%02d.%4d %02d:%02d:%02d )\n", time_sec_ms->SECONDS, time_sec_ms->MILLISECONDS, time_date->DAY, time_date->MONTH, time_date->YEAR, time_date->HOUR, time_date->MINUTE, time_date->SECOND);
}

void UTIL_time_print_rtc_time
(
        DATE_STRUCT_PTR     time_date,
        TIME_STRUCT_PTR     time_sec_ms
)
{
	  corona_print ("RTC : %02d.%02d.%4d %02d:%02d:%02d (%02d s, %03d ms)\n", time_date->DAY, time_date->MONTH, time_date->YEAR, time_date->HOUR, time_date->MINUTE, time_date->SECOND, time_sec_ms->SECONDS, time_sec_ms->MILLISECONDS);
}

void UTIL_time_show_mqx_time(void)
{
	DATE_STRUCT time_date;
	TIME_STRUCT time_mqx;
	_time_get( &time_mqx );
	
	_time_to_date(&time_mqx, &time_date);
	UTIL_time_print_mqx_time( &time_date, &time_mqx );
}

void UTIL_time_show_rtc_time(void)
{
    DATE_STRUCT     time_date;
    TIME_STRUCT     time_sec_ms;

    _rtc_get_time_mqxd (&time_date);
    _time_from_date (&time_date, &time_sec_ms);
    UTIL_time_print_rtc_time( &time_date, &time_sec_ms );
}

void UTIL_time_get(whistle_time_t *ptime)
{
	_time_get_ticks((MQX_TICK_STRUCT *)ptime);
}

int_32 UTIL_time_diff_seconds(whistle_time_t *pend, whistle_time_t *pstart)
{
    boolean overflow;
    int_32 seconds;
    
    seconds = _time_diff_seconds(pend, pstart, &overflow);

    if (overflow)
    {
        // assume overflow means pstart later than pend
        return -1;
    }
    
    return seconds;
}

// check if time is set to a defined value
boolean UTIL_time_is_set(whistle_time_t *ptime)
{
    whistle_time_t unset_time;
    
    UTIL_time_unset(&unset_time);
    
    return (UTIL_time_diff_seconds(ptime, &unset_time) != 0);
}

// set time to value which is guaranteed to be an undefined value
void UTIL_time_unset(whistle_time_t *ptime)
{
    //_time_init_ticks(ptime, MAX_MQX_UINT);
	ptime->HW_TICKS = 0;
	ptime->TICKS[0] = MAX_MQX_UINT;
	ptime->TICKS[1] = MAX_MQX_UINT;
}

static UTIL_timer_t __UTIL_time_start_oneshot_seconds_timer(boolean is_wakeup_timer, uint_32 seconds_timeout, UTIL_timer_expired_cbk_t cbk)
{
    whistle_time_t ticks_timeout;
    TIME_STRUCT time_timeout;
    _timer_id id;
    
    UTIL_time_set_future( &ticks_timeout, seconds_timeout );
    _ticks_to_time( &ticks_timeout, &time_timeout );

    if ( is_wakeup_timer )
    {
        // add an alarm timer to the RTC alarm queue
        id = RTC_add_alarm_at_mqx( &ticks_timeout, (rtc_alarm_cbk_t) cbk );
    }
    else
    {
        id = _timer_start_oneshot_at( util_one_shot_timeout, cbk, TIMER_KERNEL_TIME_MODE, &time_timeout );

    }
    
    wassert(id != TIMER_NULL_ID);
    
    return id;
}

static UTIL_timer_t __UTIL_time_start_oneshot_ms_timer(uint_32 ms_timeout, UTIL_timer_expired_cbk_t cbk)
{
    _timer_id timer;
    TIME_STRUCT time_struct;
     
    // NOTE: leaving this one to use elapsed time
    _time_get_elapsed(&time_struct);

    time_struct.MILLISECONDS += ms_timeout;
    
    return _timer_start_oneshot_at( util_one_shot_timeout, cbk, TIMER_ELAPSED_TIME_MODE, &time_struct );
}

static UTIL_timer_t __UTIL_time_start_oneshot_timer_at(boolean is_wakeup_timer, whistle_time_t *ptimeout_time, UTIL_timer_expired_cbk_t cbk)
{
    MQX_TICK_STRUCT time;
    _timer_id id;
    
    // Timeout must be a valid time
    wassert( UTIL_time_is_set(ptimeout_time ) );
    
    if ( is_wakeup_timer )
    {
        // add an alarm timer to the RTC alarm queue
        id = RTC_add_alarm_at_mqx( ptimeout_time, (rtc_alarm_cbk_t) cbk );
    }
    else
    {

        id = _timer_start_oneshot_at_ticks( util_one_shot_timeout_at, cbk, TIMER_KERNEL_TIME_MODE, ptimeout_time ); 
    }

    wassert(id != TIMER_NULL_ID);
    
    return id;
}

#ifdef TIMER_DEBUG
UTIL_timer_t _UTIL_time_start_oneshot_seconds_timer(boolean is_wakeup_timer, uint_32 seconds_timeout, UTIL_timer_expired_cbk_t cbk, char *file, int line)
{
	corona_print("UTIL_time_start_oneshot_seconds_timer %s: %d\n", file, line);
	return __UTIL_time_start_oneshot_seconds_timer(is_wakeup_timer, seconds_timeout, cbk);
}

UTIL_timer_t _UTIL_time_start_oneshot_ms_timer(uint_32 ms_timeout, UTIL_timer_expired_cbk_t cbk, char *file, int line)
{
	corona_print("UTIL_time_start_oneshot_ms_timer %s: %d\n", file, line);
	return __UTIL_time_start_oneshot_ms_timer(ms_timeout, cbk);
}

UTIL_timer_t _UTIL_time_start_oneshot_timer_at(boolean is_wakeup_timer, whistle_time_t *ptimeout_time, UTIL_timer_expired_cbk_t cbk, char *file, int line)
{
	corona_print("UTIL_time_start_oneshot_timer_at %s: %d\n", file, line);
	return __UTIL_time_start_oneshot_timer_at(is_wakeup_timer, ptimeout_time, cbk);
}
#else
UTIL_timer_t UTIL_time_start_oneshot_seconds_timer(boolean is_wakeup_timer, uint_32 seconds_timeout, UTIL_timer_expired_cbk_t cbk)
{
	return __UTIL_time_start_oneshot_seconds_timer(is_wakeup_timer, seconds_timeout, cbk);
}

UTIL_timer_t UTIL_time_start_oneshot_ms_timer(uint_32 ms_timeout, UTIL_timer_expired_cbk_t cbk)
{
	return __UTIL_time_start_oneshot_ms_timer(ms_timeout, cbk);
}

UTIL_timer_t UTIL_time_start_oneshot_timer_at(boolean is_wakeup_timer, whistle_time_t *ptimeout_time, UTIL_timer_expired_cbk_t cbk)
{
	return __UTIL_time_start_oneshot_timer_at(is_wakeup_timer, ptimeout_time, cbk);
}
#endif

void UTIL_time_stop_timer(UTIL_timer_t *timer)
{
	wint_disable();
	
	if (0 == *timer)
	{
		wint_enable();
		return;
	}
	
    _timer_cancel(*timer);
    
    *timer = 0;
    wint_enable();
}

void UTIL_time_set_future(whistle_time_t *ptime, uint_32 seconds_from_now)
{
    MQX_TICK_STRUCT time;
    
    // MAX_MQX_UINT means time not valid
    if (MAX_MQX_UINT == seconds_from_now)
    {
    	UTIL_time_unset(ptime);
    	return;
    }
    
    //_time_get_elapsed_ticks(&time);
    _time_get_ticks(&time);
    
    memcpy(ptime, _time_add_sec_to_ticks(&time, seconds_from_now), sizeof(whistle_time_t));
}

/*
 * UTIL_time_with_boot
 * 
 * Overlay boot # at top 16 bits of time
 */
void UTIL_time_with_boot(uint_64 input_time, uint_64 *output_time)
{
    if (0 == input_time)
        *output_time = 0;
    
    *output_time = (uint_64) __SAVED_ACROSS_BOOT.boot_sequence;
    *output_time = *output_time << 48;
    *output_time |= input_time;
}

/*
 *   Returns both the absolute and relative time at the instant you call the function.
 */
void UTIL_time_abs_rel(uint_64 *pRelativeTime, uint_64 *pAbsoluteTime)
{
    TIME_STRUCT timestamp;
    uint_64 timestamp_ms;

    *pRelativeTime = 0;
    *pAbsoluteTime = 0;
    
    RTC_get_time_ms( &timestamp );
    timestamp_ms = UTIL_time_ms_from_struct(&timestamp);
    
    UTIL_time_with_boot(timestamp_ms , pRelativeTime );
    *pAbsoluteTime = time_calc_abs_utc_time( timestamp_ms );
}

uint_64 UTIL_time_ms_from_struct(TIME_STRUCT *timestamp)
{
    uint_64 res;
    res = (uint_64) timestamp->SECONDS;
    res *= 1000;
    res += (uint_64) timestamp->MILLISECONDS;
    return res;
}

// Helper function to add milliseconds to a time structure while keeping 1000 > ms >=0
void _add_ms_to_time(TIME_STRUCT *timestamp, int16_t millis)
{
    int16_t new_millis;
    new_millis = timestamp->MILLISECONDS + millis;
    if (new_millis < 0)
    {
        while (new_millis < 0)
        {
            timestamp->SECONDS--;
            new_millis += 1000;
        }
    } else
    {
        timestamp->SECONDS += (new_millis / 1000);
    }
    timestamp->MILLISECONDS = (new_millis % 1000);    
}

/*
 * This function returns the UTC time structure if we have enough information to calculate it
 *  but default back to the relative time calculated via the RTC if we don't
 */
void UTIL_utc_if_possible_time_struct(TIME_STRUCT *timestamp)
{
    RTC_get_time_ms(timestamp);
    
    if (__SAVED_ACROSS_BOOT.server_offset_seconds == 0 || !pmem_is_data_valid())
        return;
    
    timestamp->SECONDS += __SAVED_ACROSS_BOOT.server_offset_seconds;
    _add_ms_to_time(timestamp, __SAVED_ACROSS_BOOT.server_offset_millis);
}



uint_64 time_calc_abs_utc_time(uint_64 rel_time)
{    
    uint_64 res = 0;
    if (__SAVED_ACROSS_BOOT.server_offset_seconds == 0)
        return 0;
    if (!pmem_is_data_valid())
        return 0;
    
    res = __SAVED_ACROSS_BOOT.server_offset_seconds;
    res *= 1000;
    res += __SAVED_ACROSS_BOOT.server_offset_millis;
    res += rel_time;
    
    return res;
}

uint_64 time_calc_abs_local_time(uint_64 rel_time)
{
	uint_64 res = 0;
	
	res = time_calc_abs_utc_time(rel_time);
	
	if (res != 0)
		res += ((uint_64) g_dy_cfg.usr_dy.timezone_offset) * 1000;
	
    return res;
}

void time_set_server_time(int_64 server_time, boolean force_update)
{
	TIME_STRUCT timestamp;
	int_16 server_offset_millis;
	int_32 server_offset_seconds;
	int_32 seconds_diff;

	RTC_get_time_ms( &timestamp );
	
	server_offset_millis = (int_16) ((int_16)(server_time % 1000) - (int_16) timestamp.MILLISECONDS);
	server_offset_seconds = (int_32) ((int_64)(server_time / 1000) - (int_64) (timestamp.SECONDS));
	
	if (force_update)
	{
		goto UPDATE;
	}
	
	// Only update if more than +/- 2 sec difference.
	seconds_diff = server_offset_seconds - __SAVED_ACROSS_BOOT.server_offset_seconds;
	
	// might be negative
	if (seconds_diff < 0 )
	{
		seconds_diff = -seconds_diff;
	}
	
	if (seconds_diff < 2)
	{
		return;
	}
	
UPDATE:
    wint_disable();
	__SAVED_ACROSS_BOOT.server_offset_millis = server_offset_millis;
	__SAVED_ACROSS_BOOT.server_offset_seconds = (int_32) server_offset_seconds;
	pmem_update_checksum();
	wint_enable();
}


