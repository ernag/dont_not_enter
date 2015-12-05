/*
 * rtc_alarm.h
 *
 *  Created on: Aug 7, 2013
 *      Author: Ben
 */

#ifndef RTC_ALARM_H_
#define RTC_ALARM_H_

#include <mqx.h>
#include <bsp.h>
#include <timer.h>
#include "time_util.h"

typedef void (*rtc_alarm_cbk_t)(void);
typedef _mqx_uint RTC_ALARM_t;

//#define RTC_DEBUG

void        RTC_init( void );
boolean     RTC_IsInitialized( void );
void        RTC_int_action( void );
void        RTC_get_time_ms ( TIME_STRUCT *ptime );
void        RTC_get_ms( uint_64 *pMilliseconds );
void        RTC_get_time_ms_ticks( whistle_time_t *ptime_ticks );
void        RTC_time_set_future( whistle_time_t *ptime, uint_32 seconds_from_now );
void        RTC_alarm_queue_debug( void );
void        RTCA_tsk( uint_32 dummy );
#ifdef RTC_DEBUG
#define RTC_add_alarm_in_seconds( seconds_from_now, cbk ) _RTC_add_alarm_in_seconds( seconds_from_now, cbk, __FILE__, __LINE__ ) 
RTC_ALARM_t _RTC_add_alarm_in_seconds( uint_32 seconds_from_now, rtc_alarm_cbk_t cbk, char *file, int line );

#define RTC_add_alarm_at_mqx( ptimeout_time, cbk ) _RTC_add_alarm_at_mqx( ptimeout_time, cbk, __FILE__, __LINE__ ) 
RTC_ALARM_t _RTC_add_alarm_at_mqx( whistle_time_t *ptimeout_time, rtc_alarm_cbk_t cbk, char *file, int line );

#define RTC_add_alarm_at( prtc_time, cbk ) _RTC_add_alarm_at( prtc_time, cbk, __FILE__, __LINE__ ) 
RTC_ALARM_t _RTC_add_alarm_at( TIME_STRUCT * prtc_time, rtc_alarm_cbk_t cbk, char *file, int line );
#else
RTC_ALARM_t RTC_add_alarm_in_seconds( uint_32 seconds_from_now, rtc_alarm_cbk_t cbk );
RTC_ALARM_t RTC_add_alarm_at_mqx( whistle_time_t *ptimeout_time, rtc_alarm_cbk_t cbk );
RTC_ALARM_t RTC_add_alarm_at( TIME_STRUCT * prtc_time, rtc_alarm_cbk_t cbk );
#endif
void        RTC_cancel_alarm( UTIL_timer_t *id );

#endif /* RTC_ALARM_H_ */
