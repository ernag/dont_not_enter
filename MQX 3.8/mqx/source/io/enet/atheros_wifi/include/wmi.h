//------------------------------------------------------------------------------
// <copyright file="wmi.h" company="Atheros">
//    Copyright (c) 2004-2008 Atheros Corporation.  All rights reserved.
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

/*
 * This file contains the definitions of the WMI protocol specified in the
 * Wireless Module Interface (WMI).  It includes definitions of all the
 * commands and events. Commands are messages from the host to the WM.
 * Events and Replies are messages from the WM to the host.
 *
 * Ownership of correctness in regards to commands
 * belongs to the host driver and the WMI is not required to validate
 * parameters for value, proper range, or any other checking.
 *
 */

#ifndef _WMI_H_
#define _WMI_H_


//#include "wmix.h"
#include "wlan_defs.h"
#include "a_osapi.h"
//athstartpack.h must be the last include else it may be
//undone by another include file including athendpack.h
#ifndef ATH_TARGET
#include "athstartpack.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HTC_PROTOCOL_VERSION    0x0002
#define HTC_PROTOCOL_REVISION   0x0000

#define WMI_PROTOCOL_VERSION    0x0002
#define WMI_PROTOCOL_REVISION   0x0000

#define ATH_MAC_LEN             6               /* length of mac in bytes */
#define WMI_CMD_MAX_LEN         100
#define WMI_CONTROL_MSG_MAX_LEN     256
#define WMI_OPT_CONTROL_MSG_MAX_LEN 1536
#define IS_ETHERTYPE(_typeOrLen)        ((_typeOrLen) >= 0x0600)
#define RFC1042OUI      {0x00, 0x00, 0x00}

#define IP_ETHERTYPE 0x0800

#define WMI_IMPLICIT_PSTREAM 0xFF
#define WMI_MAX_THINSTREAM 15

#ifdef AR6002_REV2
#define IBSS_MAX_NUM_STA          4
#else
#define IBSS_MAX_NUM_STA          8
#endif

struct host_app_area_s {
    A_UINT32 wmi_protocol_ver;
};

typedef PREPACK struct {
	A_UINT8 a FIELD_PACKED;
	A_UINT8 b FIELD_PACKED;
	A_UINT8 c FIELD_PACKED;
}POSTPACK MY_TEST;


/*
 * Data Path
 */
