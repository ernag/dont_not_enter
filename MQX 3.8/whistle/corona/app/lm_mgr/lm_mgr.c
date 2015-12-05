/*
 * lm_mgr.c
 *
 *  Created on: Mar 30, 2013
 *      Author: Ernie
 */

#include <custom_stack_offload.h>
#include "cormem.h"
#include "wassert.h"
#include "ar4100p_main.h"
#include "wifi_mgr.h"
#include "lm_mgr.h"
#include "cfg_mgr.h"
#include "wmi.h"
#include "transport_mutex.h"
#include "wmps1.h"
#include "bt_mgr.h"
#include "whistlemessage.pb.h"
#include "pb_decode.h"
#include "pb_helper.h"

//#define DISPLAY_PASSWORDS
#define MAX_SCAN_BUF 248

typedef enum 
{
	LMMGR_STATE_INITIAL,
	LMMGR_STATE_STARTING,
	LMMGR_STATE_WIFI_SCANNING,
	LMMGR_STATE_DONE
} lmm_state_e;

typedef enum 
{
	LMMGR_EVENT_START,
	LMMGR_EVENT_WIFI_SCAN,
	LMMGR_EVENT_WIFI_TEST,
	LMMGR_EVENT_WIFI_ADD,
    LMMGR_EVENT_WIFI_LIST,
    LMMGR_EVENT_WIFI_REM,
	LMMGR_EVENT_BT_DELETE_KEY,
	LMMGR_EVENT_BT_RECEIVE_FAIL,
	LMMGR_EVENT_DISCONNECT,
	LMMGR_EVENT_LM_MOB_NO_WAN,
	LMMGR_EVENT_LM_MOB_OK,
	LMMGR_EVENT_LM_MOB_DEV_MGMT,
	LMMGR_EVENT_LM_MOB_BUSY
} lmm_event_e;

/*
 * Structs for sharing between LMMGR Receive and the statemachine
 */
typedef struct {
    char ssid[MAX_SSID_LENGTH]; // Already accounts for null terminator
    char password[MAX_PASSPHRASE_SIZE+1];
    security_parameters_t security_param;
} LMMBR_wifi_network_t;


/* * Static vars
 */

static lmm_state_e g_lmmgr_state;
static int g_lmmgr_bt_handle;
static wmps_seqnum_t g_lmmgr_reply_pb_seqnum = 0; // The mobile's seq num. We reply with this
static void* LMMGR_global_param = NULL;
const char *WMP_MSG_SZ_STR = "wmpMsgSz: %d\n";

static void _clear_lmmgr_global_param()
{
    if (NULL != LMMGR_global_param)
    {
        wfree(LMMGR_global_param);
        LMMGR_global_param = NULL;
    }
}

static pointer _LMMGR_malloc_global_param(size_t size)
{
    _clear_lmmgr_global_param();
    LMMGR_global_param = NULL;
    LMMGR_global_param = walloc(sizeof(LMMBR_wifi_network_t));
    return LMMGR_global_param;
}

static int LMMGR_BT_connect_and_open(void)
{
    err_t err;
	BD_ADDR_t ANY_ADDR;
	LmMobileStatus lmMobileStatus;
	
    ASSIGN_BD_ADDR(ANY_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); // NULL means any

	err = BTMGR_connect(ANY_ADDR, g_st_cfg.bt_st.bt_sync_page_timeout, &lmMobileStatus);
    if (ERR_OK != err)
    {
    	WPRINT_ERR("LM open %x", err);
    	return -1;
    }

    if (LmMobileStatus_MOBILE_STATUS_DEV_MGMT != lmMobileStatus)
    {
    	corona_print("LM bad stat %d\n", lmMobileStatus);
    	BTMGR_disconnect();
    	return -1;
    }

    err = BTMGR_open(&g_lmmgr_bt_handle);
    if (ERR_OK != err)
    {
        WPRINT_ERR("LM open %x", err);
    	BTMGR_disconnect();
    	return 2;
    }
    
    return 0;
}

static void LMMGR_close_and_disconnect(void)
{
	BTMGR_close(g_lmmgr_bt_handle);
	BTMGR_disconnect();
}

static int generate_scan_log(char *buffer, size_t n, ATH_GET_SCAN *scan_results)
{
    int current;
    int written = 0;
    int result;
    char ssid[33];
    
    /* Check the parameters */
    wassert(buffer);
    wassert(n >= 0);
    wassert(scan_results != 0);
    
    /* Loop through the scan results */
    for (current = 0; current < scan_results->num_entries; ++current)
    {
        /* Create null terminated string */
        memcpy(ssid, scan_results->scan_list[current].ssid, scan_results->scan_list[current].ssid_len);
        ssid[scan_results->scan_list[current].ssid_len] = 0;
        
        /* Add current entry to the buffer */
        result = snprintf(buffer, n, "%x:%d:%d ", str_checksum16(ssid), scan_results->scan_list[current].channel, scan_results->scan_list[current].rssi);
        wassert(result >= 0);
        if (result < n)
        {
            /* Update the buffer info */
            buffer += result;
            n -= result;
            written += result;
        }
        else
        {
            /* Ran out of room */
            written += n;
            break;
        }
    }
    
    /* Return the number of characters written */
    return written;
}

