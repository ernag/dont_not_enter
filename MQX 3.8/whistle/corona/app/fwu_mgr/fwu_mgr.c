/*
 * fwu_mgr.c
 *
 *  Created on: May 8, 2013
 *      Author: Rob Philip
 */


////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <string.h>
#include <mqx.h>
#include <watchdog.h>
#include "fwu_mgr.h"
#include "fwu_mgr_priv.h"
#include "con_mgr.h"
#include "wifi_mgr.h"
#include "cfg_mgr.h"
#include "evt_mgr.h"
#include "ext_flash.h"
#include "sys_event.h"
#include "corona_ext_flash_layout.h"
#include "corona_console.h"
#include "crc_util.h"
#include "wassert.h"
#include "pb.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "pb_helper.h"
#include "whistlemessage.pb.h"
#include "wmp.h"
#include "whttp.h"
#include "fw_info.h"
#include "parse_fwu_pkg.h"
#include "adc_driver.h"
#include "pwr_mgr.h"
#include "fwu_wmp.c"
#include "app_errors.h"
#include "bark.h"
#include "pmem.h"
#include "cormem.h"


////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////


#define GIVE_WHISTLE_FEEDBACK_ON_FWU        (0)  // ! should be set to zero for real releases !

#define FWUMGR_PERSISTENT_DATA_SIZE         (EXT_FLASH_4KB_SZ)
#define FWUMGR_PERSISTENT_DATA_START        (EXT_FLASH_FWUPDATE_INFO_ADDR)
#define FWUMGR_PERSISTENT_DATA_END          (FWUMGR_PERSISTENT_DATA_START + FWUMGR_PERSISTENT_DATA_SIZE - 1)

#define FWUMGR_FIRMWARE_IMAGE_SIZE          (EXT_FLASH_64KB_SZ * EXT_FLASH_APP_IMG_NUM_SECTORS)
#define FWUMGR_FIRMWARE_IMAGE_START         (EXT_FLASH_APP_IMG_ADDR)
#define FWUMGR_FIRMWARE_IMAGE_END           (FWUMGR_FIRMWARE_IMAGE_START + FWUMGR_FIRMWARE_IMAGE_SIZE - 1)

#define FWUMGR_BACKUP_IMAGE_SIZE            (EXT_FLASH_64KB_SZ * EXT_FLASH_APP_IMG_BKP_NUM_SECTORS)
#define FWUMGR_BACKUP_IMAGE_START           (EXT_FLASH_APP_IMG_BKP_ADDR)
#define FWUMGR_BACKUP_IMAGE_END             (FWUMGR_BACKUP_IMAGE_START + FWUMGR_BACKUP_IMAGE_SIZE - 1)

#ifndef  FWUMGR_PRINT
#define FWUMGR_PRINT    corona_print
#endif

#define FWUMGR_CONMGR_CONNECT_SUCCEEDED_EVENT  (1 << 0)
#define FWUMGR_CONMGR_CONNECT_FAILED_EVENT     (1 << 1)
#define FWUMGR_CONMGR_DISCONNECTED_EVENT       (1 << 2)
#define FWUMGR_CONMGR_BUTTON_EVENT             (1 << 3)
#define FWUMGR_GET_MANIFEST_EVENT              (1 << 4)
#define FWUMGR_GET_PACKAGE_EVENT               (1 << 5)
#define FWUMGR_PACKAGE_FINISHED_EVENT          (1 << 6)
#define FWUMGR_PRINT_PACKAGE_EVENT             (1 << 7)

#define FWUMGR_CONMGR_DONE_EVENT               (1 << 0)

#define FWUMGR_ALL_EVENTS (                                          \
                            FWUMGR_CONMGR_CONNECT_SUCCEEDED_EVENT | \
                            FWUMGR_CONMGR_CONNECT_FAILED_EVENT    | \
                            FWUMGR_CONMGR_DISCONNECTED_EVENT      | \
                            FWUMGR_CONMGR_BUTTON_EVENT            | \
                            FWUMGR_GET_MANIFEST_EVENT             | \
                            FWUMGR_GET_PACKAGE_EVENT              | \
                            FWUMGR_PACKAGE_FINISHED_EVENT         | \
                            FWUMGR_PRINT_PACKAGE_EVENT              \
                          )


#define FWUMGR_MIN_CONMGR_TIME                (0)  // No sooner than the next connection
#define FWUMGR_MAX_CONMGR_TIME                (-1) // No later than the next connection

#define FWUMGR_BUFFER_SIZE                    (1024)

#define FWU_WMP_TX_BUFFER_SIZE                (100)
#define FWU_HTTP_TX_BUFFER_SIZE               (300)

#define FWU_MGR_NO_STATUS                     (0xdeaddead)

#define FLAG_SET                              0
#define FLAG_CLR                              0xff

#define FWUMGR_DOWNLOAD_TIMEOUT_WD          (180 * 1000)  // Amount of time allowed for FWUMGR
                                                       // to download the FW pkg before it
                                                       // times out and reboots

#define FWUMGR_DOWNLOAD_CONSECUTIVE_RETRIES (10)       // The number of times to retry the download
                                                       //  by calling CONMGR_receive

#define FWUMGR_TCP_RECEIVE_TIMEOUT          (15*1000)  // Amount of time allowed for FWUMGR
                                                       // to checkin & req manifest before it
                                                       // times out and reboots
#define FWUMGR_TCP_RECEIVE_TIMEOUT_WD 		(60*1000 + WIFI_RECEIVE_TSELECT_TOTAL_RETRY_TIME + 500)

#define FWUMGR_TCP_SEND_TIMEOUT_WD			(60*1000)

#define FWUMGR_WATCHDOG_DEF_KICK			(60*1000) /* When done with an operation the basic kick to make sure 
 	 	 	 	 	 	 	 	 	 	 	 	 	   * there is enough time to get to the next watchdog
 	 	 	 	 	 	 	 	 	 	 	 	 	   **/

/*
 *  FWUMGR_DL_BUFFER_HACK 
 *  This is used to allow the device to fail on a pkg download if it is less than
 *  this size. This is necsesary because the Atheros firmware isn't passing the
 *  payload of the final TCP message if that TCP message also includes the
 *  TCP FIN flag. This hack also requires that the pkg created has an extra 1500
 *  bytes, which has been added as a 'pad' option to fwu_pkg_gen
 */
#define FWUMGR_DL_BUFFER_HACK                 1500

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

static LWEVENT_STRUCT gfwu_event;
static LWEVENT_STRUCT gfwu_conmgr_done_event;
static CONMGR_handle_t g_fwu_conmgr_handle;
static uint_32 g_fwu_status_to_send;
static boolean g_wifi_disconnected; // WIFI link down

#define FWUMGR_min_wait_time FWUMGR_MIN_CONMGR_TIME
#define FWUMGR_max_wait_time FWUMGR_MAX_CONMGR_TIME


////////////////////////////////////////////////////////////////////////////////
// Code - forward references.
////////////////////////////////////////////////////////////////////////////////

static err_t _FWUMGR_Send_Status_Message(RmFwuStatusPostStatus status);
static err_t _FWUMGR_Check_In(CONMGR_handle_t conmgr_handle, RmCheckInResponseActionType *checkin_pending_action, int64_t *checkin_abs_time, int32_t *checkin_act_goal);
static err_t _FWUMGR_Request_Pkg_URL(char *pkg_url, const size_t manifest_max_len);
static err_t _FWUMGR_get_package(char * package_url);
static err_t _FWUMGR_process_package(void);
static void _FWUMGR_write_last_page(uint32_t flash_address);

////////////////////////////////////////////////////////////////////////////////
// Code - static functions
////////////////////////////////////////////////////////////////////////////////


/*FUNCTION****************************************************************
* 
* Function Name    : _get_version_string_from_hdr
* Returned Value   : None
* Comments         :
*    used to extract a server-compatible version string from package header
*
*END*********************************************************************/

static int _get_version_string_from_hdr(char *ver_str, size_t ver_len, fwu_bin_t *hdr)
{
    char        *out_buf;
    char        tag_buf[APP_VER_STRING_SIZE];
    size_t      out_len;
    char        c;
    int         x;

    memset (tag_buf, 0, sizeof(tag_buf));
    for (x = 0; x < sizeof(hdr->image_version_tag); x++)
    {
        c = hdr->image_version_tag[x];
        if ((c >= ' ') && (c <= '~'))
        {
            tag_buf[x] = c;
        }
        else
        {
            break;
        }
    }

    memset (ver_str, 0, ver_len);
    return snprintf(ver_str, ver_len, "%d.%d-%s", hdr->image_major_version,hdr->image_minor_version,tag_buf);
}


/*FUNCTION****************************************************************
* 
* Function Name    : FWUMGR_wmp_gen_status_message
* Returned Value   : None
* Comments         :
*    construct a WMP status message for the server
*
*END*********************************************************************/

