/*
 * wifi_mgr.c
 *
 *  Created on: Mar 13, 2013
 *      Author: Chris
 */

/*
 # Copyright (c) 2013 Whistle Labs
 #
 # Whistle Labs
 # Proprietary and Confidential
 #
 # This source code and the algorithms implemented therein constitute
 # confidential information and may comprise trade secrets of Whistle Labs
 # or its associates.
 #
 */

#include <custom_stack_offload.h>
#include <athdefs.h>
#include <mqx.h>
#include "wassert.h"
#include "ar4100p_main.h"
#include "wifi_mgr.h"
#include "cfg_mgr.h"
#include "sys_event.h"
#include "app_errors.h"
#include "wdns.h"
#include "rf_switch.h"
#include "dis_mgr.h"
#include "pwr_mgr.h"
#include "pmem.h"
#include "datamessage.pb.h"
#include "checksum.h"

typedef struct wifi_fw_version_type
{
    uint32_t wifi_fw_version;
    uint8_t  has_wifi_fw_version;
}wifi_fw_version_t;

static wifi_fw_version_t g_wifi_version = {0};
static boolean isConnected = FALSE;
static LWEVENT_STRUCT WIFIMGR_lwevent;
static _enet_handle driver_handle = 0;
static int current_security_type;
static boolean audit_wifi_is_on = FALSE; // for power manager audit only. not used for WiFi control
static uint8_t g_wifi_is_initializing = 0;

const char WPA_PARAM_VERSION_WPA[] = "wpa";
const char WPA_PARAM_VERSION_WPA2[] = "wpa2";

#define WIFIMGR_CONNECTED_EVENT			1
#define WIFIMGR_DISCONNECTED_EVENT		2
#define WIFIMGR_AUTH_FAILED_EVENT		4

/************************** #DEFs ************************************************/
#define MAX_DHCP_ATTEMPTS 4
#define MAX_DHCP_ATTEMPT_QUERIES 4
#define MAX_DHCP_ATTEMPT_QUERY_PAUSE 100 //milliseconds

#define MAX_CONSECUTIVE_DNS_FAILURES 10

#define SOCKET_DELAY_HACK

#ifdef SOCKET_DELAY_HACK

/*
 *       *       ALPHA_SOCKET_DELAY         *
 * 
 * Kevin Lloyd <kevin@whistle.com>   Jun 27  to Benjamin, Chris, Ernie, Firmware 
    We recently received a response from Atheros on the 9th socket. Can you let me know what you think.
    
        We received a tip from QCA which may solve the mystery on this ticket. 
        The R2.1 AR4100P firmware supports up to 8 concurrent sockets, but the internal resources in the AR4100P 
        for a socket do not become free immediately after the socket is closed. 
    
        Instead, the resources are kept tied up for a period of time (the TCP TIME-WAIT state) 
        before being fully cleared. The R2.1 firmware defines the length of this state as 10 seconds. 
        
        Therefore, if sockets are opened and closed rapidly, it is possible to fully consume the socket resources on the AR4100P; 
        new socket creation would be expected to fail until the timers begin expiring and resources become available again. 
        
        The remedy would be to throttle socket creation so that demand does not exceed the 10 second delayed cleanup time of previous sockets.
        
 */
#define ALPHA_SOCKET_DELAY              (11 * 1000) // Card Access says 10 seconds, give more margin out of fear/distrust.
#define NUM_CONCURRENT_SOCKETS          (7) //(8)   // we really have 8 concurrent socks we can use.  pick 7 to have more margin for error.
#define ENFORCE_MIN_DELAY_BETWEEN_SOCKS
#ifdef ENFORCE_MIN_DELAY_BETWEEN_SOCKS
	#define MIN_DELAY_BETWEEN_SOCKS		(1000)     // It SHOULD theoretically NOT be necessary to have a min delay between socks, but SEEMS to help.
                                                   // NOTE: may be somewhat dependent on EVTMGR_MAX_PACKET_LEN.
#endif
#define SOCKET_OPEN                     (1)
#define SOCKET_CLOSE                    (0)
#define WHISTLE_ALPHA_MAX_FRAME_SIZE    (1000) // Was 1576 by Atheros defaults

typedef struct sock_info_type
{
        uint64_t ms_time_sock_closed;    // sockets need 10 seconds of time to clean up.  we have 8 sockets.
} sock_info_t;

static uint32_t    g_used_socks_counter = 0;
static sock_info_t g_sock_info[NUM_CONCURRENT_SOCKETS] = {0};

#endif

// Define this to turn on debug output
#define WIFIMGR_DEBUG

#ifdef WIFIMGR_DEBUG
#define D(x)    x
#else
#define D(x)
#endif

static err_t _WIFIMGR_dns_lookup( char *host, _ip_address *pIP);
static err_t _WIFIMGR_open_ip(_ip_address remote_addr, uint_16 port, int *sock, enum SOCKET_TYPE sock_type);

// Static

// this is just for power measurements
static void WIFIMGR_set_wifi_audit_state(boolean isOn)
{
	audit_wifi_is_on = isOn;
	PWRMGR_breadcrumb();
}

/*
 *   Record the moment in time we closed a socket.
 */
static void _WIFIMGR_record_sock( uint8_t sock_open_close )
{   
    if( SOCKET_CLOSE == sock_open_close )
    {
        uint_64     ms_now;
        uint32_t    idx = g_used_socks_counter % NUM_CONCURRENT_SOCKETS;
        
        RTC_get_ms(&ms_now);
        
        g_sock_info[idx].ms_time_sock_closed = ms_now;
        g_used_socks_counter++;
    }
}

/*
 *   See note about ALPHA_SOCKET_DELAY above.
 *     We want to keep track of which sockets are available, and how much time between socket open/closes.
 */
static void _WIFIMGR_process_sock_delay( void )
{
    uint_64    ms_now;
    uint_64    time_since_this_sock_closed;
    uint32_t   idx;
    uint32_t   sock_delay;
    
    if( g_used_socks_counter < NUM_CONCURRENT_SOCKETS )
    {
        /*
         *   In this case, we SHOULD NOT need to delay, since we haven't used
         *     the 8 concurrent sockets we are allowed yet, according to Card Access conjecture.
         */
		#ifdef ENFORCE_MIN_DELAY_BETWEEN_SOCKS
			sock_delay = MIN_DELAY_BETWEEN_SOCKS;
			goto do_sock_delay;
		#else
			return;
		#endif
    }
    
    /*
     *   Since we arrived here, we know we want to delay 10 seconds from the oldest socket close.
     */
    idx = g_used_socks_counter % NUM_CONCURRENT_SOCKETS;
    
    RTC_get_ms(&ms_now);

    /*
     *   First do some sanity checks.
     */
    if( ms_now < g_sock_info[idx].ms_time_sock_closed )
    {
        PROCESS_NINFO( ERR_WIFI_SOCK_DELAYS_BOGUS, NULL);
        sock_delay = ALPHA_SOCKET_DELAY/2;   // since time is confused, assume we need "some amount" of time here. (should never get here)
        goto do_sock_delay;
    }
    
    time_since_this_sock_closed = ms_now - g_sock_info[idx].ms_time_sock_closed;
    
    if( time_since_this_sock_closed >= ALPHA_SOCKET_DELAY )
    {
        /*
         *   It has already been N+ seconds, so don't bother delaying any longer.
         */
		#ifdef ENFORCE_MIN_DELAY_BETWEEN_SOCKS
			sock_delay = MIN_DELAY_BETWEEN_SOCKS;
			goto do_sock_delay;
		#else
			return;
		#endif
    }
    
    sock_delay = ALPHA_SOCKET_DELAY - (uint32_t)time_since_this_sock_closed;

do_sock_delay:
    corona_print("\t\t* SOCK DELAY <%ums>\t(# %u)\n",
                    sock_delay,
                    g_used_socks_counter);
    
    _time_delay( sock_delay );
}

#if 0
void _WIFIMGR_test_sock_delays(void)
{
    uint32_t fake_sock_hdl = 0;
    
    PWRMGR_VoteForRUNM( PWRMGR_WIFIMGR_VOTE );
    
    while( fake_sock_hdl < 100 )
    {
        _WIFIMGR_process_sock_delay();

        //sock_peer = t_socket((void *) driver_handle, ATH_AF_INET, sock_type, 0);
        
        _time_delay( 50 );
        _WIFIMGR_record_sock( SOCKET_OPEN );
        
        corona_print("fake socket activity happening\n");
        _time_delay( 800 );
        _WIFIMGR_record_sock( SOCKET_CLOSE );
        
        fake_sock_hdl++;
    }
}
#endif

// Callback from Atheros stack when connection goes up or down
static void WIFIMGR_callback(int val)
{
    boolean wasConnected;
    boolean failed = false;
    
    wasConnected = isConnected;
    
    if (val == A_TRUE)
    {
        corona_print("Con'd\n");

        if (SEC_MODE_WPA != current_security_type)
    	{
    		isConnected = TRUE;
            //corona_print("WiFi UP\n");
    	}
    }
    else if (val == INVALID_PROFILE) // this event is used to indicate RSNA failure
    {
        WPRINT_ERR("4way");
        failed = true;
    }
    else if (val == PEER_FIRST_NODE_JOIN_EVENT) //this event is used to RSNA success
    {
        corona_print("4way OK\n");
        isConnected = TRUE;
    }
    else if (val == A_FALSE)
    {
        isConnected = FALSE;
        corona_print("WIFI DWN\n");
    }
    else
    {
        corona_print("tx %dkbps\n", val);
    }

    if (!wasConnected && isConnected)
    {
        _lwevent_set(&WIFIMGR_lwevent, WIFIMGR_CONNECTED_EVENT); // private event
    }
    else if (!wasConnected && failed)
    {
        _lwevent_set(&WIFIMGR_lwevent, WIFIMGR_AUTH_FAILED_EVENT); // private event
    }
    else if (wasConnected && !isConnected)
    {
    	WIFIMGR_notify_disconnect();
        WMP_post_breadcrumb( BreadCrumbEvent_WIFI_DONE );
        WIFIMGR_set_wifi_audit_state(FALSE); // Consider the wifi to be off.
    }
}

