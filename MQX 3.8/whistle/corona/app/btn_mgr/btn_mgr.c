/*
 * btn_mgr.c
 *
 *  Created on: Jun 2, 2013
 *      Author: Chris
 */

#include "wassert.h"
#include <mqx.h>
#include <bsp.h>
#include "sys_event.h"
#include "btn_mgr.h"
#include "con_mgr.h"
#include "bt_mgr.h"
#include "pwr_mgr.h"
#include "transport_mutex.h"
#include "button.h"
#include "dis_mgr.h"
#include "btn_mgr_priv.h"
#include "persist_var.h"
#include "wifi_mgr.h"
#include "app_errors.h"
#include "pmem.h"
#include "cfg_mgr.h"

static LWEVENT_STRUCT g_btnmgr_lwevent; // button events
#define PAIR_REQUEST_EVENT                    0x0001
#define CONNECT_REQUEST_EVENT                 0x0002
#define CONNECT_SUCCEEDED_EVENT               0x0004
#define CONNECT_FAILED_EVENT                  0x0008
#define REGISTER_CONMGR_CBK_EVENT             0x0010

#define ALL_EVENTS (PAIR_REQUEST_EVENT|CONNECT_REQUEST_EVENT|CONNECT_SUCCEEDED_EVENT|CONNECT_FAILED_EVENT|REGISTER_CONMGR_CBK_EVENT)

/*
 * maximum times the device can sequentially 
 * fail sync, restart and retry.
 * Value of 5 was determined by looking on production server on 10/15/2013 and
 * seeing distribution of # reboots per session
 * See "reboot count analysis" python notebook in utils/
 */
#define SYNC_MAX_SEQ_REBOOT_RETRIES							5

typedef struct _btnmgr_conmgr_cbk_s
{
	CONMGR_connected_cbk_t connected_cb;
	CONMGR_disconnected_cbk_t disconnected_cb;
	uint_32 max_wait_time;
	uint_32 min_wait_time;
	CONMGR_connection_type_t connection_type;
} _btnmgr_conmgr_cbk_t;

extern persist_pending_action_t g_last_pending_action;

static _btnmgr_conmgr_cbk_t btnmgr_conmgr_cbk;
static int g_btnmgr_bt_handle;
static CONMGR_handle_t g_btnmgr_conmgr_handle = NULL;
static boolean g_btnmgr_btmgr_ref_count = 0;

static boolean g_btnmgr_busy = FALSE;

// Grab a reference to the BT stack if we don't already have one
static void BTNMGR_BTMGR_request(void)
{
	if (0 == g_btnmgr_btmgr_ref_count++)
	{
		BTMGR_request();
	}
}

// Release the ref only if we still have it.
static void BTNMGR_BTMGR_release(void)
{
	if (0 == --g_btnmgr_btmgr_ref_count)
	{
		BTMGR_release();
	}
}

