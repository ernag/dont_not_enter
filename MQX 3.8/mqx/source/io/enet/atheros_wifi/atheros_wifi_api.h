//------------------------------------------------------------------------------
// Copyright (c) 2011 Qualcomm Atheros, Inc.
// All Rights Reserved.
// Qualcomm Atheros Confidential and Proprietary.
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is
// hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================
#ifndef __ATHEROS_WIFI_API_H__
#define __ATHEROS_WIFI_API_H__

/* PORT_NOTE: atheros_wifi_api.h is intended to expose user level API's and
 *  data structures.  These structures provide access to the driver that
 *  would not otherwise be provided by the system's existing driver interface.
 *  It is imagined that the existing driver interface would have some form
 *  of IO_CTRL API with pre-defined commands. These commands are intended to
 *  extend those provided by the default.  It is further imagined that in
 *  such a system the developer would add an IO_CTRL for Vendor specific
 *  commands which would act as a wrapper for the ath_ioctl_cmd commands
 *  defined below. */

#include <a_types.h>

enum ath_ioctl_cmd
{
	ATH_SET_TXPWR,
	ATH_SET_PMPARAMS,
	ATH_SET_LISTEN_INT,
	ATH_SET_CIPHER,
	ATH_SET_SEC_MODE,
	ATH_SET_PHY_MODE,
	ATH_GET_PHY_MODE,
	ATH_GET_RX_RSSI, /* output uint_32 */
	ATH_SET_CONNECT_STATE_CALLBACK, /* input ATH_CONNECT_CB */
	ATH_DEVICE_SUSPEND_ENABLE, /* input NONE */
	ATH_DEVICE_SUSPEND_START, /* */
	ATH_SET_PMK,
	ATH_GET_PMK,
	ATH_GET_VERSION,
	ATH_START_WPS,
	ATH_AWAIT_WPS_COMPLETION,
	ATH_SCAN_CTRL,
	ATH_CHIP_STATE,
	ATH_MAC_TX_RAW,
	ATH_SET_CHANNEL,
	ATH_SET_AGGREGATION,
	ATH_ASSERT_DUMP,
    ATH_SET_SSID,
    ATH_SET_CONNECT,
    ATH_SET_SEC_TYPE,
    ATH_SET_COMMIT,
    ATH_SET_MODE,
    ATH_SET_FREQ,
    ATH_SET_RTS,
    ATH_SET_PASSPHRASE,
    ATH_SET_SCAN,
    ATH_SET_ENCODE,
    ATH_SET_POWER,
    ATH_GET_POWER,
    ATH_GET_ESSID,
    ATH_GET_SEC_TYPE,
	ATH_PROGRAM_FLASH,
	ATH_EXECUTE_FLASH,
    ATH_GET_MACADDR,
    ATH_IS_DRIVER_INITIALIZED,
    ATH_GET_TX_STATUS,
    ATH_SET_PROMISCUOUS_MODE,
    ATH_GET_REG_DOMAIN,
    ATH_START_SCAN_EXT,
    ATH_GET_SCAN_EXT,
    ATH_GET_LAST_ERROR,
    ATH_GET_CHANNEL,
    ATH_CONFIG_AP,
    ATH_P2P_CONNECT,
    ATH_P2P_CONNECT_CLIENT,
    ATH_P2P_FIND,
    ATH_P2P_LISTEN,
    ATH_P2P_CANCEL,
    ATH_P2P_STOP,
    ATH_P2P_JOIN,
    ATH_P2P_NODE_LIST,
    ATH_P2P_SET_CONFIG,
    ATH_P2P_WPS_CONFIG,
    ATH_P2P_AUTH,
    ATH_P2P_DISC_REQ,
    ATH_P2P_SET,
    ATH_P2P_INVITE_AUTH,
    ATH_P2P_PERSISTENT_LIST,
    ATH_P2P_INVITE,
    ATH_P2P_INV_CONNECT,
    ATH_P2P_JOIN_PROFILE,
    ATH_P2P_APMODE,
    ATH_P2P_APMODE_PP,
    ATH_P2P_SWITCH,
    ATH_P2P_SET_NOA,
    ATH_P2P_SET_OPPPS,
    ATH_P2P_SDPD,	
    ATH_ONOFF_LPL,
    ATH_ONOFF_GTX,
    ATH_PROGRAM_MAC_ADDR,
    ATH_GET_RATE,
	/************************************/
	/* add new commands above this line */
	/************************************/
	ATH_CMD_LAST /* NOTE: ensure that this is the last entry in the enum */
};