#define LMMGR_MAX_SCAN_RES_PER_MSG                  5
#define LMMGR_WIFI_SCAN_BUFF_STATIC_SIZE            10
#define LMMGR_WIFI_SCAN_BUFF_PER_NET_STATIC_SIZE    15
static int LMMGR_do_wifi_scan(void)
{
    err_t err;
    int rc = 1;
    uint_8 max_rssi;
    ATH_GET_SCAN ext_scan_list_quick = {0};
    ATH_GET_SCAN ext_scan_list_deep = {0};
    ATH_GET_SCAN local_ext_scan_list_quick = {0};
    ATH_GET_SCAN local_ext_scan_list_deep = {0};
    uint_8 *pBuf;
    size_t scan_results_index;
    int_32 bytes_sent = 0;
    char *log_buffer = 0;
    size_t log_pos = 0;
    
    LMMGR_close_and_disconnect();

    // quick scan
    err = WIFIMGR_ext_scan(&ext_scan_list_quick, FALSE);
    if (err != ERR_OK)
    {
        PROCESS_NINFO(err, NULL);
        rc = -1;
        goto DONE;
    }
    
    local_ext_scan_list_quick.num_entries = ext_scan_list_quick.num_entries;
    
    if (0 < local_ext_scan_list_quick.num_entries)
    {
        local_ext_scan_list_quick.scan_list = walloc(local_ext_scan_list_quick.num_entries * sizeof(ATH_SCAN_EXT));
    	memcpy(local_ext_scan_list_quick.scan_list, ext_scan_list_quick.scan_list, local_ext_scan_list_quick.num_entries * sizeof(ATH_SCAN_EXT));
    }

    // deep scan
    err = WIFIMGR_ext_scan(&ext_scan_list_deep, TRUE);
    if (err != ERR_OK)
    {
        PROCESS_NINFO(err, NULL);
        rc = -1;
        goto DONE;
    }
    
    local_ext_scan_list_deep.num_entries = ext_scan_list_deep.num_entries;
    
    if (0 < local_ext_scan_list_deep.num_entries)
    {
        local_ext_scan_list_deep.scan_list = walloc(local_ext_scan_list_deep.num_entries * sizeof(ATH_SCAN_EXT));
    	memcpy(local_ext_scan_list_deep.scan_list, ext_scan_list_deep.scan_list, local_ext_scan_list_deep.num_entries * sizeof(ATH_SCAN_EXT));
    }

    /* Log the scan results */
    log_buffer = walloc(MAX_SCAN_BUF);
    log_pos = generate_scan_log(log_buffer, MAX_SCAN_BUF, &local_ext_scan_list_quick);
    log_pos = generate_scan_log(log_buffer + log_pos, MAX_SCAN_BUF - log_pos, &local_ext_scan_list_deep);
    PROCESS_UINFO("wscanr %s", log_buffer);
    
    rc = LMMGR_BT_connect_and_open();
    
    if (0 != rc)
    {
        g_lmmgr_state = LMMGR_STATE_INITIAL;
        rc = -1;
        goto DONE;
    }
    
    scan_results_index = 0;
    
    while (scan_results_index < local_ext_scan_list_quick.num_entries)
    {
        int i;
        size_t scan_results_num_send = ( local_ext_scan_list_quick.num_entries - scan_results_index > LMMGR_MAX_SCAN_RES_PER_MSG ?
                                        LMMGR_MAX_SCAN_RES_PER_MSG : 
                                        local_ext_scan_list_quick.num_entries - scan_results_index);
        
                

        rc = WMP_lm_scan_result_gen(NULL, -1, &local_ext_scan_list_quick, scan_results_index, scan_results_num_send);
        corona_print("LM NULL scan sz %d\n", rc);

        pBuf = walloc(rc);

        rc = WMP_lm_scan_result_gen(pBuf, rc, &local_ext_scan_list_quick, scan_results_index, scan_results_num_send);
        corona_print("LM scan sz %d\n", rc);
        
        if (rc <= 0)
        {
            LMMGR_close_and_disconnect();
            g_lmmgr_state = LMMGR_STATE_INITIAL;
            wfree(pBuf);
            rc = -2;
            goto DONE;
        }
        
        // scan reply
        err = BTMGR_wmps_send(g_lmmgr_bt_handle, pBuf, rc, &bytes_sent, &g_lmmgr_reply_pb_seqnum, wmps_next_header_payload_type_wmp1); // his seq num
        if (ERR_OK != err)
        {
            PROCESS_NINFO(err, "%d", rc);
            LMMGR_close_and_disconnect();
            g_lmmgr_state = LMMGR_STATE_INITIAL;
            wfree(pBuf);
            rc = -2;
            goto DONE;
        }
        wfree(pBuf);
        
        scan_results_index += scan_results_num_send;
    }
    
    scan_results_index = 0;

    while (scan_results_index < local_ext_scan_list_deep.num_entries)
    {
        int i;
        size_t scan_results_num_send = ( local_ext_scan_list_deep.num_entries - scan_results_index > LMMGR_MAX_SCAN_RES_PER_MSG ?
                                        LMMGR_MAX_SCAN_RES_PER_MSG : 
                                        local_ext_scan_list_deep.num_entries - scan_results_index);
        
                

        rc = WMP_lm_scan_result_gen(NULL, -1, &local_ext_scan_list_deep, scan_results_index, scan_results_num_send);
        corona_print("LM NULL scan sz %d\n", rc);

        pBuf = walloc(rc);

        rc = WMP_lm_scan_result_gen(pBuf, rc, &local_ext_scan_list_deep, scan_results_index, scan_results_num_send);
        corona_print("LM scan sz %d\n", rc);
        
        if (rc <= 0)
        {
            LMMGR_close_and_disconnect();
            g_lmmgr_state = LMMGR_STATE_INITIAL;
            wfree(pBuf);
            rc = -2;
            goto DONE;
        }
        
        // scan reply
        err = BTMGR_wmps_send(g_lmmgr_bt_handle, pBuf, rc, &bytes_sent, &g_lmmgr_reply_pb_seqnum, wmps_next_header_payload_type_wmp1); // his seq num
        if (ERR_OK != err)
        {
            PROCESS_NINFO(err, "%d", rc);
            LMMGR_close_and_disconnect();
            g_lmmgr_state = LMMGR_STATE_INITIAL;
            wfree(pBuf);
            rc = -2;
            goto DONE;
        }
        wfree(pBuf);
        
        scan_results_index += scan_results_num_send;
    }
    
    /*
     * 
     * Now send empty list
     * 
     */
    
    /*
     * Calculate msg size and malloc
     */
    rc = WMP_lm_scan_result_gen(NULL, -1, NULL, 0, 0);
    
    corona_print("LM null scan %d\n", rc);
    pBuf = walloc(rc);

    /*
     * Generate msg
     */
    rc = WMP_lm_scan_result_gen(pBuf, rc, NULL, 0, 0);
    corona_print("LM scan %d\n", rc);
    
    if (rc <= 0)
    {
        LMMGR_close_and_disconnect();
        g_lmmgr_state = LMMGR_STATE_INITIAL;
        wfree(pBuf);
        rc = -2;
        goto DONE;
    }
    
    // scan reply
    err = BTMGR_wmps_send(g_lmmgr_bt_handle, pBuf, rc, &bytes_sent, &g_lmmgr_reply_pb_seqnum, wmps_next_header_payload_type_wmp1); // his seq num
    if (ERR_OK != err)
    {
        PROCESS_NINFO(err, NULL);
        LMMGR_close_and_disconnect();
        g_lmmgr_state = LMMGR_STATE_INITIAL;
        wfree(pBuf);
        rc = -2;
        goto DONE;
    }
    wfree(pBuf);

    g_lmmgr_state = LMMGR_STATE_WIFI_SCANNING;
    
    rc = 1;
    
DONE:
    if (local_ext_scan_list_quick.scan_list)
    {
    	 wfree(local_ext_scan_list_quick.scan_list);
    }

    if (local_ext_scan_list_deep.scan_list)
    {
    	 wfree(local_ext_scan_list_deep.scan_list);
    }
    
    if (log_buffer)
    {
        wfree(log_buffer);
    }

    return rc;
}

