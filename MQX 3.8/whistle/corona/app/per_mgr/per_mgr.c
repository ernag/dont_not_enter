/*
 * per_mgr.c
 *
 *  Created on: Jul 22, 2013
 *      Author: Chris
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include "wassert.h"
#include "per_mgr.h"
#include "prx_mgr.h"
#include "pwr_mgr.h"
#include "cfg_mgr.h"
#include "time_util.h"
#include "persist_var.h"
#include "con_mgr.h"
#include "transport_mutex.h"

#define HACK_GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK
#ifdef HACK_GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK
#include "sys_event.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define PER_EVENT_TIMEOUT           		0x00000001
#ifdef HACK_GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK
#define GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK	0x00000002
#endif

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static LWEVENT_STRUCT g_per_lwevent;
static UTIL_timer_t g_permgr_timer_id = 0;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

static void _PERMGR_timeout_callback(void);

static void _PERMGR_set_time(void)
{
	whistle_time_t whistle_time;

	RTC_cancel_alarm( &g_permgr_timer_id );
        
	RTC_time_set_future(&whistle_time, g_st_cfg.per_st.poll_interval);
	PMEM_VAR_SET(PV_PROXIMITY_TIMEOUT, &whistle_time);

    g_permgr_timer_id = UTIL_time_start_oneshot_timer_at(TRUE, &whistle_time, _PERMGR_timeout_callback);
    wassert( g_permgr_timer_id );
}

static void _PERMGR_timeout_callback(void)
{
	whistle_time_t unset_time;
	
	g_permgr_timer_id = 0;
	
	UTIL_time_unset(&unset_time); // until we set it again
	PMEM_VAR_SET(PV_PROXIMITY_TIMEOUT, &unset_time);
	
    _lwevent_set(&g_per_lwevent, PER_EVENT_TIMEOUT);
}

#ifdef HACK_GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK
static void _PERMGR_chg_event(sys_event_t sys_event)
{
    _lwevent_set(&g_per_lwevent, GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK);  
}
#endif

void PERMGR_init(void)
{
    _lwevent_create(&g_per_lwevent, LWEVENT_AUTO_CLEAR);
    
#ifdef HACK_GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK
    sys_event_register_cbk(_PERMGR_chg_event, SYS_EVENT_USB_PLUGGED_IN|SYS_EVENT_USB_UNPLUGGED|SYS_EVENT_CHARGED);
#endif
}

void PERMGR_stop_timer(void)
{
	RTC_cancel_alarm( &g_permgr_timer_id );
}

void PERMGR_set_timeout(uint_32 seconds)
{
	g_st_cfg.per_st.poll_interval = seconds;
    CFGMGR_flush( CFG_FLUSH_STATIC );
    _PERMGR_set_time();
}

void PER_tsk(uint_32 dummy)
{	
	_mqx_uint mask;
	whistle_time_t whistle_time;
    whistle_time_t now;
	whistle_time_t *pWhistle_time;
	RTC_get_time_ms_ticks(&now);

	if (UTIL_time_is_set((whistle_time_t *)__SAVED_ACROSS_BOOT.proximity_timeout) &&
			UTIL_time_diff_seconds((whistle_time_t *)__SAVED_ACROSS_BOOT.proximity_timeout, &now) > 1)
	{
		pWhistle_time = (whistle_time_t *)__SAVED_ACROSS_BOOT.proximity_timeout;
	}
	else
	{
		pWhistle_time = &whistle_time;
	    RTC_time_set_future(pWhistle_time, g_st_cfg.per_st.poll_interval);
	    PMEM_VAR_SET(PV_PROXIMITY_TIMEOUT, pWhistle_time);
	}
	
	g_permgr_timer_id = UTIL_time_start_oneshot_timer_at(TRUE, pWhistle_time, _PERMGR_timeout_callback);
	
	wassert( g_permgr_timer_id );

	while(TRUE)
	{
		/*
		 * Make sure we run once in a while or declare the system hosed.
		 */
		_watchdog_start(g_st_cfg.per_st.poll_interval*1000*2);
	    
		/*
		 *   Wait until its time to log our proximity status again.
		 */
	    // Go to sleep until there is something to do
	    PWRMGR_VoteForSleep( PWRMGR_PERMGR_VOTE );
	    
		_lwevent_wait_ticks(&g_per_lwevent, PER_EVENT_TIMEOUT | GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK, FALSE, 0);
		
		if (PWRMGR_is_rebooting())
		{
			return;
		}
		
		// Stay awake while we process the event
		PWRMGR_VoteForAwake( PWRMGR_PERMGR_VOTE );
		
		mask = _lwevent_get_signalled();
				
		if (mask & PER_EVENT_TIMEOUT)
		{
			corona_print("PER Awk\n");
			
			/* See if we have enough power to proceed */
			if ( !PWRMGR_process_batt_status( false ) )
			{
				// Battery is too low and a system shutdown has been initiated
				goto DONE;
			}

			// Dunking this here because I just copy what the pros have done
			PWRMGR_encode_batt_status();

			// Do a prox scan
		    transport_mutex_blocking_lock(__FILE__, __LINE__);
		    
			if (PWRMGR_is_rebooting())
			{
				transport_mutex_unlock(__FILE__, __LINE__);
				goto DONE;
			}

			PRXMGR_scan();
			transport_mutex_unlock(__FILE__, __LINE__);

			_PERMGR_set_time();
		}	
		
#ifdef HACK_GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK
        // Kick off a manual sync.
		// This is here because we need to sacrifice a random unknown homeless dude
		// that no one will notice missing. That is, we need a thread to hold the 
		// context of registering for a CONMGR callback. CONMGR_register_cbk can
		// block, for a while, such as when BT pairing is in progress,
		// which is why we are making the homeless dude (PERMGR) thread
		// do the deed. If it ends up blocking, it's better to sacrifice a lowly
		// thread like PERMGR than an elite, well moneyed thread like PWRMGR (which
		// is probably the right place for it), or wasting stack space by creating a
		// dedicated thread just for this. The best solution might be for CONMGR_register_cbk
		// not to block, and then put this code in PWRMGR, where it probably belongs.
		if (mask & GRAFT_USB_TOE_ONTO_PERMGR_FACE_HACK)
		{
            PMEM_FLAG_SET(PV_FLAG_IS_SYNC_MANUAL, 1);
        	wassert(ERR_OK == CONMGR_register_cbk(NULL, NULL, 1, 1, CONMGR_CONNECTION_TYPE_ANY, CONMGR_CLIENT_ID_PERMGR, NULL));
		}
#endif
	}
	
DONE:
    PWRMGR_VoteExit( PWRMGR_PERMGR_VOTE );
    _task_block(); // Don't exit or our MQX resources may be deleted
}
