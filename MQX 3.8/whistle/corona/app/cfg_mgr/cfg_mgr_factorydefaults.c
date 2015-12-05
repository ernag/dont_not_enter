/*
 * cfg_mgr_factorydefaults.c
 *
 *  Created on: May 8, 2013
 *      Author: Ernie
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "cfg_mgr.h"
#include "cfg_mgr_priv.h"
#include "cfg_factory.h"
#include "app_errors.h"
#include "corona_ext_flash_layout.h"
#include "ext_flash.h"
#include "wifi_mgr.h"
#include "datamessage.pb.h"
#include "wassert.h"
#include "cormem.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

// This should be decremented every time we change the default config init
#define MAX_DATADUMP_SEND_FAILURES  4

#define MAX_BT_PAIRING_TIMEOUT      (3*60) /* seconds */
#define MAX_BT_CONNECTING_TIMEOUT   (10) /* seconds */
#define MAX_BT_SYNC_PAGE_TIMEOUT    (5000) /* ms */
#define MAX_BT_IOS_PROX_PAGE_TIMEOUT    (2200) /* ms */
#define MAX_BT_NON_IOS_PROX_PAGE_TIMEOUT    (4000) /* ms */
#define MAX_CONNECT_WAIT_TIMEOUT    (25*1000)
#define MAX_WIFI_SCAN_DURATION      10
#define MAX_WAIT_TIMEOUT            15
#define MAX_PRX_POLL_TIMEOUT        (10*60) /* seconds */

#define CFG_DEF_LOCAL_DEVICE_NAME_SEED  "Whistle-" // The basic string that will be appended to
#define CFG_DEF_LOCAL_DEVICE_NAME_UINQ  3 // The number of characters to copy to the end of the seed name.

// Production settings
#if 1

#define LOG_WAKEUPS                     0
#define CONSOLE_DISABLED                0

#define DEFAULT_DATA_DUMP_PERIOD         (60*60)
#define DEFAULT_FIRST_LM_DATA_DUMP_DELAY (10*60)
/*
 *   -- Maximum Number of Bytes to Send in a given HTTP packet. --
 *   
 *     We will send up to the number of event block pointer pages' worth of data,
 *     before exceeding this amount.  In other words if 3 pages points to 93,000 bytes and
 *     4 pages points to 128,000 bytes, we will send only 3 pages worth of evt blk pointers,
 *     for the given packet.
 */
#define ATHEROS_OPTIMAL_CHUNK_SZ		(1440)       // Based on WireShark & CardAccess, the Atheros firmware fragments TCP pkts into chunks this size.
#define EVTMGR_NUM_CHUNKS				(1)          // This is how many factors of optimal TCP payload lengths.  NOTE: 2 seems to work way better than anything else !
#define EVTMGR_WIFI_UPLOAD_CHUNK_SZ     (ATHEROS_OPTIMAL_CHUNK_SZ * EVTMGR_NUM_CHUNKS) // NOTE:  we have to dynamically allocate this much memory, so be careful !
#define EVTMGR_BTC_UPLOAD_CHUNK_SZ      (964)        // results in an RFCOMM payload size of 1001

/*
 *   This is how much we send to the AR4100P for each HTTP transaction.
 *   NOTE:  EA:  I observed that EVTMGR_MAX_PACKET_LEN seemed to be somewhat correlated to MIN_DELAY_BETWEEN_SOCKS in wifi_mgr.c
 */
#define EVTMGR_MAX_PACKET_LEN           (100 * 1040) // This is how much we send in each HTTP transaction.

#define DEFAULT_DATA_IGNORE_CHECKIN     FALSE

#define ACCMGR_ANY_MOTION_THRES         (5)

//#define CFGMGR_HAS_DEFAULT_SSID       // note this won't matter unless it is first device installation, live-with-it.  :-)
#define DEFAULT_SSID                    "Sasha-TEST"
#define DEFAULT_WIFI_SEC_TYPE           SEC_MODE_WPA
#define DEFAULT_WIFI_SEC_VERSION        WPA_PARAM_VERSION_WPA2
#define DEFAULT_WIFI_SEC_PASS           "c0r0nat3st"

