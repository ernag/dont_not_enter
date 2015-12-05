/*
 * con_mgr.c
 *
 *  Created on: Mar 26, 2013
 *      Author: Chris
 */

#include "wassert.h"
#include "tpt_mgr.h"
#include "wifi_mgr.h"
#include "bt_mgr.h"
#include "bt_mgr_wmps.h"
#include "con_mgr.h"
#include "cfg_mgr.h"
#include "acc_mgr.h"
#include "pwr_mgr.h"
#include "app_errors.h"
#include "time_util.h"
#include "evt_mgr.h"
#include "sys_event.h"
#include "main.h"
#include "pmem.h"
#include "wmutex.h"
#include <watchdog.h>

#define MAX_CLIENTS (CONMGR_CLIENT_ID_LAST - CONMGR_CLIENT_ID_FIRST + 1)
#define MAX_CON_TYPES (CONMGR_CONNECTION_TYPE_LAST - CONMGR_CONNECTION_TYPE_FIRST + 1)

#define OPENED_EVENT                   0x0001
#define CLOSED_EVENT                   0x0002
#define BT_DISCONNECTED_EVENT          0x0004
#define WIFI_DISCONNECTED_EVENT        0x0008
#define WIFI_CALLBACK_TIMEOUT_EVENT    0x0010
#define BTC_CALLBACK_TIMEOUT_EVENT     0x0020
#define BTLE_CALLBACK_TIMEOUT_EVENT    0x0040
#define ANY_CALLBACK_TIMEOUT_EVENT     0x0080
#define SHUTDOWN_EVENT                 0x0100
#define DISCONNECT_TIMEOUT_EVENT       0x0200

#define SHUTDOWN_ACCMGR_ON_WIFI        0

typedef enum CONMGR_client_state_enum
{
	CONMGR_CLIENT_STATE_NOT_REGISTERED,
	CONMGR_CLIENT_STATE_NOT_CONNECTED,
	CONMGR_CLIENT_STATE_CONNECTED
} CONMGR_client_state_enum_t;

//#pragma pack(push, 1)
struct CONMGR_client_s
{
        CONMGR_connected_cbk_t connected_cb; // function to call when connection is up
        CONMGR_disconnected_cbk_t disconnected_cb; //  function to call when connection is down
        whistle_time_t max_wait_time; // time from now of maximum time to wait for connection
        whistle_time_t min_wait_time; // time from now of minimum time to wait for connection
        CONMGR_connection_type_t requested_connection_type;
        int transport_handle; // WIFI socket (TODO: or BT context)
        CONMGR_client_state_enum_t state;
        int_32 magic;
};
//#pragma pack(pop)

//#pragma pack(push, 1)
typedef struct CONMGR_connection_struct
{  
        struct CONMGR_client_s client_info[MAX_CLIENTS];
        whistle_time_t next_timeout_time;
        UTIL_timer_t callback_timer;
        void (*callback_timer_callback)(void);
} CONMGR_connection_s;
//#pragma pack(pop)

static _mqx_uint CONMGR_next_timeout_time_in_seconds(CONMGR_connection_type_t connection_type);
static void CONMGR_post_disconnect(CONMGR_connection_type_t current_requested_connection_type);
static void CONMGR_process_disconnected(_mqx_uint mask);
static err_t CONMGR_BTC_open(int *pHandle);
static err_t CONMGR_BTLE_open(int *pHandle);
static err_t CONMGR_WIFI_open(int *pSocket, char *server, uint_32 port);
static void CONMGR_disconnect(CONMGR_connection_type_t requested_connection_type);
static void CONMGR_process_callbacks(CONMGR_connection_type_t current_requsted_connection_type);
static void CONMGR_WIFI_callback_timeout(void);
static void CONMGR_BTC_callback_timeout(void);
static void CONMGR_BTLE_callback_timeout(void);
static void CONMGR_ANY_callback_timeout(void);
static void CONMGR_timeout_update(struct CONMGR_client_s *cb);
static int_32 CONMGR_update_timer(CONMGR_connection_type_t requested_connection_type);
static void CONMGR_WIFI_close(int handle);
static void CONMGR_BTC_close(int handle);
static void CONMGR_BTLE_close(int handle);
static err_t CONMGR_WIFI_send(int transport_handle, uint_8 *data, uint_32 length, int_32 *pbytes_sent);
static err_t CONMGR_WIFI_receive(int transport_handle, void *buf, uint_32 buf_len, uint_32 ms_timeout, int_32 *pbytes_received, boolean continuous_aggregate);
static void CONMGR_WIFI_zero_copy_free(void *buf);
static void CONMGR_BTC_zero_copy_free(void *buf);
static err_t CONMGR_BTC_send(int transport_handle, uint_8 *data, uint_32 length, int_32 *pbytes_sent);
static err_t CONMGR_BTC_receive(int transport_handle, void *buf, uint_32 buf_len, uint_32 ms_timeout, int_32 *pbytes_received);

//#pragma pack(push, 1)
static CONMGR_connection_s g_connection[] =
{
        {
                {
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                }, {0}, 0, CONMGR_WIFI_callback_timeout
        },
        {
                {
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                }, {0}, 0, CONMGR_BTC_callback_timeout
        },
        {
                {
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                }, {0}, 0, CONMGR_BTLE_callback_timeout
        },
        {
                {
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                        {NULL, NULL, {0}, {0}, CONMGR_CONNECTION_TYPE_UNKNOWN, -1, CONMGR_CLIENT_STATE_NOT_REGISTERED, CON_MAGIC_VALUE},
                }, {0}, 0, CONMGR_ANY_callback_timeout
        },
};
//#pragma pack(pop)

// Keep track of the type of connection that was requested, which includes ANY.
static CONMGR_connection_type_t g_current_requested_connection_type = CONMGR_CONNECTION_TYPE_UNKNOWN;

// Keep track of the type of connection that was actually created.
// For the ANY case, this will be resolved into either WIFI, BTC, or BTLE
static CONMGR_connection_type_t g_current_actual_connection_type = CONMGR_CONNECTION_TYPE_UNKNOWN;

// Keep track of client that called CONMGR_release so we don't fail-out a new request from
// him while we're disconnecting due to his CONMGR_release call.
static struct CONMGR_client_s *g_conmgr_releasing_cb = NULL;

static LWEVENT_STRUCT g_conmgr_lwevent;
static MUTEX_STRUCT g_conmgr_mutex;

