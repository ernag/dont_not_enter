/*
 * time_util.h
 *
 *  Created on: Mar 27, 2013
 *      Author: Chris
 */

#ifndef TIME_UTIL_H_
#define TIME_UTIL_H_

#include <mqx.h>
#include <bsp.h>
#include <timer.h>

// TODO: this needs to use a timer that can sleep across low power MCU states
// For now, just use standard timer.
typedef MQX_TICK_STRUCT whistle_time_t;

typedef void (*UTIL_timer_expired_cbk_t)(void);

typedef _timer_id UTIL_timer_t;

//#define TIMER_DEBUG

#ifdef TIMER_DEBUG
#define UTIL_time_start_oneshot_seconds_timer(is_wakeup_timer, seconds_timeout, cbk) _UTIL_time_start_oneshot_seconds_timer(is_wakeup_timer, seconds_timeout, cbk, __FILE__, __LINE__)
UTIL_timer_t _UTIL_time_start_oneshot_seconds_timer(boolean is_wakeup_timer, uint_32 seconds_timeout, UTIL_timer_expired_cbk_t cbk, char *file, int line);

#define UTIL_time_start_oneshot_ms_timer(ms_timeout, cbk) _UTIL_time_start_oneshot_ms_timer(ms_timeout, cbk, __FILE__, __LINE__)
UTIL_timer_t _UTIL_time_start_oneshot_ms_timer(uint_32 ms_timeout, UTIL_timer_expired_cbk_t cbk, char *file, int line);

#define UTIL_time_start_oneshot_timer_at(is_wakeup_timer, ptimeout_time, cbk) _UTIL_time_start_oneshot_timer_at(is_wakeup_timer, ptimeout_time, cbk, __FILE__, __LINE__)
UTIL_timer_t _UTIL_time_start_oneshot_timer_at(boolean is_wakeup_timer, whistle_time_t *ptimeout_time, UTIL_timer_expired_cbk_t cbk, char *file, int line);
#else
UTIL_timer_t UTIL_time_start_oneshot_seconds_timer(boolean is_wakeup_timer, uint_32 seconds_timeout, UTIL_timer_expired_cbk_t cbk);
UTIL_timer_t UTIL_time_start_oneshot_ms_timer(uint_32 ms_timeout, UTIL_timer_expired_cbk_t cbk);
UTIL_timer_t UTIL_time_start_oneshot_timer_at(boolean is_wakeup_timer, whistle_time_t *ptimeout_time, UTIL_timer_expired_cbk_t cbk);
#endif

void    UTIL_time_get(whistle_time_t *ptime);
int_32  UTIL_time_diff_seconds(whistle_time_t *pend, whistle_time_t *pstart);
boolean UTIL_time_is_sooner_than( whistle_time_t this_time, whistle_time_t that_time );
void    UTIL_time_stop_timer(UTIL_timer_t *timer);
void    UTIL_time_set_future(whistle_time_t *ptime, uint_32 seconds_from_now);
boolean UTIL_time_is_set(whistle_time_t *ptime);
void    UTIL_time_unset(whistle_time_t *ptime);
void    UTIL_time_with_boot(uint_64 input_time, uint_64 *output_time);
void    UTIL_time_print_mqx_time( DATE_STRUCT_PTR time_date, TIME_STRUCT_PTR time_sec_ms );
void    UTIL_time_print_rtc_time( DATE_STRUCT_PTR time_date, TIME_STRUCT_PTR time_sec_ms );
void    UTIL_time_show_rtc_time( void );
void    UTIL_time_show_mqx_time(void);
void    UTIL_time_RTC_int_action( void );
void    UTIL_time_abs_rel(uint_64 *pRelativeTime, uint_64 *pAbsoluteTime);
uint_64 UTIL_time_ms_from_struct(TIME_STRUCT_PTR timestamp);
uint_64 time_get_abs_utc_time_from_mqx(void);
uint_64 time_calc_abs_utc_time(uint_64 rel_time);
uint_64 time_calc_abs_local_time(uint_64 rel_time);
void time_set_server_time(int_64 server_time, boolean force_update);

#endif /* TIME_UTIL_H_ */