#define DEFAULT_WIFI_SEC_UCIPHER        ATH_CIPHER_TYPE_CCMP
#define DEFAULT_WIFI_SEC_MCIPHER        AES_CRYPT

#define MAX_WIFI_OPEN_RETRIES			3

// Note that these are not needed if CALCULATE_MINUTES_ACTIVE is 0
#define DEFAULT_TIMEZONE_OFFSET         ((int64_t) -25200)
#define DEFAULT_MIN_ACTIVE_GOAL         30

#define DEFAULT_ACCEL_SHUTDOWN_ACCEL_WHEN_CHARGE    TRUE
#define DEFAULT_ACCEL_CONTINUOUS_LOGGING_MODE       FALSE
#define DEFAULT_ACCEL_USE_FAKE_DATA                 FALSE
#define DEFAULT_ACCEL_ENABLE_MINUTES_ACTIVE         TRUE // Note that this has no effect if the CALCULATE_MINUTES_ACTIVE is 0
#define DEFAULT_ACCEL_COMPRESSION                   AccelDataEncoding_ACCEL_ENC_DIFF_RICE_K1

#define PORT_TO_UPLOAD_TO               80


// Development settings
#else

#define LOG_WAKEUPS                     1

#define DEFAULT_DATA_DUMP_PERIOD        (5*60)
#define DEFAULT_FIRST_LM_DATA_DUMP_DELAY (1*60)

#define ACCMGR_ANY_MOTION_THRES 4

#define DEFAULT_SSID                    "Sasha"
#define DEFAULT_WIFI_SEC_TYPE           SEC_MODE_WPA
#define DEFAULT_WIFI_SEC_VERSION        "wpa2"
#define DEFAULT_WIFI_SEC_PASS           "walle~12"
#define DEFAULT_WIFI_SEC_UCIPHER        ATH_CIPHER_TYPE_CCMP
#define DEFAULT_WIFI_SEC_MCIPHER        AES_CRYPT

#define DEFAULT_ACCEL_SHUTDOWN_ACCEL_WHEN_CHARGE    FALSE
#define DEFAULT_ACCEL_CONTINUOUS_LOGGING_MODE       FALSE
#define DEFAULT_ACCEL_USE_FAKE_DATA                 FALSE
#define DEFAULT_ACCEL_COMPRESSION                   AccelDataEncoding_ACCEL_ENC_S8

#define PORT_TO_UPLOAD_TO               80

#endif

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Populate a factory string to the provided buffer.
 *   If factory string doesn't exist, memset to zero.
 */
static void _CFGMGR_faccfg_get( fcfg_handle_t *pHandle, 
                                fcfg_tag_t tag, 
                                uint32_t max_len, 
                                void *pVal,
                                const char *pDesc )
{
    fcfg_err_t ferr;
    fcfg_len_t len;
    
    ferr = FCFG_getlen(pHandle, tag, &len);
    if( FCFG_OK != ferr )
    {
    	if (pDesc)
    	{
            corona_print("FCFG %s\tERR %i\tlen\n", pDesc, ferr);
    	}
    	else
    	{
    		corona_print("FCFGlen fail %d\n", ferr);
    	}

        /*
         *   For the serial number case, we can't zero it out, b/c we may want to preserve 
         *     what has been previously set via the MAC Address (unless the fcfg tag is present).
         */
        if( tag != CFG_FACTORY_SER )
        {
            memset(pVal, 0, max_len);
        }
        return;
    }
    
    /*
     *   TODO:  This wassert should be an "ignore" for RELEASE_WHISTLE?
     */
    wassert( len <= max_len );
    
    memset(pVal, 0, max_len);
    
    ferr = FCFG_get(pHandle, tag, pVal);
    if( FCFG_OK != ferr )
    {
    	if (pDesc)
    	{
    		corona_print("FCFG: %s:\tERR: %i\tFCFG_get\n", pDesc, ferr);
    	}
    	else
    	{
    		PROCESS_NINFO(ERR_GENERIC, "fcfg_get %d", ferr);
    	}
        return;
    }
    
    if( FCFG_INT_SZ == len )
    {
        uint32_t val;
        
        memcpy(&val, pVal, FCFG_INT_SZ);
        
        if (pDesc)
        {
        	corona_print("FCFG %s\tTAG %i\tVAL %u\n", pDesc, tag, val);
        }
    }
    else
    {
    	if (pDesc)
    	{
    		corona_print("FCFG %s\tTAG %i\tVAL %s\n", pDesc, tag, (char *)pVal);
    	}
    }
}