#define LMMGR_MAX_LIST_RES_PER_MSG 4
static int LMMGR_do_wifi_list(void)
{
    err_t err;
    int rc;
    uint_8 max_rssi;
    ATH_GET_SCAN ext_scan_list = {0};
    uint_8 *pBuf;
    size_t wifi_index;
    int_32 bytes_sent = 0;
    
    wifi_index = 0;
    
    
    while (wifi_index < g_dy_cfg.wifi_dy.num_networks)
    {
        int i;
        size_t list_results_num_send = ( g_dy_cfg.wifi_dy.num_networks - wifi_index > LMMGR_MAX_LIST_RES_PER_MSG ?
                                        LMMGR_MAX_LIST_RES_PER_MSG : 
                                        g_dy_cfg.wifi_dy.num_networks - wifi_index);
        
        /*
         * Calculate msg size and malloc
         */
        rc = WMP_lm_list_result_gen(NULL, -1, wifi_index, list_results_num_send);
        
        corona_print("LM NULL sz %d\n", rc);
        pBuf = walloc(rc);

        /*
         * Generate msg
         */
        rc = WMP_lm_list_result_gen(pBuf, rc, wifi_index, list_results_num_send);
        corona_print("LM scan %d\n", rc);
        
        if (rc <= 0)
        {
            LMMGR_close_and_disconnect();
            g_lmmgr_state = LMMGR_STATE_INITIAL;
            wfree(pBuf);
            return -2;
        }
        
        // scan reply
        err = BTMGR_wmps_send(g_lmmgr_bt_handle, pBuf, rc, &bytes_sent, &g_lmmgr_reply_pb_seqnum, wmps_next_header_payload_type_wmp1); // his seq num
        if (ERR_OK != err)
        {
            WPRINT_ERR("LM WIFI 1st %x", err);
            LMMGR_close_and_disconnect();
            g_lmmgr_state = LMMGR_STATE_INITIAL;
            wfree(pBuf);
            return -2;
        }
        wfree(pBuf);
        
        wifi_index += list_results_num_send;
    }
    
    /*
     * 
     * Now send empty list
     * 
     */
    
    /*
     * Calculate msg size and malloc
     */
    rc = WMP_lm_list_result_gen(NULL, -1, 0, 0);
    
    corona_print("LM null scan lst sz %d\n", rc);
    pBuf = walloc(rc);

    /*
     * Generate msg
     */
    rc = WMP_lm_list_result_gen(pBuf, rc, 0, 0);
    corona_print("LM scan lst sz %d\n", rc);
    
    if (rc <= 0)
    {
        LMMGR_close_and_disconnect();
        g_lmmgr_state = LMMGR_STATE_INITIAL;
        wfree(pBuf);
        return -2;
    }
    
    // scan reply
    err = BTMGR_wmps_send(g_lmmgr_bt_handle, pBuf, rc, &bytes_sent, &g_lmmgr_reply_pb_seqnum, wmps_next_header_payload_type_wmp1); // his seq num
    if (ERR_OK != err)
    {
        WPRINT_ERR("LM send WMP %x", err);
        LMMGR_close_and_disconnect();
        g_lmmgr_state = LMMGR_STATE_INITIAL;
        wfree(pBuf);
        return -2;
    }
    wfree(pBuf);

    g_lmmgr_state = LMMGR_STATE_DONE;
    return 1;
}

static int LMMGR_do_start(void)
{
    err_t err;
    int_32 bytes_sent = 0;
    uint8_t *pcheckin_msg = NULL;
    size_t msg_size;
    
    err = BTMGR_open(&g_lmmgr_bt_handle);
    if (ERR_OK != err)
    {
        WPRINT_ERR("LM do strt %x", err);
        BTMGR_disconnect();
        g_lmmgr_state = LMMGR_STATE_INITIAL;
        return -1;
    }

    msg_size = WMP_lm_dev_stat_notify_gen(NULL, -1, LmDevStatus_LM_DEV_STATUS_READY_FOR_MGMT);
    
    pcheckin_msg = walloc(msg_size);
    
    msg_size = WMP_lm_dev_stat_notify_gen(pcheckin_msg, msg_size, LmDevStatus_LM_DEV_STATUS_READY_FOR_MGMT);
    
    err = BTMGR_wmps_send(g_lmmgr_bt_handle, pcheckin_msg, msg_size, &bytes_sent, NULL, wmps_next_header_payload_type_wmp1); // our seq num
    
    wfree(pcheckin_msg);
       
	if (ERR_OK != err)
    {
	    WPRINT_ERR("LM send WMPS %x", err);
		LMMGR_close_and_disconnect();
		g_lmmgr_state = LMMGR_STATE_INITIAL;
    	return -2;
    }
	
	// There is no explicit reply to this msg, so just advance now
	BTMGR_wmps_increment_local_seqnum();
    
    g_lmmgr_state = LMMGR_STATE_STARTING;
    return 1;
}