static void BTNMGR_button_callback( sys_event_t sys_event )
{
	persist_reboot_reason_t reboot_reason;
	
	PWRMGR_VoteForON(PWRMGR_BTNMGR_VOTE);
	PWRMGR_VoteForAwake(PWRMGR_BTNMGR_VOTE);	
	
    if(sys_event == SYS_EVENT_BUTTON_HOLD)
    {
        PMEM_FLAG_SET(PV_FLAG_IS_SYNC_MANUAL, 1);
    	
		DISMGR_run_pattern(DISMGR_PATTERN_BT_PAIRING_REPEAT);

	    if (PWRMGR_is_rebooting())
	    {
	    	reboot_reason = PV_REBOOT_REASON_BUTTON_BT_PAIR_PENDING;
	    	PMEM_VAR_SET(PV_REBOOT_REASON, &reboot_reason);
	    	
    		PWRMGR_VoteForOFF(PWRMGR_BTNMGR_VOTE);
    		PWRMGR_VoteForSleep(PWRMGR_BTNMGR_VOTE);
	    	return;
	    }

		// If we can lock it, do so now. If we can't then we first have to
		// reboot to kill whatever else is going on, then come up after reboot
		// resuming the operation.
    	if (transport_mutex_non_blocking_lock(__FILE__, __LINE__) && !g_btnmgr_busy)
    	{
    		g_btnmgr_busy = TRUE;
			transport_mutex_unlock(__FILE__, __LINE__); // we only got it to test it, so release it now
            _lwevent_set(&g_btnmgr_lwevent, PAIR_REQUEST_EVENT);
    	}
    	else
    	{
    		PWRMGR_Reboot(3000, PV_REBOOT_REASON_BUTTON_BT_PAIR_PENDING, FALSE);
    		PWRMGR_VoteForOFF(PWRMGR_BTNMGR_VOTE);
    	}
    }   

    if((sys_event == SYS_EVENT_BUTTON_SINGLE_PUSH) || (sys_event == SYS_EVENT_BUTTON_LONG_PUSH))
    {
        PMEM_FLAG_SET(PV_FLAG_IS_SYNC_MANUAL, 1);

		DISMGR_run_pattern(DISMGR_PATTERN_MAN_SYNC_START_ONESHOT);

	    if (PWRMGR_is_rebooting())
	    {
	    	reboot_reason = PV_REBOOT_REASON_BUTTON_MANUAL_SYNC_PENDING;
	    	PMEM_VAR_SET(PV_REBOOT_REASON, &reboot_reason);
	    	
    		PWRMGR_VoteForOFF(PWRMGR_BTNMGR_VOTE);
    		PWRMGR_VoteForSleep(PWRMGR_BTNMGR_VOTE);
	    	return;
	    }

		// If we can lock it, do so now. If we can't then we have first have to
		// reboot to kill whatever else is going on, then come up after reboot
		// resuming the operation.
    	if (transport_mutex_non_blocking_lock(__FILE__, __LINE__) && !g_btnmgr_busy)
    	{
    		g_btnmgr_busy = TRUE;
			transport_mutex_unlock(__FILE__, __LINE__); // we only got it to test it, so release it now
            _lwevent_set(&g_btnmgr_lwevent, CONNECT_REQUEST_EVENT);
    	}
    	else
    	{
    		PWRMGR_Reboot(0, PV_REBOOT_REASON_BUTTON_MANUAL_SYNC_PENDING, FALSE);
    		PWRMGR_VoteForOFF(PWRMGR_BTNMGR_VOTE);
    	}
    }   
}

static void BTNMGR_con_mgr_connected(CONMGR_handle_t handle, boolean success)
{
	wassert(handle == g_btnmgr_conmgr_handle);
	
	if (success)
	{
		_lwevent_set(&g_btnmgr_lwevent, CONNECT_SUCCEEDED_EVENT);
	}
	else
	{
		_lwevent_set(&g_btnmgr_lwevent, CONNECT_FAILED_EVENT);
	}
	
	// We can safely say we're done with BT now AND we know CONMGR has a reference if he needs it,
	// so we can release it
	BTNMGR_BTMGR_release();
}

// We don't really do anything with the link, and we only care if it failed so that
// we can stop showing the manual syncing pattern and show a failed pattern.
static void BTNMGR_process_connect_succeeded(void)
{
	CONMGR_release(g_btnmgr_conmgr_handle);
}

// We use the failed event to notify us to display the manual sync failed pattern
static void BTNMGR_process_connect_failed(void)
{
}

