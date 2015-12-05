/*
 * rtc_alarm.c
 *
 *  Created on: Aug 7, 2013
 *      Author: Ben
 *      
 *  RTC interrupt handler and alarm queue implementation
 *
 */

#include <mqx.h>
#include "wassert.h"
#include "pmem.h"
#include "cfg_mgr.h"
#include "rtc_alarm.h"
#include "time_util.h"
#include "pwr_mgr.h"
#include "corona_console.h"
#include "cormem.h"

#define MAX_RTC_TIMER_QUEUE_SIZE	10

#define RTC_INT_FIRED				0x1

typedef struct rtc_timer_struct
{
    QUEUE_ELEMENT_STRUCT HEADER;        // standard queue element header.
    _timer_id            id;
    TIME_STRUCT          timeout; 
    rtc_alarm_cbk_t      cbk;
} rtc_timer_qu_elem_t, *rtc_timer_qu_elem_ptr_t;

QUEUE_STRUCT            rtc_timer_queue;

static void rtc_alarm_set( TIME_STRUCT *ptimeout_time );
static boolean rtc_alarm_pop_queue( void );
static void rtc_alarm_setup_next_alarm( void );
static boolean rtc_time_is_sooner_than( TIME_STRUCT *pthis_time, TIME_STRUCT *pthat_time );
static void rtc_alarm_queue_debug( void );

static RTC_ALARM_t g_next_alarm_id = 0;
static LWEVENT_STRUCT alarm_lwevent;

/*
 * Starting date and time used to initialize the RTC if it's not set
 *  
 *   NOTE:  If you change this, let Nate know, as it changes the server's
 *          expectations for relative milliseconds from boot.
 */
static const DATE_STRUCT rtc_init_date = 
{
    2013, // Year
    7,    // Month
    19,   // Day
    1,    // Hour
    20,   // Minute
    0,    // Seconds
    0     // Milliseconds
};

// Callbacks and Interrupt Handlers

/*
 * RTC interrupt action. This function is called by the LLWU interrupt handler
 * upon wakeup from LLS or by the RTC interrupt handler if already awake.
 * 
 * 	 WARNING:  This ISR can/will run even if wint_disable() is active.
 */
void RTC_int_action( void )
{
	PWRMGR_VoteForAwake( PWRMGR_RTC_ALARM_VOTE );
	_lwevent_set(&alarm_lwevent, RTC_INT_FIRED);
}

/*
 *   Offload task for processing RTC alarms, so they can get off the RTC ISR's context.
 */
void RTCA_tsk( uint_32 dummy )
{
	_lwevent_create(&alarm_lwevent, LWEVENT_AUTO_CLEAR);

    _queue_init( &rtc_timer_queue, MAX_RTC_TIMER_QUEUE_SIZE );
	
	while(1)
	{
		uint_32 now_secs;
	    rtc_timer_qu_elem_ptr_t p_head;
	    
	    PWRMGR_VoteForSleep( PWRMGR_RTC_ALARM_VOTE );
	    
		_lwevent_wait_ticks(&alarm_lwevent, RTC_INT_FIRED, FALSE, 0);
		
	    wint_disable();
	    
	    p_head = (rtc_timer_qu_elem_ptr_t) _queue_head( &rtc_timer_queue );
	    now_secs = p_head->timeout.SECONDS;
	    
	    do
	    {
	        // Fire the callback at the head of the queue
	        ((rtc_alarm_cbk_t)p_head->cbk)();
	    
	        // Pop an entry off the queue, if it isn't empty.
	        // If the next entry is in the current second, fire it now.
	        // Otherwise, set the RTC alarm to go off again in the future.
	        if ( rtc_alarm_pop_queue() )
	        {
	            // Not empty: check to see if the new head is in this second, or the future
	            p_head = (rtc_timer_qu_elem_ptr_t) _queue_head( &rtc_timer_queue );
	        }
	        else
	        {
	            break;
	        }
	    } while (now_secs == p_head->timeout.SECONDS);

	    // Setup the next RTC alarm
	    rtc_alarm_setup_next_alarm();
	    
	    // NOTE: this event is sent on for debug, to indicate RTC alarm action was called.
	    PWRMGR_rtc_alarm_event();
	    
	    wint_enable();
	}
}

/*
 * RTC Interrupt Handler (user function)
 * This handler processes the RTC Alarm interrupt and is called by the actual 
 * RTC interrupt handler defined by the BSP.
 */