static int LMMGR_do_wifi_test(void)
{
    err_t err;
    int rc;
    boolean succeeded = FALSE;
    uint8_t *pMsgBuf = NULL;
    size_t wmpMsgSize;
    int_32 bytes_sent = 0;
    
    LMMBR_wifi_network_t* network = (LMMBR_wifi_network_t*) LMMGR_global_param;

    LMMGR_close_and_disconnect();

	rc = LMMGR_test_network(network->ssid, &(network->security_param));

	if (0 == rc)
	{
		succeeded = TRUE;
	}

	LMMGR_disconnect();

	rc = LMMGR_BT_connect_and_open();

	if (0 != rc)
	{
		g_lmmgr_state = LMMGR_STATE_INITIAL;
		return -1;
	}

    /*
     * Determine resp msg size
     */
    wmpMsgSize = WMP_lm_test_result_gen(NULL, 
            -1, 
            (succeeded ? LmWiFiTestStatus_LM_WIFI_TEST_PASSED : LmWiFiTestStatus_LM_WIFI_TEST_FAILED),
            network->ssid,
            &(network->security_param));

    corona_print(WMP_MSG_SZ_STR, wmpMsgSize);
	
    /*
     * Allocate buffers for resp msg and generate msg
     */
	pMsgBuf = walloc(wmpMsgSize);
    
    wmpMsgSize = WMP_lm_test_result_gen(pMsgBuf, 
            wmpMsgSize, 
            (succeeded ? LmWiFiTestStatus_LM_WIFI_TEST_PASSED : LmWiFiTestStatus_LM_WIFI_TEST_FAILED),
            network->ssid,
            &(network->security_param));
    
    corona_print(WMP_MSG_SZ_STR, wmpMsgSize);
    
    /*
     * Send resp msg
     */
    if (wmpMsgSize > 0)
    {
        err = BTMGR_wmps_send(g_lmmgr_bt_handle, pMsgBuf, wmpMsgSize, &bytes_sent, &g_lmmgr_reply_pb_seqnum, wmps_next_header_payload_type_wmp1); // his seq num
    }
    else
    {
        err = -1;
    }

    wfree(pMsgBuf);
    
	if (ERR_OK != err)
	{
		PROCESS_NINFO(err, NULL);
		LMMGR_close_and_disconnect();
		g_lmmgr_state = LMMGR_STATE_INITIAL;
		return -2;
	}

	g_lmmgr_state = LMMGR_STATE_DONE;
	return 1;
}

static int LMMGR_do_wifi_rem(void)
{
    err_t err;
    int rc;
    boolean succeeded = FALSE;
    uint8_t *pMsgBuf = NULL;
    size_t wmpMsgSize;
    int_32 bytes_sent = 0;
    
    LMMBR_wifi_network_t* network = (LMMBR_wifi_network_t*) LMMGR_global_param;

    /*
     * Add network to config
     */
    rc = WIFIMGR_remove(network->ssid);

    if (0 == rc)
    {
        succeeded = TRUE;
    }

    /*
     * Determine and allocate response buffer
     */
    
    wmpMsgSize = WMP_lm_rem_resp_gen(NULL, -1, 
            (succeeded == TRUE ? LmWiFiRemStatus_LM_WIFI_REM_STATUS_SUCCESS : LmWiFiRemStatus_LM_WIFI_REM_FAILED),
            network->ssid);
    
    corona_print(WMP_MSG_SZ_STR, wmpMsgSize);
    pMsgBuf = walloc(wmpMsgSize);

    /*
     * Generate Response
     */
      
    wmpMsgSize = WMP_lm_rem_resp_gen(pMsgBuf, wmpMsgSize, 
            (succeeded == TRUE ? LmWiFiRemStatus_LM_WIFI_REM_STATUS_SUCCESS : LmWiFiRemStatus_LM_WIFI_REM_FAILED),
            network->ssid);
    
    corona_print(WMP_MSG_SZ_STR, wmpMsgSize);
    
    /*
     * Send Response
     */
    if (wmpMsgSize > 0)
    {
        err = BTMGR_wmps_send(g_lmmgr_bt_handle, pMsgBuf, wmpMsgSize, &bytes_sent, &g_lmmgr_reply_pb_seqnum, wmps_next_header_payload_type_wmp1); // his seq num
    }
    else
    {
        err = -1;
    }

    wfree(pMsgBuf);
    
    if (ERR_OK != err)
    {
        PROCESS_NINFO(err, NULL);
        LMMGR_close_and_disconnect();
        g_lmmgr_state = LMMGR_STATE_INITIAL;
        return -2;
    }

    g_lmmgr_state = LMMGR_STATE_DONE;
    return 1;
}

static int LMMGR_do_wifi_add(void)
{
    err_t err;
    int rc;
    boolean succeeded = FALSE;
    uint8_t *pMsgBuf = NULL;
    size_t wmpMsgSize;
    int_32 bytes_sent = 0;
    
    LMMBR_wifi_network_t* network = (LMMBR_wifi_network_t*) LMMGR_global_param;

    /*
     * Add network to config
     */
    rc = LMMGR_set_security(network->ssid, FALSE, &(network->security_param));

    if (0 == rc)
    {
        succeeded = TRUE;
    }

    /*
     * Determine and allocate response buffer
     */
    
    wmpMsgSize = WMP_lm_add_resp_gen(NULL, -1, 
            (succeeded == TRUE ? LmWiFiAddStatus_LM_WIFI_ADD_STATUS_SUCCESS : LmWiFiAddStatus_LM_WIFI_ADD_FAILED),
            network->ssid,
            &(network->security_param));
    
    corona_print(WMP_MSG_SZ_STR, wmpMsgSize);
    pMsgBuf = walloc(wmpMsgSize);

    /*
     * Generate Response
     */
      
    wmpMsgSize = WMP_lm_add_resp_gen(pMsgBuf, wmpMsgSize, 
            (succeeded == TRUE ? LmWiFiAddStatus_LM_WIFI_ADD_STATUS_SUCCESS : LmWiFiAddStatus_LM_WIFI_ADD_FAILED),
            network->ssid,
            &(network->security_param));
    
    corona_print(WMP_MSG_SZ_STR, wmpMsgSize);
    
    /*
     * Send Response
     */
    if (wmpMsgSize > 0)
    {
        err = BTMGR_wmps_send(g_lmmgr_bt_handle, pMsgBuf, wmpMsgSize, &bytes_sent, &g_lmmgr_reply_pb_seqnum, wmps_next_header_payload_type_wmp1); // his seq num
    }
    else
    {
        err = -1;
    }

    wfree(pMsgBuf);
    
    if (ERR_OK != err)
    {
        PROCESS_NINFO(err, NULL);
        LMMGR_close_and_disconnect();
        g_lmmgr_state = LMMGR_STATE_INITIAL;
        return -2;
    }

    g_lmmgr_state = LMMGR_STATE_DONE;
    return 1;
}

/*********************************************************************************************************
 *********************************************************************************************************
 * 
 * LMMGR Receive handlers
 * 
 * 
 */