// We can't register with CONMGR while holding the transport mutex or we would deadlock.
// This function is called while we are holding the transport mutex, so we need to just
// record what we want to register, and then send an event to ourselves to come pick up
// the recorded values and call CONMGR_register_cbk for real.
static void BTNMGR_conmgr_register(CONMGR_connection_type_t connection_type)
{
	btnmgr_conmgr_cbk.connected_cb = BTNMGR_con_mgr_connected;
	btnmgr_conmgr_cbk.disconnected_cb = NULL;
	btnmgr_conmgr_cbk.max_wait_time = 0;
	btnmgr_conmgr_cbk.min_wait_time = 0;
	btnmgr_conmgr_cbk.connection_type = connection_type;
	
    _lwevent_set(&g_btnmgr_lwevent, REGISTER_CONMGR_CBK_EVENT);
}

// The transport mutex must not be held when this is called.
// It should only be called as the result of an internally generated BTNMGR event to itself.
static void BTNMGR_conmgr_register_from_deferred_event(void)
{
	wassert(ERR_OK == CONMGR_register_cbk(btnmgr_conmgr_cbk.connected_cb, btnmgr_conmgr_cbk.disconnected_cb,
			btnmgr_conmgr_cbk.max_wait_time, btnmgr_conmgr_cbk.min_wait_time,
			btnmgr_conmgr_cbk.connection_type, CONMGR_CLIENT_ID_BTNMGR, &g_btnmgr_conmgr_handle));
}

static void BTNMGR_process_connect_request(boolean is_pairing)
{
	err_t err;
	int rc;
	BD_ADDR_t ANY_ADDR;
	LmMobileStatus lmMobileStatus;
	
    ASSIGN_BD_ADDR(ANY_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); // NULL means any
	
    // if we're pairing, we already have the lock. otherewise, get it now.
    if (!is_pairing)
    {
    	/* lock transports so we don't get interrupted */
    	if (!transport_mutex_non_blocking_lock(__FILE__, __LINE__))
    	{
    		corona_print("\nTRANS LOCKED\n");
    		return;
    	} 
    }
		
	// Grab a reference to the stack
    BTNMGR_BTMGR_request();
	
	/* Try to connect to BT */
	err = BTMGR_connect(ANY_ADDR, g_st_cfg.bt_st.bt_sync_page_timeout, &lmMobileStatus);
	
	BTMGR_PRIV_set_pair(FALSE);

	if (ERR_OK == err)
	{
		/* Got a BT connection */
		corona_print("BTN:stat %d\n", lmMobileStatus);
				
		/* See what the app wants to do with us */
		switch(lmMobileStatus)
		{
		case LmMobileStatus_MOBILE_STATUS_DEV_MGMT:
			/* Checkin succeeded and LMM is requested. BT is closed but still connected */

			/* check if LM has ever succeeded on this device */
		    PMEM_FLAG_SET(PV_FLAG_2ND_SYNC_PENDING, TRUE);
			
			rc = LMMGR_process_main();
			
			corona_print("BTN:LM rc %d\n", rc);

			if (0 == rc)
			{
				/* LMM completed. BT closed but still connected */
				
#ifdef NO_PHONE_SUPPORT_LM_TO_DDOBT_SESSION
				/* Disconnect BT and let CONMGR decide what type to reconnect */
				BTMGR_disconnect();
				BTNMGR_conmgr_register(CONMGR_CONNECTION_TYPE_ANY);
#else
				 /* Attempt to reuse BT with CONMGR */
				BTNMGR_conmgr_register(CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC);
#endif
				// Defer BTMGR_release() until CONMGR calls us back since we still need BT
			}
			else
			{
				/* LMM failed. BT closed and disconnected
				 * Though we could reuse BT for manual sync, because we know it just connected,
				 * let's assume instead that BT is bad and try WIFI.
				 */
				BTNMGR_BTMGR_release(); // release it here since we're going to switch to WiFi
				BTNMGR_conmgr_register(CONMGR_CONNECTION_TYPE_WIFI);
			}

			break;
		case LmMobileStatus_MOBILE_STATUS_OK:
		    /* Checkin not required. BT is closed but still connected
			 * Attempt to reuse BT with CONMGR
			 */
			/* by the way, drinking a kahlua and cream now (outside the matrix) */
		    
		    if (pmem_flag_get(PV_FLAG_PREFER_WIFI))
		    {
				BTMGR_disconnect();
		    	BTNMGR_conmgr_register(CONMGR_CONNECTION_TYPE_ANY);
				// Defer BTMGR_release() until CONMGR calls us back since we MAY still need BT (if chosen)
		    }
		    else
		    {
		    	BTNMGR_conmgr_register(CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC);		    	
				// Defer BTMGR_release() until CONMGR calls us back since we still need BT
		    }

			break;
		default:
			/* Checkin failed. BT is closed but may still be connected
			 * Though we could reuse BT for manual sync, because we know it just connected,
			 * let's assume instead that BT is bad and try WIFI.
			 */
			BTMGR_disconnect();
			BTNMGR_BTMGR_release(); // release it here since we're going to switch to WiFi
			BTNMGR_conmgr_register(CONMGR_CONNECTION_TYPE_WIFI);

			break;
		}
	}
	else
	{
		/* BT failed to connect */
		/* Use WIFI for CONMGR */
		corona_print("BTN:con fail\n");

		BTNMGR_BTMGR_release(); // release it here since we're going to switch to WiFi
		BTNMGR_conmgr_register(CONMGR_CONNECTION_TYPE_WIFI);
	}
	
	if (!is_pairing)
	{
		// Note, after transport_mutex_unlock, we open up the window for someone
		// like PRXMGR to come in before the following CONMGR registration can be
		// processed, but that should be OK.
		transport_mutex_unlock(__FILE__, __LINE__);
	}
}

