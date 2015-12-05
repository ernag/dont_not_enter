/*
 * bt_mgr.c
 *
 *  Created on: May 15, 2013
 *      Author: Chris
 */

#include "wassert.h"
#include <mqx.h>
#include <bsp.h>
#include "cormem.h"
#include "cfg_mgr.h"
#include "bt_mgr.h"
#include "bt_mgr_priv.h"
#include "bt_mgr_wmps.h"
#include "sys_event.h"
#include "rf_switch.h"
#include "main.h"
#include "pwr_mgr.h"
#include "wmutex.h"
#include "GAPAPI.h"

#define BT_DDOBT_REBOOT_HACK 1

typedef enum _tag_BTMGR_state_e
{
	BTMGR_STATE_UNINITIALIZED,
	BTMGR_STATE_NOT_CONNECTED,
	BTMGR_STATE_CLIENT_OPENING,
	BTMGR_STATE_CLIENT_OPEN,
	BTMGR_STATE_SERVER_OPENING,
	BTMGR_STATE_SERVER_PAIRING,
	BTMGR_STATE_SERVER_PAIRED,
	BTMGR_STATE_SERVER_OPEN,
	BTMGR_STATE_CHECKING_PROXIMITY
} BTMGR_state_e;

static int g_btmgr_handle = 0;
static BTMGR_state_e g_btmgr_state = BTMGR_STATE_UNINITIALIZED;

static LWEVENT_STRUCT g_btmgr_lwevent;
#define NON_IOS_PAIRING_SUCCEEDED_EVENT 0x0001
#define PAIRING_STARTED_EVENT           0x0002
#define PAIRING_FAILED_EVENT            0x0004
#define PAIRING_SUCCEEDED_EVENT         0x0008
#define IOS_PAIRING_SUCCEEDED_EVENT     0x0010
#define CONNECTING_SUCCEEDED_EVENT      0x0020
#define CONNECTING_FAILED_EVENT         0x0040
#define DOWN_EVENT                      0x0080

static MUTEX_STRUCT g_btmgr_ref_mutex;
static int g_btmgr_ref_count = 0;

static boolean g_btmgr_audit_bt_is_on = FALSE; // for power manager audit only. not used for BT control

static LWSEM_STRUCT g_btmgr_handle_sem;
#define NUM_BT_HANDLES 1

boolean _BTMGR_lm_checkin(LmMobileStatus *plmMobileStatus, int key)
{
	int rc;
    int_32 received;
    uint_8 *encoded_msg = NULL;
    boolean success = FALSE;
    wmps_msg_struct *decoded_msg = NULL;
	uint_32 wmps_bytes_remaining;
    wmps_error_t wmps_err;
    uint8_t *pcheckin_msg = NULL;
    LmMobileStat lmMobileStat;
    size_t msg_size;
    int_32 bytes_sent;
    
    /*
     * Calculate Checkin Msg Size & malloc
     */

    msg_size = WMP_lm_checkin_req_gen(NULL, -1, key);
    
    pcheckin_msg = walloc(msg_size);
    
    /*
     * Generate checkin msg
     */
    
    msg_size = WMP_lm_checkin_req_gen(pcheckin_msg, msg_size, key);
    
	rc = BTMGR_wmps_send(-1 /* don't try this at home */, pcheckin_msg, msg_size, &bytes_sent, NULL, wmps_next_header_payload_type_wmp1); // our seq num
	
	wfree(pcheckin_msg);
	
	if (0 != rc)
	{
		PROCESS_NINFO(ERR_BT_ERROR, "wmp tx %x", rc);
		return FALSE;
	}
	
	encoded_msg = walloc(WMPS_MAX_PB_SERIALIZED_MSG_SIZE);
	
	rc = BTMGR_PRIV_receive((void *)encoded_msg, WMPS_HEADER_SZ, 10000, &received, TRUE);
	
	if (WMPS_HEADER_SZ > received)
	{
	    PROCESS_NINFO(ERR_BT_ERROR, "rx1 %d", rc);
		goto DONE;
	}
	
	wassert(WMPS_HEADER_SZ == received);

	wmps_bytes_remaining = (uint_32)(*((wmps_messagesize_t *)(encoded_msg + WMPS_MESSAGESIZE_OFFSET))) - WMPS_HEADER_SZ;
	wassert(wmps_bytes_remaining <= WMPS_MAX_PB_SERIALIZED_MSG_SIZE - WMPS_HEADER_SZ);

    decoded_msg = WMP_make_msg_struct_with_payload_alloc(WMPS_MAX_PB_SERIALIZED_MSG_SIZE);
    wassert(decoded_msg);
    
	rc = BTMGR_PRIV_receive((void *)(encoded_msg + WMPS_HEADER_SZ), wmps_bytes_remaining, 10000, &received, TRUE);
	
	if (received < wmps_bytes_remaining)
	{
	    PROCESS_NINFO(ERR_BT_ERROR, "rx2 %d", rc);
		goto DONE;
	}

    wmps_err = WMP_get_next_message(decoded_msg, encoded_msg, WMPS_MAX_PB_SERIALIZED_MSG_SIZE, (size_t *)&received);
    
    if ((0 != wmps_err) || (0 == received))
    {
		rc = PROCESS_NINFO(ERR_BT_WMP_GET, "%d %d", wmps_err, received);
		goto DONE;
    }
    
    corona_print("BT ckn sz %d\n", wmps_bytes_remaining + WMPS_HEADER_SZ);
    
#ifdef CHECK_SEQ_NUM
    if (decoded_msg->seqnum != g_btmgr_local_pb_seqnum)
    {
		corona_print("BT_checkin: decoded_msg->seqnum %d g_btmgr_send_pb_seqnum %d\n", decoded_msg->seqnum, g_btmgr_local_pb_seqnum);
		goto DONE;
    }
#endif
    
    // Now that we've gotten a response, we can increment the expected seq num
    BTMGR_wmps_increment_local_seqnum();
    
    rc = WMP_lm_checkin_resp_parse(decoded_msg->payload, decoded_msg->payload_size, &lmMobileStat);
    
	if (0 == rc)
	{
		*plmMobileStatus = lmMobileStat.status;

		success = TRUE;
		
    	if (lmMobileStat.has_serverAbsoluteTime)
    	{
    		time_set_server_time(lmMobileStat.serverAbsoluteTime, FALSE);
    	}
	}

DONE:
    if (decoded_msg)
    {
    	WMP_free_msg_struct_and_payload(decoded_msg);
    }
    
    if (encoded_msg)
    {
    	wfree(encoded_msg);
    }	
	
	return success;
}