// Set wpa with driver
static int WIFIMGR_set_wpa(wpa_parameters_t *wpa_parameters)
{
    A_INT32 a_rc;
    int rc = 0;
    
    a_rc = whistle_set_passphrase(wpa_parameters->passphrase);
    if (A_OK != a_rc)
    {
        rc = -1;
        goto DONE;
    }

    a_rc = whistle_set_wpa(wpa_parameters->version, wpa_parameters->ucipher,
            wpa_parameters->mcipher);
    if (A_OK != a_rc)
    {
        rc = -2;
        goto DONE;
    }
       
DONE:
//    wassert(ERR_OK == 0); // so we can jam rc into PROCESS_NINFO and have it work as expected
	return (int)PROCESS_NINFO(rc, NULL);
}

// Set wep with driver
static int WIFIMGR_set_wep(wep_parameters_t *wep_parameters)
{
    A_INT32 a_rc;
    int rc = 0;
    
    if (wep_parameters->key[0] && wep_parameters->key[0][0])
    {
        a_rc = whistle_set_wep_key(1, wep_parameters->key[0]);
        if (A_OK != a_rc)
        {
            rc = -1;
            goto DONE;
        }
    }

    if (wep_parameters->key[1] && wep_parameters->key[1][0])
    {
        a_rc = whistle_set_wep_key(2, wep_parameters->key[1]);
        if (A_OK != a_rc)
        {
            rc = -2;
            goto DONE;
        }
    }

    if (wep_parameters->key[2] && wep_parameters->key[2][0])
    {
        a_rc = whistle_set_wep_key(3, wep_parameters->key[2]);
        if (A_OK != a_rc)
        {
            rc = -3;
            goto DONE;
        }
    }

    if (wep_parameters->key[3] && wep_parameters->key[3][0])
    {
        a_rc = whistle_set_wep_key(4, wep_parameters->key[3]);
        if (A_OK != a_rc)
        {
            rc = -4;
            goto DONE;
        }
    }

    a_rc = whistle_set_wep(wep_parameters->default_key_index);
    if (A_OK != a_rc)
    {
        rc = -5;
        goto DONE;
    }
    
DONE:
//    wassert(ERR_OK == 0); // so we can jam rc into PROCESS_NINFO and have it work as expected
    return (int)PROCESS_NINFO(rc, NULL);
}

// Save network's security config in config store
static int WIFIMGR_save_network_security_config(char *ssid,
                                                security_parameters_t *security_parameters,
                                                WIFIMGR_SECURITY_FLUSH_TYPE_e flush)
{
    int network_idx = -1;
    int i;
    int rc = 0;
    cfg_network_t *pnetwork;
    
    for (i = 0; i < g_dy_cfg.wifi_dy.num_networks; ++i)
    {
        if (0 == strcmp(g_dy_cfg.wifi_dy.network[i].ssid, ssid))
        {
            network_idx = i;
            break;
        }
    }

    if (-1 == network_idx)
    {
        if (g_dy_cfg.wifi_dy.num_networks < CFG_WIFI_MAX_NETWORKS)
        {
            ++g_dy_cfg.wifi_dy.num_networks;
            network_idx = g_dy_cfg.wifi_dy.num_networks - 1;
        }
        else
        {
            // No room
            // TODO: overwrite LRU?
            rc = -1;
            goto DONE;
        }
    }

    pnetwork = &g_dy_cfg.wifi_dy.network[network_idx];
    
    strcpy(pnetwork->ssid, ssid);
    pnetwork->security_type = security_parameters->type;
    
    switch (security_parameters->type)
    {
        case SEC_MODE_OPEN:
            break;
            
        case SEC_MODE_WPA:
            strncpy(pnetwork->security_parameters.wpa.version,
                    security_parameters->parameters.wpa_parameters.version, WIFI_MAX_WPA_VER_SIZE);
            strncpy(pnetwork->security_parameters.wpa.passphrase,
                    security_parameters->parameters.wpa_parameters.passphrase, MAX_PASSPHRASE_SIZE+1);
            pnetwork->security_parameters.wpa.ucipher =
                    security_parameters->parameters.wpa_parameters.ucipher;
            pnetwork->security_parameters.wpa.mcipher =
                    security_parameters->parameters.wpa_parameters.mcipher;
            break;
            
        case SEC_MODE_WEP:
            strncpy(pnetwork->security_parameters.wep.key0,
                    security_parameters->parameters.wep_parameters.key[0], MAX_WEP_KEY_SIZE+1);
            strncpy(pnetwork->security_parameters.wep.key1,
                    security_parameters->parameters.wep_parameters.key[1], MAX_WEP_KEY_SIZE+1);
            strncpy(pnetwork->security_parameters.wep.key2,
                    security_parameters->parameters.wep_parameters.key[2], MAX_WEP_KEY_SIZE+1);
            strncpy(pnetwork->security_parameters.wep.key3,
                    security_parameters->parameters.wep_parameters.key[3], MAX_WEP_KEY_SIZE+1);
            pnetwork->security_parameters.wep.default_key =
                    security_parameters->parameters.wep_parameters.default_key_index;
            break;
            
        default:
            wassert(0);
    }
    
    PROCESS_NINFO(STAT_WIFI_ADD, "Add sum x%x %d ssids", str_checksum16(ssid), g_dy_cfg.wifi_dy.num_networks);
    
    if(flush == WIFIMGR_SECURITY_FLUSH_NOW)
    {
        CFGMGR_flush( CFG_FLUSH_DYNAMIC );
    }
    
DONE:
//    wassert(ERR_OK == 0); // so we can jam rc into PROCESS_NINFO and have it work as expected
    return (int)PROCESS_NINFO(rc, NULL);
}

// Get network's security config from config store
static int WIFIMGR_fetch_network_security_config(char *ssid,
        security_parameters_t *security_parameters)
{
    int network_idx = -1;
    int i;
    cfg_network_t *pnetwork;
    int rc = 0;
    
    for (i = 0; i < g_dy_cfg.wifi_dy.num_networks; ++i)
    {
        if (0 == strcmp(g_dy_cfg.wifi_dy.network[i].ssid, ssid))
        {
            network_idx = i;
            break;
        }
    }

    if (network_idx == -1)
    {
        rc = -1;
        goto DONE;
    }

    pnetwork = &g_dy_cfg.wifi_dy.network[network_idx];
    
    security_parameters->type = pnetwork->security_type;
    
    switch (security_parameters->type)
    {
        case SEC_MODE_OPEN:
            break;
            
        case SEC_MODE_WPA:
            security_parameters->parameters.wpa_parameters.version =
                    pnetwork->security_parameters.wpa.version;
            security_parameters->parameters.wpa_parameters.passphrase =
                    pnetwork->security_parameters.wpa.passphrase;
            security_parameters->parameters.wpa_parameters.ucipher =
                    pnetwork->security_parameters.wpa.ucipher;
            security_parameters->parameters.wpa_parameters.mcipher =
                    pnetwork->security_parameters.wpa.mcipher;
            break;
            
        case SEC_MODE_WEP:
            security_parameters->parameters.wep_parameters.key[0] =
                    pnetwork->security_parameters.wep.key0;
            security_parameters->parameters.wep_parameters.key[1] =
                    pnetwork->security_parameters.wep.key1;
            security_parameters->parameters.wep_parameters.key[2] =
                    pnetwork->security_parameters.wep.key2;
            security_parameters->parameters.wep_parameters.key[3] =
                    pnetwork->security_parameters.wep.key3;
            security_parameters->parameters.wep_parameters.default_key_index =
                    pnetwork->security_parameters.wep.default_key;
            break;
            
        default:
            wassert(0);
    }
    
DONE:
//    wassert(ERR_OK == 0); // so we can jam rc into PROCESS_NINFO and have it work as expected
    return (int)PROCESS_NINFO(rc, NULL);
}

// Switch to different network within driver
static int WIFIMGR_switch_network(char *ssid)
{
    security_parameters_t security_parameters;
    
    // Try to fetch from config
    if (0 == WIFIMGR_fetch_network_security_config(ssid, &security_parameters))
    {
        // Found it. Register it with the driver
        if (0 != WIFIMGR_set_security(ssid, &security_parameters, WIFIMGR_SECURITY_FLUSH_NEVER))
        {
            // Failed
            return -1;
        }
    }
    else
    {
        // Don't know this SSID
        return -2;
    }
    
	current_security_type = security_parameters.type;
    
    return 0;
}

/*
 *   This should be called back when an Atheros assert occurs.
 */
static void atheros_assert_callback(const char *pAssertString)
{
    if( g_wifi_is_initializing )
    {
        corona_print("\n\t***\n%s\n\t***\n", pAssertString);
        _time_delay( 2 * 1000 ); // give some time for sherlock.
        
        /*
         *   If WIFI fails while initializing, we assume there is something
         *      very wrong with the WIFI chip and/or its SPI interface.
         */
        PWRMGR_Fail(ERR_WIFI_HW_MAYBE_FAILED, pAssertString);
    }
    else
    {
        if(pAssertString)
        {
            corona_print("MSG\t((%s))\n", pAssertString);
        }
        
        PROCESS_NINFO(ERR_WIFI_ATHEROS_ASSERT, "%s\n\tnum opens %d", pAssertString, g_used_socks_counter);
        _time_delay(100);  // make sure that puppy gets posted/flushed to EVTMGR.
        PWRMGR_Reboot( 500, PV_REBOOT_REASON_WIFI_FAIL, TRUE );
    }
}