// IEEE 802.11 channels in MHz										Geographies where channel is allowed
#define ATH_IOCTL_FREQ_1									 (2412) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_2									 (2417) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_3									 (2422) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_4									 (2427) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_5									 (2432) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_6									 (2437) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_7									 (2442) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_8									 (2447) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_9									 (2452) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_10									 (2457) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_11									 (2462) // USA, Canada, Europe, Japan
#define ATH_IOCTL_FREQ_12									 (2467) // Europe, Japan
#define ATH_IOCTL_FREQ_13									 (2472) // Europe, Japan
#define ATH_IOCTL_FREQ_14									 (2484) // Japan


#define ATH_PMK_LEN     		(32)
#define ATH_PASSPHRASE_LEN		(64)
#define ATH_MAX_SSID_LENGTH 	(32)
#define ATH_MAX_SCAN_CHANNELS 	(16)
#define ATH_MAX_SCAN_BUFFERS    (12) // TODO Atheros says this affects memory size

#if DRIVER_CONFIG_ENABLE_STORE_RECALL
#define STORE_RECALL_BUF_SIZE   (1400)			//If store-recall is enabled, allocate shared buffer
                                                // big enough to hold 1400 bytes of target data
#else
#define STORE_RECALL_BUF_SIZE   (0)
#endif

#define ATH_WPS_PIN_LEN 		(9)
#define ATH_WPS_MODE_PIN 		(1)
#define ATH_WPS_MODE_PUSHBUTTON (2)
#define WEP_SHORT_KEY 			(5)
#define WEP_LONG_KEY 			(13)
#define ATH_ACTIVE_CHAN_DWELL_TIME (60)			//Max Dwell time per channel

/*Structure definition for passing Atheros specific data from App*/
typedef struct ath_ioctl_params
{
	A_UINT16 cmd_id;
	A_VOID* data;
	A_UINT16 length;
} ATH_IOCTL_PARAM_STRUCT, * ATH_IOCTL_PARAM_STRUCT_PTR;

typedef struct ath_ap_params
{
	A_UINT16 cmd_subset;
	A_VOID* data;
} ATH_AP_PARAM_STRUCT, * ATH_AP_PARAM_STRUCT_PTR;
//AP mode sub commands
#define AP_SUB_CMD_BCON_INT    0x0001
#define AP_SUB_CMD_HIDDEN_FLAG 0x0002
#define AP_SUB_CMD_INACT_TIME  0x0003
#define AP_SUB_CMD_SET_COUNTRY 0x0004
#define AP_SUB_CMD_WPS_FLAG    0x0005
#define AP_SUB_CMD_DTIM_INT    0x0006
#define AP_SUB_CMD_PSBUF       0x0007
/* A-MPDU aggregation is enabled on a per TID basis where each TID (0-7)
 * represents a different traffic priority.  The mapping to WMM access categories
 * is as follows; WMM Best effort = TID 0-1
 *				  WMM Background  = TID 2-3
 *				  WMM Video		  = TID 4-5
 *				  WMM Voice 	  = TID 6-7
 * Once enabled A-MPDU aggregation may be negotiated with an Access Point/Peer
 * device and then both devices may optionally use A-MPDU aggregation for
 * transmission.  Due to other bottle necks in the data path a system may not
 * get improved performance by enabling A-MPDU aggregation.
 */