/*
 *   Factory-Set Items
 */
void CFGMGR_load_fcfgs(boolean is_setting)
{
    fcfg_handle_t hdl;
    
    /*
     *   Init the handle, functions to use when talking to the factory config table.
     */
    wassert( FCFG_OK == FCFG_init(&hdl,
                                  EXT_FLASH_FACTORY_CFG_ADDR,
                                  &wson_read,
                                  &wson_write,
                                  &_lwmem_alloc_system_zero,
                                  &_lwmem_free) );
    
	/* Gotta have a serial number. Poll for it forever */
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_SER,      MAX_SERIAL_STRING_SIZE,  g_st_cfg.fac_st.serial_number, NULL );
    if (0 == g_st_cfg.fac_st.serial_number[9])
	{	
		PROCESS_NINFO( ERR_NO_SER_IN_FCFG, "bad SER" );

		if (is_setting)
		{
			return;
		}
		
		while (0 == g_st_cfg.fac_st.serial_number[9])
		{
			_time_delay(1000);
		    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_SER,      MAX_SERIAL_STRING_SIZE,  g_st_cfg.fac_st.serial_number, NULL );
		}
		
		if (is_setting)
		{
			return;
		}
	}

    /*
     *   Fill up all the factory config values, if they are present.
     */
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_SER,      MAX_SERIAL_STRING_SIZE,  g_st_cfg.fac_st.serial_number, "SER"    );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_MPCBA,    MAX_PARTNUM_STRING_SIZE, g_st_cfg.fac_st.mpcba,         "PCBA Part#"   );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_MPCBAREV, FCFG_INT_SZ,             &(g_st_cfg.fac_st.mpcbarev),   "PCBA Rev"      );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_MPCBARW,  FCFG_INT_SZ,             &(g_st_cfg.fac_st.mpcbarw),    "PCBA Rw"   );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_LPCBA,    MAX_PARTNUM_STRING_SIZE, g_st_cfg.fac_st.lpcba,         "LED Part#"    );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_LPCBAREV, FCFG_INT_SZ,             &(g_st_cfg.fac_st.lpcbarev),   "LED Rev"       );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_LPCBARW,  FCFG_INT_SZ,             &(g_st_cfg.fac_st.lpcbarw),    "LED Rw"    );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_TLA,      MAX_PARTNUM_STRING_SIZE, g_st_cfg.fac_st.tla,           "TLA #"    );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_FLAGS,    FCFG_INT_SZ,             &g_st_cfg.fac_st.flags,        "FLAG"         );
#if 0
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_PDDUR,    FCFG_INT_SZ,             &g_st_cfg.fac_st.pddur,        "PDdur"        );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_PDFULL,   FCFG_INT_SZ,             &g_st_cfg.fac_st.pdfull,       "PDful"       );
    _CFGMGR_faccfg_get( &hdl, CFG_FACTORY_PDLOW,    FCFG_INT_SZ,             &g_st_cfg.fac_st.pdlow,        "PDlow"        );
#endif
    
    g_st_cfg.fac_st.board    = FCFG_board(&hdl);
    g_st_cfg.fac_st.led      = FCFG_led(&hdl);
    g_st_cfg.fac_st.silego   = FCFG_silego(&hdl);
    g_st_cfg.fac_st.rfswitch = FCFG_rfswitch(&hdl);
    
    corona_print("BRD\t%u\nLED\t%u\nSIL\t%u\nRF\t%u\n", g_st_cfg.fac_st.board,
                                                        g_st_cfg.fac_st.led,
                                                        g_st_cfg.fac_st.silego,
                                                        g_st_cfg.fac_st.rfswitch);
}