static void BTNMGR_process_pair_request(void)
{
	/* lock transports so we don't get interrupted */
	if (!transport_mutex_non_blocking_lock(__FILE__, __LINE__))
	{
    	corona_print("\nTRANS LOCKED\n");
		return;
	} 

	// Grab a reference to the stack
    BTNMGR_BTMGR_request();

    BTMGR_PRIV_set_pair(TRUE);
    	
	if (ERR_OK == BTMGR_pair())
	{
        DISMGR_run_pattern(DISMGR_PATTERN_BT_PAIRING_SUCCEEDED_STOP);
        
	    BTNMGR_process_connect_request(TRUE);
	}
	else
	{
		DISMGR_run_pattern(DISMGR_PATTERN_BT_PAIRING_FAILED_STOP);
	}
	
    BTNMGR_BTMGR_release();
    
    transport_mutex_unlock(__FILE__, __LINE__);
}

int BTMGR_PRIV_get_bt_handle(void)
{
	return g_btnmgr_bt_handle;
}

void BTNMGR_init(void)
{
    _lwevent_create(&g_btnmgr_lwevent, LWEVENT_AUTO_CLEAR);
    sys_event_register_cbk(BTNMGR_button_callback, ALL_BUTTON_EVENTS);
}

void BTNMGR_tsk(uint_32 dummy)
{       
	_mqx_uint mask = 0;
	int rc;
	err_t err;
	uint16_t reboot_count;
	
	// Let me tell you a story about a little hack. We need a thread context to host
	// operations that need to be resumed after a reboot. We piggyback onto the BTNMGR thread here
	// to do that. There are two types of such operations:
	// 1. The user presses the button again to start a new operation while an operation is 
	//    still ongoing. In this case, we will have already aborted the ongoing operation,
	//    set the reboot reason accordingly, rebooted, and then landed here.
	// 2. BT/WiFi failed with a type of error where we want to retry. Again, we will have
	//    set the reboot reason, initiated a reboot, and landed here.
	// Once we get here, we fake out the BTNMGR main loop by setting things up and jumping
	// in as if we received a button press that would have initiated the equivalent operation.
	switch(g_last_pending_action)
	{
		case PV_PENDING_SYNC_RETRY:
			/* Handle sync crash recovery */
			/* If we crashed while syncing last time, restart where we left off */
			PWRMGR_VoteForON(PWRMGR_BTNMGR_VOTE);
			PWRMGR_VoteForAwake(PWRMGR_BTNMGR_VOTE);	
	
			if (__SAVED_ACROSS_BOOT.sync_fail_reboot_count < SYNC_MAX_SEQ_REBOOT_RETRIES)
			{
				reboot_count = __SAVED_ACROSS_BOOT.sync_fail_reboot_count + 1;
				corona_print("Rtry Sync\n");
				_lwevent_set(&g_btnmgr_lwevent, CONNECT_REQUEST_EVENT);
			}
			else
			{
				reboot_count = 0;
				corona_print("Max Seq Rtry\n");
				PROCESS_NINFO(ERR_BUTTON_MAX_SEQ_REBOOT_RETRIES, NULL);
			}
			PMEM_VAR_SET(PV_SYNC_FAIL_REBOOT_COUNT, &reboot_count);	
			break;
							
		case PV_PENDING_MANUAL_SYNC:
			PWRMGR_VoteForON(PWRMGR_BTNMGR_VOTE);
			PWRMGR_VoteForAwake(PWRMGR_BTNMGR_VOTE);	
	
			corona_print("pend conn\n");
			
			_lwevent_set(&g_btnmgr_lwevent, CONNECT_REQUEST_EVENT);
			break;
			
		case PV_PENDING_BT_PAIR:
			PWRMGR_VoteForON(PWRMGR_BTNMGR_VOTE);
			PWRMGR_VoteForAwake(PWRMGR_BTNMGR_VOTE);	
	
			corona_print("pend pair\n");
			
			DISMGR_run_pattern(DISMGR_PATTERN_BT_PAIRING_REPEAT);
			_lwevent_set(&g_btnmgr_lwevent, PAIR_REQUEST_EVENT);
			break;
			
		default:
			break;
    }
	if (PV_PENDING_SYNC_RETRY != g_last_pending_action && 0 != __SAVED_ACROSS_BOOT.sync_fail_reboot_count)
	{
		reboot_count = 0;
		PMEM_VAR_SET(PV_SYNC_FAIL_REBOOT_COUNT, &reboot_count);	
	}
    	
	while (1)
	{
		_lwevent_wait_ticks(&g_btnmgr_lwevent, ALL_EVENTS, FALSE, 0);
		
        if (PWRMGR_is_rebooting() || !PWRMGR_process_batt_status( false ))
        {
            // Rebooting or the battery is too low -- don't allow button actions to be initiated. System will shut down.
    		goto DONE;
        }
		
		mask = _lwevent_get_signalled();
				
	    PWRMGR_VoteForRUNM( PWRMGR_BTNMGR_VOTE );
		
		if (mask & PAIR_REQUEST_EVENT)
		{
			BTNMGR_process_pair_request();
		}

		if (mask & CONNECT_REQUEST_EVENT)
		{
		    EVTMGR_request_flush();
		    _time_delay(300);
			BTNMGR_process_connect_request(FALSE);
		}

		if (mask & CONNECT_SUCCEEDED_EVENT)
		{
			BTNMGR_process_connect_succeeded();
		}

		if (mask & CONNECT_FAILED_EVENT)
		{
			BTNMGR_process_connect_failed();
		}

		if (mask & REGISTER_CONMGR_CBK_EVENT)
		{
			BTNMGR_conmgr_register_from_deferred_event();
		}

		g_btnmgr_busy = FALSE;

		PWRMGR_VoteForVLPR( PWRMGR_BTNMGR_VOTE );
		PWRMGR_VoteForOFF(PWRMGR_BTNMGR_VOTE);
		PWRMGR_VoteForSleep(PWRMGR_BTNMGR_VOTE);	
	}
	
DONE:
    PWRMGR_VoteExit(PWRMGR_BTNMGR_VOTE);
    _task_block(); // Don't exit or our MQX resources may be deleted
}