typedef struct
{
	A_UINT16 txTIDMask; /* bit mask to enable tx A-MPDU aggregation */
	A_UINT16 rxTIDMask; /* bit mask to enable rx A-MPDU aggregation */
}ATH_SET_AGGREGATION_PARAM;

#define ATH_CIPHER_TYPE_TKIP 0x04
#define ATH_CIPHER_TYPE_CCMP 0x08
#define ATH_CIPHER_TYPE_WEP  0x02

typedef struct cipher
{
	A_UINT32 ucipher;
	A_UINT32 mcipher;
} cipher_t;

typedef void (*ATH_CONNECT_CB)(int value);

typedef struct _wepkeys
{
	A_UINT8 defKeyIndex;/* tx key index */
	A_UINT8 keyLength; /* must be one of WEP_SHORT_KEY || WEP_LONG_KEY */
	A_UINT8 numKeys; /* how many of the 4 keys below are populated */
	A_CHAR  *keys[4];/* keys */
}ENET_WEPKEYS,* ENET_WEPKEYS_PTR;

typedef struct
{
	A_UINT8 defKeyIndex;/* tx key index */
	A_UINT8 keyLength; /* must be one of WEP_SHORT_KEY || WEP_LONG_KEY */
	A_UINT8 numKeys; /* how many of the 4 keys below are populated */
	A_CHAR *keys[4][WEP_LONG_KEY];/* keys */
}ATH_WEPKEYS,* ATH_WEPKEYS_PTR;

typedef struct {
    A_UINT8 ssid[32];
    A_UINT8 macaddress[6];
    A_UINT16 channel;
    A_UINT8 ssid_len;
} WPS_SCAN_LST_ENTRY;

typedef struct {
	A_UINT32 host_ver;
	A_UINT32 target_ver;
	A_UINT32 wlan_ver;
	A_UINT32 abi_ver;
}ATH_VERSION,* ATH_VERSION_PTR;


typedef struct _wps_start
{
	WPS_SCAN_LST_ENTRY ssid_info;
	A_UINT8 wps_mode; /* ATH_WPS_MODE_PIN | ATH_WPS_MODE_PUSHBUTTON */
	A_UINT8 timeout_seconds;
    A_UINT8 connect_flag;
    A_UINT8 pin[ATH_WPS_PIN_LEN];
    A_UINT8 pin_length;
} ATH_WPS_START, *ATH_WPS_START_PTR;

typedef struct {
    A_UINT16    idle_period;             /* msec */
    A_UINT16    pspoll_number;
    A_UINT16    dtim_policy;       /*IGNORE_DTIM = 0x01, NORMAL_DTIM = 0x02,STICK_DTIM  = 0x03, AUTO_DTIM = 0x04*/
    A_UINT16    tx_wakeup_policy;  /*TX_WAKEUP_UPON_SLEEP = 1,TX_DONT_WAKEUP_UPON_SLEEP = 2*/
    A_UINT16    num_tx_to_wakeup;
    A_UINT16    ps_fail_event_policy; /*SEND_POWER_SAVE_FAIL_EVENT_ALWAYS = 1,IGNORE_POWER_SAVE_FAIL_EVENT_DURING_SCAN = 2*/
}  ATH_WMI_POWER_PARAMS_CMD;

typedef enum {
  ATH_WPS_ERROR_INVALID_START_INFO  = 0x1,
  ATH_WPS_ERROR_MULTIPLE_PBC_SESSIONS,
  ATH_WPS_ERROR_WALKTIMER_TIMEOUT,
  ATH_WPS_ERROR_M2D_RCVD
} ATH_WPS_ERROR_CODE;