static boolean g_conmgr_transport_mutex_locked = FALSE;
static boolean g_conmgr_mutex_locked = FALSE;

static UTIL_timer_t g_conmgr_disconnect_timer_id = 0;

static _mqx_uint CONMGR_next_timeout_time_in_seconds(CONMGR_connection_type_t connection_type)
{
    CONMGR_client_id_t client_id;
//    CONMGR_client_id_t used_client_id;
    struct CONMGR_client_s *cb;
    _mqx_uint shortest_maximum_seconds = MAX_MQX_UINT;
    _mqx_uint time_diff; 
    whistle_time_t now;
    
    RTC_get_time_ms_ticks( &now );

    for (client_id = CONMGR_CLIENT_ID_FIRST; client_id < MAX_CLIENTS; client_id++)
    {
        cb = &(g_connection[connection_type].client_info[client_id]);
        
        if (NULL != cb->connected_cb)
        {           
            time_diff = UTIL_time_diff_seconds(&g_connection[cb->requested_connection_type].next_timeout_time, &now);
            
            if (time_diff < shortest_maximum_seconds)
            {
                shortest_maximum_seconds = time_diff;
            }
        }
    }
//    corona_print("next_timeout: client %d\n", used_client_id);
    return shortest_maximum_seconds;
}

static void CONMGR_transport_lock(char *file, int line)
{
	transport_mutex_blocking_lock(file, line);  
	g_conmgr_transport_mutex_locked = TRUE;
}

static void CONMGR_transport_unlock(char *file, int line)
{
    if (g_conmgr_transport_mutex_locked)
    {
    	g_conmgr_transport_mutex_locked = FALSE;
    	transport_mutex_unlock(file, line);
    }
}

static void CONMGR_post_disconnect(CONMGR_connection_type_t current_requested_connection_type)
{
    CONMGR_connection_type_t pending_connection_type;
    _mqx_uint next_timeout_seconds = CONMGR_next_timeout_time_in_seconds(current_requested_connection_type);
       
    // Remove the old timer that brought the last connection up
    RTC_cancel_alarm(&g_connection[current_requested_connection_type].callback_timer);
    
    // Possibly start a new callback timer for the completed connection type
    RTC_time_set_future(&g_connection[current_requested_connection_type].next_timeout_time, next_timeout_seconds);

    // Restart timer with next shortest if there are any still registered
    if (UTIL_time_is_set(&g_connection[current_requested_connection_type].next_timeout_time))
    {
        whistle_time_t now;

        RTC_get_time_ms_ticks( &now );
        
        corona_print("CNMGR: next to %d\n", UTIL_time_diff_seconds(&g_connection[current_requested_connection_type].next_timeout_time, &now));

    	g_connection[current_requested_connection_type].callback_timer =
    			RTC_add_alarm_at_mqx( &g_connection[current_requested_connection_type].next_timeout_time,
    					g_connection[current_requested_connection_type].callback_timer_callback);
    	
    	wassert(g_connection[current_requested_connection_type].callback_timer);
    }
    
    if ((CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC == g_current_actual_connection_type) ||
    		(CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE == g_current_actual_connection_type))
    {
    	// Do this here instead of TPTMGR so get off sys_event thread context
    	BTMGR_release();
    }

    g_current_requested_connection_type = CONMGR_CONNECTION_TYPE_UNKNOWN;
    g_current_actual_connection_type = CONMGR_CONNECTION_TYPE_UNKNOWN;
    
    CONMGR_transport_unlock(__FILE__, __LINE__);

#if SHUTDOWN_ACCMGR_ON_WIFI
    ACCMGR_start_up();
    corona_print("CON: ACC started\n");
#endif
 

    // check all connection types to see if any are ready
    for (pending_connection_type = CONMGR_CONNECTION_TYPE_FIRST; pending_connection_type < MAX_CON_TYPES; ++pending_connection_type)
    {
    	next_timeout_seconds = CONMGR_next_timeout_time_in_seconds(pending_connection_type);
    	
        if (MAX_MQX_UINT != next_timeout_seconds && 0 >= next_timeout_seconds)
        {
            CONMGR_process_callbacks(pending_connection_type);
        }
    } 
}

static void CONMGR_reset_cb(struct CONMGR_client_s *cb)
{
    cb->disconnected_cb = NULL;
    cb->connected_cb = NULL;
    cb->transport_handle = -1;
    cb->state = CONMGR_CLIENT_STATE_NOT_REGISTERED;
}

static void CONMGR_dispose_and_unregister(struct CONMGR_client_s *cb)
{
    whistle_time_t now;
    
    if (cb == g_conmgr_releasing_cb)
    {
    	// skip the guy who just caused the disconnect in case he reregistered in the meantime.
    	return;
    }
    
    RTC_get_time_ms_ticks( &now );

	// If he was one of the registered clients for this connection (this connection, not a future one)
	// dispose of him now.
	if (cb->connected_cb)
	{
	    if (0 > UTIL_time_diff_seconds(&cb->min_wait_time, &now))
	    {
	    	// If he hasn't been notified of connection yet, tell him it failed
	        cb->connected_cb(cb, FALSE);
		    CONMGR_reset_cb(cb);
	    }
	}
	else if (cb->disconnected_cb)
	{
	    if (0 > UTIL_time_diff_seconds(&cb->min_wait_time, &now))
	    {
	    	// If he hasn't been notified that the connection is gone, tell him now
	        cb->disconnected_cb(cb);
		    CONMGR_reset_cb(cb);
	    }
	}
}

