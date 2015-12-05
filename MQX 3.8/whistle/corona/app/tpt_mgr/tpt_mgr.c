/*
 * tpt_mgr.c
 *
 *  Created on: Mar 19, 2013
 *      Author: Chris
 */

#include "wassert.h"
#include "wifi_mgr.h"
#include "tpt_mgr.h"
#include "sys_event.h"
#include "con_mgr.h"
#include "app_errors.h"
#include "bt_mgr.h"
#include "pmem.h"
#include "cfg_mgr.h"

static TPTMGR_transport_type_t g_current_transport_type;

// Connect to the best transport available.
static err_t TPTMGR_WIFIMGR_connect(void)
{
    err_t e_rc = ERR_OK;
    
    g_current_transport_type = TPTMGR_TRANSPORT_TYPE_WIFI; // even if we fail
    
    e_rc = WIFIMGR_connect(NULL, NULL, FALSE); // Any SSID
        
    return e_rc;
}

static err_t TPTMGR_BTMGR_connect(TPTMGR_transport_type_t transport_type)
{
    err_t e_rc = ERR_OK;
    BD_ADDR_t ANY_ADDR;
    LmMobileStatus lmMobileStatus = LmMobileStatus_MOBILE_STATUS_NO_WAN;
        
    ASSIGN_BD_ADDR(ANY_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); // NULL means any

	BTMGR_request();
    
    switch (transport_type)
    {
        case TPTMGR_TRANSPORT_TYPE_BTC:
        	g_current_transport_type = TPTMGR_TRANSPORT_TYPE_BTC; // even if we fail

        	e_rc = BTMGR_connect(ANY_ADDR, g_st_cfg.bt_st.bt_sync_page_timeout, &lmMobileStatus);
            break;
            
        case TPTMGR_TRANSPORT_TYPE_BTLE:
        	g_current_transport_type = TPTMGR_TRANSPORT_TYPE_BTLE; // even if we fail

        	wassert(0); // BTMGR_connect(BTLE);
            break;
            
        default:
            wassert(0);
    }
    
    if (ERR_OK == e_rc)
    {
    	switch (lmMobileStatus)
    	{
    	case LmMobileStatus_MOBILE_STATUS_OK:
#ifndef NO_PHONE_SUPPORT_LM_TO_DDOBT_SESSION
    	case LmMobileStatus_MOBILE_STATUS_DEV_MGMT:
#endif
    		break;
    	default:
    		corona_print("LM mode. No DD\n");
    		e_rc = ERR_BT_NOT_CONNECTED;
    		BTMGR_disconnect();
    		BTMGR_release();

    		break;
    	}
    }
    else
    {
    		BTMGR_release();
    }
    
    return e_rc;
}

static err_t TPTMGR_ANY_connect(void)
{
    err_t e_rc;
    
    if (pmem_flag_get(PV_FLAG_PREFER_WIFI))
    {
    	// Force WiFi first
    	e_rc = TPTMGR_WIFIMGR_connect();
    	
        if (ERR_OK != e_rc)
        {
        	e_rc = TPTMGR_BTMGR_connect(TPTMGR_TRANSPORT_TYPE_BTC);
        }
    }
    else
    {
    	// default to BT first
    	// TODO: Sky is the limit on what kind of policy to replace this with
    	e_rc = TPTMGR_BTMGR_connect(TPTMGR_TRANSPORT_TYPE_BTC);

        if (ERR_OK != e_rc)
        {
        	e_rc = TPTMGR_WIFIMGR_connect();
        }    	
    }

    return e_rc; // for now
}

static err_t TPTMGR_WIFIMGR_disconnect(void)
{
    return WIFIMGR_disconnect();
}

static err_t TPTMGR_BTMGR_disconnect(TPTMGR_transport_type_t transport_type)
{
    err_t e_rc = ERR_OK;
    
    switch (transport_type)
    {
        case TPTMGR_TRANSPORT_TYPE_BTC:
            e_rc = BTMGR_disconnect();
            break;
            
        case TPTMGR_TRANSPORT_TYPE_BTLE:
            // BTMGR_disconnect(BTLE);

            wassert(0);
            break;
            
        default:
            wassert(0);
    }
    
    return e_rc;
}

static void TPTMGR_disconnected(sys_event_t event)
{
	// No corona_print here: too much stack on sys_event thread
	
	CONMGR_disconnected(event);
}

// Public

// Connect to transport type transport_type
err_t TPTMGR_connect(TPTMGR_transport_type_t requested_transport_type, TPTMGR_transport_type_t *actual_transport_type)
{
    err_t e_rc = ERR_OK;
    
    corona_print("TRY TO CONNECT %d\n", requested_transport_type);
    
    switch (requested_transport_type)
    {
        case TPTMGR_TRANSPORT_TYPE_WIFI:
            e_rc = TPTMGR_WIFIMGR_connect();
            break;
            
        case TPTMGR_TRANSPORT_TYPE_BTC:
            e_rc = TPTMGR_BTMGR_connect(TPTMGR_TRANSPORT_TYPE_BTC);
            break;
            
        case TPTMGR_TRANSPORT_TYPE_BTLE:
            e_rc = TPTMGR_BTMGR_connect(TPTMGR_TRANSPORT_TYPE_BTLE);
            break;
            
        case TPTMGR_TRANSPORT_TYPE_ANY:
            e_rc = TPTMGR_ANY_connect();
            break;
            
        default:
            wassert(0);
    }
    
    corona_print("CONN %x %d\n", e_rc, g_current_transport_type);
    
    // Even if it failed, let the caller know what type was attempted
    *actual_transport_type = g_current_transport_type;

    return e_rc;
}

// Disconnect transport
err_t TPTMGR_disconnect(void)
{
    err_t e_rc = ERR_OK;
    
	corona_print("TPTMGR_disconnect type %d\n", g_current_transport_type);

    switch (g_current_transport_type)
    {
        case TPTMGR_TRANSPORT_TYPE_WIFI:
            e_rc = TPTMGR_WIFIMGR_disconnect();
            break;
            
        case TPTMGR_TRANSPORT_TYPE_BTC:
            e_rc = TPTMGR_BTMGR_disconnect(TPTMGR_TRANSPORT_TYPE_BTC);
            break;
            
        case TPTMGR_TRANSPORT_TYPE_BTLE:
            e_rc = TPTMGR_BTMGR_disconnect(TPTMGR_TRANSPORT_TYPE_BTLE);
            break;

        default:
            wassert(0);
    }

    return e_rc;
}

void TPTMGR_init(void)
{
	sys_event_register_cbk(TPTMGR_disconnected, SYS_EVENT_WIFI_DOWN | SYS_EVENT_BT_DOWN);
}