static boolean WIFIMGR_is_wifi_failure_rebooting(void)
{
	return PWRMGR_is_rebooting() && PV_REBOOT_REASON_WIFI_FAIL == __SAVED_ACROSS_BOOT.reboot_reason;
}

/*
 * method: _WIFIMGR_singular_dhcp_attempt
 * returns:
 *   ERR_OK - successfully retrieved IP address
 *   ERR_WIFI_ERROR - not worth trying again, at least without reconnecting WiFi
 *   ERR_WIFI_NOT_CONNECTED - WiFi not detected to be connected
 *   ERR_WIFI_DHCP_FAIL - DHCP failed, may be worth trying again.
 *   ERR_WIFI_NEED_REBOOT - CUSTOM_BLOCK timeout, should probably reboot system
 */
err_t _WIFIMGR_singular_dhcp_attempt(A_UINT32 *addr, A_UINT32 *mask, A_UINT32 *gw, uint8_t max_queries)
{
    A_INT32 rc;
    A_INT8 dhcp_query_count = 0;
    
    /*Check if driver is loaded*/
    if (IS_DRIVER_READY != A_OK)
    {
        return ERR_WIFI_ERROR;
    }
    if ( !isConnected )
    {
    	return ERR_WIFI_NOT_CONNECTED;
    }

    /*Start DHCP*/
    rc = t_ipconfig((void *)_whistle_handle, IPCFG_DHCP, addr, mask, gw); //20s command_block timeout
    if (A_OK != rc)
    {
    	WPRINT_ERR("dhcp strt %d", rc);
    	return ERR_WIFI_DHCP_FAIL; // We've the AR4100P be able to recover when retrying this.
    }

    *addr = 0;
    *mask = 0; 
    *gw = 0;
    do
    {
        if ( !isConnected )
        {
        	return ERR_WIFI_NOT_CONNECTED;
        }
        
        dhcp_query_count++;
        _time_delay(100*(dhcp_query_count));
        
        rc = t_ipconfig((void *)_whistle_handle, IPCFG_QUERY, addr, mask, gw); //20s command_block timeout
        
        corona_print("dhcpq %d\n", rc);
        
    } while ( (*addr == 0 || rc != A_OK ) && 
    		rc != A_WHISTLE_REBOOT && 
    		dhcp_query_count < max_queries);

    if ( A_OK == rc  && 0 != *addr)
    {
    	// 09-26-2013 / KL: I know this is verbose however would like this in here to help characterize DHCP retries.
    	corona_print("ip %d, %d.%d.%d.%d\n", dhcp_query_count, *addr >> 24, 0xFF & (*addr >> 16), 0xFF & (*addr >> 8), 0xFF & *addr);
    	if (dhcp_query_count > 1)
    	{
    		PROCESS_UINFO("dhcp %dx", dhcp_query_count);
    	}
    	return ERR_OK;
    }
    else if (A_WHISTLE_REBOOT == rc)
    {
    	PROCESS_NINFO(ERR_WIFI_DHCP_FAIL, "reboot");
    	return ERR_WIFI_NEED_REBOOT;
    }
    else
    {
    	PROCESS_NINFO(ERR_WIFI_DHCP_FAIL, "max");
    	return ERR_WIFI_DHCP_FAIL;
    }
}

err_t _WIFIMGR_block_on_dhcp(uint8_t max_attempts, uint8_t max_queries_per_attempt)
{
    A_INT32  rc = A_ERROR;
    A_UINT32 addr = 0, mask = 0, gw = 0;
    A_INT8 dhcp_attempt_count = 0;

    do
    {
    	rc = _WIFIMGR_singular_dhcp_attempt(&addr, &mask, &gw, max_queries_per_attempt);
    	dhcp_attempt_count++;
    }while ( ERR_WIFI_DHCP_FAIL == rc && 
    		dhcp_attempt_count < max_attempts);

    return ( rc );
}

/*
 * Function: WIFIMGR_connect_ssid
 * 
 * Parameters:
 *   *ssid: network to connect to
 *   *security_paramaters: manually specify security_parameters to pair with the SSID.
 *        If not supplied then will look up values in dynamic config.
 *    quick_test: if true then we won't reset on DHCP failure, instead we'll return an error.
 *        This is useful for when testing out an AP to see if it is connectable.
 */
static err_t WIFIMGR_connect_ssid(char *ssid, security_parameters_t *security_parameters, boolean quick_test)
{
    A_INT32 a_rc;
    _mqx_uint mqx_rc;
    _mqx_uint mask;
    err_t e_rc;
    int retries = 0;
    uint64_t start_time, time_to_connect;
    int tcon;
    uint16_t ssid_sum;
    
    if (strlen(ssid) > MAX_SSID_LENGTH - 1)
    {
    	return PROCESS_NINFO(ERR_WIFI_BAD_SSID, "%d", strlen(ssid));
    }
    
    /* Calculate simple checksum of ssid */
	ssid_sum = str_checksum16(ssid);
    
    corona_print("Con\t%s\n", ssid);
    
    // already connected?
    if (isConnected)
    {
        corona_print("Already con\n");

        // connected to desired ssid?
        if (0 == strcmp((char *) whistle_get_connected_ssid(), ssid))
        {
            return ERR_OK; // done
        }
        
        corona_print("Disc and recon\n\n");
        
        // disconnect current before connecting new
        e_rc = WIFIMGR_disconnect();
        wassert(ERR_OK == e_rc || ERR_WIFI_NOT_CONNECTED == e_rc);
    }

    while (TRUE) // Retry multiple times if we can connect, but can't achieve DHCP
    {
        if (PWRMGR_is_rebooting())
        {
        	return ERR_WIFI_CONNECT_FAIL;
        }
        
    	if (security_parameters != NULL)
    	{
    		if (0 != WIFIMGR_set_security(ssid, security_parameters, WIFIMGR_SECURITY_FLUSH_NEVER))
    		{
    			return PROCESS_NINFO(ERR_WIFI_ERROR, "x%X", ssid_sum);
    		}
    	}
    	// Assumption is we already know this ssid, so switch to it. If we don't, should have called WIFIMGR_set_security first!
    	else if (0 != WIFIMGR_switch_network(ssid))
    	{
    		// Sorry. Don't know this network
    		return PROCESS_NINFO(ERR_WIFI_BAD_SSID, "x%X", ssid_sum);
    	}

    	// set the RF switch to WiFi
    	rf_switch(RF_SWITCH_WIFI, TRUE);
    	
    	// Make sure there's still a vote for RUNM. Note, the vote could have been removed
    	// by WIFIMGR_disconnect()
    	PWRMGR_VoteForRUNM( PWRMGR_WIFIMGR_VOTE );

    	// Make sure local cache is cleaned for this set of sockets.
    	WDNS_clean_local_cache();

    	/* Clear potential stale events before evoking them */
    	_lwevent_clear(&WIFIMGR_lwevent, WIFIMGR_CONNECTED_EVENT | WIFIMGR_AUTH_FAILED_EVENT);
    	
    	if (0 == retries)
    	{
    		WMP_post_breadcrumb( BreadCrumbEvent_WIFI_START );
    	    WIFIMGR_set_wifi_audit_state(TRUE); // Consider the wifi to now be on
    	}

        RTC_get_ms(&start_time);
    	// ask the driver to connect
    	a_rc = whistle_connect_handler(ssid);
    	if (A_OK != a_rc)
    	{
            WMP_post_breadcrumb( BreadCrumbEvent_WIFI_FAILED );
            WIFIMGR_set_wifi_audit_state(FALSE); // Consider the wifi to now be off
        	get_last_error(&a_rc);
            return PROCESS_NINFO(ERR_WIFI_CONNECT_FAIL, "conn F x%X e:x%X", ssid_sum, a_rc);
    	}

    	// wait for the connected event
    	mqx_rc = _lwevent_wait_ticks(&WIFIMGR_lwevent, WIFIMGR_CONNECTED_EVENT | WIFIMGR_AUTH_FAILED_EVENT, FALSE,
    			(g_st_cfg.wifi_st.connect_wait_timeout)*BSP_ALARM_FREQUENCY/1000);
    	
		mask = _lwevent_get_signalled();

    	if (LWEVENT_WAIT_TIMEOUT == mqx_rc)
    	{
        	get_last_error(&a_rc);
        	PROCESS_NINFO(ERR_WIFI_CONNECT_FAIL, "conn TO x%X e:x%X r:%d", ssid_sum, a_rc, retries);
        	
            WMP_post_breadcrumb( BreadCrumbEvent_WIFI_FAILED );
            
    	    WIFIMGR_set_wifi_audit_state(FALSE); // Consider the wifi to now be off
    		// seems like we ought to tell it to give up.
    		wassert(A_OK == whistle_disconnect_handler());
    		return ERR_WIFI_CONNECT_FAIL;
    	}
    	else if (WIFIMGR_AUTH_FAILED_EVENT & mask)
    	{
        	get_last_error(&a_rc);
        	PROCESS_NINFO(ERR_WIFI_CONNECT_FAIL, "auth fail x%X e:x%X s:%d c:%d r:%d", ssid_sum, a_rc, whistle_get_rssi(), whistle_get_channel(), retries);
        	
    		wassert(A_OK == whistle_disconnect_handler());
    		
    		if (retries++ > 2)
    		{
                WMP_post_breadcrumb( BreadCrumbEvent_WIFI_FAILED );
        	    WIFIMGR_set_wifi_audit_state(FALSE); // Consider the wifi to now be off
    			return ERR_WIFI_CONNECT_FAIL;
    		}
    		else
    		{
    			continue;
    		}
    	}
    	else
    	{
            RTC_get_ms(&time_to_connect);
            tcon = time_to_connect - start_time;
            PROCESS_NINFO(STAT_WIFI_ASSOC_TIME, "%d", tcon); //was previously PROCESS_UINFO "wcon: "
            PROCESS_UINFO("wcon i:%x t:%d s:%d c:%d", ssid_sum, tcon, whistle_get_rssi(), whistle_get_channel());
            wassert(MQX_OK == mqx_rc);
    	}

    	D(corona_print("DHCP\n"));

        RTC_get_ms(&start_time);
    	// wait for a DHCP address
    	if (quick_test)
    	{
			e_rc = _WIFIMGR_block_on_dhcp(1, 2);
    	}
    	else
    	{
			e_rc = _WIFIMGR_block_on_dhcp(MAX_DHCP_ATTEMPTS, MAX_DHCP_ATTEMPT_QUERIES);
    	}
    	
    	if (A_WHISTLE_REBOOT == e_rc && !quick_test)
    	{
    		// Wifi isn't responding: reboot now
            WMP_post_breadcrumb( BreadCrumbEvent_WIFI_FAILED );
    	    WIFIMGR_set_wifi_audit_state(FALSE); // Consider the wifi to now be off
    		PWRMGR_Reboot( 500, PV_REBOOT_REASON_WIFI_FAIL, FALSE );
    		return ERR_WIFI_DHCP_FAIL; // Return to caller to clean up before reboot completes
    	}
    	else if (A_OK == e_rc)
    	{
    	    RTC_get_ms(&time_to_connect);
    	    tcon = time_to_connect - start_time;
    	    PROCESS_NINFO(STAT_WIFI_DHCP_TIME, "x%X t:%d s:%d c:%d r:%d", ssid_sum, tcon, whistle_get_rssi(), whistle_get_channel(), retries);
    		break;
    	}
    	else
    	{
    		PROCESS_NINFO(ERR_WIFI_DHCP_FAIL, "x%X e:x%X s:%d c:%d r:%d", ssid_sum, e_rc, whistle_get_rssi(), whistle_get_channel(), retries);
    		
    		WIFIMGR_disconnect();
    		
    		if (quick_test || retries++ > 2)
    		{
                WMP_post_breadcrumb( BreadCrumbEvent_WIFI_FAILED );
        	    WIFIMGR_set_wifi_audit_state(FALSE); // Consider the wifi to now be off
    			return ERR_WIFI_DHCP_FAIL;
    		}
    	}
    }

	//corona_print("DHCP OK\n");

    strcpy(g_dy_cfg.wifi_dy.mru_ssid, ssid);
    
    /*
     *   Wait until we are done uploading, then flush the mru SSID to dynamic configs.
     *     This is to avoid putting a SPI Flash erase in the WIFI-up path.
     */
    CFGMGR_req_flush_at_shutdown();
    
    return ERR_OK;
}