static void CONMGR_process_disconnected(_mqx_uint mask)
{
    CONMGR_client_id_t client_id;
    CONMGR_connection_type_t current_actual_connection_type = g_current_actual_connection_type;
    
    RTC_cancel_alarm(&g_conmgr_disconnect_timer_id);
    
    if ((WIFI_DISCONNECTED_EVENT & mask) && CONMGR_CONNECTION_TYPE_WIFI != current_actual_connection_type)
    {
    	return;
    }
    
    if ((BT_DISCONNECTED_EVENT & mask) && CONMGR_CONNECTION_TYPE_WIFI == current_actual_connection_type)
    {
    	return;
    }
    
    if( CONMGR_CONNECTION_TYPE_LAST < g_current_requested_connection_type )
    {
        /*
         *   No connection to disconnect.  Return to avoid punishment.
         */
        return;
    }
        
    // Notify all clients of the disconnected connection type or type ANY
    for (client_id = CONMGR_CLIENT_ID_FIRST; client_id < MAX_CLIENTS; client_id++)
    {
        if (CONMGR_CONNECTION_TYPE_ANY == g_current_requested_connection_type)
        {
        	// Check everyone with the same connection type as current connected type
        	if (CONMGR_CONNECTION_TYPE_UNKNOWN != g_current_actual_connection_type)
        	{
        		CONMGR_dispose_and_unregister(&(g_connection[g_current_actual_connection_type].client_info[client_id]));
        	}
        }
        else
        {
        	// Check everyone with the same connection type as current requested type
        	CONMGR_dispose_and_unregister(&(g_connection[g_current_requested_connection_type].client_info[client_id]));
        }
        
        // Check everyone with ANY, since ANY always matches
        CONMGR_dispose_and_unregister(&(g_connection[CONMGR_CONNECTION_TYPE_ANY].client_info[client_id]));
    }
        
    CONMGR_post_disconnect(g_current_requested_connection_type);
    
#ifdef WIFI_REBOOT_HACK
    // Reboot because we can't reliably turn WiFi off and back on.
    if (CONMGR_CONNECTION_TYPE_WIFI == current_actual_connection_type /* use cached value */)
    {
    	PWRMGR_Reboot( 500, PV_REBOOT_REASON_REBOOT_HACK, FALSE );
    }
#endif
#ifdef BT_REBOOT_HACK
	// Reboot because we've seen cases of BT stuck on and I gave up after a 2 day search.
    if (CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC == current_actual_connection_type || CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE == current_actual_connection_type /* use cached value */)
    {
    	PWRMGR_Reboot( 500, PV_REBOOT_REASON_REBOOT_HACK, FALSE );
    }
#endif
    
    CONMGR_transport_unlock(__FILE__, __LINE__);
}

static err_t CONMGR_BTC_open(int *pHandle)
{
    int i;
    err_t e_rc;
    _mqx_uint mask;
    int retries = 1;
    
    do
    {
        e_rc = BTMGR_open(pHandle);
        if (ERR_OK == e_rc)
        {
            return 0;
        }
        else if (ERR_BT_NO_HANDLES == e_rc)
        {
            _lwevent_wait_ticks(&g_conmgr_lwevent, CLOSED_EVENT | BT_DISCONNECTED_EVENT, FALSE, 0);
            
            mask = _lwevent_get_signalled();
            
            if (mask & BT_DISCONNECTED_EVENT)
            {
                return ERR_CONMGR_NOT_CONNECTED; // bail
            }
            
            if (mask & CLOSED_EVENT)
            {
                continue;
            }
        }
    } while (retries-- > 0);
    
    return ERR_CONMGR_BUSY; // max retries exceeded
}

static err_t CONMGR_BTLE_open(int *pHandle)
{
    // TODO
    return ERR_OK;
}

static err_t CONMGR_WIFI_open(int *pHandle, char *server, uint_32 port)
{
    err_t e_rc = ERR_OK;
    int retries = 0;
    int *pSocket = pHandle;
    
    do
    {
        if( ERR_OK != e_rc )
        {
            _time_delay( 200 );  // give some time on failed retries.
        }
        
        e_rc = WIFIMGR_open(server, port, pSocket, SOCK_STREAM_TYPE); //Open TCP Socket
        if (ERR_OK == e_rc || ERR_WIFI_NOT_CONNECTED == e_rc || ERR_WIFI_CRITICAL_ERROR == e_rc)
        {
        	goto CONMGR_WIFI_open_return;
        }
        else if (ERR_WIFI_NO_SOCKETS == e_rc)
        {
            _mqx_uint mask;
            
            // Wait for a socket to close and thus become available
            _lwevent_wait_ticks(&g_conmgr_lwevent, CLOSED_EVENT | WIFI_DISCONNECTED_EVENT, FALSE, 0);
            
            mask = _lwevent_get_signalled();
            
            if (mask & WIFI_DISCONNECTED_EVENT)
            {
                e_rc = ERR_WIFI_NOT_CONNECTED; // bail, fake err of NOT_CONNECTED so return processing works
                goto CONMGR_WIFI_open_return;
            }
            
            if (mask & CLOSED_EVENT)
            {
                continue;
            }
        }
    } while (retries++ < g_st_cfg.wifi_st.max_open_retries);
    
CONMGR_WIFI_open_return:
	if (e_rc != ERR_OK || retries > 1)
		PROCESS_UINFO("open err %x retry %d", e_rc, retries);

    // Log # of retries so we can dial in g_st_cfg.wifi_st.max_open_retries
    // TODO: remove this once we know how ot dial in g_st_cfg.wifi_st.max_open_retries
    if (retries > 0)
    {
    	PROCESS_NINFO(STAT_WIFI_OPEN_RETRY, "%d", retries);
    }
	
	switch (e_rc)
	{
		case ERR_OK:
			return ERR_OK;
		case ERR_WIFI_DNS_ERROR:
			return ERR_CONMGR_DNS_ERROR;
		case ERR_WIFI_NOT_CONNECTED:
			return  ERR_CONMGR_NOT_CONNECTED;
		case ERR_WIFI_NO_SOCKETS:
			return ERR_CONMGR_BUSY;
		case ERR_WIFI_CRITICAL_ERROR:
			return ERR_CONMGR_CRITICAL;
		case ERR_WIFI_SOCKET_CREATE_FAIL: 	// t_socket call failed
		case ERR_WIFI_SOCKET_CONNECT_FAIL:  // t_connect call failed
			return ERR_CONMGR_SOCK_CREATE_ERROR;
		case ERR_WIFI_ERROR:				// generic failure in _WIFIMGR_open_ip (don't think this can actually happen)
		default:
			return ERR_GENERIC;
	}
	return ERR_GENERIC;
}

static void CONMGR_disconnect_timer_callback(void)
{
    _lwevent_set(&g_conmgr_lwevent, DISCONNECT_TIMEOUT_EVENT);
}

static void CONMGR_disconnect(CONMGR_connection_type_t requested_connection_type)
{
    err_t e_rc;
    whistle_time_t disconnect_timeout_time;
    
    corona_print("CM DISC %d\n", requested_connection_type);
    
    // Allow 60 seconds for disconnect to complete or we're wedged.
    RTC_time_set_future(&disconnect_timeout_time, 60);
	g_conmgr_disconnect_timer_id = RTC_add_alarm_at_mqx(&disconnect_timeout_time, CONMGR_disconnect_timer_callback);
    
    e_rc = TPTMGR_disconnect();
    
    // If we weren't actually connected, we won't get the *_DISCONNECTED_EVENT that does
    // post disconnect cleanup, so force it now.
    if (ERR_WIFI_NOT_CONNECTED == e_rc)
    {
    	// TODO: I don't think this is needed for BT, but can't remember why :)
        _lwevent_set(&g_conmgr_lwevent, WIFI_DISCONNECTED_EVENT);
    }
}