// local functions
static int _BTMGR_connect_key(int key, LmMobileStatus *plmMobileStatus)
{
    int rc;
    _mqx_uint mask;
    _mqx_uint mqx_rc;
    uint64_t start_time, time_to_connect;

    /* David's awesome Carnitas recipe:
     
    Pork shoulder (bone-in, or de-boned – bone-in gives you the yummy bone flavor, but de-boned allows you to get the spice rub into more nooks and crannies)
    Spice rub
    ·         2 T cumin

    ·         1 T coriander

    ·         2 T pimenton (I used the sweet kind)

    ·         1 tsp cinnamon

    ·         ½ cup brown sugar

    ·         ¼ cup kosher salt

    Olive oil
    (you can make whatever spice rub you like, though)
     
    You need a couple of days.
     
    2 days before the meal, rub the shoulder all over with the rub and refrigerate it.
     
    The night before the meal (or morning of, if you plan to eat at night), heat some olive oil in a cast-iron pot, brown the meat on all sides for about 30 seconds/side, and then put the pot in the oven on about 200 degrees or the lowest setting.
     
    That’s it! In 12 hours, you will have super-tender pulled pork!
     
    If you want, you can throw it on the grill/smoker with some wood chips to try and get some smoke flavor into it (depends on the rub). That’s what I did, but it wasn’t very successful, since I don’t have a real smoker.
    */

    /* Start from a zero state.
     * Design is to disconnect and start a new connection instead of returning without doing anything
     */
    
    _watchdog_start(60*1000);
    
	rf_switch(RF_SWITCH_BT, TRUE);

    corona_print("BT con %02x:%02x:%02x:%02x:%02x:%02x\n",
    		g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR5, g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR4, g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR3,
    		g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR2, g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR1, g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR0);
    
    /* Clear any stale events before evoking them */
    _lwevent_clear(&g_btmgr_lwevent, 0xffffffff);
    
    RTC_get_ms(&start_time);

    rc = OpenRemoteServer(g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR, g_dy_cfg.bt_dy.LinkKeyInfo[key].pom);
    
    _watchdog_stop();

    if (0 >= rc)
    {
    	WPRINT_ERR("conkey %d", rc);
    	rc = -1; // not connected
    	goto ERROR;
    }

    mqx_rc =_lwevent_wait_ticks(&g_btmgr_lwevent,
    		CONNECTING_SUCCEEDED_EVENT | CONNECTING_FAILED_EVENT, FALSE,
    		(g_st_cfg.bt_st.bt_connecting_timeout)*BSP_ALARM_FREQUENCY);

    if (LWEVENT_WAIT_TIMEOUT == mqx_rc)
    {
    	PROCESS_NINFO(ERR_BT_CONN_TO, NULL);
    	rc = -1; // not connected
    	goto ERROR;
    }

    RTC_get_ms(&time_to_connect);
    time_to_connect = time_to_connect - start_time;
    PROCESS_NINFO(STAT_BT_CONN, "%d", time_to_connect);

    mask = _lwevent_get_signalled();

    if (!(mask & CONNECTING_SUCCEEDED_EVENT))
    {
    	PROCESS_NINFO(ERR_BT_CONN_FAIL, NULL);
    	rc = -1; // not connected
    	goto ERROR;
    }

    corona_print("BT Conn'd\n");
    
    _watchdog_start(60*1000);

    if (!_BTMGR_lm_checkin(plmMobileStatus, key))
    {
    	PROCESS_NINFO(ERR_BT_LM_CHCKN_FAIL, NULL);
    	rc = -1;
    	goto ERROR;
    }
    
    rc = 0;

ERROR:
    _watchdog_stop();
    return rc;
}

static int _BTMGR_get_key(BD_ADDR_t BD_ADDR)
{
	int key;
	
	for (key = 0; key < MAX_BT_REMOTES; ++key)
	{
	    if (COMPARE_BD_ADDR(g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR, BD_ADDR))
	    {
	    	return key;
	    }
	}
	
	return -1;
}