static void RTC_userint(pointer rtc_registers_ptr)
{
    uint_32 state;
    
    wint_disable(); // don't allow higher priority interrupts
    
    state = _rtc_get_status();
        
    // Process the RTC Alarm interrupt. This is the only RTC interrupt that corona uses.
    if( state & RTC_RTCISR_ALM)
    {
    	RTC_int_action();
    }
    
    _rtc_clear_requests (state);
    wint_enable();
}

// Private Functions

/*
 * Set the RTC alarm to go off when the next timer in the queue is set to expire
 */
static void rtc_alarm_set( TIME_STRUCT *ptimeout_time )
{
    DATE_STRUCT     date_rtc;
    
    // Convert the time to a date time compatbile with the RTC library
    _time_to_date( ptimeout_time, &date_rtc );
    
    // Set the RTC alarm
    _rtc_set_alarm_mqxd( &date_rtc );
}

/*
 * Set up the next RTC alarm using the element at the head of the queue.
 * If the queue is empty, then the RTC alarm interrupt is disabled.
 * NOTE: interrupts should be disabled when using this call.
 */
static void rtc_alarm_setup_next_alarm( void )
{
    rtc_timer_qu_elem_ptr_t p_elem; 
    
    wint_disable();
    
    // If the queue is still not empty, set up the next alarm now
    if (!_queue_is_empty( &rtc_timer_queue) )
    {
        p_elem = (rtc_timer_qu_elem_ptr_t) _queue_head( &rtc_timer_queue );
        rtc_alarm_set( &p_elem->timeout );
        // Enable the RTC alarm interrupt
        _rtc_int_enable( 1, RTC_ISR_ALARM );
    }
    else
    {
        // No more alarms: disable the RTC alarm interrupt
        _rtc_int_enable( 0, RTC_ISR_ALARM );
    }
    
    wint_enable();
}

/*
 * Remove an expired timer from the RTC alarm queue
 */
static boolean rtc_alarm_pop_queue( void )
{
	rtc_timer_qu_elem_ptr_t p_elem = NULL;
	boolean ret = FALSE;
	
	wint_disable();

	if (!_queue_is_empty( &rtc_timer_queue ) )
	{
		// Test queue integrity
		wassert( MQX_OK == _queue_test( &rtc_timer_queue, (pointer) p_elem ) );
		
		// Remove the old timer from the queue
		p_elem = (rtc_timer_qu_elem_ptr_t) _queue_dequeue( &rtc_timer_queue );
		
		// Free the memory that was allocated for the queue element
		wfree(p_elem);
		
		ret = TRUE;
	}
	
	wint_enable();
	
	return ret;
}

/*
 * Compare 'this_time' and 'that_time' with a resolution of 1 OS tick.
 * Returns true if 'this_time' is sooner than or equal to 'that_time'.
 */
static boolean rtc_time_is_sooner_than( TIME_STRUCT *pthis_time, TIME_STRUCT *pthat_time ) 
{
    if ( pthis_time->SECONDS == pthat_time->SECONDS)
    {
        return (pthis_time->MILLISECONDS <= pthat_time->MILLISECONDS);
    }
    return (pthis_time->SECONDS < pthat_time->SECONDS);
}

// Public Functions


/*
 * Get the RTC time, including milliseconds and convert to whistle time
 */
void RTC_get_time_ms_ticks( whistle_time_t *ptime_ticks )
{
    TIME_STRUCT rtc_time;
    RTC_get_time_ms( &rtc_time );
    _time_to_ticks( &rtc_time, ptime_ticks );
}

/*
 * Get the RTC time, including milliseconds and return in TIME_STRUCT format
 */
void RTC_get_time_ms( TIME_STRUCT *ptime )
{
    RTC_MemMapPtr rtc = RTC_BASE_PTR;
    uint32_t tmpval;

    // Capture seconds (TSR) and prescalar register (TPR) used to calculate milliseconds
    ptime->SECONDS = rtc->TSR;
    tmpval = rtc->TPR;
    
    // If milliseconds just rolled over, read the seconds register again
    if (0==tmpval)
        ptime->SECONDS = rtc->TSR;
    ptime->MILLISECONDS = (tmpval*1000)/32768; 
   
    // Milliseconds must be < 1000
    wassert(1000>ptime->MILLISECONDS);
}

/*
 *   Get the RTC milliseconds now in a 64-bit kinda way.
 */
void RTC_get_ms(uint_64 *pMilliseconds)
{
    TIME_STRUCT time;
    
    RTC_get_time_ms(&time);
    
    *pMilliseconds = UTIL_time_ms_from_struct(&time);
}

/* 
 * Set a time that is seconds in the future from the RTC clock time
 */