static boolean CONMGR_process_connected_callback(struct CONMGR_client_s *cb)
{
    whistle_time_t now;

    if (NULL == cb->connected_cb)
    {
        return FALSE;
    }
    
    RTC_get_time_ms_ticks( &now );
                
    if (0 < UTIL_time_diff_seconds(&cb->min_wait_time, &now))
    {
        // Nope
        return FALSE;
    }
    
    if (CONMGR_CONNECTION_TYPE_UNKNOWN == g_current_actual_connection_type)
    {
        // Failed. Let him know. He'll have to re-register. Sorry.
    	CONMGR_dispose_and_unregister(cb);
    	return FALSE;
    }
    
    if (NULL != cb->connected_cb)
    {
    	cb->state = CONMGR_CLIENT_STATE_CONNECTED;
    	cb->connected_cb(cb, TRUE);
    	cb->connected_cb = NULL; // consumed
    }

    return TRUE;
}

static void CONMGR_process_callbacks(CONMGR_connection_type_t current_requsted_connection_type)
{
    int rc = -1;
    CONMGR_client_id_t client_id;
    TPTMGR_transport_type_t actual_transport_type;
    struct CONMGR_client_s *cb;
    boolean success = FALSE;
    err_t e_rc;
    whistle_time_t now;
    
    corona_print("CM PROCESS %d %d\n", current_requsted_connection_type, g_current_actual_connection_type);
    
    if (PWRMGR_is_rebooting())
    {
    	return;
    }
    
    // Store the current battery status in case it will be used during this transfer
    if ( !PWRMGR_process_batt_status( false ) )
    {
        // Abort the connection if the battery is too low (system will shut down)
        return;
    }
    
    UTIL_time_unset(&g_connection[current_requsted_connection_type].next_timeout_time);
    
    // Connect if we're not connected
    if (CONMGR_CONNECTION_TYPE_UNKNOWN == g_current_actual_connection_type)
    {
        // Lock all transports so we're not interrupted
        _watchdog_start(30*60*1000); // 30 min: don't let the watchdog expire across the lock (could be blocked by BT LM for a long time)
    	CONMGR_transport_lock(__FILE__, __LINE__);
    	
        // Kick off a scan to see who's nearby now
        if (!pmem_flag_get(PV_FLAG_IS_SYNC_MANUAL))
        {
        	PRXMGR_scan();
        }
        
        if (PWRMGR_is_rebooting())
        {
            CONMGR_transport_unlock(__FILE__, __LINE__);
        	return;
        }
    
#if SHUTDOWN_ACCMGR_ON_WIFI
        ACCMGR_shut_down( false );
        corona_print("CONMGR: waiting for accel mgmr to shutdown\n");
        while (! ACCMGR_is_shutdown() )
        {
            _time_delay(150);
        }
        corona_print("CONMGR: accl mgr shutdown\n");
#endif

        _watchdog_start(5*60*1000); // 5 min (must be after the PRX scan)

        g_conmgr_releasing_cb = NULL;
        
        // clear any stale disconnect events
        _lwevent_clear(&g_conmgr_lwevent, WIFI_DISCONNECTED_EVENT | BT_DISCONNECTED_EVENT);

        // attempt to connect the requested transport
        switch(current_requsted_connection_type)
        {
            case CONMGR_CONNECTION_TYPE_WIFI:
                e_rc = TPTMGR_connect(TPTMGR_TRANSPORT_TYPE_WIFI, &actual_transport_type);
                
                break;
            case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
                e_rc = TPTMGR_connect(TPTMGR_TRANSPORT_TYPE_BTC, &actual_transport_type);
                
                break;
            case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
                e_rc = TPTMGR_connect(TPTMGR_TRANSPORT_TYPE_BTLE, &actual_transport_type);
                
                break;
            case CONMGR_CONNECTION_TYPE_ANY:
                e_rc = TPTMGR_connect(TPTMGR_TRANSPORT_TYPE_ANY, &actual_transport_type);
                
                break;
            default:
                wassert(0);
        }
        
        if (ERR_OK == e_rc)
        {
            // see if/what transport type got connected, especially to handle the ANY case
            switch(actual_transport_type)
            {
                case TPTMGR_TRANSPORT_TYPE_WIFI:
                    g_current_actual_connection_type = CONMGR_CONNECTION_TYPE_WIFI;
                    break;
                case TPTMGR_TRANSPORT_TYPE_BTC:
                    g_current_actual_connection_type = CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC;
                    break;
                case TPTMGR_TRANSPORT_TYPE_BTLE:
                    g_current_actual_connection_type = CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE;
                    break;
                default:
                    wassert(0);
            }
        }
        else
        {
#if SHUTDOWN_ACCMGR_ON_WIFI
        	ACCMGR_start_up();
        	corona_print("CONMGR: accl mgr started\n");
#endif

#ifdef WIFI_REBOOT_HACK
        	if (TPTMGR_TRANSPORT_TYPE_WIFI == actual_transport_type)
        	{
        		// Reboot because we can't reliably turn WiFi off and back on.
        		PWRMGR_Reboot( 500, PV_REBOOT_REASON_REBOOT_HACK, FALSE );
        	}
#endif
#ifdef BT_REBOOT_HACK
        	// Reboot because we've seen cases of BT stuck on and I gave up after a 2 day search.
        	if (TPTMGR_TRANSPORT_TYPE_BTC == actual_transport_type || TPTMGR_TRANSPORT_TYPE_BTLE == actual_transport_type)
        	{
        		PWRMGR_Reboot( 500, PV_REBOOT_REASON_REBOOT_HACK, FALSE );
        	}
#endif
        	
        	g_current_actual_connection_type = CONMGR_CONNECTION_TYPE_UNKNOWN;

            CONMGR_transport_unlock(__FILE__, __LINE__);
        }
    }
    else if (current_requsted_connection_type != g_current_actual_connection_type)
    {
        // We're already connected, but this isn't the type he wants. He'll have to wait.
        return;
    }
    
    if ( PWRMGR_is_rebooting())
    {
        CONMGR_transport_unlock(__FILE__, __LINE__);
        return;
    }
    
	// Drinking an Anchor Steam right now.... There. That's better.
    // call clients back
	for (client_id = CONMGR_CLIENT_ID_FIRST; client_id < MAX_CLIENTS; client_id++)
	{
	    if (CONMGR_CONNECTION_TYPE_ANY == current_requsted_connection_type)
	    {
	    	// call back each client requesting same connection type as current connected type
	    	if  (CONMGR_CONNECTION_TYPE_UNKNOWN != g_current_actual_connection_type)
	    	{
	    		success |= CONMGR_process_connected_callback(&(g_connection[g_current_actual_connection_type].client_info[client_id]));
	    	}
	    }
	    else
	    {
	    	// call back each client requesting a connection with same type as current requested type
	    	success |= CONMGR_process_connected_callback(&(g_connection[current_requsted_connection_type].client_info[client_id]));
	    }
	    
		// call back each client requesting type ANY
    	success |= CONMGR_process_connected_callback(&(g_connection[CONMGR_CONNECTION_TYPE_ANY].client_info[client_id]));
	}
        
    if (success)
    {
        // make g_current_requested_connection_type shadow the current connection type
        // unless there was a failure.
        wassert(CONMGR_CONNECTION_TYPE_UNKNOWN != g_current_actual_connection_type);
        g_current_requested_connection_type = current_requsted_connection_type;
    }
    else
    {
        CONMGR_disconnect(current_requsted_connection_type);
    }
}