static err_t _LMMGR_receive_mob_stat_notify(uint_8 *pLmMsgBuf, size_t lmMsgSize, lmm_event_e *event)
{
    LmMobileStat *pMobileStat = NULL;
    pb_istream_t in_stream;

    pMobileStat = walloc(sizeof(*pMobileStat));

    in_stream = pb_istream_from_buffer(pLmMsgBuf, lmMsgSize);
    if (!pb_decode(&in_stream, LmMobileStat_fields, pMobileStat))
    {
        PROCESS_NINFO(ERR_GENERIC, NULL);
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pMobileStat);
        return ERR_GENERIC;
    }
    
    if (!pMobileStat->has_status)
    {
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pMobileStat);
        return PROCESS_NINFO(ERR_GENERIC, "mis'g stat");
    }
    
    switch (pMobileStat->status)
    {
        case LmMobileStatus_MOBILE_STATUS_OK:
            *event = LMMGR_EVENT_LM_MOB_OK;
            break;
            
        case LmMobileStatus_MOBILE_STATUS_NO_WAN:
            *event = LMMGR_EVENT_LM_MOB_NO_WAN;
            break;
            
        case LmMobileStatus_MOBILE_STATUS_DEV_MGMT:
            *event = LMMGR_EVENT_LM_MOB_DEV_MGMT;
            break;
            
        case LmMobileStatus_MOBILE_STATUS_BUSY:
            *event = LMMGR_EVENT_LM_MOB_BUSY;
            break;
            
        default:
            WPRINT_ERR("mob stat");
            *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
            break;
    }
    wfree(pMobileStat);
    return ERR_OK;
}

static err_t _LMMGR_receive_wifi_test_req(uint_8 *pLmMsgBuf, size_t lmMsgSize, lmm_event_e *event)
{
    LmWiFiTestRequest *pTestRequest = NULL;
    LMMBR_wifi_network_t* statemachine_network = NULL;
    WMP_sized_string_cb_args_t ssid_param;
    WMP_sized_string_cb_args_t passwd_param;
    pb_istream_t in_stream = {0};

    pTestRequest = walloc(sizeof(LmWiFiTestRequest));
    
    statemachine_network = _LMMGR_malloc_global_param(sizeof(*statemachine_network));

    ssid_param.pPayload = statemachine_network->ssid;
    ssid_param.payloadMaxSize = sizeof(statemachine_network->ssid);
    pTestRequest->network.ssid.arg = &ssid_param;
    pTestRequest->network.ssid.funcs.decode = decode_whistle_str_sized; // Assumes that device won't send an SSID above 32 bytes.

    passwd_param.pPayload = statemachine_network->password;
    passwd_param.payloadMaxSize = sizeof(statemachine_network->password);
    pTestRequest->network.password.arg = &passwd_param;
    pTestRequest->network.password.funcs.decode = decode_whistle_str_sized; // Assumes that device won't send an password above 64 bytes.
    
    in_stream = pb_istream_from_buffer(pLmMsgBuf, lmMsgSize);
    if (!pb_decode(&in_stream, LmWiFiTestRequest_fields, pTestRequest))
    {
        PROCESS_NINFO(ERR_GENERIC, NULL);
        _clear_lmmgr_global_param();
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pTestRequest);
        return ERR_GENERIC;
    }
    if (!pTestRequest->has_network)
    {
        PROCESS_NINFO(ERR_GENERIC, "mis'g ntwk");
        _clear_lmmgr_global_param();
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pTestRequest);
        return ERR_GENERIC;
    }
    if (! ((WMP_sized_string_cb_args_t*)(pTestRequest->network.ssid.arg))->exists)
    {
        PROCESS_NINFO(ERR_GENERIC, "mis'g ssid");
        _clear_lmmgr_global_param();
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pTestRequest);
        return ERR_GENERIC;
    }

    WIFIMGR_lmm_network_encode_w_sized_strings_to_security_param(&(pTestRequest->network), &(statemachine_network->security_param));
    wfree(pTestRequest);

    *event = LMMGR_EVENT_WIFI_TEST;
    return ERR_OK;
}

static err_t _LMMGR_receive_wifi_rem_req(uint_8 *pLmMsgBuf, size_t lmMsgSize, lmm_event_e *event)
{
    LmWiFiRemRequest *pRemRequest = NULL;
    LMMBR_wifi_network_t* statemachine_network;
    WMP_sized_string_cb_args_t ssid_param;
    pb_istream_t in_stream = {0};

    pRemRequest = walloc(sizeof(*pRemRequest));
    
    statemachine_network = _LMMGR_malloc_global_param(sizeof(*statemachine_network));

    ssid_param.pPayload = statemachine_network->ssid;
    ssid_param.payloadMaxSize = sizeof(statemachine_network->ssid);
    pRemRequest->ssid.arg = &ssid_param;
    pRemRequest->ssid.funcs.decode = decode_whistle_str_sized; // Assumes that device won't send an SSID above 32 bytes.

    in_stream = pb_istream_from_buffer(pLmMsgBuf, lmMsgSize);
    if (!pb_decode(&in_stream, LmWiFiRemRequest_fields, pRemRequest))
    {
        PROCESS_NINFO(ERR_GENERIC, NULL);
        _clear_lmmgr_global_param();
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pRemRequest);
        return ERR_GENERIC;
    }
    if (! ((WMP_sized_string_cb_args_t*)(pRemRequest->ssid.arg))->exists)
    {
        PROCESS_NINFO(ERR_GENERIC, NULL);
        _clear_lmmgr_global_param();
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pRemRequest);
        return ERR_GENERIC;
    }

    wfree(pRemRequest);
    *event = LMMGR_EVENT_WIFI_REM;
    return ERR_OK;
}