// Open connection to ip:port. Return socket.
static err_t _WIFIMGR_open_ip(_ip_address remote_addr, uint_16 port, int *sock, enum SOCKET_TYPE sock_type)
{
    SOCKADDR_T foreign_addr;
    int sock_peer; /* Foreign socket.*/
    A_INT32 a_rc = A_ERROR;
    err_t error = ERR_WIFI_ERROR;
    
    _WIFIMGR_process_sock_delay();
    
    sock_peer = t_socket((void *) driver_handle, ATH_AF_INET, sock_type, 0);
    
    if (A_WHISTLE_REBOOT == sock_peer)
	{
		// Wifi isn't responding: reboot now
		PWRMGR_Reboot( 500, PV_REBOOT_REASON_WIFI_FAIL, FALSE );
		error = ERR_WIFI_CRITICAL_ERROR; // Return to caller to clean up before reboot completes
		goto ERROR;
	}
    else if (A_ERROR == sock_peer)
    {
        error = ERR_WIFI_SOCKET_CREATE_FAIL;
        //Todo: should we remove this or maybe retry? Could be related to 58th socket error
        // Wifi isn't responding: reboot now
        PWRMGR_Reboot( 500, PV_REBOOT_REASON_WIFI_FAIL, FALSE );
        // Return to caller to clean up before reboot completes
        goto ERROR;
    }
    
    _WIFIMGR_record_sock( SOCKET_OPEN );

    // Allow small delay to allow other thread to run per Atheros example
    _time_delay(1);
    
    // Ensure that wifi is still up after _time_delays before requesting a socket operation
    if (!isConnected)
    {
    	corona_print("Not Con\n");
        return ERR_WIFI_NOT_CONNECTED;
    }
    
    memset(&foreign_addr, 0, sizeof(foreign_addr));
    foreign_addr.sin_addr = remote_addr;
    foreign_addr.sin_port = port;
    foreign_addr.sin_family = ATH_AF_INET;
    
    /* Connect to the server.*/
    a_rc = t_connect((void *) driver_handle, sock_peer, (&foreign_addr),
            sizeof(foreign_addr));

    if (A_WHISTLE_REBOOT == a_rc)
    {
        // Wifi isn't responding: reboot now
        PWRMGR_Reboot( 500, PV_REBOOT_REASON_WIFI_FAIL, FALSE );
        error = ERR_WIFI_CRITICAL_ERROR; // Return to caller to clean up before reboot completes
        goto ERROR;
    }
    else if (a_rc != A_OK)
    {
        error = ERR_WIFI_SOCKET_CONNECT_FAIL;
        goto ERROR;
    }

    *sock = sock_peer;
    
    return ERR_OK;
    
ERROR:
	get_last_error(&a_rc);
    PROCESS_NINFO(error, "LastE x%X", a_rc);
    
    switch (a_rc)
    {
        case A_SOCK_UNAVAILABLE:
            return ERR_WIFI_NO_SOCKETS;
        default:
            return error;
    }
}

static err_t _WIFIMGR_dns_lookup( char *host, _ip_address *pIP)
{
	HOSTENT_STRUCT *dns_result;
	_ip_address result_ip = 0;
	static int fail_count = 0;

	if ( (0 == driver_handle) || (!isConnected) )
	{
		return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
	}

	//corona_print("DNS Q: %s\n", host);

	// Check to see if a cached address exists
	result_ip = WDNS_search_local_cache(host);
	if (result_ip != 0xFFFFFFFF)
	{
		//corona_print("DNS LOC\n");
		*pIP = result_ip;
		return ERR_OK;
	}
	dns_result = DNS_query_resolver_task((uint_8 *) host, DNS_A);

	if (dns_result != NULL && dns_result->h_length > 0)
	{
		corona_print("DNS OK:\tlen:%d, ", dns_result->h_length);
		corona_print(" addr: %u.%u.%u.%u (0x%.8X)\n",
				(uint_8) (dns_result->h_addr_list[0][3]),
				(uint_8) (dns_result->h_addr_list[0][2]),
				(uint_8) (dns_result->h_addr_list[0][1]),
				(uint_8) (dns_result->h_addr_list[0][0]),
				(uint_32) (((uint_32 *) (dns_result->h_addr_list[0]))[0]));
		fail_count = 0;
	}
	else
	{
		if (fail_count++ > MAX_CONSECUTIVE_DNS_FAILURES)
		{
	        PWRMGR_Reboot( 500, PV_REBOOT_REASON_REBOOT_HACK, FALSE );
			return PROCESS_NINFO(ERR_WIFI_DNS_FATAL_ERROR, NULL);
		}
		return PROCESS_NINFO(ERR_WIFI_DNS_ERROR, NULL);
	}

	*pIP = (uint_32) (((uint_32 *) (dns_result->h_addr_list[0]))[0]);
	return ERR_OK;
}

// Init the WIFIMGR
void WIFIMGR_init(void)
{
	PWRMGR_VoteForRUNM( PWRMGR_WIFIMGR_VOTE );

	if (0 != driver_handle)
	{
		return; // already initialized
	}
	
	g_wifi_is_initializing = 1;
	
	// We must have the switch set to something other than 00 when any of the radios are on,
	// so turn on the switch here even if we're not necessarily ready to use WiFi
	rf_switch(RF_SWITCH_WIFI, TRUE);
	
    ATHEROS_register_assert_cbk( atheros_assert_callback );

    if( (MQX_OK != whistle_atheros_init(&driver_handle)) || (NULL == driver_handle) )
    {
        PWRMGR_Fail(ERR_WIFI_HW_MAYBE_FAILED, "WIFI ath init");
    }
    
    whistle_set_callback_with_driver(WIFIMGR_callback);
    
    _lwevent_create(&WIFIMGR_lwevent, LWEVENT_AUTO_CLEAR);
    
    whistle_display_mac_addr();

    if(A_OK != get_version(&g_wifi_version.wifi_fw_version) )
    {
        PWRMGR_Fail(ERR_WIFI_HW_MAYBE_FAILED, "WIFI ver");
    }
    g_wifi_version.has_wifi_fw_version = 1;
    
    WDNS_init_local_cache();
    
    /*
     * Explicitly set receive buffer to max, 10
     */
    if(ERR_OK != WIFIMGR_set_RX_buf(10))
    {
        PWRMGR_Fail(ERR_WIFI_HW_MAYBE_FAILED, "WIFI rxBuf");
    }
    
    if(A_OK != whistle_set_tx_power(15))
    {
        PWRMGR_Fail(ERR_WIFI_HW_MAYBE_FAILED, "WIFI TxPwr");
    }
    
    g_wifi_is_initializing = 0;
}

void WIFIMGR_shut_down(void)
{
	whistle_ShutDownWIFI();
    rf_switch(RF_SWITCH_WIFI, FALSE);
}

/*
 * Return a uint8_t boolean for whether or not the wifi is on.
 * Return 1 if wifi is on 0 if it is off
 */