static void CONMGR_WIFI_callback_timeout(void)
{
    _lwevent_set(&g_conmgr_lwevent, WIFI_CALLBACK_TIMEOUT_EVENT);
}

static void CONMGR_BTC_callback_timeout(void)
{
    _lwevent_set(&g_conmgr_lwevent, BTC_CALLBACK_TIMEOUT_EVENT);
}

static void CONMGR_BTLE_callback_timeout(void)
{
    _lwevent_set(&g_conmgr_lwevent, BTLE_CALLBACK_TIMEOUT_EVENT);
}

static void CONMGR_ANY_callback_timeout(void)
{
    _lwevent_set(&g_conmgr_lwevent, ANY_CALLBACK_TIMEOUT_EVENT);
}

// Update the timeout for the given connection type, taking into account
// whether new cb needs service sooner than the prevailing timeout.
static void CONMGR_timeout_update(struct CONMGR_client_s *cb)
{
    whistle_time_t now;
	
    // See if the new request is sooner.. but only if next_timeout_time for this connection is already set..
    if (!UTIL_time_is_set(&cb->max_wait_time))
    {
    	corona_print("CM no max\n");
    	return;
    }
    
    RTC_get_time_ms_ticks( &now );

    corona_print("CM req %d\n", UTIL_time_diff_seconds(&cb->max_wait_time, &now));
	
    if (UTIL_time_is_set(&g_connection[cb->requested_connection_type].next_timeout_time))
    {
    	int_32 seconds;

    	seconds = UTIL_time_diff_seconds(&cb->max_wait_time, &g_connection[cb->requested_connection_type].next_timeout_time);
    	
    	corona_print("CM %d %d diff %d\n", UTIL_time_diff_seconds(&cb->max_wait_time, &now),
    			UTIL_time_diff_seconds(&g_connection[cb->requested_connection_type].next_timeout_time, &now), seconds);
    	
    	if (0 < seconds)
    	{
    		// Not sooner. Nothing to do. This guy will get called when someone sooner gets called
    		corona_print("CM not sooner\n");
    		return;
    	}
    	else
    	{
        	corona_print("CM sooner\n");
    	}
    }
    else
    {
    	corona_print("CM no next yet\n");
    }
    
    // New request becomes the one we wait for
    g_connection[cb->requested_connection_type].next_timeout_time = cb->max_wait_time;

    // Cancel any previous timer
    RTC_cancel_alarm(&g_connection[cb->requested_connection_type].callback_timer);

    g_connection[cb->requested_connection_type].callback_timer = RTC_add_alarm_at_mqx( &g_connection[cb->requested_connection_type].next_timeout_time, g_connection[cb->requested_connection_type].callback_timer_callback);
    wassert(g_connection[cb->requested_connection_type].callback_timer);
}

static void CONMGR_WIFI_close(int handle)
{
    WIFIMGR_close(handle);
}

static void CONMGR_BTC_close(int handle)
{   
    BTMGR_close(handle);
}

static void CONMGR_BTLE_close(int handle)
{
    wassert(0); // TODO
}

static err_t CONMGR_WIFI_send(int transport_handle, uint_8 *data, uint_32 length, int_32 *pbytes_sent)
{
    err_t err;
    
    err = WIFIMGR_send(transport_handle, data, length, pbytes_sent);

    switch (err)
    {
        case ERR_OK:
            break;
        case ERR_WIFI_BAD_SOCKET:
            err = ERR_CONMGR_INVALID_HANDLE;
             break;
        case ERR_WIFI_NOT_CONNECTED:
            err = ERR_CONMGR_NOT_CONNECTED;
            break;
        default:
            err = ERR_CONMGR_ERROR;
            break;
    }
    
    return err;
}

static err_t CONMGR_BTC_send(int transport_handle, uint_8 *data, uint_32 length, int_32 *pbytes_sent)
{
    err_t err;
    
    *pbytes_sent = 0;
    
    err = BTMGR_wmps_send(transport_handle, data, length, pbytes_sent, NULL, wmps_next_header_payload_type_wmp1_fragment);

    switch (err)
    {
        case ERR_OK:
            break;
        case ERR_BT_NOT_CONNECTED:
            err = ERR_CONMGR_NOT_CONNECTED;
            break;
        default:
            err = ERR_CONMGR_ERROR;
            break;
    }
    
    return err;
}


static err_t CONMGR_WIFI_receive(int transport_handle, void *buf, uint_32 buf_len, uint_32 ms_timeout, int_32 *pbytes_received, boolean continuous_aggregate)
{
    err_t err;

    err = WIFIMGR_receive(transport_handle, buf, buf_len, ms_timeout, pbytes_received, continuous_aggregate);
    
    switch (err)
    {
        case ERR_OK:
            break;
        case ERR_WIFI_BAD_SOCKET:
            err = ERR_CONMGR_INVALID_HANDLE;
             break;
        case ERR_WIFI_NOT_CONNECTED:
            err = ERR_CONMGR_NOT_CONNECTED;
            break;
        default:
            //PROCESS_NINFO(err, "wifi rx"); //already caught in WIFIMGR_Receive
            err = ERR_CONMGR_ERROR;
            break;
    }
    
    return err;
}