// this is just for power measurements
static void _BTMGR_set_bt_audit_state(boolean isOn)
{
	g_btmgr_audit_bt_is_on = isOn;
	PWRMGR_breadcrumb();
}

static void _BTMGR_init(void)
{
	int stack_id = 0;
	int retries = 0;
	
	corona_print("BTinit'g\n");
	
	PWRMGR_VoteForRUNM( PWRMGR_BTMGR_VOTE );
	
	if (BTMGR_STATE_UNINITIALIZED != g_btmgr_state)
	{
		corona_print("State %d\n", g_btmgr_state);
		return;
	}
	
	_BTMGR_set_bt_audit_state(TRUE);
		
	// We must have the switch set to something other than 00 when any of the radios are on,
	// so turn on the switch here even if we're not necessarily ready to use WiFi
	rf_switch(RF_SWITCH_BT, TRUE);
	
	// Try to Open the stack
	
	while (2 > retries++)
	{
		stack_id = OpenStack();
		
		_watchdog_start(60*1000);
		
		if (1 <= stack_id)
		{
		    BT_verify_hw();
			break;
		}
		
		CloseStack();
	} 
	
	if (1 < retries)
	{
		PROCESS_NINFO(ERR_BT_OPEN_STK_FAILED_N_TIMES, "N=%d", retries - 1);
	}
	
	if (1 > stack_id)
	{
		PROCESS_NINFO(ERR_BT_POWERUP_SEQ_FAILED, NULL);
        PWRMGR_Reboot( 500, PV_REBOOT_REASON_BT_FAIL, FALSE );
	}

	_lwevent_create(&g_btmgr_lwevent, LWEVENT_AUTO_CLEAR);
	_lwsem_create(&g_btmgr_handle_sem, NUM_BT_HANDLES);
	
	g_btmgr_state = BTMGR_STATE_NOT_CONNECTED;
	
	corona_print("BTinit'd\n");
}

static void _BTMGR_deinit(void)
{
	corona_print("BTdeinit\n");

	if (BTMGR_STATE_UNINITIALIZED == g_btmgr_state)
	{
		corona_print("alrdy\n");
		return;
	}
	
	BTMGR_disconnect();
	
	CloseStack();
	
	_lwevent_destroy(&g_btmgr_lwevent);
	_lwsem_destroy(&g_btmgr_handle_sem);
	
	g_btmgr_state = BTMGR_STATE_UNINITIALIZED;
	
	_BTMGR_set_bt_audit_state(FALSE);
	
	PWRMGR_VoteForVLPR( PWRMGR_BTMGR_VOTE );
	
	rf_switch(RF_SWITCH_BT, FALSE);
	
	corona_print("done\n");
}

static void _BTMGR_add_delete_pair_breadcrumb(int key, boolean added)
{
	int i;
	int in_use_count = 0;
	
	for (i = 0; i < MAX_BT_REMOTES; ++i)
	{
	    if (!COMPARE_NULL_BD_ADDR(g_dy_cfg.bt_dy.LinkKeyInfo[i].BD_ADDR))
	    {
	    	in_use_count++;
	    }
	}
	
	if (!added)
	{
		// Assume this function is called before actually deleting
		in_use_count--;
	}

	PROCESS_NINFO(STAT_BT_PAIR_ADD_DELETE, "%s %02x:%02x:%02x:%02x:%02x:%02x %d remain", added ? "Add" : "Del",
			g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR5, g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR4, g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR3,
			g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR2, g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR1, g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR.BD_ADDR0,
					MAX_BT_REMOTES - in_use_count);
}

void BTMGR_delete_key(int key)
{
	_BTMGR_add_delete_pair_breadcrumb(key, FALSE); // log it before actually deleting it

	memset(&g_dy_cfg.bt_dy.LinkKeyInfo[key].LinkKey, 0, sizeof(g_dy_cfg.bt_dy.LinkKeyInfo[key].LinkKey));
	g_dy_cfg.bt_dy.LinkKeyInfo[key].usage_count = 0;
	g_dy_cfg.bt_dy.LinkKeyInfo[key].pom = pomSPP;
	DeleteLinkKey(g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR); // does a CFGMGR_flush( CFG_FLUSH_DYNAMIC )
}

/*
 * Make room for a new device.
 */
static void _BTMGR_make_room(void)
{
	int key;
	int count = 0;
	int lru_key = MAX_BT_REMOTES;
	uint32_t lru_count = MAX_UINT_32;
	
	for (key = 0; key < MAX_BT_REMOTES; ++key)
	{
	    if (!COMPARE_NULL_BD_ADDR(g_dy_cfg.bt_dy.LinkKeyInfo[key].BD_ADDR))
	    {
	    	count++;
	    	if (g_dy_cfg.bt_dy.LinkKeyInfo[key].usage_count < lru_count)
	    	{
	    		lru_count = g_dy_cfg.bt_dy.LinkKeyInfo[key].usage_count;
	    		lru_key = key;
	    	}
	    }
	}

	if (MAX_BT_REMOTES > count)
	{
		return;
	}
	
	BTMGR_delete_key(lru_key);
}