void RTC_time_set_future( whistle_time_t *ptime, uint_32 seconds_from_now )
{
    TIME_STRUCT    rtc_time;
    whistle_time_t time;

    // MAX_MQX_UINT means time not valid
    if (MAX_MQX_UINT == seconds_from_now)
    {
        UTIL_time_unset(ptime);
        return;
    }
    
    // Get the RTC time with milliseconds (in ticks)
    RTC_get_time_ms( &rtc_time );
    
    // Add seconds_from_now
    rtc_time.SECONDS += seconds_from_now;
    
    // Convert to ticks and store at the returned future time value
    _time_to_ticks( &rtc_time, ptime );
}

/*
 * Add a timer to the RTC timer queue. The queue is sorted according to timeout,
 * with timers that will expire soonest at the head of the queue.
 * When the RTC alarm is triggered, the RTC interrupt handler will pop the corresponding
 * timer off the queue and reset the RTC alarm to the next timer.
 * If the RTC timer queue is empty when this function is called, it sets the first RTC alarm.
 */
static RTC_ALARM_t __RTC_add_alarm_at( TIME_STRUCT * prtc_time, rtc_alarm_cbk_t cbk )
{
	uint_8 sz;
	rtc_timer_qu_elem_ptr_t p_cur_elem, p_prev_elem;
	rtc_timer_qu_elem_ptr_t p_new_elem = NULL;
	TIME_STRUCT rtc_now;

    // Disable interrupts before touching the queue (RTC interrupt also touches it)
	// Also disable interrupts so we can do all this without interruption so as little
	// elapased time as possible expires while we're setting this up.
	wint_disable();
	    
	// Ensure that timer being added is in the future
	// Illegal to set a timer in the past.
    RTC_get_time_ms( &rtc_now );
    
	if ( rtc_time_is_sooner_than( prtc_time, &rtc_now) )
	{
		wint_enable();
	    return TIMER_NULL_ID;
	}
	
	// Since we only have 1 second resolution, min timeout is 2 sec.
    // After including the time it takes to set the timeout, the timeout must
    // be at least one second into the future.
	// It's not considered an error to ask for a timer that is in the future
	// by the less than one second. Some timers are computed programatically,
	// and the calculation could result in something less than 1 sec, so
	// be nice and allow it, but correct it.
    rtc_now.SECONDS++;
    
	if ( rtc_time_is_sooner_than( prtc_time, &rtc_now) )
	{
		prtc_time->SECONDS++;
	}

    /*
     *   Alloc memory for queue element and memcpy'd data.
     *     We will free it after the RTC timer is expired
     */
    p_new_elem = walloc( sizeof(rtc_timer_qu_elem_t) );

	// Check to ensure RTC queue isn't full, which is a fatal error
	sz = _queue_get_size( &rtc_timer_queue );
	wassert( sz < MAX_RTC_TIMER_QUEUE_SIZE );
	
	// Initialize the new element 
	p_new_elem->id = ++g_next_alarm_id;
	p_new_elem->timeout.SECONDS = prtc_time->SECONDS;
	p_new_elem->timeout.MILLISECONDS = prtc_time->MILLISECONDS;
	p_new_elem->cbk = cbk;

	if ( sz == 0 )
	{
		// Queue is empty: just enqueue the new entry 
		wassert( _queue_enqueue( &rtc_timer_queue, (QUEUE_ELEMENT_STRUCT_PTR) p_new_elem ) );
	}
	else
	{	
		// Sort the queue by increasing timeout
		p_prev_elem = (pointer) NULL;
		p_cur_elem =  (rtc_timer_qu_elem_ptr_t) _queue_head( &rtc_timer_queue );
		sz++; // use predecrement
		while(--sz)
		{
		    // Find the place in the queue to insert the new alarm timer
			if ( rtc_time_is_sooner_than( &p_new_elem->timeout, &p_cur_elem->timeout) )
			{
				// Found location to insert the new timer
				break;
			}
			p_prev_elem = p_cur_elem;
			p_cur_elem = (rtc_timer_qu_elem_ptr_t) _queue_next( &rtc_timer_queue, (QUEUE_ELEMENT_STRUCT_PTR) p_cur_elem );
		};
		// Add the new timer to the queue in order of increasing timeout
        wassert( _queue_insert( &rtc_timer_queue, (QUEUE_ELEMENT_STRUCT_PTR) p_prev_elem, (QUEUE_ELEMENT_STRUCT_PTR) p_new_elem ) );
	}
	// If the head is new, then reset the RTC alarm at the newest head
    if ( p_new_elem == (rtc_timer_qu_elem_ptr_t) _queue_head( &rtc_timer_queue ) )
    {   
        // Assign the RTC alarm now, since this is the first entry
        rtc_alarm_set( &p_new_elem->timeout );
	}
	
    // Enable the RTC alarm interrupt
	// NOTE: this shouldn't be necessary, but it seems that another thread (perhaps BT init) is killing the
	// RTC alarm interrupt
    _rtc_int_enable( 1, RTC_ISR_ALARM );
	wint_enable();

	return p_new_elem->id;
}