static err_t CONMGR_BTC_receive(int transport_handle, void *buf, uint_32 buf_len, uint_32 ms_timeout, int_32 *pbytes_received)
{
    err_t err;
    wmps_seqnum_t seq_num;

    err = BTMGR_wmps_receive(transport_handle, buf, buf_len, ms_timeout, pbytes_received, &seq_num);
    
    switch (err)
    {
        case ERR_OK:
            break;
        case ERR_BT_NOT_CONNECTED:
            err = ERR_CONMGR_NOT_CONNECTED;
            break;
        default:
            err = ERR_CONMGR_ERROR;
            break;
    }
    
    return err;
}

static void CONMGR_WIFI_zero_copy_free(void *buf)
{
    WIFIMGR_zero_copy_free(buf);
}

static void CONMGR_BTC_zero_copy_free(void *buf)
{
    BTMGR_zero_copy_free();
}

static void CONMGR_system_shutdown_callback(sys_event_t sys_event)
{
    // Send the shut down event
    _lwevent_set(&g_conmgr_lwevent, SHUTDOWN_EVENT);
}


// Publics

// Callback registration API
// This can block!!!! Call off your watch dogs!
err_t CONMGR_register_cbk(CONMGR_connected_cbk_t connected_cb,
        CONMGR_disconnected_cbk_t disconnected_cb, uint_32 max_wait_time, uint_32 min_wait_time,
        CONMGR_connection_type_t connection_type, CONMGR_client_id_t client_id, CONMGR_handle_t *handle)
{
    struct CONMGR_client_s *cb;
    
    corona_print("CM REG %x %d %d %d %d\n", &(g_connection[connection_type].client_info[client_id]), max_wait_time, min_wait_time, connection_type, client_id);
    
    if (PWRMGR_is_rebooting())
    {
    	return ERR_OK;
    }
        
    cb = &(g_connection[connection_type].client_info[client_id]);
    if (NULL != handle)
    {
    	*handle = (CONMGR_handle_t)cb;
    }
    
    wassert(connection_type >= CONMGR_CONNECTION_TYPE_FIRST && connection_type <= CONMGR_CONNECTION_TYPE_LAST);
    wassert(client_id >= CONMGR_CLIENT_ID_FIRST && client_id <= CONMGR_CLIENT_ID_LAST);
    wassert(max_wait_time >= min_wait_time);
    
    wmutex_lock(&g_conmgr_mutex);

    // see if he's already registered
    if (CONMGR_CLIENT_STATE_NOT_REGISTERED != cb->state)
    {
    	// If he'll take ANY and we're already connected,
    	// or we're already connected to the one he wants, we're done
    	if (((CONMGR_CONNECTION_TYPE_ANY == cb->requested_connection_type) &&
    			(CONMGR_CONNECTION_TYPE_UNKNOWN != g_current_actual_connection_type)) ||
    			(cb->requested_connection_type == g_current_actual_connection_type))
    	{
    		goto DONE;
    	}

    	g_conmgr_mutex_locked = TRUE;  // hack so we don't block on CONMGR_release().
    	
    	// Blow away old registration.
    	CONMGR_release((CONMGR_handle_t)cb); // acquires mutex, so can't hold it across
    	
    	g_conmgr_mutex_locked = FALSE;
    }

    cb->connected_cb = connected_cb;
    cb->disconnected_cb = disconnected_cb;
    cb->requested_connection_type = connection_type;
    cb->state = CONMGR_CLIENT_STATE_NOT_CONNECTED;
    
    if (-1 == max_wait_time)
    {
    	// -1 means infinite
    	UTIL_time_unset(&cb->max_wait_time);
    }
    else
    {
        RTC_time_set_future(&cb->max_wait_time, max_wait_time); // harmless if 0. we won't use it in that case
    }
    
    RTC_time_set_future(&cb->min_wait_time, min_wait_time); // harmless if 0. we won't use it in that case
        
    // See if we should/can process him now
    if (max_wait_time == 0)
    {
    	// 0 means now
        g_connection[connection_type].callback_timer_callback();
    }
    else
    {
        CONMGR_timeout_update(cb);
    }
    
DONE:
    wmutex_unlock(&g_conmgr_mutex);
    
    return ERR_OK;
}

/*
 *  Called to close an opened socket
 */
err_t CONMGR_close(CONMGR_handle_t handle)
{
    struct CONMGR_client_s *cb = (struct CONMGR_client_s *)handle;
    
    corona_print("CM CLOSE %x %x %d %d %d\n", handle, cb->transport_handle, cb->state, cb->requested_connection_type, g_current_actual_connection_type);
    
    if (PWRMGR_is_rebooting())
    {
    	return ERR_OK;
    }

    if (-1 != cb->transport_handle)
    {
        switch (g_current_actual_connection_type)
        {
            case CONMGR_CONNECTION_TYPE_WIFI:
                CONMGR_WIFI_close(cb->transport_handle); // blocking
                _lwevent_set(&g_conmgr_lwevent, CLOSED_EVENT);
                
                break;
                
            case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
                CONMGR_BTC_close(cb->transport_handle); // blocking
                _lwevent_set(&g_conmgr_lwevent, CLOSED_EVENT);
                
                break;
                
            case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
                CONMGR_BTLE_close(cb->transport_handle); // blocking
                _lwevent_set(&g_conmgr_lwevent, CLOSED_EVENT);
                
                break;
            default:
                // We can legitimately get here when a client releases after
                // a connection is down
                break;
        }

        cb->transport_handle = -1;
    }
    
    return ERR_OK;
}

// Clients call this to open a socket. Called and handled on client thread! Blocks till opened.
err_t CONMGR_open(CONMGR_handle_t handle, char *server, uint_32 port)
{
    err_t err = ERR_OK;
    struct CONMGR_client_s *cb = (struct CONMGR_client_s *)handle;
    
    corona_print("CM OPEN %x %d %d %s %d %d\n", handle, cb->state, cb->requested_connection_type, server, port, g_current_actual_connection_type);
        
    if (PWRMGR_is_rebooting())
    {
    	return ERR_OK;
    }
            
    wassert(CON_MAGIC_VALUE == cb->magic); // check that g_connection is fully initialized.
    
    // No mutex protection needed here (I hope)
     
    CONMGR_close(handle); // make sure any old one is released
    
    if (CONMGR_CLIENT_STATE_CONNECTED == cb->state)
    {
        switch (g_current_actual_connection_type)
        {
            case CONMGR_CONNECTION_TYPE_WIFI:
                err = CONMGR_WIFI_open(&cb->transport_handle, server, port); // blocking
                break;
                
            case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
                err = CONMGR_BTC_open(&cb->transport_handle); // blocking
                break;
                
            case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
                err = CONMGR_BTLE_open(&cb->transport_handle); // blocking
                break;
            default:
                // We can legitimately get here when a client opens after
                // a connection is closed.
                break;
        }
    }
    
    if (ERR_OK != err)
    {
        cb->transport_handle = -1;
    }
    
    corona_print("CM OPEN h %x\n", cb->transport_handle);
       
    return err;
}