// private functions that should only be called by BTMGR
void BTMGR_PRIV_pairing_started(void)
{
	corona_print("pair %d\n", g_btmgr_state);

	_lwevent_set(&g_btmgr_lwevent, PAIRING_STARTED_EVENT);
}

void BTMGR_PRIV_pairing_complete(boolean succeeded)
{
	corona_print("pair %d state %d\n", succeeded, g_btmgr_state);
	
	if (succeeded)
	{
		_lwevent_set(&g_btmgr_lwevent, PAIRING_SUCCEEDED_EVENT);
	}
	else
	{
		_lwevent_set(&g_btmgr_lwevent, PAIRING_FAILED_EVENT);
	}
}

void BTMGR_PRIV_connecting_complete(boolean succeeded)
{
	corona_print("BTconn %d state %d\n", succeeded, g_btmgr_state);

	if (succeeded)
	{
		_lwevent_set(&g_btmgr_lwevent, CONNECTING_SUCCEEDED_EVENT);
	}
	else
	{
		_lwevent_set(&g_btmgr_lwevent, CONNECTING_FAILED_EVENT);
	}
}

void BTMGR_PRIV_ios_pairing_complete(void)
{
	corona_print("BT I pair done %d\n", g_btmgr_state);

	_lwevent_set(&g_btmgr_lwevent, IOS_PAIRING_SUCCEEDED_EVENT);
}

void BTMGR_PRIV_non_ios_pairing_complete(void)
{
	corona_print("BT A pair done %d\n", g_btmgr_state);

	_lwevent_set(&g_btmgr_lwevent, NON_IOS_PAIRING_SUCCEEDED_EVENT);
}


void BTMGR_PRIV_disconnected(void)
{
	corona_print("BT dis %d\n", g_btmgr_state);

	switch(g_btmgr_state)
	{
	case BTMGR_STATE_CLIENT_OPENING:
		_lwevent_set(&g_btmgr_lwevent, CONNECTING_FAILED_EVENT);
		break;
	case BTMGR_STATE_SERVER_PAIRED:
		_lwevent_set(&g_btmgr_lwevent, PAIRING_FAILED_EVENT);
		break;
	case BTMGR_STATE_CLIENT_OPEN:
		_lwevent_set(&g_btmgr_lwevent, DOWN_EVENT);
		break;
	default:
		break;
	}
	
	sys_event_post(SYS_EVENT_BT_DOWN);  // broadcast to the public
}

void BTMGR_force_disconnect()
{
	ForceDisconnect();
}

// public functions
err_t BTMGR_pair(void)
{
    int Result;
    _mqx_uint mask;
    _mqx_uint mqx_rc;
    err_t err = ERR_BT_ERROR;
    
    corona_print("BTpair %d\n", g_btmgr_state);
    
    switch(g_btmgr_state)
    {
    case BTMGR_STATE_UNINITIALIZED:
        wassert(0);
        break;
    case BTMGR_STATE_NOT_CONNECTED:
    	break;
    case BTMGR_STATE_CHECKING_PROXIMITY:
        corona_print("prox..\n");
    	return ERR_BT_BUSY;
    default:
    	BTMGR_disconnect();
    	break;
    }
    
    _watchdog_start(60*1000);
    
    /* Make sure there's room for a new one */
    _BTMGR_make_room();
    
	rf_switch(RF_SWITCH_BT, TRUE);
    
    /* First, attempt to set the Device to be Connectable.            */
    Result = SetConnect();
    
    /* Next, check to see if the Device was successfully made         */
    /* Connectable.                                                   */
    if(Result)
    {
        err = PROCESS_NINFO(ERR_BT_ERROR, "SetConn:%d", Result);
        goto DONE2;
    }
    
    /* Clear any stale events before evoking them */
    _lwevent_clear(&g_btmgr_lwevent, 0xffffffff);
    
    /* Now that the device is Connectable attempt to make it       */
    /* Discoverable.                                               */
    Result = SetDisc();
    
    /* Next, check to see if the Device was successfully made      */
    /* Discoverable.                                               */
    if(Result)
    {
    	err = PROCESS_NINFO(ERR_BT_PAIRING_FAILED, "\t%d", Result);
        SetConnectabilityMode(cmNonConnectableMode);
        goto DONE2;
    }
            
    /* Open a server on port 1 for pairing */
    g_btmgr_state = BTMGR_STATE_SERVER_OPENING;
    Result = OpenServer(1);
    
    if(0 != Result)
    {
    	err = PROCESS_NINFO(ERR_BT_PAIRING_FAILED, "\t%d", Result);
        SetConnectabilityMode(cmNonConnectableMode);
        SetDiscoverabilityMode(dmNonDiscoverableMode);
        g_btmgr_state = BTMGR_STATE_NOT_CONNECTED;
        goto DONE2;
    }
    
    g_btmgr_state = BTMGR_STATE_SERVER_PAIRING;
    
    _watchdog_stop();
            
    /* first wait for basic pairing to complete */
    mqx_rc = _lwevent_wait_ticks(&g_btmgr_lwevent,
    		PAIRING_STARTED_EVENT | PAIRING_FAILED_EVENT | IOS_PAIRING_SUCCEEDED_EVENT | NON_IOS_PAIRING_SUCCEEDED_EVENT, FALSE, 
    		g_st_cfg.bt_st.bt_pairing_timeout*BSP_ALARM_FREQUENCY);
        
    if (LWEVENT_WAIT_TIMEOUT == mqx_rc)
    {
    	err = PROCESS_NINFO(ERR_BT_PAIRING_FAILED, "pair TO");
    	goto DONE1;
    }

    mask = _lwevent_get_signalled();
                                           
    if (mask & PAIRING_FAILED_EVENT)
    {
    	err = PROCESS_NINFO(ERR_BT_PAIRING_FAILED, "pair %x", mask);
    	goto DONE1;
    }
    
	g_btmgr_state = BTMGR_STATE_SERVER_PAIRED;
	
    if ((mask & IOS_PAIRING_SUCCEEDED_EVENT) || (mask & NON_IOS_PAIRING_SUCCEEDED_EVENT))
    {
    	goto PAIRING_COMPLETE;
    }
	
    /* Clear any stale events before evoking them */
    _lwevent_clear(&g_btmgr_lwevent, 0xffffffff);
    
    /* now wait for high level post-pairing ops */
    mqx_rc = _lwevent_wait_ticks(&g_btmgr_lwevent,
    		IOS_PAIRING_SUCCEEDED_EVENT | NON_IOS_PAIRING_SUCCEEDED_EVENT | PAIRING_FAILED_EVENT, FALSE, 
    		1000);
    
    if (LWEVENT_WAIT_TIMEOUT == mqx_rc)
    {
    	err = PROCESS_NINFO(ERR_BT_PAIRING_FAILED, "pair T.O.");
    	goto DONE1;
    }

    mask = _lwevent_get_signalled();
        
PAIRING_COMPLETE:
    if ((mask & IOS_PAIRING_SUCCEEDED_EVENT) || (mask & NON_IOS_PAIRING_SUCCEEDED_EVENT))
    {
    	corona_print("Pair OK\n");
    	g_dy_cfg.bt_dy.mru_index = GetLastPairedIndex();
    	g_dy_cfg.bt_dy.LinkKeyInfo[g_dy_cfg.bt_dy.mru_index].usage_count = 0;
        CFGMGR_req_flush_at_shutdown();
        g_btmgr_state = BTMGR_STATE_SERVER_OPEN;
        
        _BTMGR_add_delete_pair_breadcrumb(g_dy_cfg.bt_dy.mru_index, TRUE); // log it after actually creating it
        err = ERR_OK;
        goto DONE1;
    }
    else
    {
    	err = PROCESS_NINFO(ERR_BT_PAIRING_FAILED, "%x", mask);
        goto DONE1;
    }

DONE1:
    BTMGR_disconnect();
        
DONE2:
    _watchdog_stop();
    return err;
}

