/*
 * config.h
 *
 *  Created on: Mar 15, 2013
 *      Author: Chris
 */

#ifndef CONFIG_H_
#define CONFIG_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "ar4100p_main.h"
#include "atheros_wifi_api.h"
#include "bt_mgr.h"
#include "wmi.h"
#include "app_errors.h"
#include "cfg_flags.h"
#include "cfg_factory.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define CFG_WIFI_MAX_NETWORKS           10 // max we will ever support (working max is max_networks)
                                           // Never reduce CFG_WIFI_MAX_NETWORKS. Increase OK.
#define MAX_SERVER_STRING_SIZE          64
#define MAX_DOGNAME_STRING_SIZE         32
#define CFG_BT_DEVNAME_STRING_MAX_SIZE  32

#define MAX_HOST_STRING_SIZE            64
#define MAX_SERIAL_STRING_SIZE          16
#define MAX_PARTNUM_STRING_SIZE         16
#define MAX_BT_REMOTES                   6

#define FULLSCALE_2G                    (0)
#define FULLSCALE_4G                    (1)
#define FULLSCALE_8G                    (2)
#define FULLSCALE_16G                   (3)

#if 0
    /*
     *   Defining battery thresholds in counts is depcrecated.  
     *   Everything is managed at the millivolt level now.
     */
    
    // VBAT low battery threshold (ADC counts)
    #define LOW_VBAT_THRESHOLD         (34500)  // go into ship mode, batt too low for SPI writes.
    #define LOW_VBAT_CRUMB_THRESHOLD   (35000)  // drop crumb, and go into ship mode.
    
    // VBAT battery low enough to hold of FW updates.
    #define LOW_BATTERY_FWU_THRESH     (37500)

#else

    // All units here are in millivolts.
    // Internal to the app
    #define LOW_VBAT_THRESHOLD         (3400)  // drop untimely crumb and go into ship mode, batt too low for SPI writes.
    #define LOW_VBAT_FWU_THRESH        (3765)  // VBAT battery low enough to hold of FW updates.

    // Only battery indication to the user on button press.  NO ROBODOG
    #define LOW_VBAT_ALERT_THRESHOLD   (3719)  // drop crumb and alert user on button press. Currently set at 15%

#endif

// 1 = dev server, 0 or unset = prod server
#define HOST_TO_UPLOAD_DATA_TO          (g_st_cfg.fac_st.flags & CFG_FLAGS_USE_DEV_SERVER ? "app-staging.whistle.com" : "app.whistle.com")


////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

/*
 *   Flush Type
 *   
 *   DYNAMIC:  Flush dynamic (g_dy_cfg) only.
 *   STATIC:   Flush static  (g_st_cfg) only.
 *   ALL:      Flush dynamic and static configs.
 */
typedef enum config_flush_type
{
    CFG_FLUSH_DYNAMIC = 0x1,
    CFG_FLUSH_STATIC  = 0x2,
    CFG_FLUSH_ALL     = (CFG_FLUSH_DYNAMIC | CFG_FLUSH_STATIC)
} cfg_flush_t;

/*
 *   This tag cannot change.  It is how the server tells us which config it wants to set/read.
 */
typedef enum server_tag
{
    CFG_DOG_NAME        = 0,
    CFG_NETWORK         = 1,
    CFG_TIMEZONE_OFFSET = 2,
    CFG_MIN_ACT_GOAL    = 3
    
}server_tag_t;


//#pragma pack(push, 1)