boolean WIFIMGR_wifi_is_on(void)
{
    return audit_wifi_is_on;
}

/*
 *   Set the value for "version.wlan_ver", which is the Atheros firmware version.
 *   Return a boolean for whether or not the Atheros firmware has been set yet.
 */
uint8_t WIFIMGR_get_wifi_fw_version(uint32_t *pFwVersion)
{
    *pFwVersion = g_wifi_version.wifi_fw_version;
    return g_wifi_version.has_wifi_fw_version;
}

// Clear network's security config in config store
err_t WIFIMGR_clear_security(void)
{
    g_dy_cfg.wifi_dy.num_networks = 0;    
    memset( g_dy_cfg.wifi_dy.network, 0, sizeof(cfg_network_t) * CFG_WIFI_MAX_NETWORKS );
    memset( g_dy_cfg.wifi_dy.mru_ssid, 0, sizeof(g_dy_cfg.wifi_dy.mru_ssid) );
    PROCESS_NINFO(STAT_WIFI_DEL, "Clr'd ssids");
    CFGMGR_flush( CFG_FLUSH_DYNAMIC );
    return ERR_OK;
}

// Return 0 if this SSID is not configured currently, non-zero otherwise.
boolean WIFIMGR_is_configured(char *ssid)
{
    uint16_t i;
    
    for(i = 0; i < g_dy_cfg.wifi_dy.num_networks; i++)
    {
        if( !strcmp(g_dy_cfg.wifi_dy.network[i].ssid, ssid) )
        {
            return TRUE;
        }
    }
    return FALSE;
}

// Register security parameters with driver
err_t WIFIMGR_set_security(char *ssid, 
                           security_parameters_t *security_parameters,
                           WIFIMGR_SECURITY_FLUSH_TYPE_e flush)
{
    int w_rc = 0;
    
    switch (security_parameters->type)
    {
        case SEC_MODE_OPEN:
        	current_security_type = security_parameters->type;
            w_rc = 0;
            break;
            
        case SEC_MODE_WPA:
        	current_security_type = security_parameters->type;
            w_rc = WIFIMGR_set_wpa(
                    &security_parameters->parameters.wpa_parameters);
            break;
            
        case SEC_MODE_WEP:
        	current_security_type = security_parameters->type;
            w_rc = WIFIMGR_set_wep(
                    &security_parameters->parameters.wep_parameters);
            break;
            
        default:
            wassert(0);
            break;
    }

    if (0 == w_rc && flush != WIFIMGR_SECURITY_FLUSH_NEVER)
    {
        WIFIMGR_save_network_security_config(ssid, security_parameters, flush); // save across reboot
    }
    
    return 0 == w_rc ? ERR_OK : ERR_WIFI_SECURITY_ERROR;
}

// Connect to an SSID or next available SSID if ssid is NULL
err_t WIFIMGR_connect(char *ssid, security_parameters_t *security_parameters, boolean quick_test)
{
    err_t e_rc = ERR_OK;
    char next_ssid[MAX_SSID_LENGTH];
    boolean first_query = TRUE;
    
    if (NULL == ssid)
    {
    	corona_print("Con 1st avbl\n");
    }
    else
    {
    	corona_print("Con %s\n", ssid);
    }
    
#ifdef WIFIMGR_DEBUG
    WIFIMGR_list();
#endif
    
    if (PWRMGR_is_rebooting())
    {
    	return ERR_WIFI_CONNECT_FAIL;
    }
    
    WIFIMGR_init();
    
    if (NULL != ssid)
    {
    	e_rc = WIFIMGR_connect_ssid(ssid, security_parameters, quick_test);
        if (ERR_OK != e_rc)
        {
            // no configured networks
        	PROCESS_NINFO(e_rc, NULL);
        	WIFIMGR_disconnect(); // we're not connected, but still need to clean up
        }
        goto DONE;
    }
    
    // Cycle through all the known networks, looking for one that works.
    do
    {
        e_rc = WIFIMGR_query_next_ssid(first_query, next_ssid);
        first_query = FALSE;
        
        if (ERR_OK != e_rc)
        {
            // no more configured networks
        	WIFIMGR_disconnect(); // we're not connected, but still need to clean up
            goto DONE;
        }

        // Try to connect.
        e_rc = WIFIMGR_connect_ssid(next_ssid, NULL, quick_test);
        if (ERR_OK == e_rc)
        {
            // Yup. We're done.
            break;
        }
        else
        {
            WPRINT_ERR("con ssid %x", e_rc);
        }
    } while (1);
    
DONE:
    return e_rc;
}

// Disconnect the currently connected network
err_t WIFIMGR_disconnect( void )
{
	err_t err;
	_mqx_uint mqx_rc;
	
    if (PWRMGR_is_rebooting())
    {
    	return ERR_WIFI_NOT_CONNECTED;
    }
	
	if (0 == driver_handle)
	{
        err = ERR_WIFI_NOT_CONNECTED;
        goto DONE;
	}

    if (!isConnected)
    {
        err = ERR_WIFI_NOT_CONNECTED;
        goto DONE;
    }
    
    /* Clear potential stale events before evoking them */
    _lwevent_clear(&WIFIMGR_lwevent, WIFIMGR_DISCONNECTED_EVENT);

    wassert(A_OK == whistle_disconnect_handler());

    mqx_rc = _lwevent_wait_ticks(&WIFIMGR_lwevent, WIFIMGR_DISCONNECTED_EVENT, FALSE, 61000*BSP_ALARM_FREQUENCY/1000);
    wassert(MQX_OK == mqx_rc || LWEVENT_WAIT_TIMEOUT == mqx_rc);
    
    if (LWEVENT_WAIT_TIMEOUT == mqx_rc)
    {
        PWRMGR_Reboot( 500, PV_REBOOT_REASON_WIFI_FAIL, FALSE );
        err = ERR_WIFI_NOT_CONNECTED; // Return to caller to clean up before reboot completes
        goto DONE;
    }
    
    err = ERR_OK;
  
DONE:
    PWRMGR_VoteForVLPR( PWRMGR_WIFIMGR_VOTE );
    
    return err;
}

void WIFIMGR_notify_disconnect(void)
{
    _lwevent_set(&WIFIMGR_lwevent, WIFIMGR_DISCONNECTED_EVENT); // private event
    sys_event_post(SYS_EVENT_WIFI_DOWN); // broadcast to the public
}

/*
 *   Return 0 if WIFI comm is messed up, non-zero otherwise.
 */
uint8_t a4100_wifi_comm_ok(void)
{
    return ( A_OK == get_version( NULL ) )?1:0;
}

// Open connection to host:port. Return socket.
err_t WIFIMGR_open(char *host, uint_16 port, int *sock, enum SOCKET_TYPE sock_type)
{
    _ip_address remote_addr;
    err_t err = ERR_OK;
    
    *sock = -1; // Invalid to start
    
    if (PWRMGR_is_rebooting())
    {
    	return ERR_WIFI_NOT_CONNECTED;
    }
	
	if (0 == driver_handle)
	{
        return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
	}
    
    if (!isConnected)
    {
        return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
    }
    
    // Check whether the host is an IP address or alternatively a host name
    if (check_DNS_is_dotted_IP_addr(host) == DNS_OK) // Host is an IP address (e.g., 1.2.3.4)
    {
        /*
         *   TODO:  Need to handle error code here.
         */
        ath_inet_aton(host, (A_UINT32 *) &remote_addr);
    }
    else // Host is FQDN (e.g., staging.whistle.com)
    {
        err = _WIFIMGR_dns_lookup( host, &remote_addr);
        if( ERR_OK != err )
        {
            return err;
        }
    }
    
    return _WIFIMGR_open_ip(remote_addr, port, sock, sock_type);
}

// Close connection to socket.
err_t WIFIMGR_close(int sock)
{
    int32_t driver_err;
    
    if (PWRMGR_is_rebooting())
    {
    	return ERR_WIFI_NOT_CONNECTED;
    }
	
	if (0 == driver_handle)
	{
        return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
	}
    
    if (!isConnected)
    {
        return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
    }
    
    driver_err = t_shutdown((void *) driver_handle, sock);
    
    _WIFIMGR_record_sock( SOCKET_CLOSE );
    
    if (A_OK != driver_err)
    {
        if (A_WHISTLE_REBOOT == driver_err)
        {
            // A fatal error has occurred. Reboot now!!
            PWRMGR_Reboot(500, PV_REBOOT_REASON_WIFI_FAIL, FALSE);
        }
        
        // Return to caller to clean up before reboot completes
        return ERR_WIFI_SHUTDOWN_ERROR; // This is common. Socket may be closed already. Don't PROCESS_NINFO
    }
        
    return ERR_OK;
}