static void _FWUMGR_wmp_gen_status_message(WhistleMessage *msg, uint8_t *encoded_buffer,pb_ostream_t *encode_stream,uint32_t status)
{
    size_t temp_len;
    WMP_sized_string_cb_args_t serial_number_encode_args;
    WMP_sized_string_cb_args_t boot1_encode_args;
    WMP_sized_string_cb_args_t boot2_encode_args; 
    WMP_sized_string_cb_args_t app_encode_args;

    char app_ver_str[APP_VER_STRING_SIZE];
    char boot1_ver_str[APP_VER_STRING_SIZE];
    char boot2_ver_str[APP_VER_STRING_SIZE];

    uint8_t buffer[FWUMGR_BUFFER_SIZE];

    bool res;

    fwu_pkg_t   package;

    uint32_t    pkg_crc;

    /*
    **  read the first part of the package
    */
    wson_read(FWUMGR_FIRMWARE_IMAGE_START,buffer,FWUMGR_BUFFER_SIZE);

    /*
    **  parse it
    */
    parse_pkg(buffer, &package);

    *encode_stream = pb_ostream_from_buffer(encoded_buffer, 100);
    
    msg->has_objectType = true;
    msg->has_transactionType = true;
    msg->has_magicChecksum = false;
    msg->has_serialNumber = true;
    msg->has_dataDump = false;
    msg->has_remoteMgmtMsg = true;
    msg->has_localMgmtMsg = false;
    
    msg->objectType = WhistleMessageType_REMOTE_DEV_MGMT;
    msg->transactionType = TransactionType_TRANS_REQUEST;
    serial_number_encode_args.pPayload = g_st_cfg.fac_st.serial_number; 
    serial_number_encode_args.payloadMaxSize = strlen(g_st_cfg.fac_st.serial_number);
    msg->serialNumber.arg = &serial_number_encode_args;
    msg->serialNumber.funcs.encode = &encode_whistle_str_sized;
    
    msg->remoteMgmtMsg.has_messageType = true;
    msg->remoteMgmtMsg.has_checkInRequest = false;
    msg->remoteMgmtMsg.has_checkInResponse = false;
    msg->remoteMgmtMsg.has_configRequest = false;
    msg->remoteMgmtMsg.has_configResponse = false;
    msg->remoteMgmtMsg.has_fwuManifestRequest = false;
    msg->remoteMgmtMsg.has_fwuManifestResponse = false;
    msg->remoteMgmtMsg.has_fwuStatusPost = true;

    msg->remoteMgmtMsg.messageType = RmMessageType_RM_FWU_STATUS;
    msg->remoteMgmtMsg.fwuStatusPost.has_status = true;
    msg->remoteMgmtMsg.fwuStatusPost.status = status;

    msg->remoteMgmtMsg.fwuStatusPost.has_attemptedBoot1Version = false;
    msg->remoteMgmtMsg.fwuStatusPost.has_attemptedBoot2Version = false;
    msg->remoteMgmtMsg.fwuStatusPost.has_attemptedAppVersion = false;


    if (package.pkg_info.pkg_contents & PKG_CONTAINS_BOOT1)
    {
        msg->remoteMgmtMsg.fwuStatusPost.has_attemptedBoot1Version = true;
        temp_len = _get_version_string_from_hdr(boot1_ver_str, APP_VER_STRING_SIZE, &package.boot1_img);
        boot1_encode_args.pPayload = boot1_ver_str;
        boot1_encode_args.payloadMaxSize = temp_len;
        msg->remoteMgmtMsg.fwuStatusPost.attemptedBoot1Version.arg = &boot1_encode_args;
        msg->remoteMgmtMsg.fwuStatusPost.attemptedBoot1Version.funcs.encode = &encode_whistle_str_sized;

    }

    if (package.pkg_info.pkg_contents & PKG_CONTAINS_BOOT2)
    {
        msg->remoteMgmtMsg.fwuStatusPost.has_attemptedBoot2Version = true;
        temp_len = _get_version_string_from_hdr(boot2_ver_str, APP_VER_STRING_SIZE, &package.boot2_img);
        boot2_encode_args.pPayload = boot2_ver_str;
        boot2_encode_args.payloadMaxSize = temp_len;  
        msg->remoteMgmtMsg.fwuStatusPost.attemptedBoot2Version.arg = &boot2_encode_args;
        msg->remoteMgmtMsg.fwuStatusPost.attemptedBoot2Version.funcs.encode = &encode_whistle_str_sized;
    
    }

    if (package.pkg_info.pkg_contents & PKG_CONTAINS_APP)
    {
        msg->remoteMgmtMsg.fwuStatusPost.has_attemptedAppVersion = true;
        temp_len = _get_version_string_from_hdr(app_ver_str, APP_VER_STRING_SIZE, &package.app_img);
        app_encode_args.pPayload = app_ver_str;
        app_encode_args.payloadMaxSize = temp_len;
        msg->remoteMgmtMsg.fwuStatusPost.attemptedAppVersion.arg = &app_encode_args;
        msg->remoteMgmtMsg.fwuStatusPost.attemptedAppVersion.funcs.encode = &encode_whistle_str_sized;
    }
    
    res = pb_encode(encode_stream, WhistleMessage_fields, msg);
    
    return;
}


/*
** Calculate the CRC for a package in spi flash
** use a small buffer - we have more time than memory.
*/

#define CRC_RX_BUF_SIZE        256

uint32_t FMUMGR_calc_crc_pkg (int spi_address, int pkg_size)
{
    err_t        result;
    uint32_t     spi_addrs = spi_address;
    int32_t     isize  = pkg_size;
    uint32_t     the_crc   = 0;
    uint8_t      rx_buffer[CRC_RX_BUF_SIZE];
    uint8_t      *prx_buf;
    uint8_t      *t_ptr;
    int          buf_size = CRC_RX_BUF_SIZE;
    int          d_size;
    int          i = 0;
    crc_handle_t crc_handle;

    t_ptr = prx_buf = rx_buffer;

    /*
    ** Do the initial read
    */
    wson_read(spi_addrs,prx_buf,buf_size);

    /*
    ** Replace the package CRC and Signature with 0x00's for the CRC calculation.
    */
    memset(&t_ptr[58], 0, 8);

    /*
    **  Reset the CRC hardware
    */
    UTIL_crc32_reset( &crc_handle, CRC_DEFAULT_SEED );

    /*
    ** Do the CRC for the first buffer read
    */
    for (i = 0; i < buf_size; i++)
    {
        the_crc = UTIL_crc_get8(&crc_handle, *t_ptr++);
    }

    /*
    ** Adjust sizes and pointers
    */
    isize     -= buf_size;
    spi_addrs += buf_size;
    t_ptr      = prx_buf;

    /*
    ** Do CRC for the rest of the package
    */
    while (isize > 0)
    {
        /*
        ** Read the next block of data
        */
    	wson_read(spi_addrs,prx_buf,buf_size);

        /*
        ** Adjust the data size to read if near the end
        */
        if (isize >= buf_size)
        {
            d_size = buf_size;
        }
        else
        {
            d_size = isize;
        }
        /*
        ** Compute the CRC for this block of data
        */
        for (i = 0; i < d_size; i++)
        {
        	the_crc = UTIL_crc_get8(&crc_handle, *t_ptr++);
        }
        /*
        ** Adjust the sizes and pointers for next block
        */
        isize     -= d_size;
        spi_addrs += buf_size;
        t_ptr      = prx_buf;
    }
    
    UTIL_crc_release(&crc_handle);
    
    return (the_crc);
} 

/*FUNCTION****************************************************************
* 
* Function Name    : _FWUMGR_release_handle
* Returned Value   : None
* Comments         :
*    make sure that the handle from the connection manager is 
*    closed, released, and forgotten. *and* that the connection manager
*    has been turned back on. 
*
*END*********************************************************************/

static void _FWUMGR_release_handle()
{
    /*
    **  close the handle, in case it's open
    */
    CONMGR_close(g_fwu_conmgr_handle);

    /*
    **  and now release it to say we're done
    */
    CONMGR_release(g_fwu_conmgr_handle);

    g_fwu_conmgr_handle = 0;
}

static void FWUMGR_disconnected(sys_event_t unused)
{
	g_wifi_disconnected = TRUE;
}

static void _FWUMGR_ActGoalSync(int32_t activity_goal)
{
	if (g_dy_cfg.usr_dy.daily_minutes_active_goal != (uint16_t) activity_goal)
	{
		g_dy_cfg.usr_dy.daily_minutes_active_goal = (uint16_t) activity_goal;
		FWUMGR_PRINT("FWU: Act Goal Set: %d\n", g_dy_cfg.usr_dy.daily_minutes_active_goal);
		CFGMGR_flush(CFG_FLUSH_DYNAMIC);
	}
}