static void _get_next_key(int *next_key)
{
    (*next_key)++;
    if (MAX_BT_REMOTES <= *next_key)
    {
    	*next_key = 0;
    }
}

// Get the next link key index from the list of known remotes.
// Set firstQuery true for the first query, false for subsequent.
// Returns index of key of next known remote or NULL if end of list reached.
err_t BTMGR_query_next_key(boolean firstQuery, int *key)
{
	static int next_key;
	
	// Start off with none
	*key = -1;
	
    /* Try to connect to the first device we find. */
	if (firstQuery)
	{
		next_key = g_dy_cfg.bt_dy.mru_index;
	}
	else
	{
		_get_next_key(&next_key);
		
        if (next_key == g_dy_cfg.bt_dy.mru_index)
        {
            return ERR_BT_NOT_CONNECTED;
        }
	}
	
    while (TRUE)
    {
        if (!COMPARE_NULL_BD_ADDR(g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR))
        {
        	*key = next_key;
        	return ERR_OK;
        }      

        _get_next_key(&next_key);
		
        if (next_key == g_dy_cfg.bt_dy.mru_index)
        {
            // back to where we started
            break;
        }
    }
    
    return ERR_BT_NOT_CONNECTED;
}

// Connect to BD_ADDR or next available device if BD_ADDR is NULL
err_t BTMGR_connect(BD_ADDR_t BD_ADDR, uint_32 page_timeout, LmMobileStatus *plmMobileStatus)
{
    err_t e_rc = ERR_OK;
    int rc;
    int next_key;
    boolean was_open = FALSE;
    boolean first_query = TRUE;
    static LmMobileStatus lastLmMobileStatus = LmMobileStatus_MOBILE_STATUS_NO_WAN;
    
    corona_print("ConBT %d:%d:%d:%d:%d:%d state %d\n", BD_ADDR.BD_ADDR0, BD_ADDR.BD_ADDR1, BD_ADDR.BD_ADDR2, BD_ADDR.BD_ADDR3, BD_ADDR.BD_ADDR4, BD_ADDR.BD_ADDR5, g_btmgr_state);

    *plmMobileStatus = lastLmMobileStatus; // in case we find we're already connected - usually this is overwritten
    
    switch(g_btmgr_state)
    {
    case BTMGR_STATE_UNINITIALIZED:
    	wassert(0);
        break;
    case BTMGR_STATE_CLIENT_OPEN:
    case BTMGR_STATE_SERVER_OPEN:
    	was_open = TRUE;
    	break;
    case BTMGR_STATE_NOT_CONNECTED:
    	break;
    case BTMGR_STATE_CHECKING_PROXIMITY:
        corona_print("prox..\n");
    	return ERR_BT_BUSY;
    default:
    	BTMGR_disconnect();
    	break;
    }
    
    BTMGR_PRIV_set_connect_timeout(page_timeout);
        
    /* handle case where a specific BD_ADDR is requested */
    if (!COMPARE_NULL_BD_ADDR(BD_ADDR))
    {
    	next_key = _BTMGR_get_key(BD_ADDR);
    	
    	if (-1 == next_key)
    	{
    		// don't know this BD_ADDR
    		// Leave g_btmgr_state alone and bail
            BTMGR_disconnect(); // we're not connected, but still need to clean up

    		return ERR_BT_BAD_BD_ADDR;
    	}
    	
        // already connected?
        if (was_open)
        {
            // connected to desired key?
            if (next_key == g_dy_cfg.bt_dy.mru_index)
            {
                return ERR_OK; // done. g_btmgr_state is already OPEN
            }
            
            // disconnect current before connecting new
            BTMGR_disconnect();
        }
        
        g_btmgr_state = BTMGR_STATE_CLIENT_OPENING;
                	
    	rc = _BTMGR_connect_key(next_key, plmMobileStatus); // this is blocking and a context switch could occur
    	
    	if (BTMGR_STATE_CLIENT_OPENING != g_btmgr_state)
    	{
    		// State changed out from under us. Likely a disconnect (e.g., user cancelled)
    		BTMGR_disconnect(); // we need to undo our connect, but not much else we can do except disconnect
    		return ERR_BT_NOT_CONNECTED;
    	}
    	
    	if (0 == rc)
    	{
        	lastLmMobileStatus = *plmMobileStatus; // save it if for next request if we're already connected
    		g_dy_cfg.bt_dy.mru_index = next_key;
        	g_dy_cfg.bt_dy.LinkKeyInfo[g_dy_cfg.bt_dy.mru_index].usage_count++;
            CFGMGR_req_flush_at_shutdown();
    	    g_btmgr_state = BTMGR_STATE_CLIENT_OPEN;
    	    return ERR_OK;
    	}
    	else
    	{
    		BTMGR_disconnect();
    		return ERR_BT_NOT_CONNECTED;
    	}
    }
    
    /* handle case where no specific BD_ADDR is requested. we have to find one */
    
    // already connected?
    if (was_open)
    {
        return ERR_OK; // done
    }

    // If we get here, we know we're disconnected.    
    // Cycle through all the known remotes, looking for one that works.    
    while (TRUE)
    {
	    g_btmgr_state = BTMGR_STATE_CLIENT_OPENING;

    	e_rc = BTMGR_query_next_key(first_query, &next_key);
        first_query = FALSE;
        
        if (ERR_OK != e_rc)
        {
            // no more configured remotes
            BTMGR_disconnect(); // we're not connected, but still need to clean up
            return ERR_BT_NOT_CONNECTED;
        }

        // Try to connect.
    	rc = _BTMGR_connect_key(next_key, plmMobileStatus); // this is blocking and a context switch could occur
    	
    	if (BTMGR_STATE_CLIENT_OPENING != g_btmgr_state)
    	{
    		// State changed out from under us. Likely a disconnect (e.g., user cancelled)
    		BTMGR_disconnect(); // we need to undo our connect, but not much else we can do except disconnect
    		return ERR_BT_NOT_CONNECTED;
    	}
    	
    	if (0 == rc)
    	{
        	lastLmMobileStatus = *plmMobileStatus; // save it if for next request if we're already connected

    		switch (*plmMobileStatus)
    		{
    		case LmMobileStatus_MOBILE_STATUS_OK:
    		case LmMobileStatus_MOBILE_STATUS_DEV_MGMT:
    			// good to go
        		g_dy_cfg.bt_dy.mru_index = next_key;
            	g_dy_cfg.bt_dy.LinkKeyInfo[g_dy_cfg.bt_dy.mru_index].usage_count++;
                CFGMGR_req_flush_at_shutdown();
        	    g_btmgr_state = BTMGR_STATE_CLIENT_OPEN;
        	    return ERR_OK;
        	    
    		case LmMobileStatus_MOBILE_STATUS_BUSY:
    		case LmMobileStatus_MOBILE_STATUS_NO_WAN:
    		default:
    	    	PROCESS_NINFO(ERR_BT_NOT_CONNECTED, "stat %d", *plmMobileStatus);
    			break;
    		}
    	}
    	
    	// Didn't work out. Disconnect and try another
    	BTMGR_disconnect();
    }
}