// Send data to socket.
// Return number of bytes sent.
err_t WIFIMGR_send(int sock, uint_8 *data, uint_32 length, int_32 *bytes_sent)
{
    A_UINT8 *buffer;
    A_INT32 send_result;
    A_INT32 bytes_to_send;

    *bytes_sent = 0;

	if (0 == driver_handle)
	{
        return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
	}
    
    if (!isConnected)
    {
        return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
    }
    
    // CUSTOM_ALLOC has no size limit. t_send limit is IPV4_FRAGMENTATION_THRESHOLD.
    // So we have to fragment.
    
    do
    {
        if (PWRMGR_is_rebooting())
        {
        	return ERR_WIFI_NOT_CONNECTED;
        }
        
        //TODO: REMOVE ME, I'm just debugging code for kevin to figure out why we sometimes
        //      are told we send 1450 when we just request 1440
//        if (*bytes_sent > 0)
//        	PROCESS_UINFO("tsend loop: %d:%d", *bytes_sent, length);
    	
#if 1
        // Todo: Workaround for zero's in the data. We limit the frame size to 
        // WHISTLE_ALPHA_MAX_FRAME_SIZE here in place of the normally allowed frame size 
        // which is IPV4_FRAGMENTATION_THRESHOLD.
        if (length - *bytes_sent > WHISTLE_ALPHA_MAX_FRAME_SIZE)
        {
            bytes_to_send = WHISTLE_ALPHA_MAX_FRAME_SIZE;
        }
#else
        
//#define HACK_CON_MGR_500_GRANULARITY 1        
#if HACK_CON_MGR_500_GRANULARITY
        if (length - *bytes_sent > 500 )
        {
            bytes_to_send = 500;
        }    
#else
        if (length - *bytes_sent > IPV4_FRAGMENTATION_THRESHOLD)
        {
            bytes_to_send = IPV4_FRAGMENTATION_THRESHOLD;
        }       
#endif        
        
#endif
        else
        {
            bytes_to_send = length - *bytes_sent;
        }

        while ((buffer = CUSTOM_ALLOC(bytes_to_send)) == NULL)
        {
            /*Allow small delay to allow other thread to run per Atheros example*/
            _time_delay(1);   // TODO: understand this delay and why its in ATH example.
        }
        
        memcpy(buffer, data + *bytes_sent, bytes_to_send);
        
        send_result = t_send((void *) driver_handle, sock,
                (unsigned char *) (buffer), bytes_to_send, 0);
#if !NON_BLOCKING_TX
/*Free the buffer only if NON_BLOCKING_TX is not enabled*/
        if (buffer)
        {
            CUSTOM_FREE(buffer);
        }
#endif
        
#if 0
        _time_delay( 250 ); // TSEND_HACK
#endif
        if (send_result == A_ERROR)
        {
            return PROCESS_NINFO(ERR_WIFI_SEND_ERROR, NULL);
        }
        else if (send_result == A_SOCK_INVALID)
        {
            /*Socket has been closed by target due to some error, gracefully exit*/
            return PROCESS_NINFO(ERR_WIFI_BAD_SOCKET, NULL);
        }
        else if (send_result == A_WHISTLE_REBOOT)
        {
            // Wifi isn't responding: reboot now
            PWRMGR_Reboot( 500, PV_REBOOT_REASON_WIFI_FAIL, FALSE );
            return PROCESS_NINFO(ERR_WIFI_BAD_SOCKET, NULL); // Return to caller to clean up before reboot completes
        }
        
        wassert(send_result >= 0);
        
        *bytes_sent += send_result;
    } while (*bytes_sent < length);
    
    return ERR_OK;
}

/*
 *  Read bytes on a UDP or TCP socket and write the bytes to a buffer. 
 *  Returns the number of bytes read from the socket.
 *  
 *  When dealing with receives that could span multiple TCP transmissions / segments
 *  then allocated buffer and buf_len needs to be CFG_PACKET_SIZE_MAX_RX
 */

static int_32 _WIFIMGR_sock_recv( ATH_SOCKET_CONTEXT *psock_cxt, int sock, void *buf, uint_32 buf_len )
{
    int_32 bytes_received;
    
    switch ( psock_cxt->type )
    {
        case SOCK_DGRAM_TYPE:
        {
            SOCKADDR_T local_addr;
            uint_32 local_addr_len;
            memset(&local_addr, 0, sizeof(local_addr));
            bytes_received = t_recvfrom((void *) driver_handle, sock, buf,
                    buf_len, 0, &local_addr, &local_addr_len);
        }
            break;
        case SOCK_STREAM_TYPE:
            bytes_received = t_recv((void *) driver_handle, sock, buf,
                    buf_len, 0);
            break;
        default:
            wassert(0);
    }       
    
    return bytes_received;
}

void WIFIMGR_print_not_connected(void)
{
    corona_print("!conn");
}

// Receive data from a socket.
// Pointer to received data is returned in buf. Wait up to ms ms (0 = forever).
// Return number of bytes received.
// continuous_aggregate - will continue to receive on the socket until 

err_t WIFIMGR_receive(int sock, void *buf, uint_32 buf_len, uint_32 init_timeout, int_32 *pTotalReceived, boolean continuous_aggregate)
{
    int_32 num_bytes_rcvd = 0;
    uint_8 *pBuf = (uint_8*) buf;
    uint_32 timeout = init_timeout;
    int_8 tselect_retries = 0;
    A_INT32 a_rc = A_OK;
    err_t w_rc = ERR_GENERIC;
    ATH_SOCKET_CONTEXT *psock_cxt = get_ath_sock_context( sock );
    
    wassert(!ZERO_COPY);
    
    *pTotalReceived = 0;
    
    if (PWRMGR_is_rebooting())
    {
        WIFIMGR_print_not_connected();
    	return ERR_WIFI_NOT_CONNECTED;
    }
	
	if (0 == driver_handle)
	{
        return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
	}
    
    /* block init_timeout ms or until activity on socket */
	do {
		if (!isConnected)
		{
		    WIFIMGR_print_not_connected();
	    	return ERR_WIFI_NOT_CONNECTED;
		}
		a_rc = t_select((void *) _whistle_handle, sock, timeout);
    	if (A_OK == a_rc)
    	{
    	    if (!isConnected)
    	    {
    	        WIFIMGR_print_not_connected();
    	    	return ERR_WIFI_NOT_CONNECTED;
    	    }
    		// Receive bytes from the socket
    		num_bytes_rcvd = _WIFIMGR_sock_recv( psock_cxt, sock, (void *) pBuf, (buf_len - *pTotalReceived) );

    		if ( num_bytes_rcvd < 0 )
    		{
    			goto sock_recv_error;
    		}

    		// DEBUG__
    		corona_print("rx %d\n", num_bytes_rcvd);

    		*pTotalReceived += num_bytes_rcvd;  
    		pBuf += num_bytes_rcvd;
    		// Use a small (but not zero) timeout for following t_select() calls. A zero 
    		// timeout causes t_select() to block indefinitely
    		timeout = WIFI_RECEIVE_CONTINUOUS_AGGREGATE_RETRY_TIMEOUT; 

    		if (!continuous_aggregate)
    		{
    			break;
    		}
    		
    	}
    	else if ( A_ERROR == a_rc )
    	{
    		tselect_retries++;
    		corona_print("tsel rtry (%d)\n", tselect_retries);
    		// Reset timeout to be the smaller of init_timeout request or WIFI_RECEIVE_TSELECT_ERROR_RETRY_TIMEOUT
    		// Doing this because WIFI_RECEIVE_CONTINUOUS_AGGREGATE_RETRY_TIMEOUT is too agressive
    		timeout = ( init_timeout < WIFI_RECEIVE_TSELECT_ERROR_RETRY_TIMEOUT ? init_timeout : WIFI_RECEIVE_TSELECT_ERROR_RETRY_TIMEOUT );
    	}
    } while (( A_OK == a_rc || ( A_ERROR == a_rc && tselect_retries <= WIFI_RECEIVE_TSELECT_ERROR_RETRY_MAX_COUNT)) && 
    		*pTotalReceived < buf_len );
			// Only other possible a_rc return code is A_SOCK_INVALID which we do want to stop looping on
			// thus not including it in the WHILE logic.
    
    // Bytes were received
    corona_print("tlrx %d\n", *pTotalReceived);
    
    //TODO: Remove this debug code
    if (*pTotalReceived > 0 && tselect_retries > 1)
    	PROCESS_UINFO("rcvd %dx", tselect_retries);
    
    // Found that after successful receipt can error t_select with both SOCK_INVALID and ERROR
    if (A_SOCK_INVALID == a_rc)
    {
    	if (*pTotalReceived > 0)
    	{
    		return ERR_OK;
    	}
    	else
    	{
    		return PROCESS_NINFO(ERR_WIFI_BAD_SOCKET, "tsel invsock");
    	}
    }
    else if ( a_rc != A_OK )
    {
    	if (*pTotalReceived > 0)
    	{
    		return ERR_OK;
    	}
    	else
    	{
    		return PROCESS_NINFO(ERR_WIFI_SELECT_ERROR, "tsel ERR %d", a_rc);
    	}
    }
    else if (*pTotalReceived <= 0)
    {
        return PROCESS_NINFO(ERR_WIFI_RECEIVE_NULL, NULL);
    }
    else
    {
    	if (a_rc != A_OK)
    		corona_print("tsel unknown %d", a_rc);
    	return ERR_OK;
    }
   
sock_recv_error:
	// Bytes were received
	corona_print("tlrx %d\n", *pTotalReceived);

    if (num_bytes_rcvd == A_SOCK_INVALID)
    {
        *pTotalReceived = 0;
        return PROCESS_NINFO(ERR_WIFI_BAD_SOCKET, "rx inval");
    }
    else if ( num_bytes_rcvd == A_WHISTLE_REBOOT)
    {
        // Wifi isn't responding - reboot now
        PWRMGR_Reboot( 500, PV_REBOOT_REASON_WIFI_FAIL, FALSE );
        return PROCESS_NINFO(ERR_WIFI_BAD_SOCKET, "rx fatal"); // Return to caller to clean up before reboot completes
    }
    else
    {
    	return PROCESS_NINFO(ERR_WIFI_BAD_SOCKET, "unknown");
    }
    
    return ERR_GENERIC;
}

// Free a buffer returned by WIFIMGR_recieve()
err_t WIFIMGR_zero_copy_free(void *buf)
{
#if ZERO_COPY    
    zero_copy_free(buf);
#endif
    return ERR_OK;
}