#define FWUMGR_CHECKIN_MAX_RETRIES	5
#define FWUMGR_MANIF_MAX_RETRIES	5
static void _FWUMGR_process_checkin_blocking(void)
{
    char        pkg_url[MAX_FWU_URL_LEN];
    int rc;
    err_t err = ERR_OK;
    int retries;
    RmCheckInResponseActionType checkin_pending_action = -1;
    int64_t checkin_abs_time = -1;
    int32_t checkin_act_goal = -1;
    
	/*
	 **  do a checkin
	 */
	
    FWUMGR_PRINT("FWU ckn\n");
    
    g_wifi_disconnected = FALSE;

    for (retries = 0; retries < FWUMGR_CHECKIN_MAX_RETRIES; retries++)
    {
    	rc = _FWUMGR_Check_In(g_fwu_conmgr_handle, &checkin_pending_action, &checkin_abs_time, &checkin_act_goal);

    	_watchdog_start(FWUMGR_WATCHDOG_DEF_KICK);
    	if (ERR_OK == rc)
    	{
    		break;
    	}
    	if (ERR_FWUMGR_NOT_CONNECTED == rc)
    	{
        	PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
        	return;
    	}
    }
    
    if (ERR_OK != rc )
    {
    	PROCESS_NINFO(ERR_FWUMGR_NO_CHECKIN, NULL);
    	return;
    }
    
    // Log # of retries so we can dial in FWUMGR_CHECKIN_MAX_RETRIES
    // TODO: remove this once we know how ot dial in FWUMGR_CHECKIN_MAX_RETRIES
    if (retries > 0)
    {
    	PROCESS_NINFO(STAT_FWUMGR_CHECKIN_RETRY, "%d", retries);
    }
    
    if (-1 != checkin_abs_time)
    {
    	time_set_server_time(checkin_abs_time, TRUE);
    }
    
    if (-1 != checkin_act_goal)
    {
    	_FWUMGR_ActGoalSync(checkin_act_goal);
    }
    
    if (RmCheckInResponseActionType_CHECK_IN_PENDING_FIRMWARE_UPDATE != checkin_pending_action)
    {
        PMEM_FLAG_SET(PV_FLAG_PREFER_WIFI, FALSE);
    	return;
    }
    
    // Let the system know we prefer wifi since we have a pending update.
    PMEM_FLAG_SET(PV_FLAG_PREFER_WIFI, TRUE);
    
    /*
     *   We only allow an OTA firmware update if the battery is OK.
     */
    if (!PWRMGR_ota_fwu_is_allowed())
    {
    	return;
    }
    
    /*
     * Only over WiFi
     */
    if (CONMGR_CONNECTION_TYPE_WIFI != CONMGR_get_current_actual_connection_type())
    {
    	return;
    }
    
    /*
     **  there is new firmware to be had. Get the URL
     */
    for (retries = 0; retries < FWUMGR_MANIF_MAX_RETRIES; retries++)
    {
    	_watchdog_start(60*1000);
    	err = _FWUMGR_Request_Pkg_URL(pkg_url, MAX_FWU_URL_LEN);

    	if (ERR_OK == err)
    	{
    		break;
    	}
    	else if (ERR_FWUMGR_NOT_CONNECTED == err)
    	{
    		PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
    		return;
    	}
    }

    if (ERR_OK != err)
    {
    	PROCESS_NINFO( ERR_FWUMGR_MANIFEST_URL, NULL );
    	return;
    }

    _watchdog_start(60*1000);

    /*
     **  get the package and store it in flash. 
     */
    err = _FWUMGR_get_package(pkg_url);
    if (ERR_OK != err)
    {
    	PROCESS_NINFO( ERR_FWUMGR_GET_PKG, NULL );
    	return;
    }

    _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK);

    err = _FWUMGR_process_package();

    if (ERR_OK != err)
    {
    	PROCESS_NINFO( err, NULL );
    	return;
    }

    /*
     **  Now. Reboot so that Boot2 can copy the new image and 
     **  start it running.
     */
    
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "FWU done, rebooting");
        EVTMGR_request_flush();
        _time_delay(1000);
#endif
    
    FWUMGR_PRINT("FWU Reboot\n");
    _time_delay(1000);
    PMEM_FLAG_SET(PV_FLAG_FWU, 1);
    PWRMGR_Reboot( 250, PV_REBOOT_REASON_FWU_COMPLETED, TRUE );
}


/*FUNCTION****************************************************************
* 
* Function Name    : _FWUMGR_con_mgr_connected
* Returned Value   : None
* Comments         :
*    Called by the connection manager when WiFi is available (or not)
*    Triggers an event to awaken the task. 
*
*END*********************************************************************/

static void _FWUMGR_con_mgr_connected(CONMGR_handle_t handle, boolean success)
{
    wassert(handle == g_fwu_conmgr_handle);

    if (success)
    {
        _lwevent_set(&gfwu_event, FWUMGR_CONMGR_CONNECT_SUCCEEDED_EVENT);
        
        // CONMGR blocks here until FWUMGR_CONMGR_CONNECT_SUCCEEDED_EVENT fully processed.
        // Normally, connected callbacks must not block the CONMGR, but we are special since
        // we don't want the CONMGR to continue until we're done.
        _watchdog_stop(); // Pause the watchdog while we wait.
        _lwevent_wait_ticks(&gfwu_conmgr_done_event,FWUMGR_CONMGR_DONE_EVENT, FALSE, 0);
        _watchdog_start(60*1000); // Leave no trail.
    }
    else
    {
        FWUMGR_PRINT("FWU CON FAIL\n");
        _lwevent_set(&gfwu_event, FWUMGR_CONMGR_CONNECT_FAILED_EVENT);
    }
}

/*FUNCTION****************************************************************
* 
* Function Name    : _FWUMGR_con_mgr_disconnected
* Returned Value   : None
* Comments         :
*    Called by the connection manager when WiFi has disconnected
*    Triggers an event to awaken the task. 
*
*END*********************************************************************/

static void _FWUMGR_con_mgr_disconnected(CONMGR_handle_t handle)
{
    wassert(handle == g_fwu_conmgr_handle);
    _lwevent_set(&gfwu_event, FWUMGR_CONMGR_DISCONNECTED_EVENT);
//    FWUMGR_PRINT("FWU: Connection manager disconnect\n");
}

/*FUNCTION****************************************************************
* 
* Function Name    : _FWUMGR_con_mgr_register
* Returned Value   : None
* Comments         :
*    Called to register callbacks with the connection manager and thus
*    request WiFi device access..
*
*END*********************************************************************/

static void _FWUMGR_con_mgr_register(uint_32 min_wait_time, uint_32 max_wait_time)
{  
//    FWUMGR_PRINT("FWU: registering with connection manager min/max:%d/%d\n",min_wait_time,max_wait_time);
    wassert(ERR_OK == CONMGR_register_cbk(  _FWUMGR_con_mgr_connected, 
                                            _FWUMGR_con_mgr_disconnected, 
                                            max_wait_time, min_wait_time,
                                            CONMGR_CONNECTION_TYPE_ANY,
                                            CONMGR_CLIENT_ID_FUDMGR, &g_fwu_conmgr_handle));
}



////////////////////////////////////////////////////////////////////////////////
// Code - public functions
////////////////////////////////////////////////////////////////////////////////


void FWUMGR_Print_Package(void)
{


    #define BT1_HDR            0x00000200

    #define B2A                0x00002000
    #define B2B                0x00012000

    #define B2A_HDR            B2A + 0x200
    #define B2B_HDR            B2B + 0x200

    #define APP_LOAD_ADDRS     0x22000
    #define APP_HDR            APP_LOAD_ADDRS + 0x200

    fwu_pkg_t   package;

    uint32_t    pkg_crc;

    uint8_t buffer[FWUMGR_BUFFER_SIZE];

    char VER_B1[APP_VER_STRING_SIZE];
    char VER_B2A[APP_VER_STRING_SIZE];
    char VER_B2B[APP_VER_STRING_SIZE];
    char VER_APP[APP_VER_STRING_SIZE];

    _get_version_string(VER_B1, APP_VER_STRING_SIZE, BT1_HDR);
    _get_version_string(VER_B2A, APP_VER_STRING_SIZE, B2A_HDR);
    _get_version_string(VER_B2B, APP_VER_STRING_SIZE, B2B_HDR);
    _get_version_string(VER_APP, APP_VER_STRING_SIZE, APP_HDR);

    FWUMGR_PRINT("\n VER:\n\tB1=%s\n\tB2A=%s\n\tB2B=%s\n\tAPP=%s\n",
            VER_B1, VER_B2A, VER_B2B,VER_APP);


    /*
    **  read update flags.
    */
    // TODO: MEMOPT: can we dynamically allocate buffer to the size of FWU_MAX_OFFSETS?
    wson_read(FWUMGR_PERSISTENT_DATA_START,buffer,FWUMGR_BUFFER_SIZE);

    FWUMGR_PRINT("\n FWU updt flgs: 0x%02x(bt2A) 0x%02x(bt2B) 0x%02x(app)\n",
            buffer[FWU_BOOT2A_OFFSET],buffer[FWU_BOOT2B_OFFSET],buffer[FWU_APP_OFFSET]);

    /*
    **  read the first part of the package
    */
    // TODO: MEMOPT: 
    wson_read(FWUMGR_FIRMWARE_IMAGE_START,buffer,FWUMGR_BUFFER_SIZE);

    /*
    **  parse it
    */
    parse_pkg(buffer, &package);

    /*
    **  display package data
    */
    print_package(&package);

    /*
    **  CRC it and tell the world.  
    */
    pkg_crc = FMUMGR_calc_crc_pkg(FWUMGR_FIRMWARE_IMAGE_START,package.pkg_info.pkg_size);

    FWUMGR_PRINT("\n Pkg CRC 0x%08x", pkg_crc);

    if (pkg_crc == package.pkg_info.pkg_crc)
    {
        FWUMGR_PRINT(" CRC OK\n");
    }
    else
    {
        WPRINT_ERR("bad CRC");
    }
}