typedef struct cfg_network_type
{
    char ssid[MAX_SSID_LENGTH];
    int  security_type; // ar4100p_main.h: SEC_MODE_*
    union security_parameters_u
    {
        struct wpa_s
        {
            char        version[WIFI_MAX_WPA_VER_SIZE]; // Note this max size if leveraged in strncpys in the code
            char        passphrase[MAX_PASSPHRASE_SIZE+1]; // Note this max size if leveraged in strncpys in the code
            A_UINT32    ucipher; // atheros_wifi_api.h: ATH_CIPHER_TYPE_*
            CRYPTO_TYPE mcipher; // wmi.h: CRYPTO_TYPE
        } wpa;

        struct wep_s
        {
            char   key0[MAX_WEP_KEY_SIZE+1]; // Note this max size if leveraged in strncpys in the code
            char   key1[MAX_WEP_KEY_SIZE+1];  // TODO - do we need these other keys?
            char   key2[MAX_WEP_KEY_SIZE+1];
            char   key3[MAX_WEP_KEY_SIZE+1];
            uint8_t default_key;
        } wep;
    }    security_parameters;
}cfg_network_t;

/*
 *   Dynamic Configs
 *   
 *   These configs can be changed by the user/server, which means they must conform to the Proto Bufs 
 *     workflow, so that firmware updates are graceful and painless.
 *     
 *   Dynamic Configs can not change with a firmware update, they only change through user/server setting.
 *   
 *   https://whistle.atlassian.net/wiki/display/COR/CFG+Manager+Design+Thoughts
 *   
 */
typedef struct {
    uint32_t num_networks;          // how many we have configured
    cfg_network_t network[CFG_WIFI_MAX_NETWORKS]; // Never reduce CFG_WIFI_MAX_NETWORKS. Increase OK.
	char mru_ssid[MAX_SSID_LENGTH];
} wifi_dynamic_configs_t;

typedef struct dynamic_config_type
{   
    wifi_dynamic_configs_t wifi_dy;
    
    struct system_information
    {
         uint32_t installFlag;
         uint32_t lastFWUAttempt;
         uint8_t  deviceHasCheckedIn;
    }sysinfo_dy;
    
    struct user_dynamic_configs
    {
        char     dog_name[MAX_DOGNAME_STRING_SIZE];
        uint16_t daily_minutes_active_goal;
        int64_t  timezone_offset;
    }usr_dy;
    
    struct btmgr_dynamic_configs
    {
    	LinkKeyInfo_t LinkKeyInfo[MAX_BT_REMOTES]; // Link Keys for pairing
    	char deviceName[CFG_BT_DEVNAME_STRING_MAX_SIZE];
    	uint32_t mru_index;
    } bt_dy;

} cfg_dynamic_t;


/*
 *   STATIC Configs
 *   
 *   These are configs that simply get overwritten when CFG_STATIC_MAGIC is changed between firmware revisions.
 *   There are no ProtoBuf requirements for Static Configs.
 *   For production release builds, Static Configs could theoretically just be #define factory defaults.
 *      They are kept in static RAM only so we can make debug changes in console and persist.
 *   
 *   https://whistle.atlassian.net/wiki/display/COR/CFG+Manager+Design+Thoughts
 *   
 */