static err_t _LMMGR_receive_wifi_add_req(uint_8 *pLmMsgBuf, size_t lmMsgSize, lmm_event_e *event)
{
    LmWiFiAddRequest *pAddRequest = NULL;
    LMMBR_wifi_network_t* statemachine_network;
    WMP_sized_string_cb_args_t ssid_param;
    WMP_sized_string_cb_args_t passwd_param;
    pb_istream_t in_stream = {0};

    pAddRequest = walloc(sizeof(*pAddRequest));
    
    statemachine_network = _LMMGR_malloc_global_param(sizeof(*statemachine_network));

    ssid_param.pPayload = statemachine_network->ssid;
    ssid_param.payloadMaxSize = sizeof(statemachine_network->ssid);
    pAddRequest->network.ssid.arg = &ssid_param;
    pAddRequest->network.ssid.funcs.decode = decode_whistle_str_sized; // Assumes that device won't send an SSID above 32 bytes.

    passwd_param.pPayload = statemachine_network->password;
    passwd_param.payloadMaxSize = sizeof(statemachine_network->password);
    pAddRequest->network.password.arg = &passwd_param;
    pAddRequest->network.password.funcs.decode = decode_whistle_str_sized; // Assumes that device won't send an password above 64 bytes.
    
    in_stream = pb_istream_from_buffer(pLmMsgBuf, lmMsgSize);
    if (!pb_decode(&in_stream, LmWiFiAddRequest_fields, pAddRequest))
    {
        PROCESS_NINFO(ERR_GENERIC, NULL);
        _clear_lmmgr_global_param();
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pAddRequest);
        return ERR_GENERIC;
    }
    if (!pAddRequest->has_network)
    {
        PROCESS_NINFO(ERR_GENERIC, NULL);
        _clear_lmmgr_global_param();
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pAddRequest);
        return ERR_GENERIC;
    }
    if (! ((WMP_sized_string_cb_args_t*)(pAddRequest->network.ssid.arg))->exists)
    {
        PROCESS_NINFO(ERR_GENERIC, NULL);
        _clear_lmmgr_global_param();
        *event = LMMGR_EVENT_BT_RECEIVE_FAIL;
        wfree(pAddRequest);
        return ERR_GENERIC;
    }

    WIFIMGR_lmm_network_encode_w_sized_strings_to_security_param(&(pAddRequest->network), &(statemachine_network->security_param));
    wfree(pAddRequest);
    *event = LMMGR_EVENT_WIFI_ADD;
    return ERR_OK;
}

/*********************************************************************************************************
 *********************************************************************************************************
 * 
 * State Machine Functions
 * 
 * 
 */
static int LMMGR_do_disconnect(void)
{
	LMMGR_close_and_disconnect();
	g_lmmgr_state = LMMGR_STATE_INITIAL;
	return -1;
}

static int LMMGR_do_lm_done(void)
{
	BTMGR_close(g_lmmgr_bt_handle); // don't disconnect
	g_lmmgr_state = LMMGR_STATE_INITIAL;
	return 0;
}

static int LMMGR_state_machine(lmm_event_e event)
{
    err_t err;
    int rc;
    int_32 bytes_sent = 0;
    uint_8 max_rssi;
    lmm_state_e old_state = g_lmmgr_state;
            
	switch(g_lmmgr_state)
	{
	case LMMGR_STATE_INITIAL:
		switch(event)
		{
		case LMMGR_EVENT_START:
			rc = LMMGR_do_start();
			break;
		default:
			LMMGR_close_and_disconnect();
			g_lmmgr_state = LMMGR_STATE_INITIAL;
			rc = -1;
			break;
		}
		break;
		
	case LMMGR_STATE_STARTING:
	case LMMGR_STATE_WIFI_SCANNING:
	case LMMGR_STATE_DONE:
		switch(event)
		{
		case LMMGR_EVENT_START:
			wassert(0);
		case LMMGR_EVENT_WIFI_SCAN:
			rc = LMMGR_do_wifi_scan();
			break;
		case LMMGR_EVENT_WIFI_TEST:
			rc = LMMGR_do_wifi_test();
			break;
        case LMMGR_EVENT_WIFI_ADD:
            rc = LMMGR_do_wifi_add();
            break;
        case LMMGR_EVENT_WIFI_REM:
            rc = LMMGR_do_wifi_rem();
            break;
        case LMMGR_EVENT_WIFI_LIST:
            rc = LMMGR_do_wifi_list();
            break;
		case LMMGR_EVENT_DISCONNECT:
			rc = LMMGR_do_disconnect();
			break;
		case LMMGR_EVENT_LM_MOB_OK: // If mobile reports status OK that means it's done.
			rc = LMMGR_do_lm_done();
			break;
        case LMMGR_EVENT_LM_MOB_DEV_MGMT: // If mobile reports its still in the same state stay put
            rc = 1;
            break;
        case LMMGR_EVENT_LM_MOB_BUSY: // If mobile reports its busy we should quit
            rc = LMMGR_do_lm_done();
            break;
		case LMMGR_EVENT_BT_RECEIVE_FAIL:
		case LMMGR_EVENT_LM_MOB_NO_WAN:
		default:
			LMMGR_close_and_disconnect();
			g_lmmgr_state = LMMGR_STATE_INITIAL;
			rc = -1;
			break;
		}
		break;
	}
	
    _clear_lmmgr_global_param();
    
	corona_print("LM %d->%d E %d rc %d\n", old_state, g_lmmgr_state, event, rc);

	return rc;
}

static lmm_event_e LMMGR_wait_event(void)
{
    err_t err;
    int_32 received;
    uint_8 *response = NULL;
    lmm_event_e event;
    LmMessageType lmMsgType;
    uint_8 *pLmMsgBuf;
    size_t lmMsgSize;
    
    response = walloc(WMPS_MAX_PB_SERIALIZED_MSG_SIZE);
    wassert(response);
    
    err = BTMGR_wmps_receive(g_lmmgr_bt_handle, response, WMPS_MAX_PB_SERIALIZED_MSG_SIZE, 10*60*1000, &received, &g_lmmgr_reply_pb_seqnum);

    lmMsgType = WMP_lm_receive_deferred_generic(response, (size_t)received, &pLmMsgBuf, &lmMsgSize);
    
    switch (lmMsgType)
    {
        case LmMessageType_LM_MOBILE_STAT_NOTIFY:
            corona_print("LM MOB STAT NOT\n");
            _LMMGR_receive_mob_stat_notify(pLmMsgBuf, lmMsgSize, &event);
            break;
        case LmMessageType_LM_WIFI_SCAN_REQ:
            corona_print("LM SCAN\n");
            event = LMMGR_EVENT_WIFI_SCAN;
            break;
        case LmMessageType_LM_WIFI_TEST_REQ:
            corona_print("LM TEST\n");
            _LMMGR_receive_wifi_test_req(pLmMsgBuf, lmMsgSize, &event);
            break;
        case LmMessageType_LM_WIFI_ADD_REQ:
            corona_print("LM ADD\n");
            _LMMGR_receive_wifi_add_req(pLmMsgBuf, lmMsgSize, &event);
            break;
        case LmMessageType_LM_WIFI_LIST_REQ:
            corona_print("LM LST\n");
            event = LMMGR_EVENT_WIFI_LIST;
            goto LMMGR_wait_event_DONE;
        case LmMessageType_LM_WIFI_REM_REQ:
            WPRINT_ERR("LM rx hdl");  // not yet implemented
            _LMMGR_receive_wifi_rem_req(pLmMsgBuf, lmMsgSize, &event);
            goto LMMGR_wait_event_DONE;
        default:
            PROCESS_NINFO(ERR_BT_ERROR, "bad LM %d", lmMsgType);
            event = LMMGR_EVENT_BT_RECEIVE_FAIL;
            goto LMMGR_wait_event_DONE;
    }
	
LMMGR_wait_event_DONE:
	if (NULL != response)
	{
		wfree(response);
	}

	return event;
}