/*FUNCTION****************************************************************
* 
* Function Name    : FWUMGR_Write_Flash
* Returned Value   : None
* Comments         :
*    Called to write data to flash, one page at a time. 
*
*END*********************************************************************/

static void _FWUMGR_Write_Flash(uint32_t flash_address, uint8_t *buffer, int32_t nbytes)
{
    uint32_t bytes_to_write;

    static uint8_t saved_buf[EXT_FLASH_PAGE_SIZE];
    static uint32_t saved_idx = 0;
    static uint32_t next_flash_address = EXT_FLASH_BOGUS_ADDR;
    static uint16_t have_extra = false;

    /*
    **  we assume that the *first* write will be on a page boundary. 
    **  after that we do the arithmetic to increment the flash address appropriately
    **
    **  the assumption is that we are reading a stream of bytes that will be programmed
    **  sequentially into flash. 
    */

    if (next_flash_address == EXT_FLASH_BOGUS_ADDR)
    {
        /*
        **  this is the first write to flash. Get the starting address
        **  that we will proceed to ignore for the rest of the calls to this 
        **  function. 
        */
        next_flash_address = flash_address;
    }

#if GIVE_WHISTLE_FEEDBACK_ON_FWU
    // Just upload debug info on the writes at the beginning and end of the image area.
    if( (flash_address > 0x93000) || (flash_address < 0x22000) )
    {
        // FWU WR Flash                                                                          0x20000 from 0x20004690 of size 1452
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "FWU WR Flash 0x%x from 0x%x of size %i extra %u", flash_address, buffer, nbytes, have_extra);
        
        _time_delay(50);
    }
#endif

    /*
    **  if we have extra data from before
    */
    if (have_extra)
    {
        int32_t bytes_to_copy;
        uint32_t where_to_write;

        bytes_to_copy  = (sizeof(saved_buf) - saved_idx);
        where_to_write = next_flash_address;
        next_flash_address += sizeof(saved_buf);
        
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        if( 0x95b10 <= flash_address )
        {
            PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "memcpy %x %i %x %x %u %u", 
                    // 96c14 236 96c00 96d00 256 20
                    flash_address, bytes_to_copy, where_to_write, next_flash_address, sizeof(saved_buf), saved_idx);
            
            _time_delay(100);
            EVTMGR_request_flush();
            _time_delay(300);
        }
#endif
        
        /*
        **  copy new data into page buffer, write one page
        */
        if( (saved_idx >= EXT_FLASH_PAGE_SIZE) || (bytes_to_copy <= 0) )
        {
            // Sanity check failed, process an error, and wassert(0).
            PROCESS_NINFO( ERR_FWUMGR_SANITY_CHECK_INDEXING, "%u %i", saved_idx, bytes_to_copy);
            
            _time_delay(100);
            EVTMGR_request_flush();
            _time_delay(300);
            
            wassert(0);
            _task_block();
        }
        memcpy((void *)&saved_buf[saved_idx], buffer, bytes_to_copy);
        buffer += bytes_to_copy;
        nbytes -= bytes_to_copy;
        
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        if( 0x95b10 <= flash_address )
        {
            PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "memcpy OK %x %x %i", &saved_buf[saved_idx], buffer, nbytes);
            
            _time_delay(100);
            EVTMGR_request_flush();
            _time_delay(300);
        }
#endif

        /*
        **  NOTE: I know that it is possible there are not enough bytes 
        **        in "buffer" to copy. This will only happen (maybe) 
        **        when I'm at the end of my data stream. The extra
        **        bytes don't matter. 
        */
//        FWUMGR_PRINT("FWU: xtra (0x%04x) @ 0x%08x\n",sizeof(saved_buf),where_to_write);

        wson_write(where_to_write, saved_buf, sizeof(saved_buf), false);
        
        /*
        **  we now have an empty page buffer, and next flash address should be an even page 
        **  boundary
        */
        have_extra = false;
    }

    if (nbytes <= 0)
    {
        /*
        **  we're done. 
        */
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "DONE nbytes: %i", nbytes);
        _time_delay(100);
        EVTMGR_request_flush();
        _time_delay(300);
#endif
        return;
    }

    /*
    **  there are more bytes to write. Write an even number of pages worth
    */
    bytes_to_write = nbytes - (nbytes % EXT_FLASH_PAGE_SIZE);
    
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        if( 0x95b10 <= flash_address )
        {
            PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "fart: %i %i", bytes_to_write, nbytes);
            
            _time_delay(100);
            EVTMGR_request_flush();
            _time_delay(300);
        }
#endif

    if (bytes_to_write != nbytes)
    {
        /*
        **  save to be written next time
        */
        have_extra = true;
        /*
        **  save bytes
        */
        saved_idx = nbytes-bytes_to_write;
        
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        if( 0x95b10 <= flash_address )
        {
            PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "bytestoWrite != nbytes %u %u %u", saved_idx, nbytes, bytes_to_write);
            
            _time_delay(100);
            EVTMGR_request_flush();
            _time_delay(300);
        }
#endif

        memcpy((void *)saved_buf, (void *)&buffer[bytes_to_write], saved_idx );
    }

    /*
    **  now write the bytes we can. 
    */
//    FWUMGR_PRINT("FWU: big  (0x%04x) @ 0x%08x \n",bytes_to_write, next_flash_address);
    
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        if( 0x95b10 <= flash_address )
        {
            PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "dunk %x %u %u", next_flash_address, bytes_to_write, have_extra);
            
            _time_delay(100);
            EVTMGR_request_flush();
            _time_delay(300);
        }
#endif
        
    if( bytes_to_write > (EXT_FLASH_PAGE_SIZE*32) )  // just a sanity check, we never write this much!
    {
        // Sanity check failed, process an error, and wassert(0).
        PROCESS_NINFO( ERR_FWUMGR_SANITY_CHECK_INDEXING, "%x %i %u %u", bytes_to_write, nbytes, saved_idx, have_extra);
        
        _time_delay(100);
        EVTMGR_request_flush();
        _time_delay(300);
        
        wassert(0);
        _task_block();
    }

    wson_write(next_flash_address, buffer, bytes_to_write, false);

    /*
    **  compute the next flash address for I/O.
    */
    next_flash_address += (uint32_t)bytes_to_write;
}


/*FUNCTION****************************************************************
* 
* Function Name    : FWUMGR_Parse_URL
* Returned Value   : None
* Comments         :
*    find the server and filename in a url from the server.
*    Yes, I know this is a little ugly. 
*
*END*********************************************************************/

static const char http_accessor[] = "http://";
static const char https_accessor[] = "https://";

static void _FWUMGR_Parse_URL(char *url, char **server, char **filename)
{
    char * scratch;
    /*
    **  skip past https://
    */
    scratch=strstr(url,https_accessor);
    if (NULL != scratch)
    {
        scratch += 8;
    }
    else
    {
        scratch=strstr(url,http_accessor)+7;
    }
    
    /*
    **  save the pointer to the server name
    */
    *server = scratch;

    /*
    **  find the start of the filename
    */
    scratch = strstr(scratch,"/");
    /*
    **  stuff a zero into the string to differentiate between
    **  server and file. 
    */
    *scratch = 0;

    /*
    **  return the pointer to the filename
    */
    *filename = scratch+1;
}


static void _FWUMGR_clear_flags(void)
{
    /*
    **  re-use the http header buffer for our small, temporary structure.
    */
    FWU_PERSISTENT_SPI_T *spi_data_p ;

    uint8_t buffer[FWUMGR_BUFFER_SIZE];

    /*
    **  read the flags
    */
    wson_read(FWUMGR_PERSISTENT_DATA_START,buffer,FWUMGR_BUFFER_SIZE);
    
    spi_data_p = (FWU_PERSISTENT_SPI_T *)buffer;

    spi_data_p->boot_flags[FWU_BOOT2A_OFFSET] = FLAG_CLR;
    spi_data_p->boot_flags[FWU_BOOT2B_OFFSET] = FLAG_CLR;
    spi_data_p->boot_flags[FWU_APP_OFFSET]    = FLAG_CLR;
    spi_data_p->boot_flags[FWU_BOOT1_OFFSET]  = FLAG_CLR;

    /*
    **  update the persistent data
    */
    
    wson_write(FWUMGR_PERSISTENT_DATA_START,buffer,FWUMGR_BUFFER_SIZE, true);

}