Port_Operating_Mode_t BTMGR_get_current_pom(void)
{
	return BTMGR_PRIV_get_current_pom();
}

// Test proximity of all known devices
err_t BTMGR_check_proximity(uint8_t *pNumPairedDevices, uint32_t max_devices, uint64_t *pMacAddrs)
{
    err_t e_rc = ERR_OK;
    int rc;
    int next_key;
    boolean first_query = TRUE;
    uint64_t time_elapsed, start_time;
    boolean found = FALSE;
    
    corona_print("prox %d\n", g_btmgr_state);
    
    switch(g_btmgr_state)
    {
        case BTMGR_STATE_UNINITIALIZED:
        	wassert(0);
            break;
        case BTMGR_STATE_NOT_CONNECTED:
        	break;
        default:
            return ERR_BT_BUSY;
    }
    	
	rf_switch(RF_SWITCH_BT, TRUE);
	
	//HCI_Write_Page_Scan_Activity()
    
    // If we get here, we know we're disconnected.    
    // Cycle through all the known remotes, looking for one that works.    
    while (TRUE)
    {
        _watchdog_start(60*1000);
    	e_rc = BTMGR_query_next_key(first_query, &next_key);
        
        if (ERR_OK != e_rc)
        {
            // no more configured remotes
            e_rc = first_query ? ERR_BT_NO_DEVICES : ERR_OK;
            goto DONE;
        }
        
        first_query = FALSE;

        // Try to connect.
        corona_print("Search %02x:%02x:%02x:%02x:%02x:%02x\n",
        		g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR.BD_ADDR5, g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR.BD_ADDR4, g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR.BD_ADDR3,
        		g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR.BD_ADDR2, g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR.BD_ADDR1, g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR.BD_ADDR0);
        
        RTC_get_ms(&start_time);
        
        BTMGR_PRIV_set_connect_timeout(pomiAP == g_dy_cfg.bt_dy.LinkKeyInfo[next_key].pom ? g_st_cfg.bt_st.bt_ios_prox_page_timeout : g_st_cfg.bt_st.bt_non_ios_prox_page_timeout);

        if (BTMGR_PRIV_test_connection(g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR))
        {
            RTC_get_ms(&time_elapsed);
            time_elapsed = time_elapsed - start_time;
            PROCESS_NINFO(STAT_BT_PROX_CONN, "%d", (uint32_t) time_elapsed);
        	corona_print("Found\n");
        	
        	found = TRUE;
        	
        	g_dy_cfg.bt_dy.LinkKeyInfo[next_key].usage_count++;
        	
        	if( (*pNumPairedDevices + 1) <= (uint8_t)max_devices)
        	{
        	    memset( &pMacAddrs[*pNumPairedDevices], 0, sizeof(uint64_t) );
        	    memcpy( &pMacAddrs[*pNumPairedDevices], &(g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR), sizeof(g_dy_cfg.bt_dy.LinkKeyInfo[next_key].BD_ADDR) );
        	    *pNumPairedDevices = *pNumPairedDevices + 1;
        	}
        }
        else
        {
        	corona_print("No\n");
        }
    }
    
DONE:
	if (found)
	{
	    CFGMGR_req_flush_at_shutdown();
	}
    _watchdog_stop();
	return e_rc;
}