// Scan for SSIDs. Simple scan. No security parameters.
err_t WIFIMGR_scan(ENET_SCAN_LIST *scan_list)
{
    if (PWRMGR_is_rebooting())
    {
    	return ERR_WIFI_NOT_CONNECTED;
    }
	
    WIFIMGR_init();

    // set the RF switch to WiFi
	rf_switch(RF_SWITCH_WIFI, TRUE);
    
    wassert(A_OK == whistle_wmi_iwconfig(scan_list));
    
    // not calling BTMGR_disconnect, so release our vote here.
    PWRMGR_VoteForVLPR( PWRMGR_WIFIMGR_VOTE );

    return ERR_OK;
}

// Scan for SSIDs. Include security parameters in results.
err_t WIFIMGR_ext_scan(ATH_GET_SCAN *ext_scan_list, boolean deep_scan)
{
    WIFIMGR_init();

    // set the RF switch to WiFi
	rf_switch(RF_SWITCH_WIFI, TRUE);
	
	corona_print("SCAN deep=%d\n", deep_scan);
	
	// setup scan control parameters. See AR4100_Programmers_Guide_Confidential.pdf.
	// quick scan finds fewer networks but is better at getting SSID names.
	// deep scan finds more networks, but has problems getting SSID names
	whistle_scan_control(deep_scan);
    
    wassert(A_OK == whistle_ext_scan(ext_scan_list));
    
    // not calling BTMGR_disconnect, so release our vote here.
    PWRMGR_VoteForVLPR( PWRMGR_WIFIMGR_VOTE );

    return ERR_OK;
}

// Get the next SSID from the list of known networks.
// Set firstQuery true for the first query, false for subsequent.
// Returns SSID of next known network or NULL if end of list reached.
err_t WIFIMGR_query_next_ssid(boolean firstQuery, char *ssid)
{
	// index of next network to query
	// -2: no queries active + don't know yet
    // -1: first query found no prior connected networks
    // >=0: index of previous network (will become next)
    static int next_ssid_idx = -2;

    // index of network found in first query
    static int first_ssid_idx;
    
    // Start off with none
    *ssid = 0;
           
    if(0 == g_dy_cfg.wifi_dy.num_networks)
    {
        // no networks to speak of.
        return PROCESS_NINFO(ERR_WIFI_BAD_NETWORK, NULL);
    }
    
    if (firstQuery)
    {
        // first query - check if we've ever been connected
        if (0 != g_dy_cfg.wifi_dy.mru_ssid[0])
        {
            // we've been connected. get who it was.
            int i;
            
            // find its index in the config
            next_ssid_idx = -2; // we don't know it yet
            for (i = 0; i < g_dy_cfg.wifi_dy.num_networks; ++i)
            {
                if (0
                        == strcmp(g_dy_cfg.wifi_dy.network[i].ssid,
                        		g_dy_cfg.wifi_dy.mru_ssid))
                {
                    next_ssid_idx = i;
                    break;
                }
            }
        }
        else
        {
            // first query, but no networks have been connected.
            // -1 forces query to start with index 0.
            next_ssid_idx = -1;
        }
        
        if (next_ssid_idx >= 0)
        {
            // found it
            first_ssid_idx = next_ssid_idx;
            
            strcpy(ssid, g_dy_cfg.wifi_dy.mru_ssid);
            return ERR_OK;
        }
        else if (next_ssid_idx == -2)
        {
            // we were connected, but somehow can't find ourselves in the config (-2)
            return PROCESS_NINFO(ERR_WIFI_BAD_SSID, NULL);
        }
        else
        {
            // we've never been connected but found in config (-1)
            wassert(next_ssid_idx == -1);
        }
    }

    if (next_ssid_idx > -2)
    {
        // subsequent query. move on to the next index in the list, accounting for wrap-around
        next_ssid_idx++;
        
        if (next_ssid_idx == g_dy_cfg.wifi_dy.num_networks)
        {
            // wrap-around
            next_ssid_idx = 0;
        }

        if (firstQuery)
        {
            first_ssid_idx = next_ssid_idx;
        }
        else
        {
            if (next_ssid_idx == first_ssid_idx)
            {
                // we've visited them all. reset to initial values.
                next_ssid_idx = -2;
                corona_print("WF: no more networks\n");
                return ERR_WIFI_BAD_NETWORK;
            }
        }
        
        // select the next one in the list
        strcpy(ssid, g_dy_cfg.wifi_dy.network[next_ssid_idx].ssid);
        return ERR_OK;
    }
    
    return PROCESS_NINFO(ERR_WIFI_BAD_NETWORK, NULL);
}

// Remove a network
err_t WIFIMGR_remove(char *ssid)
{
    int network_idx = -1;
    int i;
    
    for (i = 0; i < g_dy_cfg.wifi_dy.num_networks; ++i)
    {
        if (0 == strcmp(g_dy_cfg.wifi_dy.network[i].ssid, ssid))
        {
            network_idx = i;
            break;
        }
    }

    if (network_idx == -1)
    {
        return PROCESS_NINFO(ERR_WIFI_BAD_NETWORK, NULL);
    }

    memset(&(g_dy_cfg.wifi_dy.network[network_idx]), 0, sizeof(g_dy_cfg.wifi_dy.network[network_idx]));
    
    for (i = network_idx; i + 1 < g_dy_cfg.wifi_dy.num_networks; i++)
    {
        memcpy(&(g_dy_cfg.wifi_dy.network[i]), &(g_dy_cfg.wifi_dy.network[i + 1]), sizeof(g_dy_cfg.wifi_dy.network[i]));
        memset(&(g_dy_cfg.wifi_dy.network[i+1]), 0, sizeof(g_dy_cfg.wifi_dy.network[i + 1]));
    }
    
    --g_dy_cfg.wifi_dy.num_networks;

    // if we just deleted the MRU, pick a new one
    if (0 == strcmp(g_dy_cfg.wifi_dy.mru_ssid, ssid))
    {
    	// force null terminate the entire string regardless if any remain
    	memset(g_dy_cfg.wifi_dy.mru_ssid, 0, sizeof(g_dy_cfg.wifi_dy.mru_ssid));

    	// If any remain, pick the most recently configured.
    	// TODO: Get real fancy and pick the next most recently used
    	if (g_dy_cfg.wifi_dy.num_networks > 0)
    	{
    		strcpy(g_dy_cfg.wifi_dy.mru_ssid, g_dy_cfg.wifi_dy.network[g_dy_cfg.wifi_dy.num_networks - 1].ssid);
    	}
    }
    
    PROCESS_NINFO(STAT_WIFI_DEL, "Del sum x%x %d ssids", str_checksum16(ssid), g_dy_cfg.wifi_dy.num_networks);
    
    CFGMGR_flush( CFG_FLUSH_DYNAMIC );
    
    return ERR_OK;
}

/*
 * Function: WIFIMGR_ATH_GET_SCAN_to_security_param
 * 
 * WARNING: WPA passphrase/WEP key set in security_parameters points to the supplied
 *          wpa_passphrase/wep_key pointer (i.e. dependent on an external object)
 */