static void _FWUMGR_erase_flash(size_t content_size)
{
	int32_t bytes_to_erase = (int32_t)content_size;
	uint32_t sectors_erased = 0;
    uint32_t flash_address;

	flash_address = (uint32_t) FWUMGR_FIRMWARE_IMAGE_START;

	while ( (bytes_to_erase > 0) && (FWUMGR_FIRMWARE_IMAGE_END > flash_address) )
	{
	    
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
	    PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "FWU Erasing 0x%x", flash_address);
	    //EVTMGR_request_flush();
	    _time_delay(10);
#endif
	    
		wson_erase(flash_address);

		sectors_erased++;
		flash_address += EXT_FLASH_64KB_SZ;
		bytes_to_erase -= EXT_FLASH_64KB_SZ;
	}

	FWUMGR_PRINT("\n\nFWUMGR OK %d sec of %d B (%d B tot)\n",
			sectors_erased,EXT_FLASH_64KB_SZ,sectors_erased*EXT_FLASH_64KB_SZ);
}


static err_t _FWUMGR_download_to_spi_flash(uint32_t totalbytes, uint8_t *resp_buf, uint_32 resp_buf_len, int_32 receive_count, size_t content_size, size_t header_size)
{
    err_t       err;
    uint32_t    byteswritten;
    int32_t     bytestoread;
    uint32_t    flash_address = (uint32_t) FWUMGR_FIRMWARE_IMAGE_START;
    uint8_t     retries=0;
    TIME_STRUCT start_time = {0};
    TIME_STRUCT stop_time = {0};
    
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "FWU DL to SPI Flash %u %x %u %i %u %u",
                totalbytes, resp_buf, resp_buf_len, receive_count, content_size, header_size);
        EVTMGR_request_flush();
        _time_delay(1500);
#endif

    _watchdog_start(FWUMGR_DOWNLOAD_TIMEOUT_WD);
    
    FWUMGR_PRINT("\nFWU Wrt %d (0x%08x) B frm 0x%08x to 0x%08x\n",
			content_size,content_size,flash_address,flash_address+content_size);

	byteswritten = receive_count - header_size;

	_FWUMGR_Write_Flash(flash_address, (uint8_t *)((uint8_t *)resp_buf + header_size), byteswritten);

	flash_address += byteswritten;
	
	RTC_get_time_ms(&start_time);

	/*
	 **  give back the buffer
	 */
//	CONMGR_zero_copy_free(g_fwu_conmgr_handle, response);

	/*
	 **  compute what's left to read
	 */
	bytestoread = content_size + header_size - receive_count;
	FWUMGR_PRINT("Tot_B:%d, B2read:%d rec_cnt %d flsh_add %d\n", totalbytes, bytestoread, receive_count, flash_address);
    do
    {
        _watchdog_start(FWUMGR_TCP_RECEIVE_TIMEOUT_WD);
        err = CONMGR_receive(g_fwu_conmgr_handle, (void *) resp_buf, resp_buf_len, FWUMGR_TCP_RECEIVE_TIMEOUT, &receive_count, FALSE);
        
        _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK);
        if (ERR_OK == err)
        {
        	
            /*
            **  write flash
            */

            _FWUMGR_Write_Flash(flash_address, (uint8_t *)resp_buf, receive_count);
    
            flash_address += receive_count;
            byteswritten  += receive_count;

            /*
            **  give back the buffer
            */
//            CONMGR_zero_copy_free(g_fwu_conmgr_handle, response);
            
            totalbytes += receive_count;
            bytestoread -= receive_count;
            if (totalbytes % (receive_count*10) == 0 )
            {
            	FWUMGR_PRINT("Tot_B:%d B2read:%d rec'd %d flsh_add %d\n",totalbytes, bytestoread,receive_count, flash_address);
            }
            retries = 0;
        }
        else if (ERR_CONMGR_INVALID_HANDLE == err)
        {
        	// Server closed the socket. Bail
            FWUMGR_PRINT("serv close sock %d B rx'd\n", receive_count);
            if (bytestoread <= FWUMGR_DL_BUFFER_HACK)
            {
                PROCESS_NINFO(ERR_FWUMGR_INCOMPLETE_DL, "HACK");
                break;
            }
            else
            {
            	return PROCESS_NINFO(ERR_FWUMGR_INCOMPLETE_DL, NULL);
            }
        }
        else if (g_wifi_disconnected)
        {
        	return PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
        }
        else
        {
            retries++;
//           FWUMGR_PRINT("\n\nFWUMGR: err:0x%04X\n",err);
        }
        
        /*
        **  repeat until an error or we've read all the data...
        */
        if (retries > FWUMGR_DOWNLOAD_CONSECUTIVE_RETRIES)
        {
        	return PROCESS_NINFO(ERR_FWUMGR_MAX_ERRORS, NULL);
        }
    }
    while(bytestoread > 0);
    WMP_post_breadcrumb(BreadCrumbEvent_FWUMGR_DL_COMPLETE);
    RTC_get_time_ms(&stop_time);
    PROCESS_UINFO("FWUDL %d %d", retries, stop_time.SECONDS - start_time.SECONDS);
    
    FWUMGR_PRINT("FWU B wrtn %d\n",byteswritten);
    
    /*
    **  write one more page of data, to ensure the last page from the image is flushed
    **  to flash. 
    */
    _FWUMGR_write_last_page(flash_address);
    
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "DONE FWU DL to SPI Flash");
        EVTMGR_request_flush();
        _time_delay(500);
#endif
    
    return ERR_OK;
}

static void _FWUMGR_write_last_page(uint32_t flash_address)
{
    uint8_t scratch[EXT_FLASH_PAGE_SIZE];
    
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "WR LAST PAGE to 0x%x", flash_address);
        EVTMGR_request_flush();
        _time_delay(500);
#endif
    
    memset((void *)scratch, 0xdeadbeef,sizeof(scratch));
    _FWUMGR_Write_Flash(flash_address,(uint8_t *)scratch,sizeof(scratch));

}

static err_t _FWUMGR_process_package(void)
{
    uint32_t pkg_crc;
    fwu_pkg_t   package;
    err_t	err;
    int  	retries = 100;

    FWU_PERSISTENT_SPI_T *spi_data_p ;

    uint8_t buffer[FWUMGR_BUFFER_SIZE];
    
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "Wait accel off");
        EVTMGR_request_flush();
        _time_delay(1000);
#endif
    
    // Shut down accel before we begin, just to be safe.
    ACCMGR_shut_down( false );
    FWUMGR_PRINT("FWU wait acc off\n");;
    while (! ACCMGR_is_shutdown() && retries-- > 0)
    {
        _time_delay(150);
    }
    
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "Accel sleep retries left %u", retries);
        EVTMGR_request_flush();
        _time_delay(500);
#endif
    
    FWUMGR_PRINT("FWU acc off trys %d\n", retries);

    /*
    **  read the first part of the package
    */
    wson_read(FWUMGR_FIRMWARE_IMAGE_START,buffer,FWUMGR_BUFFER_SIZE);


    /*
    **  parse it
    */
    parse_pkg(buffer, &package);

    /*
    **  display package data
    */
    print_package(&package);

    /*
    **  CRC it and tell the world.  
    */
    pkg_crc = FMUMGR_calc_crc_pkg(FWUMGR_FIRMWARE_IMAGE_START,package.pkg_info.pkg_size);

    FWUMGR_PRINT("\nCRC 0x%08x",pkg_crc);

    if (pkg_crc == package.pkg_info.pkg_crc)
    {
        FWUMGR_PRINT(" CRC OK\n");
        _FWUMGR_Send_Status_Message(RmFwuStatusPostStatus_FWU_DOWNLOAD_SUCCESS);
    }
    else
    {
        
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "CRC Failed");
        EVTMGR_request_flush();
        _time_delay(2000);
#endif
        WPRINT_ERR("CRC");
        _FWUMGR_Send_Status_Message(RmFwuStatusPostStatus_FWU_DOWNLOAD_FAILED_CRC_FAILED);
        return ERR_FWUMGR_CRC_FAILED;
    }

    /*
    **  reuse the buffer to read in the nonvolatile FWU_MGR data
    */
    wson_read(FWUMGR_PERSISTENT_DATA_START,buffer,FWUMGR_BUFFER_SIZE);

    spi_data_p = (FWU_PERSISTENT_SPI_T *)buffer;

    if (package.pkg_info.pkg_contents & PKG_CONTAINS_BOOT2A)
    {
        spi_data_p->boot_flags[FWU_BOOT2A_OFFSET] = FLAG_SET;
    }
    else
    {
        spi_data_p->boot_flags[FWU_BOOT2A_OFFSET] = FLAG_CLR;
    }

    if (package.pkg_info.pkg_contents & PKG_CONTAINS_BOOT2B)
    {
        spi_data_p->boot_flags[FWU_BOOT2B_OFFSET] = FLAG_SET;
    }
    else
    {
        spi_data_p->boot_flags[FWU_BOOT2B_OFFSET] = FLAG_CLR;
    }

    if (package.pkg_info.pkg_contents & PKG_CONTAINS_APP)
    {
        spi_data_p->boot_flags[FWU_APP_OFFSET] = FLAG_SET;
    }
    else
    {
        spi_data_p->boot_flags[FWU_APP_OFFSET] = FLAG_CLR;
    }
    
    if (package.pkg_info.pkg_contents & PKG_CONTAINS_BOOT1)
    {
        spi_data_p->boot_flags[FWU_BOOT1_OFFSET] = FLAG_SET;
    }
    else
    {
        spi_data_p->boot_flags[FWU_BOOT1_OFFSET] = FLAG_CLR;
    }


    /*
    **  update the persistent data
    */
    wson_write(FWUMGR_PERSISTENT_DATA_START,buffer,FWUMGR_BUFFER_SIZE, true);
    
    return ERR_OK;
}