#define  ATH_WPS_ERROR_SUCCESS					0x00
#define  ATH_WPS_ERROR_INVALID_START_INFO   	0x01
#define  ATH_WPS_ERROR_MULTIPLE_PBC_SESSIONS 	0x02
#define  ATH_WPS_ERROR_WALKTIMER_TIMEOUT 		0x03
#define  ATH_WPS_ERROR_M2D_RCVD 				0x04
#define  ATH_WPS_ERROR_PWD_AUTH_FAIL            0x05
#define  ATH_WPS_ERROR_CANCELLED                0x06
#define  ATH_WPS_ERROR_INVALID_PIN              0x07

typedef struct {
	A_INT8 ssid[ATH_MAX_SSID_LENGTH+1]; /* [OUT] network ssid */
	A_INT16 ssid_len; /* [OUT] number of valid chars in ssid[] */
	cipher_t cipher; /* [OUT] network cipher type values not defined */
	A_UINT8 key_index; /* [OUT] for WEP only. key index for tx */
	union{ /* [OUT] security key or passphrase */
		A_UINT8 wepkey[ATH_PASSPHRASE_LEN+1];
		A_UINT8 passphrase[ATH_PASSPHRASE_LEN+1];
	}u;

	A_UINT8 sec_type; /* [OUT] security type; one of ENET_MEDIACTL_SECURITY_TYPE... */
	A_UINT8 error; /* [OUT] error code one of ATH_WPS_ERROR_... */
	A_UINT8 dont_block;  /* [IN] 1 - returns immediately if operation is not complete.
						 * 		0 - blocks until operation completes. */
}ATH_NETPARAMS;

#define ATH_DISABLE_BG_SCAN (0x00000001)
#define ATH_DISABLE_FG_SCAN (0x00000002)

typedef struct {
	A_UINT32 flags;
}ATH_SCANPARAMS;

/*
 *   Helper functions to register a callback to call when an Atheros driver assert occurs.
 */
typedef void ( *atheros_assert_cbk_t )( const char *pAssertString );
void ATHEROS_register_assert_cbk( atheros_assert_cbk_t pCbk );



typedef struct ath_program_flash
{
	A_UINT8* buffer;
	A_UINT32 load_addr;
	A_UINT16 length;
	A_UINT32 result;
} ATH_PROGRAM_FLASH_STRUCT;

/* ATH_MAC_TX_FLAG... are used as TX qualifiers for frames containing WMI_TX_RATE_SCHEDULE in the
 * meta data.  0 or more of these flags should be assigned to the flags member of the schedule. */
#define ATH_MAC_TX_FLAG_ACK            0x01 // frame needs ACK response from receiver
#define ATH_MAC_TX_FLAG_SET_RETRY_BIT  0x02 // device will set retry bit in MAC header for retried frames.
#define ATH_MAC_TX_FLAG_SET_DURATION   0x04 // device will fill duration field in MAC header
/* NOTE: If ATH_MAC_TX_FLAG_USE_PREFIX == 0 device will NOT use prefix frame.
 *       If ATH_MAC_TX_FLAG_USE_PREFIX == 1 && ATH_MAC_TX_FLAG_PREFIX_RTS == 0 device will use CTS prefix.
 *       If ATH_MAC_TX_FLAG_USE_PREFIX == 1 && ATH_MAC_TX_FLAG_PREFIX_RTS == 1 device will use RTS prefix.
 */
#define ATH_MAC_TX_FLAG_USE_PREFIX     0x08 // device will send either RTS or CTS frame prior to subject frame.
#define ATH_MAC_TX_FLAG_PREFIX_RTS     0x10 // device will send RTS and wait for CTS prior to sending subject frame.
#define ATH_MAC_TX_LOAD_TSF            0x20 // device will fill the TSF field during transmit procedure. <Beacons/probe responses>

#define ATH_ACCESS_CAT_BE 	0       /* best effort */
#define ATH_ACCESS_CAT_BK 	1 		/* background */
#define ATH_ACCESS_CAT_VI 	2 		/* video */
#define ATH_ACCESS_CAT_VO 	3 		/* voice */