/*
 * Add an RTC alarm timer to go off seconds in the future
 */
static RTC_ALARM_t __RTC_add_alarm_in_seconds( uint_32 seconds_from_now, rtc_alarm_cbk_t cbk )
{
   // whistle_time_t ticks_timeout;
    TIME_STRUCT rtc_time;
    RTC_ALARM_t alarm;
    
    wint_disable();
    
    // Get the RTC time with milliseconds (in ticks)
    RTC_get_time_ms( &rtc_time );
    
    // Add seconds_from_now
    rtc_time.SECONDS += seconds_from_now;
    
    alarm = __RTC_add_alarm_at( &rtc_time, cbk );
    
    wint_enable();
    
    return alarm;
}

/*
 * Add a timer to the RTC timer queue. Timeout is in MQX ticks (old whistle_time_t)
 */
static RTC_ALARM_t __RTC_add_alarm_at_mqx( whistle_time_t *ptimeout_time, rtc_alarm_cbk_t cbk )
{
    TIME_STRUCT rtc_time;
    RTC_ALARM_t alarm;
    
    wint_disable();
    
    _ticks_to_time( ptimeout_time, &rtc_time );
    
    alarm = __RTC_add_alarm_at( &rtc_time, cbk );
    
    wint_enable();
    
    return alarm;
}

#ifdef RTC_DEBUG
RTC_ALARM_t _RTC_add_alarm_in_seconds( uint_32 seconds_from_now, rtc_alarm_cbk_t cbk, char *file, int line )
{
	corona_print("RTC_add_alarm_in_seconds %s: %d\n", file, line);
	return __RTC_add_alarm_in_seconds(seconds_from_now, cbk);
}

RTC_ALARM_t _RTC_add_alarm_at_mqx( whistle_time_t *ptimeout_time, rtc_alarm_cbk_t cbk, char *file, int line )
{
	corona_print("RTC_add_alarm_at_mqx %s: %d\n", file, line);
	return __RTC_add_alarm_at_mqx( ptimeout_time, cbk );
}

RTC_ALARM_t _RTC_add_alarm_at( TIME_STRUCT * prtc_time, rtc_alarm_cbk_t cbk, char *file, int line )
{
	corona_print("RTC_add_alarm_at %s: %d\n", file, line);
	return __RTC_add_alarm_at( prtc_time, cbk );
}
#else
RTC_ALARM_t RTC_add_alarm_in_seconds( uint_32 seconds_from_now, rtc_alarm_cbk_t cbk )
{
	return __RTC_add_alarm_in_seconds(seconds_from_now, cbk);
}

RTC_ALARM_t RTC_add_alarm_at_mqx( whistle_time_t *ptimeout_time, rtc_alarm_cbk_t cbk )
{
	return __RTC_add_alarm_at_mqx( ptimeout_time, cbk );
}

RTC_ALARM_t RTC_add_alarm_at( TIME_STRUCT * prtc_time, rtc_alarm_cbk_t cbk )
{
	return __RTC_add_alarm_at( prtc_time, cbk );
}
#endif

/*
 * Cancel an RTC alarm and remove it from the rtc timer queue.
 * The RTC interrupt is disabled if the last timer was removed from the queue.
 * Zeros the id when done.
 */
void RTC_cancel_alarm( UTIL_timer_t *id )
{
	uint_8 idx;
	uint_8 sz;
	rtc_timer_qu_elem_ptr_t p_cur_elem;

	wint_disable();
	
	// legal to call this function with zero id so everyone doesn't have to check
	if (0 == *id)
	{
		wint_enable();
		return;
	}
	
	sz = _queue_get_size( &rtc_timer_queue );
	if (!sz)
	{
		// Queue is empty, so nothing to do
		wint_enable();
		return;
	}
	p_cur_elem = (rtc_timer_qu_elem_ptr_t) _queue_head( &rtc_timer_queue );
	wassert( p_cur_elem );
	for (idx=0;idx < sz; idx++)
	{
		if ( p_cur_elem->id == *id )
		{	
			// Remove the element from the queue
			_queue_unlink( &rtc_timer_queue, (QUEUE_ELEMENT_STRUCT_PTR) p_cur_elem );

            // Free the memory that was allocated for the queue element
            wfree(p_cur_elem);
			break;
		}
		p_cur_elem = (rtc_timer_qu_elem_ptr_t) _queue_next( &rtc_timer_queue, (QUEUE_ELEMENT_STRUCT_PTR) p_cur_elem );
	}
	// If this was the head then reset the RTC alarm to the next element
	if ( 0 == idx )
	{
		rtc_alarm_setup_next_alarm();
	}
	
	*id = 0;
	
	wint_enable();
}