/*FUNCTION****************************************************************
* 
* Function Name    : FWUMGR_get_package
* Returned Value   : None
* Comments         :
*    Called to download a firmware package from the passed
*    server. Writes image to SPI flash. 
*
*END*********************************************************************/
#define FWU_DL_HTTP_HEADER_LEN (300)

static err_t _FWUMGR_get_package(char * package_url)
{
 
    err_t       err = ERR_OK;
    int         rc;
    uint16_t    http_header_len;
    int_32      send_result;
    uint8_t     *response = NULL;
    int_32      receive_count;
    int16_t     return_code;
    int16_t     is_chunked;
    size_t      header_size;
    size_t      content_size;

    uint32_t    totalbytes = 0;    

    char * server;
    char * filename;

    /*
    **  buffer for "get" header.
    */
    char pHttpHeader[FWU_DL_HTTP_HEADER_LEN] = {0};


    /*
    **  first, clear the flags in SPI flash in case there is a download error. 
    */
    _FWUMGR_clear_flags();
    
    
    /*
    **  get the server name and the filename
    */
    _FWUMGR_Parse_URL(package_url, &server, &filename);
    
    /*
    **  Open the server. Use IP address for now because of WiFi bug.
    */
    FWUMGR_PRINT("FWU Set RX Buf=10\n");
    WIFIMGR_set_RX_buf(10);
        
    response = walloc( MAX_WIFI_RX_BUFFER_SIZE );
    
    FWUMGR_PRINT("FWU Try con \"%s\"\n",server);
    err = CONMGR_open(g_fwu_conmgr_handle, server, 80);
    if(err != ERR_OK)
    {
        if (ERR_CONMGR_NOT_CONNECTED == err || ERR_CONMGR_CRITICAL == err)
        	err =  PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
        else
        	err = PROCESS_NINFO(ERR_FWUMGR_CONMGR_OPEN, NULL);
        
        goto DONE;
    }

    /*
    **  construct "get" header
    */
    sprintf(pHttpHeader,"GET /%s HTTP/1.1\r\n"
                        "User-Agent: Wget/1.14 (darwin12.1.0)\r\n"
                        "Accept: */*\r\n"
                        "Host: %s\r\n"
                        "Connection: Keep-Alive\r\n\r\n",filename,server);
    
    http_header_len = strlen(pHttpHeader);
    if( http_header_len >= FWU_DL_HTTP_HEADER_LEN )
    {
        err = PROCESS_NINFO(ERR_FWUMGR_INVALID_HTTP_HDR_LENGTH, "max %u\tis %u", FWU_DL_HTTP_HEADER_LEN, http_header_len);
        goto DONE;
    }

    FWUMGR_PRINT("Hdr(%d) = \r\n%s",http_header_len,pHttpHeader);

    /*
    **  send "get" message to server
    */
    _watchdog_start(FWUMGR_TCP_SEND_TIMEOUT_WD);
    err = CONMGR_send(g_fwu_conmgr_handle, (uint8_t *)pHttpHeader, http_header_len , &send_result);
    if (ERR_CONMGR_NOT_CONNECTED == err)
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
        goto DONE;
    }
    if (send_result != http_header_len)
    {
        err = PROCESS_NINFO(ERR_FWUMGR_CONMGR_SEND, "TX HTTP hdr %d bytes", send_result);
        goto DONE;
    }


    /*
    **  read & decode header
    */
    _watchdog_start(FWUMGR_TCP_RECEIVE_TIMEOUT_WD);
	err = CONMGR_receive(g_fwu_conmgr_handle, (void *) response, MAX_WIFI_RX_BUFFER_SIZE, FWUMGR_TCP_RECEIVE_TIMEOUT, &receive_count, FALSE);
    _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK);
	if (ERR_OK != err)
	{
		PROCESS_NINFO(err, "rx err");
		goto DONE;
	}
	
	if( 0 >= receive_count )
	{
        err = PROCESS_NINFO(ERR_FWUMGR_INVALID_HTTP_HDR_LENGTH, "rx %i", receive_count);
        goto DONE;
	}
	
	totalbytes += (uint32_t)receive_count;

	/*
	 **  decode header
	 */
	rc = WHTTP_parse_return_header((char*) response, (size_t)receive_count , &return_code, &is_chunked, &header_size,&content_size);
	
	if (0 != rc)
	{
		err = PROCESS_NINFO(ERR_FWUMGR_CONMGR_RX, "parse %d", rc);
		goto DONE;
	}
	
	FWUMGR_PRINT("FWU %d 1st sz %d, chnkd=%d hdr=%d, pkg=%d\n\n",
			return_code,receive_count,is_chunked,header_size,content_size);

	FWUMGR_PRINT("FWU ERS\n");


	/*
	 **  now, erase the flash into which we'll write the image. But not more than we need. 
	 */
	_FWUMGR_erase_flash(content_size);
	
    _watchdog_start(FWUMGR_DOWNLOAD_TIMEOUT_WD);
	
	/*
	 **  write to flash
	 */
    err = _FWUMGR_download_to_spi_flash(totalbytes, response, MAX_WIFI_RX_BUFFER_SIZE, receive_count, content_size, header_size);

    _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK);
    
	CONMGR_close(g_fwu_conmgr_handle);
	
	if (ERR_OK != err)
	{
		PROCESS_NINFO(err, "DL");
		goto DONE;
	}
    
    FWUMGR_PRINT("\nFWU done rx %d B (%d hdr + %d data)\n", 
                        totalbytes, header_size, content_size);
    
DONE:

//    CONMGR_zero_copy_free(g_fwu_conmgr_handle, response);

#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "Try free response at 0x%x", response);
        EVTMGR_request_flush();
        _time_delay(500);
#endif
        
	if (response)
	{
		wfree(response);
	}
	
#if GIVE_WHISTLE_FEEDBACK_ON_FWU
        PROCESS_NINFO( ERR_WHTTP_JUST_A_MSG, "FWU wfree OK");
        EVTMGR_request_flush();
        _time_delay(500);
#endif
	
    return err;
}