// Clients call this to release their need for the connection. Called and handled on client thread! Blocks till closed.
// Also called internally.
err_t CONMGR_release(CONMGR_handle_t handle)
{
    struct CONMGR_client_s *releasing_cb = (struct CONMGR_client_s *)handle;
    struct CONMGR_client_s *cb;
    CONMGR_client_id_t client_id;
    boolean inUse = FALSE;
    CONMGR_connection_type_t requested_connection_type = releasing_cb->requested_connection_type;
    
    corona_print("CM RELEASE %x %x %d %d %d\n", handle, releasing_cb->transport_handle, releasing_cb->state, releasing_cb->requested_connection_type, g_current_actual_connection_type);
    
    wassert(CON_MAGIC_VALUE == releasing_cb->magic); // check that g_connection is fully initialized.
    
    if( !g_conmgr_mutex_locked )
    {
        wmutex_lock(&g_conmgr_mutex);
    }
    
    if (PWRMGR_is_rebooting())
    {
    	goto DONE;
    }
    
    CONMGR_close(handle);
            
    CONMGR_reset_cb(releasing_cb);
    
    // If all clients are closed, close the transport
    for (client_id = CONMGR_CLIENT_ID_FIRST; client_id < MAX_CLIENTS; client_id++)
    {
        if (CONMGR_CONNECTION_TYPE_ANY == requested_connection_type)
        {
        	// Check everyone with the same connection type as current connected type
        	if (g_current_actual_connection_type != CONMGR_CONNECTION_TYPE_UNKNOWN)
        	{
        		cb = &(g_connection[g_current_actual_connection_type].client_info[client_id]);
        		if (CONMGR_CLIENT_STATE_CONNECTED == cb->state)
        		{
        			// still in use
        			inUse = TRUE;
        			break;
        		}
        	}
        }
        else
        {
        	// Check everyone with the same connection type as current requested type
            cb = &(g_connection[requested_connection_type].client_info[client_id]);
        	if (CONMGR_CLIENT_STATE_CONNECTED == cb->state)
        	{
        		// still in use
        		inUse = TRUE;
        		break;
        	}
        }
        
        // Check everyone with ANY, since ANY always matches
        cb = &(g_connection[CONMGR_CONNECTION_TYPE_ANY].client_info[client_id]);
    	if (CONMGR_CLIENT_STATE_CONNECTED == cb->state)
        {
            // still in use
            inUse = TRUE;
            break;
        }
    }
    
    corona_print("CM RELEASE in use %d\n", inUse);
    
    if (!inUse)
    {
    	g_conmgr_releasing_cb = releasing_cb;
        CONMGR_disconnect(requested_connection_type);
    }
        
DONE:
    if( !g_conmgr_mutex_locked )
    {
        wmutex_unlock(&g_conmgr_mutex);
    }
    
    return ERR_OK;
}

// Send data
err_t CONMGR_send(CONMGR_handle_t handle, uint_8 *data, uint_32 length, int_32 *pbytes_sent)
{
    struct CONMGR_client_s *cb = (struct CONMGR_client_s *)handle;
    err_t err = ERR_CONMGR_NOT_CONNECTED;
    
    //corona_print("CM SEND %x %x %d %d %d %d\n", handle, cb->transport_handle, cb->state, cb->requested_connection_type, length, g_current_actual_connection_type);
        
    *pbytes_sent = 0;

    if (PWRMGR_is_rebooting())
    {
    	return ERR_OK;
    }
        
    if (-1 == cb->transport_handle)    
    {
        return ERR_CONMGR_INVALID_HANDLE;
    }

    switch (g_current_actual_connection_type)
     {
         case CONMGR_CONNECTION_TYPE_WIFI:
             err = CONMGR_WIFI_send(cb->transport_handle, data, length, pbytes_sent); // blocking
             break;
                          
         case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
        	 err = CONMGR_BTC_send(cb->transport_handle, data, length, pbytes_sent); // blocking
             break;
                          
         case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
             wassert(0); // TODO
             break;
                          
         default:
             err = ERR_CONMGR_NOT_CONNECTED;
             break;
     }
    
    return err;
}

int CONGMR_get_max_seg_size(void)
{
	switch(g_current_actual_connection_type)
	{
	case CONMGR_CONNECTION_TYPE_WIFI:
		return g_st_cfg.evt_st.upload_chunk_sz_wifi;
	case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
		return g_st_cfg.evt_st.upload_chunk_sz_btc;
	default:
		wassert(0);
	}
	
	return 0;
}

// Receive data with timeout.
err_t CONMGR_receive(CONMGR_handle_t handle, void *buf, uint_32 buf_len, uint_32 ms_timeout, int_32 *pBytesReceived, boolean continuous_aggregate)
{
    struct CONMGR_client_s *cb = (struct CONMGR_client_s *)handle;
    err_t err = ERR_CONMGR_NOT_CONNECTED;
    
    //corona_print("CM RECV %x %d %d %d %d %d %d %d\n", handle, cb->transport_handle, cb->state, cb->requested_connection_type, buf_len, ms_timeout, continuous_aggregate, g_current_actual_connection_type);
    
    *pBytesReceived = 0;
    
    if (PWRMGR_is_rebooting())
    {
    	return ERR_OK;
    }
        
    if (-1 == cb->transport_handle)
    {
        return ERR_CONMGR_INVALID_HANDLE;
    }

    switch (g_current_actual_connection_type)
     {
         case CONMGR_CONNECTION_TYPE_WIFI:
             err = CONMGR_WIFI_receive(cb->transport_handle, buf, buf_len, ms_timeout, pBytesReceived, continuous_aggregate); // blocking
             break;
                          
         case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
             err = CONMGR_BTC_receive(cb->transport_handle, buf, buf_len, ms_timeout, pBytesReceived); // blocking
             break;
                          
         case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
             wassert(0); // TODO
             break;
                          
         default:
             err = ERR_CONMGR_NOT_CONNECTED;
             break;
     }
    
    return err;
}