/*
 * Read the RTC time and compare it to the RTC init time
 * Return value:
 *   TRUE = RTC is initialized (now time is later than init time)
 *   FALSE = RTC is not initialized (now time is equal to or earlier than init time)
 */
boolean RTC_IsInitialized( void )
{
    TIME_STRUCT rtc_now;
    TIME_STRUCT init_time;
    
    _time_from_date((DATE_STRUCT_PTR) &rtc_init_date, &init_time);
    
    // Ensure that RTC time is later than the init time
    RTC_get_time_ms( &rtc_now );
    return rtc_time_is_sooner_than( &init_time, &rtc_now);
}

/*
 * Initialize the RTC, set the initial time, sync MQX time
 */
void RTC_init( void )
{
    TIME_STRUCT time_mqx;  
    
    if ( !RTC_IsInitialized() )
    {
        // Convert the date format to mqx time format
        _time_from_date ( (DATE_STRUCT_PTR) &rtc_init_date, &time_mqx);
        
        // Set MQX absolute time using the RTC init date
        _time_set( &time_mqx);
        
        // Sync RTC to the MQX absolute time we just set
        wassert( _rtc_sync_with_mqx(FALSE) == MQX_OK );
    }
    else
    {
        // Sync MQX time to RTC time
        wassert( _rtc_sync_with_mqx(TRUE) == MQX_OK );
    }  
    
    // Install the RTC interrupt 
    wassert( MQX_OK == _rtc_int_install ((pointer) RTC_userint) );
    _rtc_clear_requests (RTC_RTCISR_SW | RTC_RTCISR_ALM);
    
    // Disable the RTC stopwatch interrupt
    _rtc_int_enable(FALSE, RTC_RTCIENR_SW); 
   
    // Enable the RTC alarm interrupt
    // NOTE: BSP enables RTC_ISR_TOF and RTC_ISR_TIF RTC error interrupts
    wassert ( RTC_RTCIENR_ALM == _rtc_int_enable (TRUE, RTC_RTCIENR_ALM) );
}


/*
 * Display RTC alarm debug info
 */
void RTC_alarm_queue_debug( void )
{
    uint32_t i;
    int sz;
    rtc_timer_qu_elem_ptr_t p_elem;
    TIME_STRUCT rtc_now;
    
#define RTC_DEBUG_STR_SZ 1024
    static char rtc_debug_str[RTC_DEBUG_STR_SZ];
    char *pstr = rtc_debug_str;
    
    // Verbose RTC debug output: show all timers
    RTC_get_time_ms( &rtc_now );
    corona_print("now:\t\t%d:%d\n", rtc_now.SECONDS, rtc_now.MILLISECONDS);
    
    memset(pstr, 0, RTC_DEBUG_STR_SZ);

    wint_disable();
    
    sz =_queue_get_size( &rtc_timer_queue );
    p_elem = (rtc_timer_qu_elem_ptr_t) _queue_head( &rtc_timer_queue );
    
    for (i=0;i<sz;i++)
    {
    	// No corona_prints inside the int_disable. Save them for printing outside the int_disable.
        pstr += snprintf(pstr, rtc_debug_str + RTC_DEBUG_STR_SZ - pstr, "# %d: id=%d (%p) %d:%d\n", i, p_elem->id, p_elem->cbk, p_elem->timeout.SECONDS, p_elem->timeout.MILLISECONDS ) + 1;
        wassert(pstr < rtc_debug_str + RTC_DEBUG_STR_SZ );
        p_elem = (rtc_timer_qu_elem_ptr_t) _queue_next( &rtc_timer_queue, (QUEUE_ELEMENT_STRUCT_PTR) p_elem );
    }
    
    wint_enable();
    
    pstr = rtc_debug_str;

    for (i=0;i<sz;i++)
    {
        corona_print(pstr);
        pstr += strlen(pstr) + 1;
    }
}