/*
 *   Static Config Factory Defaults
 */
void _CFGMGR_static_defaults(void)
{
    /*
     *   Set memory to zeroes to ensure no foul play with strings.
     */
    memset( &g_st_cfg, 0, sizeof(g_st_cfg) );
    
    corona_print("CFG Defaults\n");
    
    /*
     *   Magic Value allows us to version static configs and change them with
     *      a simple change of the magic number.
     */
    g_st_cfg.magic = CFG_STATIC_MAGIC;
    
    /*
     *   PWRMGR STATIC
     */
    g_st_cfg.pwr_st.enable_lls_deep_sleep = 1;
    
    /*
     *   Corona Console STATIC
     */
    g_st_cfg.cc_st.log_wakeups = LOG_WAKEUPS;
    g_st_cfg.cc_st.enable = CONSOLE_DISABLED;
    
    /*
     *   WIFIMGR STATIC
     */
    g_st_cfg.wifi_st.max_open_retries = MAX_WIFI_OPEN_RETRIES;
    g_st_cfg.wifi_st.connect_wait_timeout = MAX_CONNECT_WAIT_TIMEOUT;
    g_st_cfg.wifi_st.scan_duration = MAX_WIFI_SCAN_DURATION;
    
    /*
     *   EVTMGR STATIC
     */
    g_st_cfg.evt_st.data_dump_seconds = DEFAULT_DATA_DUMP_PERIOD;
    g_st_cfg.evt_st.first_lm_data_dump_seconds = DEFAULT_FIRST_LM_DATA_DUMP_DELAY;
    g_st_cfg.evt_st.max_upload_transfer_len = EVTMGR_MAX_PACKET_LEN;
    g_st_cfg.evt_st.upload_chunk_sz_wifi = EVTMGR_WIFI_UPLOAD_CHUNK_SZ;
    g_st_cfg.evt_st.upload_chunk_sz_btc = EVTMGR_BTC_UPLOAD_CHUNK_SZ;
    g_st_cfg.evt_st.ignore_checkin_status = DEFAULT_DATA_IGNORE_CHECKIN;
    g_st_cfg.evt_st.debug_hold_evt_rd_ptr = 0;

    /*
     *   CONMGR STATIC
     */
    g_st_cfg.con_st.port = PORT_TO_UPLOAD_TO;
    g_st_cfg.con_st.max_seq_parse_failures = MAX_DATADUMP_SEND_FAILURES;
    g_st_cfg.con_st.max_wait_timeout = MAX_WAIT_TIMEOUT;
    
    /*
     *   ACCMGR STATIC
     */
    g_st_cfg.acc_st.any_motion_thresh = ACCMGR_ANY_MOTION_THRES; // % of full-scale
    g_st_cfg.acc_st.full_scale = FULLSCALE_8G;
    g_st_cfg.acc_st.seconds_to_wait_before_sleeping = 5;
    g_st_cfg.acc_st.shutdown_accel_when_charging = DEFAULT_ACCEL_SHUTDOWN_ACCEL_WHEN_CHARGE;
    g_st_cfg.acc_st.continuous_logging_mode = DEFAULT_ACCEL_CONTINUOUS_LOGGING_MODE;
    g_st_cfg.acc_st.compression_type = DEFAULT_ACCEL_COMPRESSION;
    g_st_cfg.acc_st.use_fake_data = DEFAULT_ACCEL_USE_FAKE_DATA;
    g_st_cfg.acc_st.calc_activity_metric = DEFAULT_ACCEL_ENABLE_MINUTES_ACTIVE; // Note that this has no effect if the CALCULATE_MINUTES_ACTIVE is 0

    /*
     *   BT STATIC
     */
    g_st_cfg.bt_st.bt_pairing_timeout = MAX_BT_PAIRING_TIMEOUT;
    g_st_cfg.bt_st.bt_connecting_timeout = MAX_BT_CONNECTING_TIMEOUT;
    g_st_cfg.bt_st.bt_sync_page_timeout = MAX_BT_SYNC_PAGE_TIMEOUT;
    g_st_cfg.bt_st.bt_ios_prox_page_timeout = MAX_BT_IOS_PROX_PAGE_TIMEOUT;
    g_st_cfg.bt_st.bt_non_ios_prox_page_timeout = MAX_BT_NON_IOS_PROX_PAGE_TIMEOUT;
    
    /*
     * PRXMGR STATIC
     */
    g_st_cfg.per_st.poll_interval = MAX_PRX_POLL_TIMEOUT;
}