err_t BTMGR_disconnect(void)
{
	BTMGR_state_e current_state = g_btmgr_state;
	err_t err = ERR_OK;
	
    corona_print("discon'g %d\n", g_btmgr_state);
    
	_watchdog_start(60*1000);
    
    switch(current_state)
    {
    case BTMGR_STATE_SERVER_OPENING:
    case BTMGR_STATE_SERVER_PAIRING:
    case BTMGR_STATE_SERVER_PAIRED:
    	BTMGR_PRIV_pairing_complete(FALSE);
    	// no break. drop through
    	
    case BTMGR_STATE_SERVER_OPEN:
        SetConnectabilityMode(cmNonConnectableMode);
        SetDiscoverabilityMode(dmNonDiscoverableMode);   

        // Set the event before issuing any disconnect commands to the stack so BTMGR_PRIV_disconnected
        // won't find the state still connected and so won't confuse clients that think the link is already down
        // by notifying them again.
        g_btmgr_state = BTMGR_STATE_NOT_CONNECTED;

        CloseServer();
    	break;
    	
    case BTMGR_STATE_CLIENT_OPENING:
    	BTMGR_PRIV_connecting_complete(FALSE);
    	// no break. drop through
    	
    case BTMGR_STATE_CLIENT_OPEN:
        SetConnectabilityMode(cmNonConnectableMode);
        SetDiscoverabilityMode(dmNonDiscoverableMode);   

        // Set the event before issuing any disconnect commands to the stack so BTMGR_PRIV_disconnected
        // won't find the state still connected and so won't confuse clients that think the link is already down
        // by notifying them again.
        g_btmgr_state = BTMGR_STATE_NOT_CONNECTED;

        CloseRemoteServer(); // Blocks until closed
        
        break;
    case BTMGR_STATE_NOT_CONNECTED:
        SetConnectabilityMode(cmNonConnectableMode);
        SetDiscoverabilityMode(dmNonDiscoverableMode);   
        
        corona_print("not con'd\n");
        
        // No break;
    case BTMGR_STATE_UNINITIALIZED:
    	err = ERR_BT_NOT_CONNECTED;
    	goto DONE;
    default:
    	wassert(0);
    }
        
    corona_print("discon'd\n");
	
    while (0 < g_btmgr_handle)
    {
    	g_btmgr_handle--;
        _lwsem_post(&g_btmgr_handle_sem);
    }
    
DONE:
	_watchdog_stop();
    return err;
}

// Create worker thread to host disconnect process
void BTMGR_disconnect_async(void)
{
    if (BTMGR_STATE_UNINITIALIZED != g_btmgr_state)
    {
    	whistle_task_create( TEMPLATE_IDX_BTMGR_DISC );
    }
}