/*FUNCTION****************************************************************
* 
* Function Name    : FWUMGR_Check_In
* Returned Value   : 0 = sucess. -1 = failed, should retry. -2 = minor failure, don't retry
* Comments         :
*    This function opens a socket to the staging server, does a "checkin" 
*    and then gets the manifest for the firmware update....
*
*END*********************************************************************/
static err_t _FWUMGR_Check_In(CONMGR_handle_t conmgr_handle, RmCheckInResponseActionType *checkin_pending_action, int64_t *checkin_abs_time, int32_t *checkin_act_goal)
{
    uint8_t     pFwuWmpTxBuffer[FWU_WMP_TX_BUFFER_SIZE];
    size_t		FWU_WMP_Tx_Buffer_Used = 0;
    int_32      receive_count;

    WhistleMessage *msg = NULL;
    pb_istream_t stream;
    int_32 send_result;
    uint32_t *response = NULL;
    err_t w_rc = ERR_GENERIC;
    union bark_res b_rc;
    
    WMP_whistle_msg_alloc(&msg);

    /*
    **  build checkin message
    */
    //TODO: make this intelligent so it dynamically determines size and that it's not larger
    //      than FWU_WMP_TX_BUFFER_SIZE

    FWU_WMP_Tx_Buffer_Used = _FWUMGR_wmp_gen_checkin_message(msg, pFwuWmpTxBuffer, FWU_WMP_TX_BUFFER_SIZE);
    if (FWU_WMP_Tx_Buffer_Used <= 0)
    {
    	PROCESS_NINFO(ERR_FWUMGR_PROTOBUF, "ckn gen");
    	w_rc = ERR_GENERIC;
    	goto DONE;
    }


    /*
    **  open a connection to the WiFi stream
    */

    FWUMGR_PRINT("ckn open\n");

    _watchdog_start(FWUMGR_TCP_SEND_TIMEOUT_WD);
    b_rc.open_res = bark_open(conmgr_handle, FWU_WMP_Tx_Buffer_Used);
    if( b_rc.open_res != BARK_OPEN_OK )
    {
    	if (BARK_OPEN_CRITICAL == b_rc.open_res || BARK_OPEN_NOT_CONNECTED == b_rc.open_res)
    	{
    		w_rc = PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, "open");
    	}
    	else
    	{
    		w_rc = PROCESS_NINFO(ERR_FWUMGR_CONMGR_OPEN, "open");
    	}
    	
    	goto DONE;
    }

    /*
    **  send WMP data
    */
    _watchdog_start(FWUMGR_TCP_SEND_TIMEOUT_WD);
    b_rc.send_res = bark_send(conmgr_handle, pFwuWmpTxBuffer, FWU_WMP_Tx_Buffer_Used, &send_result);
	if (BARK_SEND_NOT_CONNECTED == b_rc.send_res || BARK_SEND_CRITICAL == b_rc.send_res)
	{
		w_rc = ERR_FWUMGR_NOT_CONNECTED;
		goto DONE;
	}
    if (send_result != FWU_WMP_Tx_Buffer_Used || BARK_SEND_OK != b_rc.send_res)
    {
    	CONMGR_close(conmgr_handle);
        w_rc = PROCESS_NINFO(ERR_FWUMGR_CONMGR_SEND, "WMP %d %d", b_rc.send_res, send_result);
        goto DONE;
    }

    // Allocate buffer memory for the response
    response = walloc( MAX_WIFI_RX_BUFFER_SIZE );

    /*
     **  receive data from the server.
     */
    _watchdog_start(FWUMGR_TCP_RECEIVE_TIMEOUT_WD);
    b_rc.receive_res = bark_receive(conmgr_handle, (void *)response, MAX_WIFI_RX_BUFFER_SIZE, FWUMGR_TCP_RECEIVE_TIMEOUT, &receive_count);
    wassert( _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK) );

	/*
	 **  close the socket
    */
    bark_close(conmgr_handle);
    wassert( _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK) );
    
    //    FWUMGR_PRINT("FWU: CONMGR_receive returned %d. receivecount = %d response=0x%08x\n",err,receivecount,response);;
    //    FWUMGR_PRINT("FWU: CONMGR_receive buffer = %s\n",response);;

	
	if (BARK_RECEIVE_NOT_CONNECTED == b_rc.receive_res || BARK_RECEIVE_CRITICAL == b_rc.receive_res)
	{
		w_rc = ERR_FWUMGR_NOT_CONNECTED;
		goto DONE;
	}
    if (0 >= receive_count || BARK_RECEIVE_OK != b_rc.receive_res)
    {
    	if (BARK_RECEIVE_RESP_PARSE_FAIL == b_rc.receive_res)
    	{
    		w_rc = PROCESS_NINFO(ERR_FWUMGR_CHECKIN_FAIL_PARSE, "FWU No resp %d %d", b_rc.receive_res, receive_count);
    	}
    	else
    	{
    		w_rc = PROCESS_NINFO(ERR_FWUMGR_CONMGR_RX, "FWU No resp %d %d", b_rc.receive_res, receive_count);
    	}
    	
    	goto DONE;
    }

    _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK);

    /* Construct a pb_istream_t for reading from the buffer */
    stream = pb_istream_from_buffer((uint8_t*) response, receive_count);

    memset((void*) msg, 0, sizeof(*msg));

    if ( !pb_decode(&stream, WhistleMessage_fields, msg) )
    {
    	w_rc = PROCESS_NINFO(ERR_FWUMGR_CHECKIN_FAIL_PARSE, "WMP resp");
    	goto DONE;
    }

    if (msg->has_remoteMgmtMsg != TRUE)
    {
    	w_rc = PROCESS_NINFO(ERR_FWUMGR_CHECKIN_FAIL_PARSE, "no rmt msg 584?");
    	goto DONE;
    }
    
    if (msg->has_transactionType && TransactionType_TRANS_RESPONSE_NAK == msg->transactionType)
    {
    	w_rc = PROCESS_NINFO(ERR_FWUMGR_CHECKIN_FAIL_PARSE, "wmp nak");
    	goto DONE;
    }

    if (msg->remoteMgmtMsg.has_checkInResponse != TRUE)
    {
    	w_rc = PROCESS_NINFO(ERR_FWUMGR_CHECKIN_FAIL_PARSE, "no resp");
    	goto DONE;
    }

    /*
     *   Set a persistent variable to indicate we have checked in the device.
     */
    if( 0 == g_dy_cfg.sysinfo_dy.deviceHasCheckedIn )
    {
    	g_dy_cfg.sysinfo_dy.deviceHasCheckedIn = 1;
    	CFGMGR_flush( CFG_FLUSH_DYNAMIC );
    }

    if (msg->remoteMgmtMsg.checkInResponse.has_pendingAction != TRUE)
    {
    	PROCESS_NINFO(ERR_FWUMGR_CHECKIN_MISSING_PARAM, "action");
    }
    else
    {
    	*checkin_pending_action = msg->remoteMgmtMsg.checkInResponse.pendingAction;
    }

    if (msg->remoteMgmtMsg.checkInResponse.has_serverAbsoluteTime != TRUE)
    {
    	PROCESS_NINFO(ERR_FWUMGR_CHECKIN_MISSING_PARAM, "time");
    }
    else
    {
    	*checkin_abs_time = msg->remoteMgmtMsg.checkInResponse.serverAbsoluteTime;
    }

    if (msg->remoteMgmtMsg.checkInResponse.has_activityGoal != TRUE)
    {
    	PROCESS_NINFO(ERR_FWUMGR_CHECKIN_MISSING_PARAM, "actgoal");
    }
    else
    {
    	*checkin_act_goal = msg->remoteMgmtMsg.checkInResponse.activityGoal;
    }

    FWUMGR_PRINT("chkn pend %d abs T 0x%08x\n",
    		*checkin_pending_action,
    		*checkin_abs_time);

    w_rc = ERR_OK;
    
DONE:
    if (response)
    {
    	wfree(response);
    }
    
    if (msg)
    {
    	WMP_whistle_msg_free(&msg);
    }
    
    return w_rc;
}


static err_t _FWUMGR_Request_Pkg_URL(char *pPkgUrl, const size_t pkg_url_len)
{
    uint8_t 	*buffer = NULL;
    int_32		buffer_used = 0;
    err_t err = ERR_OK;
    
    int_32 bytes_sent;
    
//    FWUMGR_PRINT("FWU: Task opening socket for manifest message\n");


    /*
    **  build manifest request message
    */
    // First test out encoding pkg_url request to get encoded size for malloc'ing the buffer
    buffer_used = _FWUMGR_wmp_gen_manifest_request_message(NULL, -1);
    
    // Allocate common buffer for WMP Send Msg & WMP Resp Msg, use maximum size
    buffer = walloc( ( buffer_used > MAX_WIFI_RX_BUFFER_SIZE ? buffer_used : MAX_WIFI_RX_BUFFER_SIZE) );
    
    // Actually encode the manfiest request
    buffer_used = _FWUMGR_wmp_gen_manifest_request_message(buffer, buffer_used);

    err = bark_open(g_fwu_conmgr_handle, buffer_used);
    if(err != BARK_OPEN_OK)
    {
        WPRINT_ERR("open");
        
        if (BARK_OPEN_NOT_CONNECTED == err || BARK_OPEN_CRITICAL == err)
        	err = PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
        else
        	err = PROCESS_NINFO(ERR_FWUMGR_CONMGR_OPEN, NULL);
        
        goto done_get_manifest_url_con_closed;
    }


    _watchdog_start(FWUMGR_TCP_RECEIVE_TIMEOUT_WD);
    err = bark_send(g_fwu_conmgr_handle, buffer, buffer_used , &bytes_sent);
    _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK);
    if (BARK_SEND_NOT_CONNECTED == err)
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
    	goto done_get_manifest_url;
    }
    if (bytes_sent != buffer_used) // N
    {
        err = PROCESS_NINFO(ERR_FWUMGR_CONMGR_SEND, "res %d", bytes_sent);
        goto done_get_manifest_url;
    }

    FWUMGR_PRINT("FWU rx manif\n");
    
    // Allocate buffer memory for the response
    memset(buffer, 0, MAX_WIFI_RX_BUFFER_SIZE);
    buffer_used = 0;
    
    /*
    **  receive data from the server.
    */
    _watchdog_start(FWUMGR_TCP_RECEIVE_TIMEOUT_WD);
    err = bark_receive(g_fwu_conmgr_handle, (void *)buffer, MAX_WIFI_RX_BUFFER_SIZE, FWUMGR_TCP_RECEIVE_TIMEOUT, &buffer_used);
    _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK);

    if (BARK_RECEIVE_NOT_CONNECTED == err)
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
    	goto done_get_manifest_url;
    }
    else if (BARK_RECEIVE_RESP_PARSE_FAIL == err)
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_CONMGR_RX, "manif bad HTTP");
    	goto done_get_manifest_url;
    	
    }
    
    if(buffer_used <= 0)
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_CONMGR_RX, "manif rx %d %d", err, buffer_used);
    	goto done_get_manifest_url;
    }
    
    err = _FWUMGR_wmp_parse_manifest_response((uint8_t*) buffer, 
    					buffer_used, 
    					pPkgUrl, 
    					pkg_url_len);

//        FWUMGR_PRINT("Received & parsed checkInResponse: \r\n   pendingAction: %d\r\n   serverAbsoluteTime(ms): 0x%x\r\n",
//                whistle_msg->remoteMgmtMsg.fwuManifestResponse.boot1Version.arg
//                whistle_msg->remoteMgmtMsg.checkInResponse.serverAbsoluteTime);