/*
 *   Dynamic Config Factory Defaults
 */
void _CFGMGR_dynamic_defaults(void)
{
#ifdef CFGMGR_HAS_DEFAULT_SSID
    security_parameters_t security_parameters1;
#endif
    int w_rc;
    
    // Init the whole structure to ensure no foul play with strings.
    memset( &g_dy_cfg, 0, sizeof(g_dy_cfg) );
    
    /*
     *   System Info DYNAMIC
     */
    g_dy_cfg.sysinfo_dy.installFlag = 0;
    g_dy_cfg.sysinfo_dy.lastFWUAttempt = 0;
    g_dy_cfg.sysinfo_dy.deviceHasCheckedIn = 0;

    /*
     *   User Info DYNAMIC
     */
    // Note that these are not used if the CALCULATE_MINUTES_ACTIVE is 0
    strcpy(g_dy_cfg.usr_dy.dog_name, "My Dog\0");
    g_dy_cfg.usr_dy.daily_minutes_active_goal = DEFAULT_MIN_ACTIVE_GOAL;
    g_dy_cfg.usr_dy.timezone_offset = DEFAULT_TIMEZONE_OFFSET;
    
    g_dy_cfg.wifi_dy.num_networks = 0;
    
    strncpy(g_dy_cfg.bt_dy.deviceName, CFG_DEF_LOCAL_DEVICE_NAME_SEED, CFG_BT_DEVNAME_STRING_MAX_SIZE);
    if (strlen(CFG_DEF_LOCAL_DEVICE_NAME_SEED) + CFG_DEF_LOCAL_DEVICE_NAME_UINQ < CFG_BT_DEVNAME_STRING_MAX_SIZE)
    {
        strncpy(g_dy_cfg.bt_dy.deviceName + strlen(g_dy_cfg.bt_dy.deviceName), //Dest: appending to end of device_name seed
                g_st_cfg.fac_st.serial_number + strlen(g_st_cfg.fac_st.serial_number) - CFG_DEF_LOCAL_DEVICE_NAME_UINQ, //Src: last 3 digits of the serial_number
                CFG_DEF_LOCAL_DEVICE_NAME_UINQ);
        
    }
    
#ifdef CFGMGR_HAS_DEFAULT_SSID
    security_parameters1.type = DEFAULT_WIFI_SEC_TYPE;
    security_parameters1.parameters.wpa_parameters.version = (char*) DEFAULT_WIFI_SEC_VERSION;
    security_parameters1.parameters.wpa_parameters.passphrase = DEFAULT_WIFI_SEC_PASS;
    security_parameters1.parameters.wpa_parameters.ucipher = DEFAULT_WIFI_SEC_UCIPHER;
    security_parameters1.parameters.wpa_parameters.mcipher = DEFAULT_WIFI_SEC_MCIPHER;
    
    /*
     *   Save/Set the default SSID.
     */
    w_rc = WIFIMGR_set_security(DEFAULT_SSID, &security_parameters1, WIFIMGR_SECURITY_FLUSH_LATER);
    if (0 != w_rc)
    {
        corona_print("CON_MGR_UT: con_mgr_ut1: WIFIMGR_set_authentication failed %d\n", w_rc);
        wassert(0);
    }
#endif
}

/*
 *   Loads factory default settings into config RAM struct.
 *     flush:  TRUE if you want factory defaults to persist, FALSE for just loading.
 */
void CFGMGR_load_factory_defaults(uint8_t flush)
{
    _CFGMGR_static_defaults();
    _CFGMGR_dynamic_defaults();

    if(flush)
    {
        CFGMGR_flush( CFG_FLUSH_ALL );
    }
}