err_t BTMGR_open(int *pHandle)
{
    corona_print("open %d\n", g_btmgr_state);

    switch(g_btmgr_state)
    {
    case BTMGR_STATE_SERVER_OPEN:
    case BTMGR_STATE_CLIENT_OPEN:
    	break;
    default:
    	return ERR_BT_NOT_CONNECTED;
    }

    // Try to decrement the sem
    // We don't really care about who gets which handle, nor who
    // closes which handle when. We just want to know when all the
    // the handles are closed.
    if (!_lwsem_poll(&g_btmgr_handle_sem))
	{
    	// Nope. No handles left
    	return ERR_BT_NO_HANDLES;
	}
    
	g_btmgr_handle++;
    *pHandle = g_btmgr_handle;
    
	return ERR_OK;
}

err_t BTMGR_close(int handle)
{
    switch(g_btmgr_state)
    {
    case BTMGR_STATE_SERVER_OPEN:
    case BTMGR_STATE_CLIENT_OPEN:
    	break;
    default:
    	return ERR_OK;
    }

    wassert(handle <= g_btmgr_handle);
    g_btmgr_handle--;
    _lwsem_post(&g_btmgr_handle_sem);

    return ERR_OK;
}

err_t BTMGR_send(int handle, uint_8 *data, uint_32 length, int_32 *pbytes_sent)
{
	int rc;
	
	/* none sent yet */
	*pbytes_sent = 0;
	
	if (-1 == handle)
	{
		/* Special case of us (BTMGR) calling */
		goto SEND;
	}
	
    if (handle != g_btmgr_handle)
    {
    	return ERR_BT_BAD_HANDLE;
    }
	
    switch(g_btmgr_state)
    {
    case BTMGR_STATE_SERVER_OPEN:
    case BTMGR_STATE_CLIENT_OPEN:
    	goto SEND;

    default:
        return ERR_BT_NOT_CONNECTED;    	
    }
    
SEND:
	rc = BTMGR_PRIV_send(data, length);
	if (0 != rc)
	{
		return PROCESS_NINFO(ERR_BT_SEND, "%d", rc);
	}
	
	*pbytes_sent = length;
	return ERR_OK;
}

err_t BTMGR_receive(int handle, void *buf, uint_32 buf_len, uint_32 ms, int_32 *pbytes_received, boolean continuous_aggregate)
{
	int rc;
	
    *pbytes_received = 0;
    
    if (handle != g_btmgr_handle)
    {
    	return ERR_BT_BAD_HANDLE;
    }
	
    switch(g_btmgr_state)
    {
    case BTMGR_STATE_SERVER_OPEN:
    case BTMGR_STATE_CLIENT_OPEN:
    	break;
    default:
        return ERR_BT_NOT_CONNECTED;    	
    }
    
    rc = BTMGR_PRIV_receive(buf, buf_len, ms, pbytes_received, continuous_aggregate);
#if BT_DDOBT_REBOOT_HACK
    if (0 != rc)
    {
    	PROCESS_NINFO(ERR_BT_RX_FAILURE, "%d %d", rc, *pbytes_received);
    	PWRMGR_Reboot(500, PV_REBOOT_REASON_BT_FAIL, FALSE);
    }
#endif
	
	return rc == 0 ? ERR_OK : ERR_BT_NOT_CONNECTED;
}

void BTMGR_zero_copy_free(void)
{
	BTMGR_PRIV_zero_copy_free();
}

void BTMGR_set_debug(boolean hci_debug_enable, boolean ssp_debug_enable)
{
	HCITR_Set_Debug(hci_debug_enable);
	BTMGR_PRIV_set_ssp_debug(ssp_debug_enable);
}

// Async disconnect
// temp worker to disconnect and break out of ongoing operations
void BTDISC_tsk(uint_32 dummy)
{
	_watchdog_start(60*1000);
	BTMGR_disconnect();
	_watchdog_stop();
}

void BTMGR_init(void)
{
    wmutex_init(&g_btmgr_ref_mutex);
}

void BTMGR_request(void)
{	
	_mutex_lock(&g_btmgr_ref_mutex);
	
	_watchdog_start(60*1000);
	
	corona_print("ref #%d\t%d\n", g_btmgr_ref_count, g_btmgr_state);
	
	if (0 == g_btmgr_ref_count)
	{
		wassert(BTMGR_STATE_UNINITIALIZED == g_btmgr_state);
		
		_BTMGR_init();
	}
		
	g_btmgr_ref_count++;
	
	_watchdog_stop();
	
	_mutex_unlock(&g_btmgr_ref_mutex);
}

void BTMGR_release(void)
{
	_mutex_lock(&g_btmgr_ref_mutex);
	
	_watchdog_start(60*1000);
	
	corona_print("release #%d\t%d\n", g_btmgr_ref_count, g_btmgr_state);
	
	wassert(0 < g_btmgr_ref_count);

	if (1 == g_btmgr_ref_count)
	{
		wassert(BTMGR_STATE_UNINITIALIZED != g_btmgr_state);
		
		_BTMGR_deinit();
	}
	
	g_btmgr_ref_count--;
	
	_watchdog_stop();
	
	_mutex_unlock(&g_btmgr_ref_mutex);
}

/*
 * Assume that BT is on unless the BTMGR is uninitialized 
 */
boolean BTMGR_bt_is_on(void)
{
    return g_btmgr_audit_bt_is_on;
}