/* Main processing routine for running an LMM session with app. BT is already open.
 * return 0 if no errors and LMM required (BT will be closed, but connected)
 * return negative value if error (BT will be closed and disconnected)
 */
int LMMGR_process_main(void)
{
    err_t err;
    int_32 received;
    uint_8 *response;
    lmm_event_e event;
    int rc;
    
    event = LMMGR_EVENT_START;
    
    while (TRUE)
    {
    	
    	rc = LMMGR_state_machine(event);
    	if (rc < 0)
    	{
    		// Failed
    		return -1;
    	}
    	else if (0 == rc)
    	{
    		// Completed successfully
    		return 0;
    	}
    	
    	event = LMMGR_wait_event();
    } 
}    
    

/*
 *   Connect to network with given SSID.
 */
int LMMGR_test_network(char *ssid, security_parameters_t *security_parameters)
{
    err_t e_rc;
    
    switch (security_parameters->type)
    {
    case SEC_MODE_OPEN:
    	corona_print("OPEN\n");
        PROCESS_UINFO("OPEN");
    	break;
    case SEC_MODE_WEP:
    	if (1 > security_parameters->parameters.wep_parameters.default_key_index ||
    			MAX_NUM_WEP_KEYS < security_parameters->parameters.wep_parameters.default_key_index)
    	{
    		return PROCESS_NINFO(ERR_WIFI_SECURITY_ERROR, "idx %d", security_parameters->parameters.wep_parameters.default_key_index);
    	}
    	
    	corona_print("WEP idx %d %s\n", security_parameters->parameters.wep_parameters.default_key_index,
    			security_parameters->parameters.wep_parameters.key[security_parameters->parameters.wep_parameters.default_key_index - 1]);
#ifdef DISPLAY_PASSWORDS
        PROCESS_UINFO("WEP idx %d %s", security_parameters->parameters.wep_parameters.default_key_index,
        		security_parameters->parameters.wep_parameters.key[security_parameters->parameters.wep_parameters.default_key_index - 1]);
#else
        PROCESS_UINFO("WEP idx %d", security_parameters->parameters.wep_parameters.default_key_index);
#endif
        break;
    case SEC_MODE_WPA:
        corona_print("WPA idx %d %d %s %s\n", security_parameters->parameters.wpa_parameters.mcipher,
        		security_parameters->parameters.wpa_parameters.ucipher, security_parameters->parameters.wpa_parameters.version,
        		security_parameters->parameters.wpa_parameters.passphrase);
#ifdef DISPLAY_PASSWORDS
        PROCESS_UINFO("WPA idx %d %d %s %s", security_parameters->parameters.wpa_parameters.mcipher,
        		security_parameters->parameters.wpa_parameters.ucipher, security_parameters->parameters.wpa_parameters.version,
        		security_parameters->parameters.wpa_parameters.passphrase);
#else
        PROCESS_UINFO("WPA idx %d %d %s", security_parameters->parameters.wpa_parameters.mcipher,
        		security_parameters->parameters.wpa_parameters.ucipher, security_parameters->parameters.wpa_parameters.version);
#endif
        break;
    default:
		return PROCESS_NINFO(ERR_WIFI_SECURITY_ERROR, "type %d", security_parameters->type);
    }
    
    e_rc = WIFIMGR_connect(ssid, security_parameters, TRUE);
    
    return ERR_OK == e_rc ? 0 : -1;
}

/*
 *   Disconnect network.
 */
int LMMGR_disconnect(void)
{
    err_t e_rc;
    
    e_rc = WIFIMGR_disconnect();
    
    return ERR_OK == e_rc ? 0 : -1;
}

/*
 *   Set security for given SSID.
 *   
 *   TODO - There's some duplication between this function and the printscan routine.
 *          Definitely room for code optimization/organization here.
 */
int LMMGR_set_security(char *ssid, boolean doScan, security_parameters_t *psecurity_parameters)
{
    ATH_GET_SCAN          ext_scan_list;
    A_UINT8               temp_ssid[MAX_SSID_LENGTH];
    security_parameters_t *pgot_security_parameters;
    security_parameters_t security_parameters;
    char                  *password;
    CRYPTO_TYPE           mcipher = NONE_CRYPT;
    uint_32               ucipher = 0;
    int                   w_rc;
    err_t                 err;
    int                   i;
    int                   type;
    uint_8                wpa2 = 0;
    int                   j;
    
    if (!doScan)
    {
        // passed in
        pgot_security_parameters = psecurity_parameters;
        goto wcomgr_got_security;        
    }
    
    pgot_security_parameters = &security_parameters;
    
    for (j = 0; j < 2; j++)
    {
    	err = WIFIMGR_ext_scan(&ext_scan_list, j == 0 ? TRUE : FALSE);
    	if (ERR_OK != err)
    	{
    		PROCESS_NINFO(err, "%s", ssid);
    		return -1;
    	}

    	for (i = 0; i < ext_scan_list.num_entries; i++)
    	{
    		A_UINT8 j = MAX_RSSI_TABLE_SZ - 1;

    		memcpy(temp_ssid, ext_scan_list.scan_list[i].ssid, ext_scan_list.scan_list[i].ssid_len);

    		temp_ssid[ext_scan_list.scan_list[i].ssid_len] = '\0';

    		if (ext_scan_list.scan_list[i].ssid_len == 0)
    		{
    			corona_print("\nssid Unvail\n");
    		}
    		else
    		{
    			//corona_print("\nssid: %s\n", temp_ssid);
    			if (0 == strcmp((char *)temp_ssid, ssid))
    			{  
    				//corona_print("Found SSID Match!\n");
    				goto wcomgr_set_security;
    			}
    		}
    	}
    }
    
    PROCESS_NINFO(ERR_WIFI_BAD_NETWORK, "%s", ssid);
    return -1;

wcomgr_set_security:

    WIFIMGR_ATH_GET_SCAN_to_security_param(&(ext_scan_list.scan_list[i]),
            psecurity_parameters->parameters.wpa_parameters.passphrase,
            psecurity_parameters->parameters.wep_parameters.key[0],
            &security_parameters);
    
wcomgr_got_security:
    err = WIFIMGR_set_security(ssid, pgot_security_parameters, WIFIMGR_SECURITY_FLUSH_NOW);
    if (ERR_OK != err)
    {
        PROCESS_NINFO(err, "%s", ssid);
        return -2;
    }
    corona_print("SSID: %s OK\n", ssid);
    
#if TEST_WIFICFG_CONNECTION_WORKS
    w_rc = WIFIMGR_connect(ssid);
    if (0 != w_rc)
    {
        corona_print("FAIL %d\n", w_rc);
    }
#endif
    
    return 0;
}