/* set ar4XXX_boot_param according to the desired boot options */
/* AR4XXX_PARAM_MODE_NORMAL - instructs chip to boot normally; load wlan firmware to provide
 *							  WMI services used in normal WIFI operation.
 * AR4XXX_PARAM_MODE_BMI - instructs chip to boot to BMI and await further BMI communication
 *	from host processor.  This option can be used to re-program chip serial flash and
 *	perform other non-standard tasks.
 * AR4XXX_PARAM_QUAD_SPI_FLASH - instructs the chip to access flash chip using QUAD mode
 *  this mode is faster than standard mode but the flash chip must support it. This
 *	option can be OR'd (|) with other options.
 * AR4XXX_PARAM_RAWMODE_BOOT - instructs the chip to boot a firmware image that supports the
 *	RAW TX mode but lacks support for many other WMI commands.  This mode will allow for
 *	a faster boot in those instances where the full set of WMI commands is not necessary.
 *	This mode must be OR'd with AR4XXX_PARAM_NORMAL_BOOT.
 */

#define AR4XXX_PARAM_MODE_NORMAL 	(0x00000002)
#define AR4XXX_PARAM_MODE_BMI	 	(0x00000003)
#define AR4XXX_PARAM_MODE_MASK		(0x0000000f)
#define AR4XXX_PARAM_QUAD_SPI_FLASH (0x80000000)
#define AR4XXX_PARAM_RAWMODE_BOOT   (0x00000010 | AR4XXX_PARAM_MODE_NORMAL)
#define AR4XXX_PARAM_MACPROG_MODE   (0x00000020 | AR4XXX_PARAM_MODE_NORMAL)
#define AR4XXX_PARAM_MANUFAC_MODE   (0x00000030 | AR4XXX_PARAM_MODE_NORMAL)

/* combined params for common cases */
#define AR4XXX_PARAM_NORMAL_QUAD	(AR4XXX_PARAM_MODE_NORMAL | AR4XXX_PARAM_QUAD_SPI_FLASH)
#define AR4XXX_PARAM_RAW_QUAD		(AR4XXX_PARAM_RAWMODE_BOOT | AR4XXX_PARAM_QUAD_SPI_FLASH)

/*The following regulatory domain settings can be passed to target during boot*/

#define AR4XXX_PARAM_REG_DOMAIN_DEFAULT  (0x00000000)    /*Default regulatory domain*/
#define AR4XXX_PARAM_REG_DOMAIN_1        (0x00000100)    /* FCC3_FCCA reg domain*/
#define AR4XXX_PARAM_REG_DOMAIN_2        (0x00000200)    /* ETSI1_WORLD reg domain*/
#define AR4XXX_PARAM_REG_DOMAIN_3        (0x00000300)    /* MKK5_MKKC reg domain */

/* ATH_MAC_TX_RATE_SCHEDULE - Acts as a host-provided rate schedule to replace what would be normally determined
 * by firmware.  This allows the host to specify what rates and attempts should be used to transmit the
 * frame. */
typedef struct {
#define ATH_MAC_TX_MAX_RATE_SERIES (4)
    A_UINT8 rateSeries[ATH_MAC_TX_MAX_RATE_SERIES]; //rate index for each series. first invalid rate terminates series.
    A_UINT8 trySeries[ATH_MAC_TX_MAX_RATE_SERIES]; //number of tries for each series.
    A_UINT8 flags; // combination of ATH_MAC_TX_FLAG...
    A_UINT8 accessCategory; // should be ATH_ACCESS_CAT_BE for managment frames and multicast frames.
}ATH_MAC_TX_RATE_SCHEDULE;

typedef struct {
    ATH_MAC_TX_RATE_SCHEDULE rateSched;
    A_UINT8     pktID;           /* The packet ID to identify the tx request */
}ATH_MAC_TX_PARAMS;