err_t WIFIMGR_ATH_GET_SCAN_to_security_param(ATH_SCAN_EXT *scan_list,
        char* wpa_passphrase, char* wep_key, 
        security_parameters_t *out_security_parameters)
{
    A_UINT32    wpa_ucipher;     // atheros_wifi_api.h: ATH_CIPHER_TYPE_*
    CRYPTO_TYPE wpa_mcipher = NONE_CRYPT;
    A_UINT32    wpa2_ucipher;     // atheros_wifi_api.h: ATH_CIPHER_TYPE_*
    CRYPTO_TYPE wpa2_mcipher = NONE_CRYPT;
    A_UINT32    wep_ucipher;     // atheros_wifi_api.h: ATH_CIPHER_TYPE_*
    CRYPTO_TYPE wep_mcipher = NONE_CRYPT;
    enum {
    	WPA,
    	WPA2,
    	WEP,
    	OPEN
    } sec_type = OPEN;
    
    if (scan_list->ssid_len < 0 || scan_list->ssid_len > MAX_SSID_LENGTH - 1)
    {
    	corona_print("\nssid: Illegal %d", scan_list->ssid_len);
    }
	else if (scan_list->ssid_len == 0)
    {
        corona_print("\nssid: Unavail\n");
    }
    else
    {
        char temp_ssid[MAX_SSID_LENGTH];
    	memcpy(temp_ssid, scan_list->ssid, scan_list->ssid_len);
    	temp_ssid[scan_list->ssid_len] = 0;
        corona_print("\nssid: %s\n", temp_ssid);
    }
    
    corona_print("bssid: %x:%x:%x:%x:%x:%x\n", scan_list->bssid[0], scan_list->bssid[1], 
            scan_list->bssid[2], scan_list->bssid[3], 
            scan_list->bssid[4], scan_list->bssid[5]);
    corona_print("chan: %d\n", scan_list->channel);

    corona_print("RSSI: %d\n", scan_list->rssi);
    corona_print("security:");

    /* AP security can support multiple options hence
     * we check each one separately. Note rsn == wpa2 */
    if (scan_list->security_enabled)
    {
        if (scan_list->rsn_auth || scan_list->rsn_cipher)
        {
            //corona_print("\n");
            corona_print("RSN/WPA2= ");
            sec_type = WPA2;
        }

        if (scan_list->rsn_auth)
        {
            corona_print(" {");
            if (scan_list->rsn_auth & SECURITY_AUTH_1X)
            {
                corona_print("802.1X ");
            }

            if (scan_list->rsn_auth & SECURITY_AUTH_PSK)
            {
                corona_print("PSK ");
            }
            corona_print("}");
        }

        if (scan_list->rsn_cipher)
        {
            corona_print(" {");
            if (scan_list->rsn_cipher & ATH_CIPHER_TYPE_TKIP)
            {
            	wpa2_ucipher = ATH_CIPHER_TYPE_TKIP;
            	wpa2_mcipher = TKIP_CRYPT;
                corona_print("TKIP ");
            }

            if (scan_list->rsn_cipher & ATH_CIPHER_TYPE_CCMP)
            {
            	wpa2_ucipher = ATH_CIPHER_TYPE_CCMP;
            	wpa2_mcipher = AES_CRYPT;
                corona_print("AES ");
            }
            corona_print("}");
        }

        if (scan_list->wpa_auth || scan_list->wpa_cipher)
        {
            corona_print("\n");
            corona_print("WPA= ");
            
            /* If both WPA2 and WPA, prefer WPA2 */
            if (WPA2 != sec_type)
            {
            	sec_type = WPA;
            }
        }

        if (scan_list->wpa_auth)
        {
            corona_print(" {");
            if (scan_list->wpa_auth & SECURITY_AUTH_1X)
            {
                corona_print("802.1X ");
            }

            if (scan_list->wpa_auth & SECURITY_AUTH_PSK)
            {
                corona_print("PSK ");
            }
            corona_print("}");
        }

        if (scan_list->wpa_cipher)
        {
            corona_print(" {");
            if (scan_list->wpa_cipher & ATH_CIPHER_TYPE_TKIP)
            {
            	wpa_ucipher = ATH_CIPHER_TYPE_TKIP;
            	wpa_mcipher = TKIP_CRYPT;
                corona_print("TKIP ");
            }

            if (scan_list->wpa_cipher & ATH_CIPHER_TYPE_CCMP)
            {
            	wpa_ucipher = ATH_CIPHER_TYPE_CCMP;
            	wpa_mcipher = AES_CRYPT;
                corona_print("AES ");
            }
            corona_print("}");
        }

        /* it may be old-fashioned WEP this is identified by
         * absent wpa and rsn ciphers */
        if ((scan_list->rsn_cipher == 0) &&
            (scan_list->wpa_cipher == 0))
        {
        	sec_type = WEP;
            wep_ucipher = ATH_CIPHER_TYPE_WEP;
            wep_mcipher = WEP_CRYPT;
            corona_print("WEP ");
        }
    }
    else
    {
        corona_print("NONE");
    }
    corona_print("\n");
    
    
    /*
     *    Populate the security parameter fields, based on what kind of network it is.
     */
    switch(sec_type)
    {
    case WPA:
        out_security_parameters->type = SEC_MODE_WPA;
        out_security_parameters->parameters.wpa_parameters.version = (char*) WPA_PARAM_VERSION_WPA;
        
        // See WARNING in function comments (RE wpa_passphrase)
        out_security_parameters->parameters.wpa_parameters.passphrase = wpa_passphrase;
        out_security_parameters->parameters.wpa_parameters.ucipher = wpa_ucipher;
        out_security_parameters->parameters.wpa_parameters.mcipher = wpa_mcipher;
    	break;

    case WPA2:
        out_security_parameters->type = SEC_MODE_WPA;
        out_security_parameters->parameters.wpa_parameters.version = (char*) WPA_PARAM_VERSION_WPA2;
        
        // See WARNING in function comments (RE wpa_passphrase)
        out_security_parameters->parameters.wpa_parameters.passphrase = wpa_passphrase;
        out_security_parameters->parameters.wpa_parameters.ucipher = wpa2_ucipher;
        out_security_parameters->parameters.wpa_parameters.mcipher = wpa2_mcipher;
    	break;
    	
    case WEP:
        out_security_parameters->type = SEC_MODE_WEP;

        // See WARNING in function comments (RE wep_key)
        out_security_parameters->parameters.wep_parameters.key[0] = wep_key;
        out_security_parameters->parameters.wep_parameters.key[1] = "";
        out_security_parameters->parameters.wep_parameters.key[2] = "";
        out_security_parameters->parameters.wep_parameters.key[3] = "";
        out_security_parameters->parameters.wep_parameters.default_key_index = 1; // 1 is key[0]
    	break;
    	
    case OPEN:
        out_security_parameters->type = SEC_MODE_OPEN;
    	break;
    	
    default:
        WPRINT_ERR("dec mciph");
        return -4;
    }
        
    return MQX_OK;
}

err_t WIFIMGR_set_RX_buf(int8_t bufCount)
{
    err_t res;
    res = custom_ipconfig_set_tcp_rx_buffer((void *) driver_handle, bufCount);
    if (res == A_OK)
        return ERR_OK;
    else
        return ERR_GENERIC;
}

void WIFIMGR_list(void)
{
    int status;
    int i;
    cfg_network_t *pnetwork;
       
    for (i = 0; i < g_dy_cfg.wifi_dy.num_networks; ++i)
    {
        if (0 != g_dy_cfg.wifi_dy.network[i].ssid[0])
        {
            pnetwork = &g_dy_cfg.wifi_dy.network[i];
            
            corona_print("%s\n", pnetwork->ssid);
            
            switch (pnetwork->security_type)
            {
                case SEC_MODE_OPEN:
                    corona_print("OPEN\n");
                    break;
                    
                case SEC_MODE_WPA:
                    corona_print("%s\n", pnetwork->security_parameters.wpa.version);
                    corona_print("pw %s\n", pnetwork->security_parameters.wpa.passphrase);
                    switch(pnetwork->security_parameters.wpa.ucipher)
                    {
                        case ATH_CIPHER_TYPE_TKIP:
                            corona_print("ucip TKIP\n");
                            break;
                        case ATH_CIPHER_TYPE_CCMP:
                            corona_print("ucip CCMP\n");
                            break;
                        case ATH_CIPHER_TYPE_WEP:
                            corona_print("ucip WEP\n");
                            break;
                        default:
                            corona_print("ucip (?)\n");
                            break;
                    }
                    
                    switch(pnetwork->security_parameters.wpa.mcipher)
                    {
                        
                        case NONE_CRYPT:
                            corona_print("mcip none\n");
                            break;
                        case WEP_CRYPT:
                            corona_print("mcip WEP\n");
                            break;
                        case TKIP_CRYPT:
                            corona_print("mcip TKIP\n");
                            break;
                        case AES_CRYPT:
                            corona_print("mcip AES\n");
                            break;
                        default:
                            corona_print("mcip: (?)\n");
                            break;
                    }
                    break;
                    
                        case SEC_MODE_WEP:
                            corona_print("type WEP\n");
                            corona_print("key0 %s\n", pnetwork->security_parameters.wep.key0);
                            corona_print("key1 %s\n", pnetwork->security_parameters.wep.key1);
                            corona_print("key2 %s\n", pnetwork->security_parameters.wep.key2);
                            corona_print("key3 %s\n", pnetwork->security_parameters.wep.key3);
                            corona_print("default %d\n", pnetwork->security_parameters.wep.default_key);
                            break;
                            
                        default:
                            wassert(0);
            }
        }
        corona_print("\n\n");
    }
}

#if 0
/*
 * Tried to get this to work but looks like there is a missalignment of received data. Also, seems like MSS may be auto set.
 */
#define WIFIMGR_GET_MAX_SEG_BUFFER_LENGTH		20
err_t WIFIMGR_get_max_seg(int sock)
{
	err_t res;
	SOCK_OPT_T options;
	int_32 i;
	uint_8 *ptr;
	
	ptr = (uint_8*) &options;

	memset(ptr, 0xFF, sizeof(options));
	
	if (0 == driver_handle)
	{
        return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
	}
    
    if (!isConnected)
    {
        return PROCESS_NINFO(ERR_WIFI_NOT_CONNECTED, NULL);
    }

    res = t_getsockopt((void *) driver_handle, sock, 0, SO_MAXMSG, (uint_8*) &options, WIFIMGR_GET_MAX_SEG_BUFFER_LENGTH);
    printf("sock opt: %x |\n", res);
    for (i = 0; i < sizeof(options); i++)
    {
    	printf(" %.2X", ptr[i]);
    }
    printf("\n");

//    res = t_getsockopt((void *) driver_handle, sock, 1, SO_MAXMSG, (uint_8*) &options, WIFIMGR_GET_MAX_SEG_BUFFER_LENGTH);
//    printf("sock opt: %x |\n", res);
//    for (i = 0; i < sizeof(options); i++)
//    {
//    	printf(" %.2X", ptr[i]);
//    }
//    printf("\n");
//    res = t_getsockopt((void *) driver_handle, sock, ATH_IPPROTO_TCP, SO_MAXMSG, (uint_8*) &options, WIFIMGR_GET_MAX_SEG_BUFFER_LENGTH);
//    printf("sock opt: %x |\n", res);
//    for (i = 0; i < sizeof(options); i++)
//    {
//    	printf(" %.2X", ptr[i]);
//    }
//    printf("\n");
//
//    res = t_getsockopt((void *) driver_handle, sock, 0, SO_KEEPALIVE, (uint_8*) &options, WIFIMGR_GET_MAX_SEG_BUFFER_LENGTH);
//    printf("sock opt: %x |\n", res);
//    for (i = 0; i < sizeof(options); i++)
//    {
//    	printf(" %.2X", ptr[i]);
//    }
//    printf("\n");
//    res = t_getsockopt((void *) driver_handle, sock, 1, SO_KEEPALIVE, (uint_8*) &options, WIFIMGR_GET_MAX_SEG_BUFFER_LENGTH);
//    printf("sock opt: %x |\n", res);
//    for (i = 0; i < sizeof(options); i++)
//    {
//    	printf(" %.2X", ptr[i]);
//    }
//    printf("\n");
//    res = t_getsockopt((void *) driver_handle, sock, ATH_IPPROTO_TCP, SO_KEEPALIVE, (uint_8*) &options, WIFIMGR_GET_MAX_SEG_BUFFER_LENGTH);
//    printf("sock opt: %x |\n", res);
//    for (i = 0; i < sizeof(options); i++)
//    {
//    	printf(" %.2X", ptr[i]);
//    }
//    printf("\n");
    
    
    return ERR_OK;
}
#endif