/*
 *   Scan for all available networks and print them to console.
 *     Return A_OK for success, error code otherwise.
 *     pMaxRSSI:  set to max found RSSI.
 *     consider_only_configured:  TRUE to print only saved SSID's, FALSE for ALL.
 */
int LMMGR_printscan(uint8_t *pMaxRSSI, uint8_t consider_only_configured)
{
    ATH_GET_SCAN          ext_scan_list;
    A_UINT8               temp_ssid[MAX_SSID_LENGTH];
    int                   w_rc;
    err_t                 err;
    int                   i, j;
    
    *pMaxRSSI = 0;
    
    for (j = 0; j < 2; j++)
    {
    	err = WIFIMGR_ext_scan(&ext_scan_list, j == 0 ? TRUE : FALSE);
    	if (ERR_OK != err)
    	{
    	    WPRINT_ERR("extScan %x", err);
    		return -1;
    	}

    	for (i = 0; i < ext_scan_list.num_entries; i++)
    	{
    		A_UINT8 j = MAX_RSSI_TABLE_SZ - 1;

    		memcpy(temp_ssid, ext_scan_list.scan_list[i].ssid, ext_scan_list.scan_list[i].ssid_len);

    		temp_ssid[ext_scan_list.scan_list[i].ssid_len] = '\0';

    		if( consider_only_configured )
    		{
    			if( !WIFIMGR_is_configured( (char *)temp_ssid ) )
    			{
    				continue; // skip this SSID if it is not configured.
    			}
    		}

    		if (ext_scan_list.scan_list[i].ssid_len == 0)
    		{
    			corona_print("\nssid Unavail\n");
    		}
    		else
    		{
    			corona_print("\nssid %s\n", temp_ssid);
    		}

    		corona_print("bssid %x:%x:%x:%x:%x:%x\nchan %d\nrssi %d\n", 
    		        ext_scan_list.scan_list[i].bssid[0], 
    		        ext_scan_list.scan_list[i].bssid[1], 
    		        ext_scan_list.scan_list[i].bssid[2], 
    		        ext_scan_list.scan_list[i].bssid[3], 
    		        ext_scan_list.scan_list[i].bssid[4], 
    		        ext_scan_list.scan_list[i].bssid[5],
    		        ext_scan_list.scan_list[i].channel,
    		        ext_scan_list.scan_list[i].rssi);
    		
    		if( ext_scan_list.scan_list[i].rssi > *pMaxRSSI)
    		{
    			*pMaxRSSI = ext_scan_list.scan_list[i].rssi; // update the max
    		}

    		corona_print("sec: ");

    		if (ext_scan_list.scan_list[i].security_enabled)
    		{
    			if (ext_scan_list.scan_list[i].rsn_auth || ext_scan_list.scan_list[i].rsn_cipher)
    			{
    				corona_print("RSN/WPA2= ");
    			}

    			if (ext_scan_list.scan_list[i].rsn_auth)
    			{
    				corona_print(" {");
    				if (ext_scan_list.scan_list[i].rsn_auth & SECURITY_AUTH_1X)
    				{
    					corona_print("802.1X ");
    				}

    				if (ext_scan_list.scan_list[i].rsn_auth & SECURITY_AUTH_PSK)
    				{
    					corona_print("PSK ");
    				}
    				corona_print("}");
    			}

    			if (ext_scan_list.scan_list[i].rsn_cipher)
    			{
    				corona_print(" {");

    				/* AP security can support multiple options hence
    				 * we check each one separately. Note rsn == wpa2 */
    				if (ext_scan_list.scan_list[i].rsn_cipher & ATH_CIPHER_TYPE_WEP)
    				{
    					corona_print("WEP ");
    				}

    				if (ext_scan_list.scan_list[i].rsn_cipher & ATH_CIPHER_TYPE_TKIP)
    				{
    					corona_print("TKIP ");
    				}

    				if (ext_scan_list.scan_list[i].rsn_cipher & ATH_CIPHER_TYPE_CCMP)
    				{
    					corona_print("AES ");
    				}
    				corona_print("}");
    			}

    			if (ext_scan_list.scan_list[i].wpa_auth || ext_scan_list.scan_list[i].wpa_cipher)
    			{
    				corona_print("\nWPA= ");
    			}

    			if (ext_scan_list.scan_list[i].wpa_auth)
    			{
    				corona_print(" {");
    				if (ext_scan_list.scan_list[i].wpa_auth & SECURITY_AUTH_1X)
    				{
    					corona_print("802.1X ");
    				}

    				if (ext_scan_list.scan_list[i].wpa_auth & SECURITY_AUTH_PSK)
    				{
    					corona_print("PSK ");
    				}
    				corona_print("}");
    			}

    			if (ext_scan_list.scan_list[i].wpa_cipher)
    			{
    				corona_print(" {");
    				if (ext_scan_list.scan_list[i].wpa_cipher & ATH_CIPHER_TYPE_WEP)
    				{
    					corona_print("WEP ");
    				}

    				if (ext_scan_list.scan_list[i].wpa_cipher & ATH_CIPHER_TYPE_TKIP)
    				{
    					corona_print("TKIP ");
    				}

    				if (ext_scan_list.scan_list[i].wpa_cipher & ATH_CIPHER_TYPE_CCMP)
    				{
    					corona_print("AES ");
    				}
    				corona_print("}");
    			}

    			/* it may be old-fashioned WEP this is identified by
    			 * absent wpa and rsn ciphers */
    			if ((ext_scan_list.scan_list[i].rsn_cipher == 0) &&
    					(ext_scan_list.scan_list[i].wpa_cipher == 0))
    			{
    				corona_print("WEP ");
    			}
    		}
    		else
    		{
    			corona_print("NONE");
    		}

    		corona_print("\n");
    	}
    }
    return 0;
}