typedef struct ath_mac_tx_raw
{
	A_UINT8* 			buffer; /* pointer to contiguous tx buffer */
	A_UINT16 			length; /* valid length in bytes of tx buffer */
	ATH_MAC_TX_PARAMS 	params; /* params governing transmit rules */
} ATH_MAC_TX_RAW_S;

typedef struct ath_tx_status
{
#define ATH_TX_STATUS_IDLE 0x01 /* the TX pipe is 100% idle */
#define ATH_TX_STATUS_HOST_PENDING 0x02 /* the TX pipe has 1 or more frames waiting in the host queue */
#define ATH_TX_STATUS_WIFI_PENDING 0x03 /* the TX pipe has 1 or more frames in process on the WIFI device */
	A_UINT16 status; /* one of ATH_TX_STATUS_ */
} ATH_TX_STATUS;

typedef A_VOID (*ATH_PROMISCUOUS_CB)(A_VOID*);

typedef struct ath_prom_mode
{
	A_UINT8 src_mac[6]; /* filter source mac address if desired. */
	A_UINT8 dst_mac[6]; /* filter destination mac address if desired. */
	A_UINT8 enable; /* 0 to disable promiscuous mode 1 to enable. */
#define ATH_PROM_FILTER_SOURCE 0x01 /* only allow frames whose source mac matches src_mac */
#define ATH_PROM_FILTER_DEST 0x02 /* only allow frames whose destination mac matches dst_mac */
	A_UINT8 filter_flags; /* filtering rules */
	ATH_PROMISCUOUS_CB cb; /* callback function driver will use to feed rx frames */
}ATH_PROMISCUOUS_MODE;

typedef struct ath_reg_domain
{
	A_UINT32 domain;
} ATH_REG_DOMAIN;


#define SECURITY_AUTH_PSK    0x01
#define SECURITY_AUTH_1X     0x02

typedef struct ath_scan_ext
{
	A_UINT8 channel;
    A_UINT8 ssid_len;
    A_UINT8 rssi;
    A_UINT8 security_enabled;
    A_UINT16 beacon_period;
    A_UINT8 preamble;
    A_UINT8 bss_type;
    A_UINT8 bssid[6];
    A_UINT8 ssid[32];
    A_UINT8 rsn_cipher;
    A_UINT8 rsn_auth;
    A_UINT8 wpa_cipher;
    A_UINT8 wpa_auth;
}ATH_SCAN_EXT;

typedef struct ath_get_scan
{
	A_UINT16 num_entries;
	ATH_SCAN_EXT *scan_list;
}ATH_GET_SCAN;

typedef struct ath_program_mac_addr
{
	A_UINT8 addr[6];
	A_UINT8 result;
}ATH_PROGRAM_MAC_ADDR_PARAM;

/* these result codes are provided in the result field of the ATH_PROGRAM_MAC_ADDR_PARAM structure */
/*ATH_PROGRAM_MAC_RESULT_SUCCESS - successfully (re-)programmed mac address into wifi device*/
#define ATH_PROGRAM_MAC_RESULT_SUCCESS 			(1)
/*ATH_PROGRAM_MAC_RESULT_DEV_DENIED - Device denied the operation for several possible reasons 
 * the most common reason for this result is that the mac address equals the current mac 
 * address found already in the device.  Also an invalid mac address value can cause this result
 */
#define ATH_PROGRAM_MAC_RESULT_DEV_DENIED 		(2)
/*ATH_PROGRAM_MAC_RESULT_DEV_FAILED - Device tried but failed to program mac address.
 * An error occurred on the device as it tried to program the mac address. 
 */
#define ATH_PROGRAM_MAC_RESULT_DEV_FAILED 		(3)
/*ATH_PROGRAM_MAC_RESULT_DRIVER_FAILED - Driver tried but failed to program mac address 
 * Possibly the driver did not have the proper code compiled to perform this operation.
 */
#define ATH_PROGRAM_MAC_RESULT_DRIVER_FAILED 	(4)


#endif /* __ATHEROS_WIFI_H__ */