// Free the zero-copy buffer
void CONMGR_zero_copy_free(CONMGR_handle_t handle, void *buf)
{
    struct CONMGR_client_s *cb = (struct CONMGR_client_s *)handle;

    //corona_print("CM ZCF %x %x %d %d %d\n", handle, cb->transport_handle, cb->state, cb->requested_connection_type, g_current_actual_connection_type);

    switch (g_current_actual_connection_type)
     {
         case CONMGR_CONNECTION_TYPE_WIFI:
             CONMGR_WIFI_zero_copy_free(buf);
             break;
                          
         case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC:
             wassert(0); // TODO
                          
         case CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE:
             wassert(0); // TODO
                          
         default:
             break;
     }
}

// Called by TPTMGR
void CONMGR_disconnected(sys_event_t event)
{
    _lwevent_set(&g_conmgr_lwevent, SYS_EVENT_WIFI_DOWN == event ? WIFI_DISCONNECTED_EVENT : BT_DISCONNECTED_EVENT);
}

/*
 *   Connection manager shutdown routine.
 */
void CONMGR_shutdown(void)
{
    CONMGR_stop_timers();
    
    whistle_task_destroy(CONMGR_TASK_NAME);
}

/*
 *   Cancels all connection manager timers.
 */
void CONMGR_stop_timers(void)
{
    CONMGR_connection_type_t i;
    
    for(i = CONMGR_CONNECTION_TYPE_FIRST; i <= CONMGR_CONNECTION_TYPE_LAST; i++)
    {
        RTC_cancel_alarm(&g_connection[i].callback_timer);
    }
}

CONMGR_connection_type_t CONMGR_get_current_actual_connection_type(void)
{
	return g_current_actual_connection_type;
}

void CONMGR_init(void)
{
    _lwevent_create(&g_conmgr_lwevent, LWEVENT_AUTO_CLEAR);
    
    wmutex_init(&g_conmgr_mutex);
    
    sys_event_register_cbk(CONMGR_system_shutdown_callback, SYS_EVENT_SYS_SHUTDOWN);
               
    UTIL_time_unset(&g_connection[CONMGR_CONNECTION_TYPE_WIFI].next_timeout_time);
    UTIL_time_unset(&g_connection[CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC].next_timeout_time);
    UTIL_time_unset(&g_connection[CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE].next_timeout_time);
    UTIL_time_unset(&g_connection[CONMGR_CONNECTION_TYPE_ANY].next_timeout_time);
}

void CON_tsk(uint_32 dummy)
{       
    _mqx_uint mask;
    
    while (1)
    {
		PWRMGR_VoteForSleep( PWRMGR_CONMGR_VOTE );

        _lwevent_wait_ticks(&g_conmgr_lwevent, WIFI_DISCONNECTED_EVENT     | 
        									   BT_DISCONNECTED_EVENT       |
                                               WIFI_CALLBACK_TIMEOUT_EVENT | 
                                               BTC_CALLBACK_TIMEOUT_EVENT  | 
                                               BTLE_CALLBACK_TIMEOUT_EVENT |
                                               ANY_CALLBACK_TIMEOUT_EVENT  |
                                               SHUTDOWN_EVENT |
                                               DISCONNECT_TIMEOUT_EVENT, 
                                               FALSE, 0);

		// Stay awake while we process the event
		PWRMGR_VoteForAwake( PWRMGR_CONMGR_VOTE );
		
        mask = _lwevent_get_signalled();
        
        wmutex_lock(&g_conmgr_mutex);
        
        corona_print("CM EVENT %x %d\n", mask, g_current_actual_connection_type);
        
        if (PWRMGR_is_rebooting())
        {
        	goto DONE;
        }
        
        _watchdog_start(5*60*1000); // 5 min
                        
        if (mask & WIFI_DISCONNECTED_EVENT || mask & BT_DISCONNECTED_EVENT)
        {
            CONMGR_process_disconnected(mask);
        }
        
        if (mask & WIFI_CALLBACK_TIMEOUT_EVENT)
        {
            CONMGR_process_callbacks(CONMGR_CONNECTION_TYPE_WIFI);
        }       
        
        if (mask & BTC_CALLBACK_TIMEOUT_EVENT)
        {
            CONMGR_process_callbacks(CONMGR_CONNECTION_TYPE_SMARTPHONE_BTC);
        }       
        
        if (mask & BTLE_CALLBACK_TIMEOUT_EVENT)
        {
            CONMGR_process_callbacks(CONMGR_CONNECTION_TYPE_SMARTPHONE_BTLE);
        }       
        
        if (mask & ANY_CALLBACK_TIMEOUT_EVENT)
        {
            CONMGR_process_callbacks(CONMGR_CONNECTION_TYPE_ANY);
        }
        
        if (mask & SHUTDOWN_EVENT)
        {
        	goto DONE;
        }
        
        if (mask & DISCONNECT_TIMEOUT_EVENT)
        {
        	// The transport stack is wedged. Good bye.
        	wassert(0);
        }
        
        _watchdog_stop();
        wmutex_unlock(&g_conmgr_mutex);
    }
DONE:
    _watchdog_stop();
    wmutex_unlock(&g_conmgr_mutex);
    
    CONMGR_transport_unlock(__FILE__, __LINE__);
    
    PWRMGR_VoteExit( PWRMGR_CONMGR_VOTE );
    _task_block(); // Don't exit or our MQX resources may be deleted
}

#if 0
// Send data
err_t CONMGR_get_seg_size(CONMGR_handle_t handle)
{
    struct CONMGR_client_s *cb = (struct CONMGR_client_s *)handle;
    err_t err = ERR_CONMGR_NOT_CONNECTED;
        
    if (PWRMGR_is_rebooting())
    {
    	return ERR_OK;
    }
        

    if (-1 == cb->transport_handle)    
    {
        return ERR_CONMGR_INVALID_HANDLE;
    }

    switch (g_current_actual_connection_type)
     {
         case CONMGR_CONNECTION_TYPE_WIFI:
             err = WIFIMGR_get_max_seg(cb->transport_handle); // blocking
             break;
  
         default:
             err = ERR_CONMGR_NOT_CONNECTED;
             break;
     }
    
    return err;
}
#endif