typedef PREPACK struct {
    A_UINT8     dstMac[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT8     srcMac[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT16    typeOrLen FIELD_PACKED;
} POSTPACK ATH_MAC_HDR;

typedef PREPACK struct {
    A_UINT8     dsap FIELD_PACKED;
    A_UINT8     ssap FIELD_PACKED;
    A_UINT8     cntl FIELD_PACKED;
    A_UINT8     orgCode[3] FIELD_PACKED;
    A_UINT16    etherType FIELD_PACKED;
} POSTPACK ATH_LLC_SNAP_HDR;

typedef enum {
    DATA_MSGTYPE = 0x0,
    CNTL_MSGTYPE,
    SYNC_MSGTYPE,
    OPT_MSGTYPE
} WMI_MSG_TYPE;


/*
 * Macros for operating on WMI_DATA_HDR (info) field
 */

#define WMI_DATA_HDR_MSG_TYPE_MASK  0x03
#define WMI_DATA_HDR_MSG_TYPE_SHIFT 0
#define WMI_DATA_HDR_UP_MASK        0x07
#define WMI_DATA_HDR_UP_SHIFT       2
/* In AP mode, the same bit (b5) is used to indicate Power save state in
 * the Rx dir and More data bit state in the tx direction.
 */
#define WMI_DATA_HDR_PS_MASK        0x1
#define WMI_DATA_HDR_PS_SHIFT       5

#define WMI_DATA_HDR_MORE_MASK      0x1
#define WMI_DATA_HDR_MORE_SHIFT     5

typedef enum {
    WMI_DATA_HDR_DATA_TYPE_802_3 = 0,
    WMI_DATA_HDR_DATA_TYPE_802_11,
    WMI_DATA_HDR_DATA_TYPE_ACL
} WMI_DATA_HDR_DATA_TYPE;

#define WMI_DATA_HDR_DATA_TYPE_MASK     0x3
#define WMI_DATA_HDR_DATA_TYPE_SHIFT    6

#define WMI_DATA_HDR_SET_MORE_BIT(h) ((h)->info |= (WMI_DATA_HDR_MORE_MASK << WMI_DATA_HDR_MORE_SHIFT))

#define WMI_DATA_HDR_IS_MSG_TYPE(h, t)  (((h)->info & (WMI_DATA_HDR_MSG_TYPE_MASK)) == (t))
#define WMI_DATA_HDR_SET_MSG_TYPE(h, t) (h)->info = (A_UINT8)(((h)->info & ~(WMI_DATA_HDR_MSG_TYPE_MASK << WMI_DATA_HDR_MSG_TYPE_SHIFT)) | (t << WMI_DATA_HDR_MSG_TYPE_SHIFT))
#define WMI_DATA_HDR_GET_UP(h)    (((h)->info >> WMI_DATA_HDR_UP_SHIFT) & WMI_DATA_HDR_UP_MASK)
#define WMI_DATA_HDR_SET_UP(h, p) (h)->info = (A_UINT8)(((h)->info & ~(WMI_DATA_HDR_UP_MASK << WMI_DATA_HDR_UP_SHIFT)) | (p << WMI_DATA_HDR_UP_SHIFT))

#define WMI_DATA_HDR_GET_DATA_TYPE(h)   (((h)->info >> WMI_DATA_HDR_DATA_TYPE_SHIFT) & WMI_DATA_HDR_DATA_TYPE_MASK)
#define WMI_DATA_HDR_SET_DATA_TYPE(h, p) (h)->info = (A_UINT8)(((h)->info & ~(WMI_DATA_HDR_DATA_TYPE_MASK << WMI_DATA_HDR_DATA_TYPE_SHIFT)) | ((p) << WMI_DATA_HDR_DATA_TYPE_SHIFT))

#define WMI_DATA_HDR_GET_DOT11(h)   (WMI_DATA_HDR_GET_DATA_TYPE((h)) == WMI_DATA_HDR_DATA_TYPE_802_11)
#define WMI_DATA_HDR_SET_DOT11(h, p) WMI_DATA_HDR_SET_DATA_TYPE((h), (p))

/* Macros for operating on WMI_DATA_HDR (info2) field */
#define WMI_DATA_HDR_SEQNO_MASK     0xFFF
#define WMI_DATA_HDR_SEQNO_SHIFT    0

#define WMI_DATA_HDR_AMSDU_MASK     0x1
#define WMI_DATA_HDR_AMSDU_SHIFT    12

#define WMI_DATA_HDR_META_MASK      0x7
#define WMI_DATA_HDR_META_SHIFT     13

#define GET_SEQ_NO(_v)                  ((_v) & WMI_DATA_HDR_SEQNO_MASK)
#define GET_ISMSDU(_v)                  ((_v) & WMI_DATA_HDR_AMSDU_MASK)

#define WMI_DATA_HDR_GET_SEQNO(h)        GET_SEQ_NO((h)->info2 >> WMI_DATA_HDR_SEQNO_SHIFT)
#define WMI_DATA_HDR_SET_SEQNO(h, _v)   ((h)->info2 = ((h)->info2 & ~(WMI_DATA_HDR_SEQNO_MASK << WMI_DATA_HDR_SEQNO_SHIFT)) | (GET_SEQ_NO(_v) << WMI_DATA_HDR_SEQNO_SHIFT))

#define WMI_DATA_HDR_IS_AMSDU(h)        GET_ISMSDU((h)->info2 >> WMI_DATA_HDR_AMSDU_SHIFT)
#define WMI_DATA_HDR_SET_AMSDU(h, _v)   ((h)->info2 = (A_UINT16)((h)->info2 & ~(WMI_DATA_HDR_AMSDU_MASK << WMI_DATA_HDR_AMSDU_SHIFT)) | (GET_ISMSDU(_v) << WMI_DATA_HDR_AMSDU_SHIFT))

#define WMI_DATA_HDR_GET_META(h)        (((h)->info2 >> WMI_DATA_HDR_META_SHIFT) & WMI_DATA_HDR_META_MASK)
#define WMI_DATA_HDR_SET_META(h, _v)    ((h)->info2 = (A_UINT16)(((h)->info2 & ~(WMI_DATA_HDR_META_MASK << WMI_DATA_HDR_META_SHIFT)) | ((_v) << WMI_DATA_HDR_META_SHIFT)))

typedef PREPACK struct {
    A_INT8      rssi FIELD_PACKED;
    A_UINT8     info FIELD_PACKED;               /* usage of 'info' field(8-bit):
                                     *  b1:b0       - WMI_MSG_TYPE
                                     *  b4:b3:b2    - UP(tid)
                                     *  b5          - Used in AP mode. More-data in tx dir, PS in rx.
                                     *  b7:b6       -  Dot3 header(0),
                                     *                 Dot11 Header(1),
                                     *                 ACL data(2)
                                     */

    A_UINT16    info2 FIELD_PACKED;              /* usage of 'info2' field(16-bit):
                                     * b11:b0       - seq_no
                                     * b12          - A-MSDU?
                                     * b15:b13      - META_DATA_VERSION 0 - 7
                                     */
    A_UINT16    reserved FIELD_PACKED;
} POSTPACK WMI_DATA_HDR;

/*
 *  TX META VERSION DEFINITIONS
 */
#define WMI_MAX_TX_META_SZ  (12)
#define WMI_MAX_TX_META_VERSION (7)
#define WMI_META_VERSION_1 (0x01)
#define WMI_META_VERSION_2 (0x02)
#define WMI_META_VERSION_3 (0x03)
#define WMI_META_VERSION_4 (0x04)
#define WMI_META_VERSION_5 (0x05)	//USed for stack offload packets

#define WMI_ACL_TO_DOT11_HEADROOM   36

#if 0 /* removed to prevent compile errors for WM.. */
typedef PREPACK struct {
/* intentionally empty. Default version is no meta data. */
} POSTPACK WMI_TX_META_V0;
#endif

typedef PREPACK struct {
    A_UINT8     pktID FIELD_PACKED;           /* The packet ID to identify the tx request */
    A_UINT8     ratePolicyID FIELD_PACKED;    /* The rate policy to be used for the tx of this frame */
} POSTPACK WMI_TX_META_V1;


#define WMI_CSUM_DIR_TX (0x1)
#define TX_CSUM_CALC_FILL (0x1)
typedef PREPACK struct {
    A_UINT8    csumStart FIELD_PACKED;       /*Offset from start of the WMI header for csum calculation to begin */
    A_UINT8    csumDest FIELD_PACKED;        /*Offset from start of WMI header where final csum goes*/
    A_UINT8     csumFlags FIELD_PACKED;    /*number of bytes over which csum is calculated*/
} POSTPACK WMI_TX_META_V2;


/* WMI_META_TX_FLAG... are used as TX qualifiers for frames containing WMI_TX_RATE_SCHEDULE in the
 * meta data.  0 or more of these flags should be assigned to the flags member of the schedule. */
#define WMI_META_TX_FLAG_ACK            0x01 // frame needs ACK response from receiver
#define WMI_META_TX_FLAG_SET_RETRY_BIT  0x02 // device will set retry bit in MAC header for retried frames.
#define WMI_META_TX_FLAG_SET_DURATION   0x04 // device will fill duration field in MAC header
/* NOTE: If WMI_META_TX_FLAG_USE_PREFIX == 0 device will NOT use prefix frame.
 *       If WMI_META_TX_FLAG_USE_PREFIX == 1 && WMI_META_TX_FLAG_PREFIX_RTS == 0 device will use CTS prefix.
 *       If WMI_META_TX_FLAG_USE_PREFIX == 1 && WMI_META_TX_FLAG_PREFIX_RTS == 1 device will use RTS prefix.
 */
#define WMI_META_TX_FLAG_USE_PREFIX     0x08 // device will send either RTS or CTS frame prior to subject frame.
#define WMI_META_TX_FLAG_PREFIX_RTS     0x10 // device will send RTS and wait for CTS prior to sending subject frame.
#define WMI_META_TX_LOAD_TSF            0x20 // device will fill the TSF field during transmit procedure. <Beacons/probe responses>

/* WMI_TX_RATE_SCHEDULE - Acts as a host-provided rate schedule to replace what would be normally determined
 * by firmware.  This allows the host to specify what rates and attempts should be used to transmit the
 * frame. */
typedef PREPACK struct {
#define WMI_TX_MAX_RATE_SERIES (4)
    A_UINT8 rateSeries[WMI_TX_MAX_RATE_SERIES] FIELD_PACKED; //rate index for each series. first invalid rate terminates series.
    A_UINT8 trySeries[WMI_TX_MAX_RATE_SERIES] FIELD_PACKED; //number of tries for each series.
    A_UINT8 flags FIELD_PACKED; // combination of WMI_META_TX_FLAG...
    A_UINT8 accessCategory FIELD_PACKED; // should be WMM_AC_BE for managment frames and multicast frames.
    //A_UINT8 keyIndex FIELD_PACKED;
    //
}POSTPACK WMI_TX_RATE_SCHEDULE;

typedef PREPACK struct {
    WMI_TX_RATE_SCHEDULE rateSched FIELD_PACKED;
    A_UINT8     pktID FIELD_PACKED;           /* The packet ID to identify the tx request */
} POSTPACK WMI_TX_META_V3;

typedef PREPACK struct {
    A_UINT8 reserved FIELD_PACKED;
} POSTPACK WMI_TX_META_V5;

/*
 *  RX META VERSION DEFINITIONS
 */
/* if RX meta data is present at all then the meta data field
 *  will consume WMI_MAX_RX_META_SZ bytes of space between the
 *  WMI_DATA_HDR and the payload. How much of the available
 *  Meta data is actually used depends on which meta data
 *  version is active. */
#define WMI_MAX_RX_META_SZ  (12)
#define WMI_MAX_RX_META_VERSION (7)

#define WMI_RX_STATUS_OK            0 /* success */
#define WMI_RX_STATUS_DECRYPT_ERR   1 /* decrypt error */
#define WMI_RX_STATUS_MIC_ERR       2 /* tkip MIC error */
#define WMI_RX_STATUS_ERR           3 /* undefined error */

#define WMI_RX_FLAGS_AGGR           0x0001 /* part of AGGR */
#define WMI_RX_FlAGS_STBC           0x0002 /* used STBC */
#define WMI_RX_FLAGS_SGI            0x0004 /* used SGI */
#define WMI_RX_FLAGS_HT             0x0008 /* is HT packet */
/* the flags field is also used to store the CRYPTO_TYPE of the frame
 * that value is shifted by WMI_RX_FLAGS_CRYPTO_SHIFT */
#define WMI_RX_FLAGS_CRYPTO_SHIFT   4
#define WMI_RX_FLAGS_CRYPTO_MASK    0x1f
#define WMI_RX_META_GET_CRYPTO(flags) (((flags) >> WMI_RX_FLAGS_CRYPTO_SHIFT) & WMI_RX_FLAGS_CRYPTO_MASK)

#if 0 /* removed to prevent compile errors for WM.. */
typedef PREPACK struct {
/* intentionally empty. Default version is no meta data. */
} POSTPACK WMI_RX_META_VERSION_0;
#endif

typedef PREPACK struct {
    A_UINT8     status FIELD_PACKED; /* one of WMI_RX_STATUS_... */
    A_UINT8     rix FIELD_PACKED;    /* rate index mapped to rate at which this packet was received. */
    A_UINT8     rssi FIELD_PACKED;   /* rssi of packet */
    A_UINT8     channel FIELD_PACKED;/* rf channel during packet reception */
    A_UINT16    flags FIELD_PACKED;  /* a combination of WMI_RX_FLAGS_... */
} POSTPACK WMI_RX_META_V1;

#define RX_CSUM_VALID_FLAG (0x1)
typedef PREPACK struct {
    A_UINT16    csum FIELD_PACKED;
    A_UINT8     csumFlags FIELD_PACKED;/* bit 0 set -partial csum valid
                             bit 1 set -test mode */
} POSTPACK WMI_RX_META_V2;



#define WMI_GET_DEVICE_ID(info1) ((info1) & 0xF)

/*
 * Control Path
 */
typedef PREPACK struct {
    A_UINT16    commandId FIELD_PACKED;
/*
 * info1 - 16 bits
 * b03:b00 - id
 * b15:b04 - unused
 */
    A_UINT16    info1 FIELD_PACKED;

    A_UINT16    reserved FIELD_PACKED;      /* For alignment */
} POSTPACK WMI_CMD_HDR;        /* used for commands and events */

/*
 * List of Commnands
 */
typedef enum {
    WMI_CONNECT_CMDID           = 0x0001,
    WMI_RECONNECT_CMDID 		= 0x0002,
    WMI_DISCONNECT_CMDID		= 0x0003,
    WMI_SYNCHRONIZE_CMDID		= 0x0004,
    WMI_START_SCAN_CMDID		= 0x0007,
    WMI_SET_SCAN_PARAMS_CMDID	= 0x0008,
    WMI_SET_BSS_FILTER_CMDID	= 0x0009,
    WMI_SET_PROBED_SSID_CMDID	= 0x000a,               /* 10 */
    WMI_SET_LISTEN_INT_CMDID	= 0x000b,
    WMI_GET_CHANNEL_LIST_CMDID	= 0x000e,
    WMI_SET_BEACON_INT_CMDID	= 0x000f,
    WMI_GET_STATISTICS_CMDID	= 0x0010,
    WMI_SET_CHANNEL_PARAMS_CMDID	= 0x0011,
    WMI_SET_POWER_MODE_CMDID	        = 0x0012,
    WMI_SET_POWER_PARAMS_CMDID          = 0x0014,             /* 20 */
    WMI_ADD_CIPHER_KEY_CMDID	        = 0x0016,
    WMI_DELETE_CIPHER_KEY_CMDID	        = 0x0017,
    WMI_SET_TX_PWR_CMDID		= 0x001b,
    WMI_GET_TX_PWR_CMDID		= 0x001c,
    WMI_TARGET_ERROR_REPORT_BITMASK_CMDID	= 0x0022,
    WMI_SET_RTS_CMDID			= 0x0032,                  /* 50 */
#if MANUFACTURING_SUPPORT
    WMI_TEST_CMDID              = 0x003a,
#endif
    WMI_SET_KEEPALIVE_CMDID		= 0x003d,
    WMI_GET_KEEPALIVE_CMDID		= 0x003e,
#if ENABLE_AP_MODE
    WMI_AP_SET_NUM_STA_CMDID    = 0xf00c,
    WMI_AP_HIDDEN_SSID_CMDID    = 0xf00b,
    WMI_AP_CONFIG_COMMIT_CMDID  = 0xf00f,
	WMI_AP_CONN_INACT_CMDID 	= 0xf012,
	WMI_AP_SET_COUNTRY_CMDID 	= 0xf014,
	WMI_AP_SET_DTIM_CMDID    	= 0xf015,
	WMI_AP_PSBUF_OFFLOAD_CMDID  = 0xf0AF,
#endif
    WMI_ALLOW_AGGR_CMDID 		= 0xf01b, /* F01B */
    WMI_SET_HT_CAP_CMDID		= 0xf01e,
    WMI_SET_PMK_CMDID 			= 0xf028,
#if ATH_FIRMWARE_TARGET == TARGET_AR4100_REV2
    WMI_SET_CHANNEL_CMDID		        = 0xf042,
    WMI_GET_PMK_CMDID			        = 0xf047,
    WMI_SET_PASSPHRASE_CMDID	        = 0xf048,
    WMI_STORERECALL_CONFIGURE_CMDID	    = 0xf04e,
    WMI_STORERECALL_RECALL_CMDID        = 0xf04f,
    WMI_STORERECALL_HOST_READY_CMDID    = 0xf050,
    /* WPS Commands */
    WMI_WPS_START_CMDID			        = 0xf053,
    WMI_GET_WPS_STATUS_CMDID 	        = 0xf054,
    /*Socket commands*/
    WMI_SOCKET_CMDID                    = 0xf055,
    WMI_SET_FILTERED_PROMISCUOUS_MODE_CMDID = 0xf056,
#elif ATH_FIRMWARE_TARGET == TARGET_AR400X_REV1
    WMI_SET_CHANNEL_CMDID               = 0xf042,
    WMI_GET_PMK_CMDID                   = 0xf047,
    WMI_SET_PASSPHRASE_CMDID            = 0xf048,
    WMI_STORERECALL_CONFIGURE_CMDID	    = 0xf05e,
    WMI_STORERECALL_RECALL_CMDID        = 0xf05f,
    WMI_STORERECALL_HOST_READY_CMDID    = 0xf060,
    /* WPS Commands */
    WMI_WPS_START_CMDID			        = 0xf07a,
    WMI_GET_WPS_STATUS_CMDID 	        = 0xf07b,
    /*Socket commands*/
    WMI_SOCKET_CMDID                    = 0xf08d,
    WMI_SET_FILTERED_PROMISCUOUS_MODE_CMDID = 0xf099,
#endif

#if ENABLE_P2P_MODE
    /* P2P CMDS */

#if ATH_FIRMWARE_TARGET == TARGET_AR4100_REV2 /* Jade */
        WMI_P2P_SET_CONFIG_CMDID            = 0xF038,
        WMI_WPS_SET_CONFIG_CMDID            = 0xF039,
        WMI_SET_REQ_DEV_ATTR_CMDID          = 0xF03A,
        WMI_P2P_FIND_CMDID                  = 0xF03B,
        WMI_P2P_STOP_FIND_CMDID             = 0xF03C,
        WMI_P2P_LISTEN_CMDID                = 0xF03D,

        WMI_P2P_SET_CMDID                   = 0xF057,

        WMI_P2P_SDPD_TX_CMDID               = 0xF058, /* F058 */
        WMI_P2P_STOP_SDPD_CMDID             = 0xF059,
        WMI_P2P_CANCEL_CMDID                = 0xF05a,
        WMI_P2P_CONNECT_CMDID               = 0xf05b,
        WMI_P2P_GET_NODE_LIST_CMDID         = 0xf05c,
        WMI_P2P_AUTH_GO_NEG_CMDID           = 0xf05d,
        WMI_P2P_FW_PROV_DISC_REQ_CMDID      = 0xf05e,
        WMI_P2P_PERSISTENT_PROFILE_CMDID    = 0xf05f,
        WMI_P2P_SET_JOIN_PROFILE_CMDID      = 0xf060,
        WMI_P2P_GRP_INIT_CMDID              = 0xf061,
        WMI_P2P_SET_PROFILE_CMDID           = 0xf065,
        WMI_P2P_INVITE_CMDID                = 0xF070,
        WMI_P2P_INVITE_REQ_RSP_CMDID        = 0xF071,
        WMI_P2P_FW_SET_NOA_CMDID            = 0xf072,
        WMI_P2P_FW_GET_NOA_CMDID            = 0xf073,
        WMI_P2P_FW_SET_OPPPS_CMDID          = 0xf074,
        WMI_P2P_FW_GET_OPPPS_CMDID          = 0xf075,

#elif ATH_FIRMWARE_TARGET == TARGET_AR400X_REV1
        WMI_P2P_SET_CONFIG_CMDID           = 0xF038,
        WMI_WPS_SET_CONFIG_CMDID           = 0xF039,
        WMI_SET_REQ_DEV_ATTR_CMDID         = 0xF03A,
        WMI_P2P_FIND_CMDID                 = 0xF03B,
        WMI_P2P_STOP_FIND_CMDID            = 0xF03C,
        WMI_P2P_LISTEN_CMDID               = 0xF03E,

        WMI_P2P_GRP_INIT_CMDID             = 0xF051,
        WMI_P2P_GRP_FORMATION_DONE_CMDID   = 0xF052,
        WMI_P2P_INVITE_CMDID               = 0xF053,
        WMI_P2P_INVITE_REQ_RSP_CMDID       = 0xF054,
        WMI_P2P_SET_CMDID                  = 0xF056,

        WMI_P2P_SDPD_TX_CMDID              = 0xF05B, /* F058 */
        WMI_P2P_STOP_SDPD_CMDID            = 0xF05C,
        WMI_P2P_CANCEL_CMDID               = 0xF05D,
        WMI_P2P_CONNECT_CMDID              = 0xf091,
        WMI_P2P_GET_NODE_LIST_CMDID        = 0xf092,
        WMI_P2P_AUTH_GO_NEG_CMDID          = 0xf093,
        WMI_P2P_FW_PROV_DISC_REQ_CMDID     = 0xf094,
        WMI_P2P_PERSISTENT_PROFILE_CMDID   = 0xf09D,
        WMI_P2P_SET_JOIN_PROFILE_CMDID     = 0xf09E,
        WMI_P2P_SET_PROFILE_CMDID          = 0xf0A8,
        WMI_P2P_FW_SET_NOA_CMDID           = 0xf0A9,
        WMI_P2P_FW_GET_NOA_CMDID           = 0xf0AA,
        WMI_P2P_FW_SET_OPPPS_CMDID         = 0xf0AB,
        WMI_P2P_FW_GET_OPPPS_CMDID         = 0xf0AC,
#endif

#endif
        WMI_GET_BITRATE_CMDID              = 0xf001,
    	/*GreenTx specific commands*/
    	WMI_GREENTX_PARAMS_CMDID           = 0xF079,
    	WMI_LPL_FORCE_ENABLE_CMDID         = 0xF072,
        WMI_ARGOS_CMDID                    = 0xf0B1
} WMI_COMMAND_ID;

/*
 * Frame Types
 */
typedef enum {
    WMI_FRAME_BEACON        =   0,
    WMI_FRAME_PROBE_REQ,
    WMI_FRAME_PROBE_RESP,
    WMI_FRAME_ASSOC_REQ,
    WMI_FRAME_ASSOC_RESP,
    WMI_NUM_MGMT_FRAME
} WMI_MGMT_FRAME_TYPE;

/*
 * Connect Command
 */
typedef enum {
    INFRA_NETWORK       = 0x01,
    ADHOC_NETWORK       = 0x02,
    ADHOC_CREATOR       = 0x04,
    AP_NETWORK          = 0x10,
    NETWORK_CONNECTED_USING_WPS = 0x20
} NETWORK_TYPE;

typedef enum {
    SUBTYPE_NONE,
    SUBTYPE_BT,
    SUBTYPE_P2PDEV,
    SUBTYPE_P2PCLIENT,
    SUBTYPE_P2PGO
} NETWORK_SUBTYPE;

typedef enum {
    OPEN_AUTH           = 0x01,
    SHARED_AUTH         = 0x02,
    LEAP_AUTH           = 0x04  /* different from IEEE_AUTH_MODE definitions */
} DOT11_AUTH_MODE;

typedef enum {
    NONE_AUTH           = 0x01,
    WPA_AUTH            = 0x02,
    WPA2_AUTH           = 0x04,
    WPA_PSK_AUTH        = 0x08,
    WPA2_PSK_AUTH       = 0x10,
    WPA_AUTH_CCKM       = 0x20,
    WPA2_AUTH_CCKM      = 0x40
} AUTH_MODE;

typedef enum {
    NONE_CRYPT          = 0x01,
    WEP_CRYPT           = 0x02,
    TKIP_CRYPT          = 0x04,
    AES_CRYPT           = 0x08
#ifdef WAPI_ENABLE
	,
    WAPI_CRYPT          = 0x10
#endif /*WAPI_ENABLE*/
} CRYPTO_TYPE;

#define WMI_MIN_CRYPTO_TYPE NONE_CRYPT
#define WMI_MAX_CRYPTO_TYPE (AES_CRYPT + 1)

#ifdef WAPI_ENABLE
#undef WMI_MAX_CRYPTO_TYPE
#define WMI_MAX_CRYPTO_TYPE (WAPI_CRYPT + 1)
#endif /* WAPI_ENABLE */

#ifdef WAPI_ENABLE
#define IW_ENCODE_ALG_SM4       0x20
#define IW_AUTH_WAPI_ENABLED    0x20
#endif

#define WMI_MIN_KEY_INDEX   0
#define WMI_MAX_KEY_INDEX   3

#ifdef WAPI_ENABLE
#undef WMI_MAX_KEY_INDEX
#define WMI_MAX_KEY_INDEX   7 /* wapi grpKey 0-3, prwKey 4-7 */
#endif /* WAPI_ENABLE */

#define WMI_MAX_KEY_LEN     32

#define WMI_MAX_SSID_LEN    32

typedef enum {
    CONNECT_ASSOC_POLICY_USER           = 0x0001,
    CONNECT_SEND_REASSOC                = 0x0002,
    CONNECT_IGNORE_WPAx_GROUP_CIPHER    = 0x0004,
    CONNECT_PROFILE_MATCH_DONE          = 0x0008,
    CONNECT_IGNORE_AAC_BEACON           = 0x0010,
    CONNECT_CSA_FOLLOW_BSS              = 0x0020,
    CONNECT_DO_WPA_OFFLOAD              = 0x0040,
    CONNECT_DO_NOT_DEAUTH               = 0x0080,
    CONNECT_WPS_FLAG                    = 0x0100
} WMI_CONNECT_CTRL_FLAGS_BITS;

#define DEFAULT_CONNECT_CTRL_FLAGS    (CONNECT_CSA_FOLLOW_BSS)

typedef PREPACK struct {
    A_UINT8     networkType FIELD_PACKED;
    A_UINT8     dot11AuthMode FIELD_PACKED;
    A_UINT8     authMode FIELD_PACKED;
    A_UINT8     pairwiseCryptoType FIELD_PACKED;
    A_UINT8     pairwiseCryptoLen FIELD_PACKED;
    A_UINT8     groupCryptoType FIELD_PACKED;
    A_UINT8     groupCryptoLen FIELD_PACKED;
    A_UINT8     ssidLength FIELD_PACKED;
    A_UCHAR     ssid[WMI_MAX_SSID_LEN] FIELD_PACKED;
    A_UINT16    channel FIELD_PACKED;
    A_UINT8     bssid[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT32    ctrl_flags FIELD_PACKED;
} POSTPACK WMI_CONNECT_CMD;

#if ENABLE_AP_MODE
/*
 * Used with WMI_AP_HIDDEN_SSID_CMDID
 */
#define HIDDEN_SSID_FALSE   0
#define HIDDEN_SSID_TRUE    1

typedef PREPACK struct {
    A_UINT8     hidden_ssid FIELD_PACKED;
} POSTPACK WMI_AP_HIDDEN_SSID_CMD;

typedef PREPACK struct {
    A_UCHAR countryCode[3] FIELD_PACKED;
} POSTPACK WMI_AP_SET_COUNTRY_CMD;

typedef PREPACK struct {
    A_UINT32 period FIELD_PACKED;
} POSTPACK WMI_AP_CONN_INACT_CMD;

typedef PREPACK struct {
    A_UINT16     beaconInterval FIELD_PACKED;
} POSTPACK WMI_BEACON_INT_CMD;

typedef PREPACK struct {
    A_UINT8 dtim FIELD_PACKED;
} POSTPACK WMI_AP_SET_DTIM_CMD;

typedef PREPACK struct {
    A_UINT8    enable FIELD_PACKED; /* enable/disable IoE AP PS Offload */
    A_UINT8    psBufCount FIELD_PACKED; /* PS Buf count per PS Client */
} POSTPACK WMI_AP_PSBUF_OFFLOAD_CMD;
#endif

/*
 * WMI_RECONNECT_CMDID
 */
typedef PREPACK struct {
    A_UINT16    channel FIELD_PACKED;                    /* hint */
    A_UINT8     bssid[ATH_MAC_LEN] FIELD_PACKED;         /* mandatory if set */
} POSTPACK WMI_RECONNECT_CMD;

#define WMI_PMK_LEN     32
typedef PREPACK struct {
    A_UINT8 pmk[WMI_PMK_LEN] FIELD_PACKED;
} POSTPACK WMI_SET_PMK_CMD, WMI_GET_PMK_REPLY;


#define WMI_PASSPHRASE_LEN    64
typedef PREPACK struct {
    A_UCHAR ssid[WMI_MAX_SSID_LEN] FIELD_PACKED;
    A_UINT8 passphrase[WMI_PASSPHRASE_LEN] FIELD_PACKED;
    A_UINT8 ssid_len FIELD_PACKED;
    A_UINT8 passphrase_len FIELD_PACKED;
} POSTPACK WMI_SET_PASSPHRASE_CMD;

/*
 * WMI_ADD_CIPHER_KEY_CMDID
 */
typedef enum {
    PAIRWISE_USAGE      = 0x00,
    GROUP_USAGE         = 0x01,
    TX_USAGE            = 0x02     /* default Tx Key - Static WEP only */
} KEY_USAGE;

/*
 * Bit Flag
 * Bit 0 - Initialise TSC - default is Initialize
 */
#define KEY_OP_INIT_TSC       0x01
#define KEY_OP_INIT_RSC       0x02
#ifdef WAPI_ENABLE
#define KEY_OP_INIT_WAPIPN    0x10
#endif /* WAPI_ENABLE */

#define KEY_OP_INIT_VAL     0x03     /* Default Initialise the TSC & RSC */
#define KEY_OP_VALID_MASK   0x03

typedef PREPACK struct {
    A_UINT8     keyIndex FIELD_PACKED;
    A_UINT8     keyType FIELD_PACKED;
    A_UINT8     keyUsage FIELD_PACKED;           /* KEY_USAGE */
    A_UINT8     keyLength FIELD_PACKED;
    A_UINT8     keyRSC[8] FIELD_PACKED;          /* key replay sequence counter */
    A_UINT8     key[WMI_MAX_KEY_LEN] FIELD_PACKED;
    A_UINT8     key_op_ctrl FIELD_PACKED;       /* Additional Key Control information */
    A_UINT8    key_macaddr[ATH_MAC_LEN] FIELD_PACKED;
} POSTPACK WMI_ADD_CIPHER_KEY_CMD;

/*
 * WMI_DELETE_CIPHER_KEY_CMDID
 */
typedef PREPACK struct {
    A_UINT8     keyIndex FIELD_PACKED;
} POSTPACK WMI_DELETE_CIPHER_KEY_CMD;

/*
 * WMI_START_SCAN_CMD
 */
typedef enum {
    WMI_LONG_SCAN  = 0,
    WMI_SHORT_SCAN = 1
} WMI_SCAN_TYPE;

typedef PREPACK struct {
    A_BOOL   forceFgScan FIELD_PACKED;
    A_BOOL   isLegacy FIELD_PACKED;        /* For Legacy Cisco AP compatibility */
    A_UINT32 homeDwellTime FIELD_PACKED;   /* Maximum duration in the home channel(milliseconds) */
    A_UINT32 forceScanInterval FIELD_PACKED;    /* Time interval between scans (milliseconds)*/
    A_UINT8  scanType FIELD_PACKED;           /* WMI_SCAN_TYPE */
    A_UINT8  numChannels FIELD_PACKED;            /* how many channels follow */
    A_UINT16 channelList[1] FIELD_PACKED;         /* channels in Mhz */
} POSTPACK WMI_START_SCAN_CMD;

/*
 * WMI_SET_SCAN_PARAMS_CMDID
 */
#define WMI_SHORTSCANRATIO_DEFAULT      3
/*
 *  Warning: ScanCtrlFlag value of 0xFF is used to disable all flags in WMI_SCAN_PARAMS_CMD
 *  Do not add any more flags to WMI_SCAN_CTRL_FLAG_BITS
 */
typedef enum {
    CONNECT_SCAN_CTRL_FLAGS = 0x01,    /* set if can scan in the Connect cmd */
    SCAN_CONNECTED_CTRL_FLAGS = 0x02,  /* set if scan for the SSID it is */
                                       /* already connected to */
    ACTIVE_SCAN_CTRL_FLAGS = 0x04,     /* set if enable active scan */
    ROAM_SCAN_CTRL_FLAGS = 0x08,       /* set if enable roam scan when bmiss and lowrssi */
    REPORT_BSSINFO_CTRL_FLAGS = 0x10,   /* set if follows customer BSSINFO reporting rule */
    ENABLE_AUTO_CTRL_FLAGS = 0x20,      /* if disabled, target doesn't
                                          scan after a disconnect event  */
    ENABLE_SCAN_ABORT_EVENT = 0x40      /* Scan complete event with canceled status will be generated when a scan is prempted before it gets completed */
} WMI_SCAN_CTRL_FLAGS_BITS;

#define CAN_SCAN_IN_CONNECT(flags)      (flags & CONNECT_SCAN_CTRL_FLAGS)
#define CAN_SCAN_CONNECTED(flags)       (flags & SCAN_CONNECTED_CTRL_FLAGS)
#define ENABLE_ACTIVE_SCAN(flags)       (flags & ACTIVE_SCAN_CTRL_FLAGS)
#define ENABLE_ROAM_SCAN(flags)         (flags & ROAM_SCAN_CTRL_FLAGS)
#define CONFIG_REPORT_BSSINFO(flags)     (flags & REPORT_BSSINFO_CTRL_FLAGS)
#define IS_AUTO_SCAN_ENABLED(flags)      (flags & ENABLE_AUTO_CTRL_FLAGS)
#define SCAN_ABORT_EVENT_ENABLED(flags) (flags & ENABLE_SCAN_ABORT_EVENT)

#define DEFAULT_SCAN_CTRL_FLAGS         (CONNECT_SCAN_CTRL_FLAGS| SCAN_CONNECTED_CTRL_FLAGS| ACTIVE_SCAN_CTRL_FLAGS| ROAM_SCAN_CTRL_FLAGS | ENABLE_AUTO_CTRL_FLAGS)


typedef PREPACK struct {
    A_UINT16    fg_start_period FIELD_PACKED;        /* seconds */
    A_UINT16    fg_end_period FIELD_PACKED;          /* seconds */
    A_UINT16    bg_period FIELD_PACKED;              /* seconds */
    A_UINT16    maxact_chdwell_time FIELD_PACKED;    /* msec */
    A_UINT16    pas_chdwell_time FIELD_PACKED;       /* msec */
    A_UINT8     shortScanRatio FIELD_PACKED;         /* how many shorts scan for one long */
    A_UINT8     scanCtrlFlags FIELD_PACKED;
    A_UINT16    minact_chdwell_time FIELD_PACKED;    /* msec */
    A_UINT16    maxact_scan_per_ssid FIELD_PACKED;   /* max active scans per ssid */
    A_UINT32    max_dfsch_act_time FIELD_PACKED;  /* msecs */
} POSTPACK WMI_SCAN_PARAMS_CMD;

/*
 * WMI_SET_BSS_FILTER_CMDID
 */
typedef enum {
    NONE_BSS_FILTER = 0x0,              /* no beacons forwarded */
    ALL_BSS_FILTER,                     /* all beacons forwarded */
    PROFILE_FILTER,                     /* only beacons matching profile */
    ALL_BUT_PROFILE_FILTER,             /* all but beacons matching profile */
    CURRENT_BSS_FILTER,                 /* only beacons matching current BSS */
    ALL_BUT_BSS_FILTER,                 /* all but beacons matching BSS */
    PROBED_SSID_FILTER,                 /* beacons matching probed ssid */
    LAST_BSS_FILTER                    /* marker only */
} WMI_BSS_FILTER;

typedef PREPACK struct {
    A_UINT8    bssFilter FIELD_PACKED;                      /* see WMI_BSS_FILTER */
    A_UINT8    reserved1 FIELD_PACKED;                      /* For alignment */
    A_UINT16   reserved2 FIELD_PACKED;                      /* For alignment */
    A_UINT32   ieMask FIELD_PACKED;
} POSTPACK WMI_BSS_FILTER_CMD;

/*
 * WMI_SET_PROBED_SSID_CMDID
 */
#define MAX_PROBED_SSID_INDEX   9

typedef enum {
    DISABLE_SSID_FLAG  = 0,                  /* disables entry */
    SPECIFIC_SSID_FLAG = 0x01,               /* probes specified ssid */
    ANY_SSID_FLAG      = 0x02               /* probes for any ssid */
} WMI_SSID_FLAG;

typedef PREPACK struct {
    A_UINT8     entryIndex FIELD_PACKED;                     /* 0 to MAX_PROBED_SSID_INDEX */
    A_UINT8     flag FIELD_PACKED;                           /* WMI_SSID_FLG */
    A_UINT8     ssidLength FIELD_PACKED;
    A_UINT8     ssid[32] FIELD_PACKED;
} POSTPACK WMI_PROBED_SSID_CMD;

/*
 * WMI_SET_LISTEN_INT_CMDID
 * The Listen interval is between 15 and 3000 TUs
 */
#define MIN_LISTEN_INTERVAL 15
#define MAX_LISTEN_INTERVAL 5000
#define MIN_LISTEN_BEACONS 1
#define MAX_LISTEN_BEACONS 50

typedef PREPACK struct {
    A_UINT16     listenInterval FIELD_PACKED;
    A_UINT16     numBeacons FIELD_PACKED;
} POSTPACK WMI_LISTEN_INT_CMD;

/*
 * WMI_SET_BMISS_TIME_CMDID
 * valid values are between 1000 and 5000 TUs
 */

#define MIN_BMISS_TIME     1000
#define MAX_BMISS_TIME     5000
#define MIN_BMISS_BEACONS  1
#define MAX_BMISS_BEACONS  50

typedef PREPACK struct {
    A_UINT16     bmissTime FIELD_PACKED;
    A_UINT16     numBeacons FIELD_PACKED;
} POSTPACK WMI_BMISS_TIME_CMD;

/*
 * WMI_SET_POWER_MODE_CMDID
 */
typedef enum {
    REC_POWER = 0x01,
    MAX_PERF_POWER
} WMI_POWER_MODE;

typedef PREPACK struct {
    A_UINT8     powerMode FIELD_PACKED;      /* WMI_POWER_MODE */
} POSTPACK WMI_POWER_MODE_CMD;

typedef PREPACK struct {
    A_INT8 status FIELD_PACKED;      /* WMI_SET_PARAMS_REPLY */
} POSTPACK WMI_SET_PARAMS_REPLY;

/*
 * WMI_SET_POWER_PARAMS_CMDID
 */
typedef enum {
    IGNORE_DTIM = 0x01,
    NORMAL_DTIM = 0x02,
    STICK_DTIM  = 0x03,
    AUTO_DTIM   = 0x04
} WMI_DTIM_POLICY;

/* Policy to determnine whether TX should wakeup WLAN if sleeping */
typedef enum {
    TX_WAKEUP_UPON_SLEEP = 1,
    TX_DONT_WAKEUP_UPON_SLEEP = 2
} WMI_TX_WAKEUP_POLICY_UPON_SLEEP;

/*
 * Policy to determnine whether power save failure event should be sent to
 * host during scanning
 */
typedef enum {
    SEND_POWER_SAVE_FAIL_EVENT_ALWAYS = 1,
    IGNORE_POWER_SAVE_FAIL_EVENT_DURING_SCAN = 2
} POWER_SAVE_FAIL_EVENT_POLICY;

typedef PREPACK struct {
    A_UINT16    idle_period FIELD_PACKED;             /* msec */
    A_UINT16    pspoll_number FIELD_PACKED;
    A_UINT16    dtim_policy FIELD_PACKED;
    A_UINT16    tx_wakeup_policy FIELD_PACKED;
    A_UINT16    num_tx_to_wakeup FIELD_PACKED;
    A_UINT16    ps_fail_event_policy FIELD_PACKED;
} POSTPACK WMI_POWER_PARAMS_CMD;

/* Adhoc power save types */
typedef enum {
    ADHOC_PS_DISABLE=1,
    ADHOC_PS_ATH=2,
    ADHOC_PS_IEEE=3,
    ADHOC_PS_OTHER=4
} WMI_ADHOC_PS_TYPE;

/*
 * WMI_SET_DISC_TIMEOUT_CMDID
 */
typedef PREPACK struct {
    A_UINT8     disconnectTimeout FIELD_PACKED;          /* seconds */
} POSTPACK WMI_DISC_TIMEOUT_CMD;

typedef enum {
    UPLINK_TRAFFIC = 0,
    DNLINK_TRAFFIC = 1,
    BIDIR_TRAFFIC = 2
} DIR_TYPE;

typedef enum {
    DISABLE_FOR_THIS_AC = 0,
    ENABLE_FOR_THIS_AC  = 1,
    ENABLE_FOR_ALL_AC   = 2
} VOICEPS_CAP_TYPE;

typedef enum {
    TRAFFIC_TYPE_APERIODIC = 0,
    TRAFFIC_TYPE_PERIODIC = 1
}TRAFFIC_TYPE;

/*
 * WMI_SYNCHRONIZE_CMDID
 */
typedef PREPACK struct {
    A_UINT8 dataSyncMap FIELD_PACKED;
} POSTPACK WMI_SYNC_CMD;

/*
 * WMI_CREATE_PSTREAM_CMDID
 */
typedef PREPACK struct {
    A_UINT32        minServiceInt FIELD_PACKED;           /* in milli-sec */
    A_UINT32        maxServiceInt FIELD_PACKED;           /* in milli-sec */
    A_UINT32        inactivityInt FIELD_PACKED;           /* in milli-sec */
    A_UINT32        suspensionInt FIELD_PACKED;           /* in milli-sec */
    A_UINT32        serviceStartTime FIELD_PACKED;
    A_UINT32        minDataRate FIELD_PACKED;             /* in bps */
    A_UINT32        meanDataRate FIELD_PACKED;            /* in bps */
    A_UINT32        peakDataRate FIELD_PACKED;            /* in bps */
    A_UINT32        maxBurstSize FIELD_PACKED;
    A_UINT32        delayBound FIELD_PACKED;
    A_UINT32        minPhyRate FIELD_PACKED;              /* in bps */
    A_UINT32        sba FIELD_PACKED;
    A_UINT32        mediumTime FIELD_PACKED;
    A_UINT16        nominalMSDU FIELD_PACKED;             /* in octects */
    A_UINT16        maxMSDU FIELD_PACKED;                 /* in octects */
    A_UINT8         trafficClass FIELD_PACKED;
    A_UINT8         trafficDirection FIELD_PACKED;        /* DIR_TYPE */
    A_UINT8         rxQueueNum FIELD_PACKED;
    A_UINT8         trafficType FIELD_PACKED;             /* TRAFFIC_TYPE */
    A_UINT8         voicePSCapability FIELD_PACKED;       /* VOICEPS_CAP_TYPE */
    A_UINT8         tsid FIELD_PACKED;
    A_UINT8         userPriority FIELD_PACKED;            /* 802.1D user priority */
    A_UINT8         nominalPHY FIELD_PACKED;              /* nominal phy rate */
} POSTPACK WMI_CREATE_PSTREAM_CMD;

/*
 * WMI_DELETE_PSTREAM_CMDID
 */
typedef PREPACK struct {
    A_UINT8     txQueueNumber FIELD_PACKED;
    A_UINT8     rxQueueNumber FIELD_PACKED;
    A_UINT8     trafficDirection FIELD_PACKED;
    A_UINT8     trafficClass FIELD_PACKED;
    A_UINT8     tsid FIELD_PACKED;
} POSTPACK WMI_DELETE_PSTREAM_CMD;

/*
 * WMI_SET_CHANNEL_PARAMS_CMDID
 */
typedef enum {
    WMI_11A_MODE  = 0x1,
    WMI_11G_MODE  = 0x2,
    WMI_11AG_MODE = 0x3,
    WMI_11B_MODE  = 0x4,
    WMI_11GONLY_MODE = 0x5
} WMI_PHY_MODE;

#define WMI_MAX_CHANNELS        32

typedef PREPACK struct {
    A_UINT8     reserved1 FIELD_PACKED;
    A_UINT8     scanParam FIELD_PACKED;              /* set if enable scan */
    A_UINT8     phyMode FIELD_PACKED;                /* see WMI_PHY_MODE */
    A_UINT8     numChannels FIELD_PACKED;            /* how many channels follow */
    A_UINT16    channelList[1] FIELD_PACKED;         /* channels in Mhz */
} POSTPACK WMI_CHANNEL_PARAMS_CMD;


typedef enum {
    WMI_LPREAMBLE_DISABLED = 0,
    WMI_LPREAMBLE_ENABLED
} WMI_LPREAMBLE_STATUS;

typedef enum {
    WMI_IGNORE_BARKER_IN_ERP = 0,
    WMI_DONOT_IGNORE_BARKER_IN_ERP
} WMI_PREAMBLE_POLICY;


typedef PREPACK struct {
    A_UINT16    threshold FIELD_PACKED;
}POSTPACK WMI_SET_RTS_CMD;

/*
 *  WMI_TARGET_ERROR_REPORT_BITMASK_CMDID
 *  Sets the error reporting event bitmask in target. Target clears it
 *  upon an error. Subsequent errors are counted, but not reported
 *  via event, unless the bitmask is set again.
 */
typedef PREPACK struct {
    A_UINT32    bitmask FIELD_PACKED;
} POSTPACK  WMI_TARGET_ERROR_REPORT_BITMASK;

/*
 * WMI_SET_TX_PWR_CMDID
 */
typedef PREPACK struct {
    A_UINT8     dbM FIELD_PACKED;                  /* in dbM units */
} POSTPACK WMI_SET_TX_PWR_CMD, WMI_TX_PWR_REPLY;



/*
 * WMI_GET_TX_PWR_CMDID does not take any parameters
 */


/*
 * WMI_SET_ACCESS_PARAMS_CMDID
 */
#define WMI_DEFAULT_TXOP_ACPARAM    0       /* implies one MSDU */
#define WMI_DEFAULT_ECWMIN_ACPARAM  4       /* corresponds to CWmin of 15 */
#define WMI_DEFAULT_ECWMAX_ACPARAM  10      /* corresponds to CWmax of 1023 */
#define WMI_MAX_CW_ACPARAM          15      /* maximum eCWmin or eCWmax */
#define WMI_DEFAULT_AIFSN_ACPARAM   2
#define WMI_MAX_AIFSN_ACPARAM       15
typedef PREPACK struct {
    A_UINT16 txop FIELD_PACKED;                      /* in units of 32 usec */
    A_UINT8  eCWmin FIELD_PACKED;
    A_UINT8  eCWmax FIELD_PACKED;
    A_UINT8  aifsn FIELD_PACKED;
    A_UINT8  ac FIELD_PACKED;
} POSTPACK WMI_SET_ACCESS_PARAMS_CMD;

typedef PREPACK struct {
    A_UINT32 sleepState FIELD_PACKED;
}WMI_REPORT_SLEEP_STATE_EVENT;

/*
 * Command Replies
 */

/*
 * WMI_GET_CHANNEL_LIST_CMDID reply
 */
typedef PREPACK struct {
    A_UINT8     reserved1 FIELD_PACKED;
    A_UINT8     numChannels FIELD_PACKED;            /* number of channels in reply */
    A_UINT16    channelList[1] FIELD_PACKED;         /* channel in Mhz */
} POSTPACK WMI_CHANNEL_LIST_REPLY;

typedef enum {
    A_SUCCEEDED = A_OK,
    A_FAILED_DELETE_STREAM_DOESNOT_EXIST=250,
    A_SUCCEEDED_MODIFY_STREAM=251,
    A_FAILED_INVALID_STREAM = 252,
    A_FAILED_MAX_THINSTREAMS = 253,
    A_FAILED_CREATE_REMOVE_PSTREAM_FIRST = 254
} PSTREAM_REPLY_STATUS;

typedef PREPACK struct {
    A_UINT8     status FIELD_PACKED;                 /* PSTREAM_REPLY_STATUS */
    A_UINT8     txQueueNumber FIELD_PACKED;
    A_UINT8     rxQueueNumber FIELD_PACKED;
    A_UINT8     trafficClass FIELD_PACKED;
    A_UINT8     trafficDirection FIELD_PACKED;       /* DIR_TYPE */
} POSTPACK WMI_CRE_PRIORITY_STREAM_REPLY;

typedef PREPACK struct {
    A_UINT8     status FIELD_PACKED;                 /* PSTREAM_REPLY_STATUS */
    A_UINT8     txQueueNumber FIELD_PACKED;
    A_UINT8     rxQueueNumber FIELD_PACKED;
    A_UINT8     trafficDirection FIELD_PACKED;       /* DIR_TYPE */
    A_UINT8     trafficClass FIELD_PACKED;
} POSTPACK WMI_DEL_PRIORITY_STREAM_REPLY;

/*
 * List of Events (target to host)
 */
typedef enum {
    WMI_READY_EVENTID                   = 0x1001,
    WMI_CONNECT_EVENTID			= 0x1002,
    WMI_DISCONNECT_EVENTID		= 0x1003,
    WMI_BSSINFO_EVENTID			= 0x1004,
    WMI_CMDERROR_EVENTID		= 0x1005,
    WMI_REGDOMAIN_EVENTID		= 0x1006,
    WMI_PSTREAM_TIMEOUT_EVENTID	        = 0x1007,
    WMI_NEIGHBOR_REPORT_EVENTID	        = 0x1008,
    WMI_TKIP_MICERR_EVENTID		= 0x1009,
    WMI_SCAN_COMPLETE_EVENTID	        = 0x100a,           /* 0x100a */
    WMI_REPORT_STATISTICS_EVENTID       = 0x100b,
    WMI_RSSI_THRESHOLD_EVENTID		= 0x100c,
    WMI_ERROR_REPORT_EVENTID		= 0x100d,
    WMI_REPORT_ROAM_TBL_EVENTID		= 0x100f,
    WMI_SNR_THRESHOLD_EVENTID		= 0x1012,
    WMI_LQ_THRESHOLD_EVENTID		= 0x1013,
    WMI_TX_RETRY_ERR_EVENTID		= 0x1014,            /* 0x1014 */
    WMI_REPORT_ROAM_DATA_EVENTID	= 0x1015,
#if MANUFACTURING_SUPPORT
    WMI_TEST_EVENTID                = 0x1016,
#endif
    WMI_APLIST_EVENTID			= 0x1017,
    WMI_GET_PMKID_LIST_EVENTID		= 0x1019,
    WMI_CHANNEL_CHANGE_EVENTID		= 0x101a,
    WMI_PEER_NODE_EVENTID		= 0x101b,
    WMI_DTIMEXPIRY_EVENTID		= 0x101d,
    WMI_WLAN_VERSION_EVENTID		= 0x101e,
    WMI_SET_PARAMS_REPLY_EVENTID	= 0x101f,
    WMI_ADDBA_REQ_EVENTID		= 0x1020,              /*0x1020 */
    WMI_ADDBA_RESP_EVENTID		= 0x1021,
    WMI_DELBA_REQ_EVENTID		= 0x1022,
    WMI_REPORT_SLEEP_STATE_EVENTID	= 0x1026,
    WMI_GET_PMK_EVENTID		        = 0x102a,
    WMI_SET_CHANNEL_EVENTID		= 0x9000,

#if ATH_FIRMWARE_TARGET == TARGET_AR4100_REV2
    WMI_STORERECALL_STORE_EVENTID   = 0x9004,
    WMI_WPS_GET_STATUS_EVENTID      = 0x9005,
    WMI_WPS_PROFILE_EVENTID         = 0x9006,
    WMI_SOCKET_RESPONSE_EVENTID     = 0x9007,

    /* P2P Events */
    WMI_P2P_GO_NEG_RESULT_EVENTID       = 0x1036, /* 1035 */
    WMI_P2P_INVITE_REQ_EVENTID          = 0x103C,
    WMI_P2P_INVITE_RCVD_RESULT_EVENTID  = 0x103d,
    WMI_P2P_INVITE_SENT_RESULT_EVENTID  = 0x103e,
    WMI_P2P_PROV_DISC_RESP_EVENTID      = 0x103f,
    WMI_P2P_PROV_DISC_REQ_EVENTID       = 0x1040,
    WMI_P2P_START_SDPD_EVENTID          = 0x1045,
    WMI_P2P_SDPD_RX_EVENTID             = 0x1046,
    WMI_P2P_NODE_LIST_EVENTID           = 0x901a,
    WMI_P2P_REQ_TO_AUTH_EVENTID         = 0x901b,
#elif ATH_FIRMWARE_TARGET == TARGET_AR400X_REV1
    WMI_STORERECALL_STORE_EVENTID   = 0x9003,
    WMI_WPS_GET_STATUS_EVENTID      = 0x900e,
    WMI_WPS_PROFILE_EVENTID         = 0x900f,
    WMI_SOCKET_RESPONSE_EVENTID     = 0x9016,
    WMI_P2P_GO_NEG_RESULT_EVENTID       = 0x1036,
    WMI_P2P_INVITE_REQ_EVENTID          = 0x103E,
    WMI_P2P_INVITE_RCVD_RESULT_EVENTID  = 0x103F,
    WMI_P2P_INVITE_SENT_RESULT_EVENTID  = 0x1040,
    WMI_P2P_PROV_DISC_RESP_EVENTID      = 0x1041,
    WMI_P2P_PROV_DISC_REQ_EVENTID       = 0x1042,
    WMI_P2P_START_SDPD_EVENTID          = 0x1045,
    WMI_P2P_SDPD_RX_EVENTID             = 0x1046,
    WMI_P2P_NODE_LIST_EVENTID           = 0x901a,
    WMI_P2P_REQ_TO_AUTH_EVENTID         = 0x901b,
    
#endif
    WMI_ARGOS_EVENTID                   = 0x901f
} WMI_EVENT_ID;


typedef enum {
    WMI_11A_CAPABILITY   = 1,
    WMI_11G_CAPABILITY   = 2,
    WMI_11AG_CAPABILITY  = 3,
    WMI_11NA_CAPABILITY  = 4,
    WMI_11NG_CAPABILITY  = 5,
    WMI_11NAG_CAPABILITY = 6,
    // END CAPABILITY
    WMI_11N_CAPABILITY_OFFSET = (WMI_11NA_CAPABILITY - WMI_11A_CAPABILITY)
} WMI_PHY_CAPABILITY;

typedef PREPACK struct {
    A_UINT8     macaddr[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT8     phyCapability FIELD_PACKED;              /* WMI_PHY_CAPABILITY */
} POSTPACK WMI_READY_EVENT_1;

typedef PREPACK struct {
    A_UINT32    sw_version FIELD_PACKED;
    A_UINT32    abi_version FIELD_PACKED;
    A_UINT8     macaddr[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT8     phyCapability FIELD_PACKED;              /* WMI_PHY_CAPABILITY */
} POSTPACK WMI_READY_EVENT_2;

#define WMI_READY_EVENT WMI_READY_EVENT_2 /* host code */


/*
 * Connect Event
 */
typedef PREPACK struct {
    A_UINT16    channel FIELD_PACKED;
    A_UINT8     bssid[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT16    listenInterval FIELD_PACKED;
    A_UINT16    beaconInterval FIELD_PACKED;
    A_UINT32    networkType FIELD_PACKED;
    A_UINT8     beaconIeLen FIELD_PACKED;
    A_UINT8     assocReqLen FIELD_PACKED;
    A_UINT8     assocRespLen FIELD_PACKED;
    A_UINT8     assocInfo[1] FIELD_PACKED;
} POSTPACK WMI_CONNECT_EVENT;

/*
 * Disconnect Event
 */
typedef enum {
    NO_NETWORK_AVAIL   = 0x01,
    LOST_LINK          = 0x02,     /* bmiss */
    DISCONNECT_CMD     = 0x03,
    BSS_DISCONNECTED   = 0x04,
    AUTH_FAILED        = 0x05,
    ASSOC_FAILED       = 0x06,
    NO_RESOURCES_AVAIL = 0x07,
    CSERV_DISCONNECT   = 0x08,
    INVALID_PROFILE    = 0x0a,
    DOT11H_CHANNEL_SWITCH = 0x0b,
    PROFILE_MISMATCH   = 0x0c,
    CONNECTION_EVICTED = 0x0d,
    IBSS_MERGE         = 0xe
} WMI_DISCONNECT_REASON;

typedef PREPACK struct {
    A_UINT16    protocolReasonStatus FIELD_PACKED;  /* reason code, see 802.11 spec. */
    A_UINT8     bssid[ATH_MAC_LEN] FIELD_PACKED;    /* set if known */
    A_UINT8     disconnectReason  FIELD_PACKED;      /* see WMI_DISCONNECT_REASON */
    A_UINT8     assocRespLen FIELD_PACKED;
    A_UINT8     assocInfo[1] FIELD_PACKED;
} POSTPACK WMI_DISCONNECT_EVENT;

/*
 * BSS Info Event.
 * Mechanism used to inform host of the presence and characteristic of
 * wireless networks present.  Consists of bss info header followed by
 * the beacon or probe-response frame body.  The 802.11 header is not included.
 */
typedef enum {
    BEACON_FTYPE = 0x1,
    PROBERESP_FTYPE,
    ACTION_MGMT_FTYPE,
    PROBEREQ_FTYPE
} WMI_BI_FTYPE;

enum {
    BSS_ELEMID_CHANSWITCH = 0x01,
    BSS_ELEMID_ATHEROS = 0x02
};

typedef PREPACK struct {
    A_UINT16    channel FIELD_PACKED;
    A_UINT8     frameType FIELD_PACKED;          /* see WMI_BI_FTYPE */
    A_UINT8     snr FIELD_PACKED;
    A_INT16     rssi FIELD_PACKED;
    A_UINT8     bssid[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT32    ieMask FIELD_PACKED;
} POSTPACK WMI_BSS_INFO_HDR;

/*
 * BSS INFO HDR version 2.0
 * With 6 bytes HTC header and 6 bytes of WMI header
 * WMI_BSS_INFO_HDR cannot be accomodated in the removed 802.11 management
 * header space.
 * - Reduce the ieMask to 2 bytes as only two bit flags are used
 * - Remove rssi and compute it on the host. rssi = snr - 95
 */
typedef PREPACK struct {
    A_UINT16    channel FIELD_PACKED;
    A_UINT8     frameType FIELD_PACKED;          /* see WMI_BI_FTYPE */
    A_UINT8     snr FIELD_PACKED;
    A_UINT8     bssid[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT16    ieMask FIELD_PACKED;
} POSTPACK WMI_BSS_INFO_HDR2;

/*
 * Command Error Event
 */
typedef enum {
    INVALID_PARAM  = 0x01,
    ILLEGAL_STATE  = 0x02,
    INTERNAL_ERROR = 0x03,
    STACK_ERROR    = 0x04
} WMI_ERROR_CODE;

typedef PREPACK struct {
    A_UINT16    commandId FIELD_PACKED;
    A_UINT8     errorCode FIELD_PACKED;
} POSTPACK WMI_CMD_ERROR_EVENT;

/*
 * New Regulatory Domain Event
 */
typedef PREPACK struct {
    A_UINT32    regDomain FIELD_PACKED;
} POSTPACK WMI_REG_DOMAIN_EVENT;

typedef PREPACK struct {
    A_UINT8     txQueueNumber FIELD_PACKED;
    A_UINT8     rxQueueNumber FIELD_PACKED;
    A_UINT8     trafficDirection FIELD_PACKED;
    A_UINT8     trafficClass FIELD_PACKED;
} POSTPACK WMI_PSTREAM_TIMEOUT_EVENT;

/*
 * TKIP MIC Error Event
 */
typedef PREPACK struct {
    A_UINT8 keyid FIELD_PACKED;
    A_UINT8 ismcast FIELD_PACKED;
} POSTPACK WMI_TKIP_MICERR_EVENT;

/*
 * WMI_SCAN_COMPLETE_EVENTID - no parameters (old), staus parameter (new)
 */
typedef PREPACK struct {
    A_INT32 status FIELD_PACKED;
} POSTPACK WMI_SCAN_COMPLETE_EVENT;

#define MAX_OPT_DATA_LEN 1400



/*
 * Reporting statistics.
 */
typedef PREPACK struct {
    A_UINT32   tx_packets FIELD_PACKED;
    A_UINT32   tx_bytes FIELD_PACKED;
    A_UINT32   tx_unicast_pkts FIELD_PACKED;
    A_UINT32   tx_unicast_bytes FIELD_PACKED;
    A_UINT32   tx_multicast_pkts FIELD_PACKED;
    A_UINT32   tx_multicast_bytes FIELD_PACKED;
    A_UINT32   tx_broadcast_pkts FIELD_PACKED;
    A_UINT32   tx_broadcast_bytes FIELD_PACKED;
    A_UINT32   tx_rts_success_cnt FIELD_PACKED;
    A_UINT32   tx_packet_per_ac[4] FIELD_PACKED;
    A_UINT32   tx_errors_per_ac[4] FIELD_PACKED;

    A_UINT32   tx_errors FIELD_PACKED;
    A_UINT32   tx_failed_cnt FIELD_PACKED;
    A_UINT32   tx_retry_cnt FIELD_PACKED;
    A_UINT32   tx_mult_retry_cnt FIELD_PACKED;
    A_UINT32   tx_rts_fail_cnt FIELD_PACKED;
    A_INT32    tx_unicast_rate FIELD_PACKED;
}POSTPACK tx_stats_t;

typedef PREPACK struct {
    A_UINT32   rx_packets FIELD_PACKED;
    A_UINT32   rx_bytes FIELD_PACKED;
    A_UINT32   rx_unicast_pkts FIELD_PACKED;
    A_UINT32   rx_unicast_bytes FIELD_PACKED;
    A_UINT32   rx_multicast_pkts FIELD_PACKED;
    A_UINT32   rx_multicast_bytes FIELD_PACKED;
    A_UINT32   rx_broadcast_pkts FIELD_PACKED;
    A_UINT32   rx_broadcast_bytes FIELD_PACKED;
    A_UINT32   rx_fragment_pkt FIELD_PACKED;

    A_UINT32   rx_errors FIELD_PACKED;
    A_UINT32   rx_crcerr FIELD_PACKED;
    A_UINT32   rx_key_cache_miss FIELD_PACKED;
    A_UINT32   rx_decrypt_err FIELD_PACKED;
    A_UINT32   rx_duplicate_frames FIELD_PACKED;
    A_INT32    rx_unicast_rate FIELD_PACKED;
}POSTPACK rx_stats_t;

typedef PREPACK struct {
    A_UINT32   tkip_local_mic_failure FIELD_PACKED;
    A_UINT32   tkip_counter_measures_invoked FIELD_PACKED;
    A_UINT32   tkip_replays FIELD_PACKED;
    A_UINT32   tkip_format_errors FIELD_PACKED;
    A_UINT32   ccmp_format_errors FIELD_PACKED;
    A_UINT32   ccmp_replays FIELD_PACKED;
}POSTPACK tkip_ccmp_stats_t FIELD_PACKED;

typedef PREPACK struct {
    A_UINT32   power_save_failure_cnt FIELD_PACKED;
    A_UINT16   stop_tx_failure_cnt FIELD_PACKED;
    A_UINT16   atim_tx_failure_cnt FIELD_PACKED;
    A_UINT16   atim_rx_failure_cnt FIELD_PACKED;
    A_UINT16   bcn_rx_failure_cnt FIELD_PACKED;
}POSTPACK pm_stats_t;

typedef PREPACK struct {
    A_UINT32    cs_bmiss_cnt FIELD_PACKED;
    A_UINT32    cs_lowRssi_cnt FIELD_PACKED;
    A_UINT16    cs_connect_cnt FIELD_PACKED;
    A_UINT16    cs_disconnect_cnt FIELD_PACKED;
    A_INT16     cs_aveBeacon_rssi FIELD_PACKED;
    A_UINT16    cs_roam_count FIELD_PACKED;
    A_INT16     cs_rssi FIELD_PACKED;
    A_UINT8     cs_snr FIELD_PACKED;
    A_UINT8     cs_aveBeacon_snr FIELD_PACKED;
    A_UINT8     cs_lastRoam_msec FIELD_PACKED;
} POSTPACK cserv_stats_t;

typedef PREPACK struct {
    tx_stats_t          tx_stats FIELD_PACKED;
    rx_stats_t          rx_stats FIELD_PACKED;
    tkip_ccmp_stats_t   tkipCcmpStats FIELD_PACKED;
}POSTPACK wlan_net_stats_t;

typedef PREPACK struct {
    A_UINT32    arp_received FIELD_PACKED;
    A_UINT32    arp_matched FIELD_PACKED;
    A_UINT32    arp_replied FIELD_PACKED;
} POSTPACK arp_stats_t;

typedef PREPACK struct {
    A_UINT32    wow_num_pkts_dropped FIELD_PACKED;
    A_UINT16    wow_num_events_discarded FIELD_PACKED;
    A_UINT8     wow_num_host_pkt_wakeups FIELD_PACKED;
    A_UINT8     wow_num_host_event_wakeups FIELD_PACKED;
} POSTPACK wlan_wow_stats_t;

typedef PREPACK struct {
    A_UINT32            lqVal FIELD_PACKED;
    A_INT32             noise_floor_calibation FIELD_PACKED;
    pm_stats_t          pmStats FIELD_PACKED;
    wlan_net_stats_t    txrxStats FIELD_PACKED;
    wlan_wow_stats_t    wowStats FIELD_PACKED;
    arp_stats_t         arpStats FIELD_PACKED;
    cserv_stats_t       cservStats FIELD_PACKED;
} POSTPACK WMI_TARGET_STATS;

/*
 *  WMI_ERROR_REPORT_EVENTID
 */
typedef enum{
    WMI_TARGET_PM_ERR_FAIL      = 0x00000001,
    WMI_TARGET_KEY_NOT_FOUND    = 0x00000002,
    WMI_TARGET_DECRYPTION_ERR   = 0x00000004,
    WMI_TARGET_BMISS            = 0x00000008,
    WMI_PSDISABLE_NODE_JOIN     = 0x00000010,
    WMI_TARGET_COM_ERR          = 0x00000020,
    WMI_TARGET_FATAL_ERR        = 0x00000040
} WMI_TARGET_ERROR_VAL;

typedef PREPACK struct {
    A_UINT32 errorVal FIELD_PACKED;
}POSTPACK  WMI_TARGET_ERROR_REPORT_EVENT;

typedef PREPACK struct {
    A_UINT8 retrys FIELD_PACKED;
}POSTPACK  WMI_TX_RETRY_ERR_EVENT;

/*
 * developer commands
 */


/*
 * WMI_SET_BITRATE_CMDID
 *
 * Get bit rate cmd uses same definition as set bit rate cmd
 */
typedef enum {
    RATE_AUTO   = -1,
    RATE_1Mb    = 0,
    RATE_2Mb    = 1,
    RATE_5_5Mb  = 2,
    RATE_11Mb   = 3,
    RATE_6Mb    = 4,
    RATE_9Mb    = 5,
    RATE_12Mb   = 6,
    RATE_18Mb   = 7,
    RATE_24Mb   = 8,
    RATE_36Mb   = 9,
    RATE_48Mb   = 10,
    RATE_54Mb   = 11,
    RATE_MCS_0_20 = 12,
    RATE_MCS_1_20 = 13,
    RATE_MCS_2_20 = 14,
    RATE_MCS_3_20 = 15,
    RATE_MCS_4_20 = 16,
    RATE_MCS_5_20 = 17,
    RATE_MCS_6_20 = 18,
    RATE_MCS_7_20 = 19,
    RATE_MCS_0_40 = 20,
    RATE_MCS_1_40 = 21,
    RATE_MCS_2_40 = 22,
    RATE_MCS_3_40 = 23,
    RATE_MCS_4_40 = 24,
    RATE_MCS_5_40 = 25,
    RATE_MCS_6_40 = 26,
    RATE_MCS_7_40 = 27
} WMI_BIT_RATE;

typedef PREPACK struct {
    A_INT8      rateIndex FIELD_PACKED;          /* see WMI_BIT_RATE */
    A_INT8      mgmtRateIndex FIELD_PACKED;
    A_INT8      ctlRateIndex FIELD_PACKED;
} POSTPACK WMI_BIT_RATE_CMD;


typedef PREPACK struct {
    A_INT8      rateIndex FIELD_PACKED;          /* see WMI_BIT_RATE */
} POSTPACK  WMI_BIT_RATE_REPLY;


/*
 * WMI_SET_FIXRATES_CMDID
 *
 * Get fix rates cmd uses same definition as set fix rates cmd
 */
#define FIX_RATE_1Mb            ((A_UINT32)0x1)
#define FIX_RATE_2Mb            ((A_UINT32)0x2)
#define FIX_RATE_5_5Mb          ((A_UINT32)0x4)
#define FIX_RATE_11Mb           ((A_UINT32)0x8)
#define FIX_RATE_6Mb            ((A_UINT32)0x10)
#define FIX_RATE_9Mb            ((A_UINT32)0x20)
#define FIX_RATE_12Mb           ((A_UINT32)0x40)
#define FIX_RATE_18Mb           ((A_UINT32)0x80)
#define FIX_RATE_24Mb           ((A_UINT32)0x100)
#define FIX_RATE_36Mb           ((A_UINT32)0x200)
#define FIX_RATE_48Mb           ((A_UINT32)0x400)
#define FIX_RATE_54Mb           ((A_UINT32)0x800)
#define FIX_RATE_MCS_0_20       ((A_UINT32)0x1000)
#define FIX_RATE_MCS_1_20       ((A_UINT32)0x2000)
#define FIX_RATE_MCS_2_20       ((A_UINT32)0x4000)
#define FIX_RATE_MCS_3_20       ((A_UINT32)0x8000)
#define FIX_RATE_MCS_4_20       ((A_UINT32)0x10000)
#define FIX_RATE_MCS_5_20       ((A_UINT32)0x20000)
#define FIX_RATE_MCS_6_20       ((A_UINT32)0x40000)
#define FIX_RATE_MCS_7_20       ((A_UINT32)0x80000)
#define FIX_RATE_MCS_0_40       ((A_UINT32)0x100000)
#define FIX_RATE_MCS_1_40       ((A_UINT32)0x200000)
#define FIX_RATE_MCS_2_40       ((A_UINT32)0x400000)
#define FIX_RATE_MCS_3_40       ((A_UINT32)0x800000)
#define FIX_RATE_MCS_4_40       ((A_UINT32)0x1000000)
#define FIX_RATE_MCS_5_40       ((A_UINT32)0x2000000)
#define FIX_RATE_MCS_6_40       ((A_UINT32)0x4000000)
#define FIX_RATE_MCS_7_40       ((A_UINT32)0x8000000)


typedef enum {
    WMI_WMM_DISABLED = 0,
    WMI_WMM_ENABLED
} WMI_WMM_STATUS;

typedef PREPACK struct {
    A_UINT8    status FIELD_PACKED;
}POSTPACK WMI_SET_WMM_CMD;

typedef PREPACK struct {
    A_UINT8    status FIELD_PACKED;
}POSTPACK WMI_SET_QOS_SUPP_CMD;

typedef enum {
    WMI_TXOP_DISABLED = 0,
    WMI_TXOP_ENABLED
} WMI_TXOP_CFG;

typedef PREPACK struct {
    A_UINT8    txopEnable FIELD_PACKED;
}POSTPACK WMI_SET_WMM_TXOP_CMD;

typedef PREPACK struct {
    A_UINT8 keepaliveInterval FIELD_PACKED;
} POSTPACK WMI_SET_KEEPALIVE_CMD;

typedef PREPACK struct {
    A_BOOL configured FIELD_PACKED;
    A_UINT8 keepaliveInterval FIELD_PACKED;
} POSTPACK WMI_GET_KEEPALIVE_CMD;


typedef PREPACK struct {
    A_UINT16 oldChannel FIELD_PACKED;
    A_UINT32 newChannel FIELD_PACKED;
} POSTPACK WMI_CHANNEL_CHANGE_EVENT;

typedef PREPACK struct {
    A_UINT32 version FIELD_PACKED;
} POSTPACK WMI_WLAN_VERSION_EVENT;


/* WMI_ADDBA_REQ_EVENTID */
typedef PREPACK struct {
    A_UINT8     tid FIELD_PACKED;
    A_UINT8     win_sz FIELD_PACKED;
    A_UINT16    st_seq_no FIELD_PACKED;
    A_UINT8     status FIELD_PACKED;         /* f/w response for ADDBA Req; OK(0) or failure(!=0) */
} POSTPACK WMI_ADDBA_REQ_EVENT;

/* WMI_ADDBA_RESP_EVENTID */
typedef PREPACK struct {
    A_UINT8     tid FIELD_PACKED;
    A_UINT8     status FIELD_PACKED;         /* OK(0), failure (!=0) */
    A_UINT16    amsdu_sz FIELD_PACKED;       /* Three values: Not supported(0), 3839, 8k */
} POSTPACK WMI_ADDBA_RESP_EVENT;

/* WMI_DELBA_EVENTID
 * f/w received a DELBA for peer and processed it.
 * Host is notified of this
 */
typedef PREPACK struct {
    A_UINT8     tid FIELD_PACKED;
    A_UINT8     is_peer_initiator FIELD_PACKED;
    A_UINT16    reason_code FIELD_PACKED;
} POSTPACK WMI_DELBA_EVENT;


/* WMI_ALLOW_AGGR_CMDID
 * Configures tid's to allow ADDBA negotiations
 * on each tid, in each direction
 */
typedef PREPACK struct {
    A_UINT16    tx_allow_aggr FIELD_PACKED;  /* 16-bit mask to allow uplink ADDBA negotiation - bit position indicates tid*/
    A_UINT16    rx_allow_aggr FIELD_PACKED;  /* 16-bit mask to allow donwlink ADDBA negotiation - bit position indicates tid*/
} POSTPACK WMI_ALLOW_AGGR_CMD;

/* WMI_ADDBA_REQ_CMDID
 * f/w starts performing ADDBA negotiations with peer
 * on the given tid
 */
typedef PREPACK struct {
    A_UINT8     tid FIELD_PACKED;
} POSTPACK WMI_ADDBA_REQ_CMD;

/* WMI_DELBA_REQ_CMDID
 * f/w would teardown BA with peer.
 * is_send_initiator indicates if it's or tx or rx side
 */
typedef PREPACK struct {
    A_UINT8     tid FIELD_PACKED;
    A_UINT8     is_sender_initiator FIELD_PACKED;

} POSTPACK WMI_DELBA_REQ_CMD;

#define PEER_NODE_JOIN_EVENT 0x00
#define PEER_NODE_LEAVE_EVENT 0x01
#define PEER_FIRST_NODE_JOIN_EVENT 0x10
#define PEER_LAST_NODE_LEAVE_EVENT 0x11
typedef PREPACK struct {
    A_UINT8 eventCode FIELD_PACKED;
    A_UINT8 peerMacAddr[ATH_MAC_LEN] FIELD_PACKED;
} POSTPACK WMI_PEER_NODE_EVENT;

#define IEEE80211_FRAME_TYPE_MGT          0x00
#define IEEE80211_FRAME_TYPE_CTL          0x04



typedef PREPACK struct {
    A_UINT8  band FIELD_PACKED; /* specifies which band to apply these values */
    A_UINT8  enable FIELD_PACKED; /* allows 11n to be disabled on a per band basis */
    A_UINT8  chan_width_40M_supported FIELD_PACKED;
    A_UINT8  short_GI_20MHz FIELD_PACKED;
    A_UINT8  short_GI_40MHz FIELD_PACKED;
    A_UINT8  intolerance_40MHz FIELD_PACKED;
    A_UINT8  max_ampdu_len_exp FIELD_PACKED;
} POSTPACK WMI_SET_HT_CAP_CMD;

typedef PREPACK struct {
    A_UINT8   sta_chan_width FIELD_PACKED;
} POSTPACK WMI_SET_HT_OP_CMD;

typedef PREPACK struct {
    A_UINT8 metaVersion FIELD_PACKED; /* version of meta data for rx packets <0 = default> (0-7 = valid) */
    A_UINT8 dot11Hdr FIELD_PACKED; /* 1 == leave .11 header intact , 0 == replace .11 header with .3 <default> */
    A_UINT8 defragOnHost FIELD_PACKED; /* 1 == defragmentation is performed by host, 0 == performed by target <default> */
    A_UINT8 reserved[1] FIELD_PACKED; /* alignment */
} POSTPACK WMI_RX_FRAME_FORMAT_CMD;


/* STORE / RECALL Commands AND Events DEFINITION START */



typedef PREPACK struct {
    A_UINT8 enable FIELD_PACKED;
#define STRRCL_RECIPIENT_HOST 1
    A_UINT8 recipient FIELD_PACKED;
} POSTPACK WMI_STORERECALL_CONFIGURE_CMD;

typedef PREPACK struct {
    A_UINT32 length FIELD_PACKED; /* number of bytes of data to follow */
    A_UINT8  data[1] FIELD_PACKED; /* start of data */
} POSTPACK WMI_STORERECALL_RECALL_CMD;

typedef PREPACK struct {
    A_UINT32 sleep_msec FIELD_PACKED;
    A_UINT8 store_after_tx_empty FIELD_PACKED;
    A_UINT8 store_after_fresh_beacon_rx FIELD_PACKED;
} POSTPACK WMI_STORERECALL_HOST_READY_CMD;

typedef PREPACK struct {
    A_UINT32    msec_sleep FIELD_PACKED; /* time between power off/on */
    A_UINT32    length FIELD_PACKED; /* length of following data */
    A_UINT8     data[1] FIELD_PACKED; /* start of data */
} POSTPACK WMI_STORERECALL_STORE_EVENT;

/* STORE / RECALL Commands AND Events DEFINITION END */
/* WPS Commands AND Events DEFINITION START */

typedef PREPACK struct {
    A_UINT8 ssid[WMI_MAX_SSID_LEN] FIELD_PACKED;
    A_UINT8 macaddress[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT16 channel FIELD_PACKED;
    A_UINT8 ssid_len FIELD_PACKED;
} POSTPACK WPS_SCAN_LIST_ENTRY;


typedef enum {
    WPS_REGISTRAR_ROLE    = 0x1,
    WPS_ENROLLEE_ROLE     = 0x2,
    WPS_AP_ENROLLEE_ROLE  = 0x3
} WPS_OPER_MODE;

typedef enum {
    WPS_DO_CONNECT_AFTER_CRED_RECVD = 0x1
} WPS_START_CTRL_FLAG;


#define WPS_PIN_LEN (8)

typedef PREPACK struct {
    A_UINT8 pin[WPS_PIN_LEN+1] FIELD_PACKED;
    A_UINT8 pin_length FIELD_PACKED;
}POSTPACK WPS_PIN;

typedef enum {
    WPS_PIN_MODE = 0x1,
    WPS_PBC_MODE = 0x2
} WPS_MODE;

typedef PREPACK struct {
    WPS_SCAN_LIST_ENTRY ssid_info FIELD_PACKED;
    A_UINT8 config_mode FIELD_PACKED; /* WPS_MODE */
    WPS_PIN wps_pin FIELD_PACKED;
    A_UINT8 timeout FIELD_PACKED;     /* in Seconds */
    A_UINT8 role FIELD_PACKED;        /* WPS_OPER_MOD */
    A_UINT8 ctl_flag FIELD_PACKED;    /* WPS_START_CTRL_FLAG */
} POSTPACK WMI_WPS_START_CMD;


typedef enum {
    WPS_STATUS_SUCCESS = 0x0,
    WPS_STATUS_FAILURE = 0x1,
    WPS_STATUS_IDLE = 0x2,
    WPS_STATUS_IN_PROGRESS  = 0x3
} WPS_STATUS;

typedef PREPACK struct {
    A_UINT8  wps_status FIELD_PACKED;  /* WPS_STATUS */
    A_UINT8  wps_state FIELD_PACKED;
} POSTPACK WMI_WPS_GET_STATUS_EVENT;

typedef enum {
  WPS_ERROR_INVALID_START_INFO  = 0x1,
  WPS_ERROR_MULTIPLE_PBC_SESSIONS,
  WPS_ERROR_WALKTIMER_TIMEOUT,
  WPS_ERROR_M2D_RCVD,
  WPS_ERROR_PWD_AUTH_FAIL,
  WPS_ERROR_CANCELLED,
  WPS_ERROR_INVALID_PIN
} WPS_ERROR_CODE;


/* Authentication Type Flags */
#define WPS_CRED_AUTH_OPEN    0x0001
#define WPS_CRED_AUTH_WPAPSK  0x0002
#define WPS_CRED_AUTH_SHARED  0x0004
#define WPS_CRED_AUTH_WPA     0x0008
#define WPS_CRED_AUTH_WPA2    0x0010
#define WPS_CRED_AUTH_WPA2PSK 0x0020

/* Encryption Type Flags */
#define WPS_CRED_ENCR_NONE 0x0001
#define WPS_CRED_ENCR_WEP  0x0002
#define WPS_CRED_ENCR_TKIP 0x0004
#define WPS_CRED_ENCR_AES  0x0008

typedef PREPACK struct {
  A_UINT16 ap_channel FIELD_PACKED;
  A_UINT8  ssid[WMI_MAX_SSID_LEN] FIELD_PACKED;
  A_UINT8  ssid_len FIELD_PACKED;
  A_UINT16 auth_type FIELD_PACKED;
  A_UINT16 encr_type FIELD_PACKED;
  A_UINT8  key_idx FIELD_PACKED;
  A_UINT8  key[64] FIELD_PACKED;
  A_UINT8  key_len FIELD_PACKED;
  A_UINT8  mac_addr[ATH_MAC_LEN] FIELD_PACKED;
} POSTPACK WPS_CREDENTIAL;

typedef PREPACK struct {
  A_UINT8 status FIELD_PACKED;      /* WPS_STATUS */
  A_UINT8 error_code FIELD_PACKED;  /* WPS_ERROR_CODE */
  WPS_CREDENTIAL  credential FIELD_PACKED;
  A_UINT8  peer_dev_addr[ATH_MAC_LEN] FIELD_PACKED;
} POSTPACK WMI_WPS_PROFILE_EVENT;
/* WPS Commands AND Events DEFINITION END */



typedef PREPACK struct {
    A_UINT8 enable FIELD_PACKED;     /* 1 == device operates in promiscuous mode , 0 == normal mode <default> */
#define WMI_PROM_FILTER_SRC_ADDR 0x01
#define WMI_PROM_FILTER_DST_ADDR 0x02
    A_UINT8 filters FIELD_PACKED;
    A_UINT8 srcAddr[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT8 dstAddr[ATH_MAC_LEN] FIELD_PACKED;
} POSTPACK WMI_SET_FILTERED_PROMISCUOUS_MODE_CMD;



typedef PREPACK struct {
    A_UINT16    channel FIELD_PACKED; /* frequency in Mhz */
} POSTPACK WMI_SET_CHANNEL_CMD;

typedef PREPACK struct {
    A_UINT32 cmd_type FIELD_PACKED; /*Socket command type*/
    A_UINT32 length FIELD_PACKED;   /* number of bytes of data to follow */
    //A_UINT8  data[1] FIELD_PACKED;  /* start of data */
} POSTPACK WMI_SOCKET_CMD;

typedef PREPACK struct {
	A_UINT32    resp_type FIELD_PACKED;			/*Socket command response type*/
    A_UINT32    sock_handle FIELD_PACKED;  		/*Socket handle*/
    A_UINT32    error FIELD_PACKED;  			/*Return value*/
    A_UINT8     data[1] FIELD_PACKED;     		/*data for socket command e.g. select FDs*/
} POSTPACK WMI_SOCK_RESPONSE_EVENT;

#if ENABLE_P2P_MODE

typedef PREPACK struct {
    A_UINT8 flag FIELD_PACKED;
}POSTPACK WMI_P2P_SET_CONCURRENT_MODE;

typedef PREPACK struct {
    A_UINT8 value FIELD_PACKED;
}POSTPACK WMI_P2P_SET_GO_INTENT;

typedef PREPACK struct {
    A_UINT8 go_intent FIELD_PACKED;
    A_UINT8 country[3] FIELD_PACKED;
    A_UINT8 reg_class FIELD_PACKED;
    A_UINT8 listen_channel FIELD_PACKED;
    A_UINT8 op_reg_class FIELD_PACKED;
    A_UINT8 op_channel FIELD_PACKED;
    A_UINT32 node_age_to FIELD_PACKED;
    A_UINT8 max_node_count FIELD_PACKED;
} POSTPACK WMI_P2P_FW_SET_CONFIG_CMD;

typedef PREPACK struct {
    A_UINT8 enable FIELD_PACKED;
}POSTPACK WMI_P2P_SET_PROFILE_CMD;

typedef PREPACK struct {
    A_UINT16 categ;
    A_UINT16 sub_categ;
} POSTPACK device_type_tuple;

typedef PREPACK struct {
    device_type_tuple pri_dev_type;
    A_UINT8 pri_device_type[8];
#define MAX_P2P_SEC_DEVICE_TYPES 5
    device_type_tuple sec_dev_type[MAX_P2P_SEC_DEVICE_TYPES];
#define WPS_UUID_LEN 16
    A_UINT8 uuid[WPS_UUID_LEN];
#define WPS_MAX_DEVNAME_LEN 32
    A_UINT8 device_name[WPS_MAX_DEVNAME_LEN];
    A_UINT8 dev_name_len;
} POSTPACK WMI_WPS_SET_CONFIG_CMD;

typedef PREPACK struct {
    device_type_tuple pri_dev_type;
    device_type_tuple sec_dev_type[MAX_P2P_SEC_DEVICE_TYPES];
    A_UINT8  device_addr[ATH_MAC_LEN];
} POSTPACK WMI_SET_REQ_DEV_ATTR_CMD;

typedef PREPACK struct {
    A_UINT8 ssidLength;
    A_UINT8 ssid[WMI_MAX_SSID_LEN];
} POSTPACK P2P_SSID;

typedef enum wmi_p2p_discovery_type {
    WMI_P2P_FIND_START_WITH_FULL,
    WMI_P2P_FIND_ONLY_SOCIAL,
    WMI_P2P_FIND_PROGRESSIVE
} WMI_P2P_DISC_TYPE;

typedef PREPACK struct {
    A_UINT32 timeout;
    WMI_P2P_DISC_TYPE type;
} POSTPACK WMI_P2P_FIND_CMD;

typedef PREPACK struct {
    A_UINT16 listen_freq;
    A_UINT16 force_freq;
    A_UINT16 go_oper_freq;
    A_UINT8  dialog_token;
    A_UINT8  peer_addr[ATH_MAC_LEN];
    A_UINT8  own_interface_addr[ATH_MAC_LEN];
    A_UINT8  member_in_go_dev[ATH_MAC_LEN];
    A_UINT8  go_dev_dialog_token;
    P2P_SSID peer_go_ssid;
    A_UINT8  wps_method;
    A_UINT8 dev_capab;
    A_INT8  go_intent;
    A_UINT8 persistent_grp;
} POSTPACK WMI_P2P_GO_NEG_START_CMD;

typedef PREPACK struct {
    A_UINT8 persistent_group FIELD_PACKED;
    A_UINT8 group_formation FIELD_PACKED;
} POSTPACK WMI_P2P_GRP_INIT_CMD;

typedef PREPACK struct {
    A_UINT8 peer_addr[ATH_MAC_LEN];
    A_UINT8 grp_formation_status;
} POSTPACK WMI_P2P_GRP_FORMATION_DONE_CMD;

typedef PREPACK struct {
    A_UINT32 timeout;
}POSTPACK WMI_P2P_LISTEN_CMD;

typedef PREPACK struct {
    A_UINT16 listen_freq;
    A_UINT16 force_freq;
    A_UINT8 status;
    A_INT8 go_intent;
    A_UINT8 wps_buf[512];
    A_UINT16 wps_buflen;
    A_UINT8 p2p_buf[512];
    A_UINT16 p2p_buflen;
    A_UINT8 dialog_token;
    A_UINT8 wps_method;
    A_UINT8 persistent_grp;
}POSTPACK WMI_P2P_GO_NEG_REQ_RSP_CMD;

typedef enum {
    WMI_P2P_INVITE_ROLE_GO,
    WMI_P2P_INVITE_ROLE_ACTIVE_GO,
    WMI_P2P_INVITE_ROLE_CLIENT
} WMI_P2P_INVITE_ROLE;

typedef PREPACK struct {
    WMI_P2P_INVITE_ROLE role FIELD_PACKED;
    A_UINT16 listen_freq FIELD_PACKED;
    A_UINT16 force_freq FIELD_PACKED;
    A_UINT8 dialog_token FIELD_PACKED;
    A_UINT8 peer_addr[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT8 bssid[ATH_MAC_LEN] FIELD_PACKED;
    A_UINT8 go_dev_addr[ATH_MAC_LEN] FIELD_PACKED;
    P2P_SSID ssid FIELD_PACKED;
    A_UINT8 is_persistent FIELD_PACKED;
    A_UINT8 wps_method FIELD_PACKED;
}POSTPACK WMI_P2P_INVITE_CMD;

typedef PREPACK struct {
    A_UINT16 force_freq;
    A_UINT8 status;
    A_UINT8 dialog_token;
    //    A_UINT8 p2p_buf[512];
    A_UINT16 p2p_buflen;
    A_UINT8 is_go;
    A_UINT8 group_bssid[ATH_MAC_LEN];
}POSTPACK WMI_P2P_INVITE_REQ_RSP_CMD;

typedef PREPACK struct {
    A_UINT16 force_freq;
    A_UINT8 status;
    A_UINT8 dialog_token;
    A_UINT8 is_go;
    A_UINT8 group_bssid[ATH_MAC_LEN];
}POSTPACK WMI_P2P_FW_INVITE_REQ_RSP_CMD;

typedef PREPACK struct {
    A_UINT16 wps_method;
    A_UINT16 listen_freq;
    A_UINT8 dialog_token;
    A_UINT8 peer[ATH_MAC_LEN];
    A_UINT8 go_dev_addr[ATH_MAC_LEN];
    P2P_SSID go_oper_ssid;
}POSTPACK WMI_P2P_PROV_DISC_REQ_CMD;

typedef enum {
    WMI_P2P_CONFID_LISTEN_CHANNEL=1,
    WMI_P2P_CONFID_CROSS_CONNECT=2,
    WMI_P2P_CONFID_SSID_POSTFIX=3,
    WMI_P2P_CONFID_INTRA_BSS=4,
    WMI_P2P_CONFID_CONCURRENT_MODE=5,
    WMI_P2P_CONFID_GO_INTENT=6,
    WMI_P2P_CONFID_DEV_NAME=7,
    WMI_P2P_CONFID_P2P_OPMODE=8
} WMI_P2P_CONF_ID;

typedef PREPACK struct {
    A_UINT8 reg_class;
    A_UINT8 listen_channel;
}POSTPACK WMI_P2P_LISTEN_CHANNEL;

typedef PREPACK struct {
    A_UINT8 flag;
}POSTPACK WMI_P2P_SET_CROSS_CONNECT;

typedef PREPACK struct {
    A_UINT8 ssid_postfix[WMI_MAX_SSID_LEN-9];
    A_UINT8 ssid_postfix_len;
}POSTPACK WMI_P2P_SET_SSID_POSTFIX;

typedef PREPACK struct {
    A_UINT8 flag;
}POSTPACK WMI_P2P_SET_INTRA_BSS;

#define P2P_DEV    (1<<0)
#define P2P_CLIENT (1<<1)
#define P2P_GO     (1<<2)

typedef PREPACK struct {
    A_UINT8 p2pmode;
}POSTPACK WMI_P2P_SET_MODE;


#define RATECTRL_MODE_DEFAULT 0
#define RATECTRL_MODE_PERONLY 1

typedef PREPACK struct {
    A_UINT32 mode;
}POSTPACK WMI_SET_RATECTRL_PARM_CMD;

#define WMI_P2P_MAX_TLV_LEN 1024
typedef enum {
    WMI_P2P_SD_TYPE_GAS_INITIAL_REQ     = 0x1,
    WMI_P2P_SD_TYPE_GAS_INITIAL_RESP    = 0x2,
    WMI_P2P_SD_TYPE_GAS_COMEBACK_REQ    = 0x3,
    WMI_P2P_SD_TYPE_GAS_COMEBACK_RESP   = 0x4,
    WMI_P2P_PD_TYPE_RESP                = 0x5,
    WMI_P2P_SD_TYPE_STATUS_IND          = 0x6
} WMI_P2P_SDPD_TYPE;

typedef enum {
    WMI_P2P_SDPD_TRANSACTION_PENDING = 0x1,
    WMI_P2P_SDPD_TRANSACTION_COMP = 0x2
} WMI_P2P_SDPD_TRANSACTION_STATUS;

typedef PREPACK struct {
    A_UINT8 type;
    A_UINT8 dialog_token;
    A_UINT8 frag_id;
    A_UINT8 reserved1;          /* alignment */
    A_UINT8 peer_addr[ATH_MAC_LEN];
    A_UINT16 freq;
    A_UINT16 status_code;
    A_UINT16 comeback_delay;
    A_UINT16 tlv_length;
    A_UINT16 update_indic;
    A_UINT16 total_length;
    A_UINT16 reserved2;         /* future */
    A_UINT8  tlv[WMI_P2P_MAX_TLV_LEN];
} POSTPACK WMI_P2P_SDPD_TX_CMD;

typedef PREPACK struct {
    A_UINT16 go_oper_freq;
    A_UINT8  dialog_token;
    A_UINT8  peer_addr[ATH_MAC_LEN];
    A_UINT8  own_interface_addr[ATH_MAC_LEN];
    A_UINT8  go_dev_dialog_token;
    P2P_SSID peer_go_ssid;
    A_UINT8  wps_method;
    A_UINT8 dev_capab;
    A_UINT8  dev_auth;
    A_UINT8 go_intent;
} POSTPACK WMI_P2P_CONNECT_CMD;

typedef PREPACK struct {
    A_UINT16 wps_method;
    A_UINT8 dialog_token;
    A_UINT8 peer[ATH_MAC_LEN];
}POSTPACK WMI_P2P_FW_PROV_DISC_REQ_CMD;

/* P2P module events */

typedef PREPACK struct {
    A_UINT8 num_p2p_dev;
    A_UINT8 data[1];
} POSTPACK WMI_P2P_NODE_LIST_EVENT;

typedef PREPACK struct {
    A_UINT8 sa[ATH_MAC_LEN];
    A_UINT8 dialog_token;
    A_UINT16 dev_password_id;
}POSTPACK WMI_P2P_REQ_TO_AUTH_EVENT;

typedef PREPACK struct {
    A_UINT8 sa[ATH_MAC_LEN];
    A_UINT8 wps_buf[512];
    A_UINT16 wps_buflen;
    A_UINT8 p2p_buf[512];
    A_UINT16 p2p_buflen;
    A_UINT8 dialog_token;
}POSTPACK WMI_P2P_GO_NEG_REQ_EVENT;

typedef PREPACK struct {
    A_UINT16 freq;
    A_INT8 status;
    A_UINT8 role_go;
    A_UINT8 ssid[WMI_MAX_SSID_LEN];
    A_UINT8 ssid_len;
#define WMI_MAX_PASSPHRASE_LEN 9
    A_CHAR pass_phrase[WMI_MAX_PASSPHRASE_LEN];
    A_UINT8 peer_device_addr[ATH_MAC_LEN];
    A_UINT8 peer_interface_addr[ATH_MAC_LEN];
    A_UINT8 wps_method;
    A_UINT8 persistent_grp;
} POSTPACK WMI_P2P_GO_NEG_RESULT_EVENT;

typedef PREPACK struct {
    A_UINT8 sa[ATH_MAC_LEN];
    A_UINT8 bssid[ATH_MAC_LEN];
    A_UINT8 go_dev_addr[ATH_MAC_LEN];
    P2P_SSID ssid;
    A_UINT8 is_persistent;
    A_UINT8 dialog_token;
} POSTPACK WMI_P2P_FW_INVITE_REQ_EVENT;

typedef PREPACK struct {
    A_UINT16 oper_freq;
    A_UINT8 sa[ATH_MAC_LEN];
    A_UINT8 bssid[ATH_MAC_LEN];
    A_UINT8 is_bssid_valid;
    A_UINT8 go_dev_addr[ATH_MAC_LEN];
    P2P_SSID ssid;
    A_UINT8 status;
} POSTPACK WMI_P2P_INVITE_RCVD_RESULT_EVENT;

typedef PREPACK struct {
    A_UINT8 status;
    A_UINT8 bssid[ATH_MAC_LEN];
    A_UINT8 is_bssid_valid;
} POSTPACK WMI_P2P_INVITE_SENT_RESULT_EVENT;

typedef PREPACK struct {
    A_UINT8 sa[ATH_MAC_LEN];
    A_UINT16 wps_config_method;
    A_UINT8 dev_addr[ATH_MAC_LEN];
#define WPS_DEV_TYPE_LEN 8
#define WPS_MAX_DEVNAME_LEN 32
    A_UINT8 pri_dev_type[WPS_DEV_TYPE_LEN];
    A_UINT8 device_name[WPS_MAX_DEVNAME_LEN];
    A_UINT8 dev_name_len;
    A_UINT16 dev_config_methods;
    A_UINT8 device_capab;
    A_UINT8 group_capab;
} POSTPACK WMI_P2P_PROV_DISC_REQ_EVENT;

typedef PREPACK struct {
    A_UINT8 peer[ATH_MAC_LEN];
    A_UINT16 config_methods;
} POSTPACK WMI_P2P_PROV_DISC_RESP_EVENT;

typedef PREPACK struct {
    A_UINT8 type;
    A_UINT8 transaction_status;
    A_UINT8 dialog_token;
    A_UINT8 frag_id;
    A_UINT8 peer_addr[ATH_MAC_LEN];
    A_UINT16 freq;
    A_UINT16 status_code;
    A_UINT16 comeback_delay;
    A_UINT16 tlv_length;
    A_UINT16 update_indic;
    //  Variable length TLV will be placed after the event
} POSTPACK WMI_P2P_SDPD_RX_EVENT;

typedef PREPACK struct {
    A_CHAR wps_pin[WPS_PIN_LEN];
    A_UINT8 peer_addr[ATH_MAC_LEN];
    A_UINT8 wps_role;
}POSTPACK WMI_P2P_PROV_INFO;

typedef PREPACK struct {
    WPS_CREDENTIAL  credential;
}POSTPACK WMI_P2P_PERSISTENT_PROFILE_CMD;

#define FILL  "fillme"
#endif


#if ENABLE_P2P_MODE

typedef PREPACK struct {
    A_UINT8 dev_name[WPS_MAX_DEVNAME_LEN] FIELD_PACKED;
    A_UINT8 dev_name_len FIELD_PACKED;
}POSTPACK WMI_P2P_SET_DEV_NAME;

typedef PREPACK struct {
    A_UINT8 config_id;    /* set to one of WMI_P2P_CONF_ID */
    PREPACK union {
        WMI_P2P_LISTEN_CHANNEL listen_ch;
        WMI_P2P_SET_CROSS_CONNECT cross_conn;
        WMI_P2P_SET_SSID_POSTFIX ssid_postfix;
        WMI_P2P_SET_INTRA_BSS intra_bss;
        WMI_P2P_SET_CONCURRENT_MODE concurrent_mode;
        WMI_P2P_SET_GO_INTENT go_intent;
        WMI_P2P_SET_DEV_NAME device_name;
        WMI_P2P_SET_MODE mode;
    }POSTPACK val;
}POSTPACK WMI_P2P_SET_CMD;

typedef PREPACK struct {
    A_UINT32 duration FIELD_PACKED;
    A_UINT32 interval FIELD_PACKED;
    A_UINT32 start_or_offset FIELD_PACKED;
    A_UINT8 count_or_type FIELD_PACKED;
} POSTPACK P2P_NOA_DESCRIPTOR;

typedef PREPACK struct {
    A_UINT8 enable FIELD_PACKED;
    A_UINT8 count FIELD_PACKED;
    A_UINT8 noas[1] FIELD_PACKED;
} POSTPACK WMI_NOA_INFO_STRUCT;

typedef PREPACK struct {
    A_UINT8 enable;
    A_UINT8 ctwin;
} POSTPACK WMI_OPPPS_INFO_STRUCT;

#endif

typedef PREPACK struct {
    A_BOOL  enable FIELD_PACKED;
    A_UINT8 nextProbeCount FIELD_PACKED;
    A_UINT8 maxBackOff FIELD_PACKED;
    A_UINT8 minGtxRssi FIELD_PACKED;
    A_UINT8 forceBackOff FIELD_PACKED;
} POSTPACK WMI_GREENTX_PARAMS_CMD;

/* LPL commands */
typedef PREPACK struct  {
    A_UINT8 lplPolicy FIELD_PACKED;          /*0 - force off, 1 force on, 2 dynamic*/
    A_UINT8 noBlockerDetect FIELD_PACKED;    /*don't do blocker detection if lpl policy is set
                                              to dynamic*/
    A_UINT8 noRfbDetect FIELD_PACKED;        /*don't do rate fall back  detection if lpl policy is set
                                              to dynamic*/
    A_UINT8 rsvd FIELD_PACKED;
} POSTPACK  WMI_LPL_FORCE_ENABLE_CMD;

#ifndef ATH_TARGET
#include "athendpack.h"
#endif

#ifdef __cplusplus
}
#endif

#endif /* _WMI_H_ */