typedef struct static_config_type
{   
    struct wifi_static_configs
    {
        uint32_t max_open_retries;      // how many times we retry to connect
        uint32_t max_connect_retries;   // how many times we retry to connect
        uint32_t connect_wait_timeout;  // how long to wait for a connect event (success or fail)
        uint32_t scan_duration;
        
    } wifi_st;

    struct eventmgr_static_configs
    {
        uint32_t data_dump_seconds;     // Number of seconds to wait before dumping to server
        uint32_t first_lm_data_dump_seconds;    // Number of seconds to wait before dumping after first LM
        uint32_t max_upload_transfer_len;           // Max number of bytes we send per open socket.
        uint32_t upload_chunk_sz_btc;       // Max number of bytes we send per CONMGR_send() BTC
        uint32_t upload_chunk_sz_wifi;       // Max number of bytes we send per CONMGR_send() WiFi
        uint8_t  ignore_checkin_status;
        uint8_t	 debug_hold_evt_rd_ptr; // Stops EVTMGR from moving the rd pointer forward
    } evt_st;
    
    struct power_static_configs
    {
        uint8_t  enable_lls_deep_sleep;
    }pwr_st;

    struct console_static_configs
    {
        uint8_t  log_wakeups;
        uint8_t  enable;
    }cc_st;
    
    struct accel_static_configs
    {
        uint8_t  any_motion_thresh;                 // expressed as a percentage of full scale.
        uint8_t  full_scale;                        // use the same convention as the ST Micro register.
        uint8_t  seconds_to_wait_before_sleeping;   // threshold to define the seconds to wait before going back to sleep.
        uint8_t  shutdown_accel_when_charging;
        uint8_t  continuous_logging_mode;
        uint8_t  compression_type;
        uint8_t  use_fake_data;
        uint8_t  calc_activity_metric;              // This has no effect unless CALCULATE_MINUTES_ACTIVE == 1 in min_act.h
    } acc_st;
    
    struct conmgr_static_configs
    {
         uint32_t port;
         uint8_t  max_seq_parse_failures;                // used to control how many times the datadump process will re-attempt to send data on a successful connection.
         uint32_t max_wait_timeout;
    }con_st;
    
    struct btmgr_static_configs
    {
        uint32_t bt_pairing_timeout;
        uint32_t bt_connecting_timeout;
        uint32_t bt_sync_page_timeout;
        uint32_t bt_ios_prox_page_timeout;
        uint32_t bt_non_ios_prox_page_timeout;
    } bt_st;
    
    struct factory_static_configs
    {
        // https://whistle.atlassian.net/wiki/display/ENG/Device+Serial+and+Config+ID+tags

        char     serial_number[MAX_SERIAL_STRING_SIZE]; // CFG_FACTORY_SER
        char     mpcba[MAX_PARTNUM_STRING_SIZE];        // CFG_FACTORY_MPCBA
        uint32_t mpcbarev;                              // CFG_FACTORY_MPCBAREV
        uint32_t mpcbarw;                               // CFG_FACTORY_MPCBARW
        char     lpcba[MAX_PARTNUM_STRING_SIZE];        // CFG_FACTORY_LPCBA
        uint32_t lpcbarev;                              // CFG_FACTORY_LPCBAREV
        uint32_t lpcbarw;                               // CFG_FACTORY_LPCBARW
        char     tla[MAX_PARTNUM_STRING_SIZE];          // CFG_FACTORY_TLA
        uint32_t flags;                                 // CFG_FACTORY_FLAGS
        uint32_t pddur;                                 // pwr drain test duration
        uint32_t pdfull;                                // pwr drain full charge level mv
        uint32_t pdlow;                                 // pwr drain low level mv
        
        fcfg_board_t    board;
        fcfg_silego_t   silego;
        fcfg_led_t      led;
        fcfg_rfswitch_t rfswitch;
        
    } fac_st;
    
    struct permgr_static_configs
    {
    	uint32_t poll_interval; // Number of seconds between periodic tasks, like polling for BT proximity
    } per_st;
    
    /*
     *   NOTE:  Magic Value and CRC MUST remain at the bottom of this structure !
     */
    uint32_t magic; // Magic number provides a way to reset configs.
    uint32_t crc;
    
} cfg_static_t;

//#pragma pack(pop)

////////////////////////////////////////////////////////////////////////////////
// External Variables
////////////////////////////////////////////////////////////////////////////////

/*
 *   All clients of CFGMGR just use these variable directly, reason = no overhead/simplicity.
 *     NOTE: Using g_st_cfg in the encoding/decoding/protobufs stuff is FORBIDDEN!
 */
extern cfg_dynamic_t g_dy_cfg;
extern cfg_static_t  g_st_cfg;

////////////////////////////////////////////////////////////////////////////////
// Public Declarations
////////////////////////////////////////////////////////////////////////////////
void  CFGMGR_init(void);
void  CFGMGR_load_fcfgs(boolean is_setting);
void  CFGMGR_load_factory_defaults(uint8_t flush);
void  CFGMGR_flush( cfg_flush_t type);
void  CFGMGR_req_flush_at_shutdown( void );
void  CFGMGR_postponed_flush( void );
err_t CFGMGR_set_config(server_tag_t tag, const void *pValue);

#endif /* CONFIG_H_ */