done_get_manifest_url:
	bark_close(g_fwu_conmgr_handle);

done_get_manifest_url_con_closed:
    if (buffer != NULL)
    {
    	wfree(buffer);        
    }
    
    return(err);
}

/*FUNCTION****************************************************************
* 
* Function Name    : _FWUMGR_Send_Status_Message
* Returned Value   : err_t
* Comments         :
*    This function opens a socket to the staging server, does a "checkin" 
*    and then gets the manifest for the firmware update....
*
*END*********************************************************************/

err_t _FWUMGR_Send_Status_Message(RmFwuStatusPostStatus status)
{
    _mqx_uint   mask;
    uint8_t     FWUMGR_buffer[FWUMGR_BUFFER_SIZE];
    int_32      receivecount;

    pb_istream_t stream;

    WhistleMessage *whistle_msg;
    pb_ostream_t encode_stream;
    err_t err = ERR_OK;
    int_32 send_result;
    uint32_t *response = NULL;
    
    WMP_whistle_msg_alloc(&whistle_msg);

//    FWUMGR_PRINT("FWU: encode status message\n");

    /*
    **  ensure that we have a valid status to report
    */
    switch (g_fwu_status_to_send)
    {
        case RmFwuStatusPostStatus_FWU_DOWNLOAD_SUCCESS:
        case RmFwuStatusPostStatus_FWU_DOWNLOAD_FAILED_TIMEOUT:
        case RmFwuStatusPostStatus_FWU_DOWNLOAD_FAILED_ACCESS_DENIED:
        case RmFwuStatusPostStatus_FWU_DOWNLOAD_FAILED_CRC_FAILED:
        case RmFwuStatusPostStatus_FWU_DOWNLOAD_FAILED_SIGNATURE_FAILED:
        case RmFwuStatusPostStatus_FWU_DOWNLOAD_FAILED_OTHER:
        case RmFwuStatusPostStatus_FWU_INSTALL_SUCCESS:
        case RmFwuStatusPostStatus_FWU_INSTALL_FAILED_PARSE_ERROR:
        case RmFwuStatusPostStatus_FWU_INSTALL_DEPENDENCY_ERROR:
        case RmFwuStatusPostStatus_FWU_INSTALL_FAILED_OTHER:
            break;
            
        case FWU_MGR_NO_STATUS:
            corona_print("no status\n");
            err = ERR_FWU_NO_STATUS;
            goto done_send_status_msg;

        default:
            PROCESS_NINFO(ERR_FWU_UNKNOWN_STATUS_TO_SEND, "stat %u", g_fwu_status_to_send);
            goto done_send_status_msg;
     }


    /*
    **  build checkin message
    */
    _FWUMGR_wmp_gen_status_message(whistle_msg, FWUMGR_buffer, &encode_stream,status);
    corona_print("FWU buf=%d actual=%d\n",FWUMGR_BUFFER_SIZE, encode_stream.bytes_written);

    _watchdog_start(FWUMGR_TCP_RECEIVE_TIMEOUT_WD);
    err = bark_open(g_fwu_conmgr_handle, encode_stream.bytes_written);
    if(err != BARK_OPEN_OK)
    {
        FWUMGR_PRINT("No sock\n");
        if (BARK_OPEN_NOT_CONNECTED == err || BARK_OPEN_CRITICAL == err)
        	err = PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
        else
        	err = PROCESS_NINFO(ERR_FWUMGR_CONMGR_OPEN, NULL);
        goto done_send_status_msg_not_opened;
    }

    /*
    **  send WMP data
    */

//    FWUMGR_PRINT("FWU: sending %d bytes WMP data\n",encode_stream.bytes_written); ;

    _watchdog_start(FWUMGR_TCP_RECEIVE_TIMEOUT_WD);
    err = bark_send(g_fwu_conmgr_handle, FWUMGR_buffer, encode_stream.bytes_written , &send_result);
    if (BARK_OPEN_NOT_CONNECTED == err || BARK_OPEN_CRITICAL == err)
    {
    	err = PROCESS_NINFO(ERR_FWUMGR_NOT_CONNECTED, NULL);
    	goto done_send_status_msg;
    }
    if (send_result != encode_stream.bytes_written)
    {
        err = PROCESS_NINFO(ERR_FWUMGR_CONMGR_SEND, "res: %d", send_result);
        goto done_send_status_msg;
    }
    _watchdog_start(FWUMGR_WATCHDOG_DEF_KICK);

//    FWUMGR_PRINT("FWU: closing handle\n"); ;
done_send_status_msg_not_opened:
	bark_close(g_fwu_conmgr_handle);
	
done_send_status_msg:
    WMP_whistle_msg_free(&whistle_msg);
    return err;
}

void FWUMGR_Send_Checkin(void)
{
	_FWUMGR_process_checkin_blocking();
}


/*FUNCTION****************************************************************
* 
* Function Name    : FWUMGR_main_task
* Returned Value   : None
* Comments         :
*    The Firmware update manager task. 
*
*END*********************************************************************/

void FWU_tsk(uint_32 dummy)
{
    _mqx_uint   mask;

    _FWUMGR_con_mgr_register(FWUMGR_min_wait_time,FWUMGR_max_wait_time);

    FWUMGR_PRINT("FWU strt\n");

    while(1)
    {
        _watchdog_stop();

        _lwevent_wait_ticks(&gfwu_event,FWUMGR_ALL_EVENTS, FALSE, 0);
        
		if (PWRMGR_is_rebooting())
		{
			return;
		}
        
        _watchdog_start(60*1000);

        mask = _lwevent_get_signalled();

        switch (mask)
        {
            case FWUMGR_CONMGR_CONNECT_SUCCEEDED_EVENT:
                // Must process checkin message synchronously. Can't return until done.
                _FWUMGR_process_checkin_blocking();
    
                // Release the waiting thread (CONMGR) that started us.
                _lwevent_set(&gfwu_conmgr_done_event, FWUMGR_CONMGR_DONE_EVENT);
    
                // This must be called on our thread, AFTER setting FWUMGR_CONMGR_DONE_EVENT, BEFORE _FWUMGR_con_mgr_register
                _FWUMGR_release_handle();
                
                _FWUMGR_con_mgr_register(FWUMGR_min_wait_time,FWUMGR_max_wait_time);
    
                break;
                
            case FWUMGR_CONMGR_CONNECT_FAILED_EVENT:
                FWUMGR_PRINT("conn fail\n");
                _FWUMGR_con_mgr_register(FWUMGR_min_wait_time,FWUMGR_max_wait_time);
                break;
    
            case FWUMGR_PRINT_PACKAGE_EVENT:
                FWUMGR_Print_Package();
                break;
    
    
            case FWUMGR_CONMGR_DISCONNECTED_EVENT:
    
                /*
                **  clean up state variables after disconnect
                */
                g_fwu_status_to_send = FWU_MGR_NO_STATUS;
                _FWUMGR_con_mgr_register(FWUMGR_min_wait_time,FWUMGR_max_wait_time);
    
                break;

        }
    }
}

/*FUNCTION****************************************************************
* 
* Function Name    : FWUMGR_init
* Returned Value   : 
* Comments         :
*    Do any initialization necessary before the task begins to run. 
*
*END*********************************************************************/

extern uint32_t _mqx_kernel_data_size;

void FWUMGR_init(void)
{
#if 0
    FWUMGR_PRINT("FWU: Firmware Manager initialization\n");

    FWUMGR_PRINT("FWU: Persistent data @ 0x%05x, %4dKB (%7d bytes)\n",
            FWUMGR_PERSISTENT_DATA_START,FWUMGR_PERSISTENT_DATA_SIZE/1024,FWUMGR_PERSISTENT_DATA_SIZE);

    FWUMGR_PRINT("FWU: Firmware Image  @ 0x%05x, %4dKB (%7d bytes)\n",
            FWUMGR_FIRMWARE_IMAGE_START,FWUMGR_FIRMWARE_IMAGE_SIZE/1024,FWUMGR_FIRMWARE_IMAGE_SIZE);

    FWUMGR_PRINT("FWU: Backup Image    @ 0x%05x, %4dKB (%7d bytes)\n",
            FWUMGR_BACKUP_IMAGE_START,FWUMGR_BACKUP_IMAGE_SIZE/1024,FWUMGR_BACKUP_IMAGE_SIZE);

#endif

    g_fwu_status_to_send = FWU_MGR_NO_STATUS;
    g_fwu_conmgr_handle = 0;

    _lwevent_create(&gfwu_event, LWEVENT_AUTO_CLEAR);
    _lwevent_create(&gfwu_conmgr_done_event, LWEVENT_AUTO_CLEAR);
    
    // Since we block the CONMGR thread during our callback, CONMGR won't be able to tell us when the
    // connection goes down, so we need to watch that ourselves
	sys_event_register_cbk(FWUMGR_disconnected, SYS_EVENT_WIFI_DOWN);


    return;
}

