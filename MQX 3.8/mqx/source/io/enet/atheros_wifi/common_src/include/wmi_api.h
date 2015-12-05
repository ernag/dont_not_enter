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
#ifndef _WMI_API_H_
#define _WMI_API_H_

#ifdef __cplusplus
extern "C" {
#endif

    /* WMI converts a dix frame with an ethernet payload (up to 1500 bytes) 
     * to an 802.3 frame (adds SNAP header) and adds on a WMI data header */
#define WMI_MAX_TX_DATA_FRAME_LENGTH (1500 + sizeof(WMI_DATA_HDR) + WMI_MAX_TX_META_SZ + sizeof(ATH_MAC_HDR) + sizeof(ATH_LLC_SNAP_HDR))

    /* A normal WMI data frame */
#define WMI_MAX_NORMAL_RX_DATA_FRAME_LENGTH (1500 + HTC_HEADER_LEN + sizeof(WMI_DATA_HDR) + WMI_MAX_RX_META_SZ + sizeof(ATH_MAC_HDR) + sizeof(ATH_LLC_SNAP_HDR))
    
    /* An AMSDU frame */
#define WMI_MAX_AMSDU_RX_DATA_FRAME_LENGTH  (4096 + sizeof(WMI_DATA_HDR) + sizeof(ATH_MAC_HDR) + sizeof(ATH_LLC_SNAP_HDR))

/*
 * IP QoS Field definitions according to 802.1p
 */
#define BEST_EFFORT_PRI         0
#define BACKGROUND_PRI          1
#define EXCELLENT_EFFORT_PRI    3
#define CONTROLLED_LOAD_PRI     4
#define VIDEO_PRI               5
#define VOICE_PRI               6
#define NETWORK_CONTROL_PRI     7
#define MAX_NUM_PRI             8

#define UNDEFINED_PRI           (0xff)

#define WMI_IMPLICIT_PSTREAM_INACTIVITY_INT 5000 /* 5 seconds */

#define A_ROUND_UP(x, y)  ((((x) + ((y) - 1)) / (y)) * (y))

typedef enum {
    ATHEROS_COMPLIANCE = 0x1
}TSPEC_PARAM_COMPLIANCE;

struct wmi_t;

void *wmi_init(void *devt);

void wmi_qos_state_init(struct wmi_t *wmip);
void wmi_shutdown(struct wmi_t *wmip);
HTC_ENDPOINT_ID wmi_get_control_ep(struct wmi_t * wmip);
void wmi_set_control_ep(struct wmi_t * wmip, HTC_ENDPOINT_ID eid);
A_STATUS wmi_dix_2_dot3(struct wmi_t *wmip, void *osbuf);
A_STATUS wmi_meta_add(struct wmi_t *wmip, void *osbuf, A_UINT8 *pVersion,void *pTxMetaS);
A_STATUS wmi_data_hdr_add(struct wmi_t *wmip, void *osbuf, A_UINT8 msgType, A_BOOL bMoreData, WMI_DATA_HDR_DATA_TYPE data_type,A_UINT8 metaVersion, void *pTxMetaS);
A_STATUS wmi_dot3_2_dix(void *osbuf);

A_STATUS wmi_dot11_hdr_remove (struct wmi_t *wmip, void *osbuf);
A_STATUS wmi_dot11_hdr_add(struct wmi_t *wmip, void *osbuf, NETWORK_TYPE mode);

A_STATUS wmi_data_hdr_remove(struct wmi_t *wmip, void *osbuf);

A_UINT8 wmi_implicit_create_pstream(struct wmi_t *wmip, void *osbuf, A_UINT32 layer2Priority, A_BOOL wmmEnabled);

A_STATUS wmi_control_rx(struct wmi_t *wmip, void *osbuf);



typedef enum {
    NO_SYNC_WMIFLAG = 0,
    SYNC_BEFORE_WMIFLAG,            /* transmit all queued data before cmd */
    SYNC_AFTER_WMIFLAG,             /* any new data waits until cmd execs */
    SYNC_BOTH_WMIFLAG,
    END_WMIFLAG                     /* end marker */
} WMI_SYNC_FLAG;


A_STATUS
wmi_cmd_start(struct wmi_t *wmip, A_CONST A_VOID *pInput, WMI_COMMAND_ID cmdID, A_UINT16 buffsize);

A_STATUS wmi_cmd_send(struct wmi_t *wmip, void *osbuf, WMI_COMMAND_ID cmdId,
                      WMI_SYNC_FLAG flag);


A_STATUS wmi_bssfilter_cmd(struct wmi_t *wmip, A_UINT8 filter, A_UINT32 ieMask);

#if WLAN_CONFIG_ENABLE_WMI_SYNC
A_STATUS wmi_dataSync_send(struct wmi_t *wmip, void *osbuf, HTC_ENDPOINT_ID eid);
#endif



A_STATUS
wmi_storerecall_recall_cmd(struct wmi_t *wmip, A_UINT32 length, void* pData);


A_STATUS wmi_socket_cmd(struct wmi_t *wmip, A_UINT32 cmd_type, void* pData,A_UINT32 length);

#if ENABLE_P2P_MODE
void
wmi_save_key_info(WMI_P2P_PROV_INFO *p2p_info);

A_STATUS
wmi_p2p_discover(struct wmi_t *wmip, WMI_P2P_FIND_CMD *find_param);

A_STATUS
wmi_wps_credential_tx(struct wmi_t *wmip, WMI_P2P_PERSISTENT_PROFILE_CMD *cred_param);

A_STATUS
wmi_p2p_join_profile_cmd(struct wmi_t *wmip, WMI_P2P_CONNECT_CMD *connect_param);

A_STATUS
wmi_p2p_connect(struct wmi_t *wmip, WMI_P2P_CONNECT_CMD *connect_param);

A_STATUS
wmi_p2p_auth(struct wmi_t *wmip, WMI_P2P_CONNECT_CMD *auth_param);

A_STATUS
wmi_p2p_stop_find(struct wmi_t *wmip);

A_STATUS
wmi_p2p_cancel(struct wmi_t *wmip);

A_STATUS
wmi_p2p_node_list(struct wmi_t *wmip);

A_STATUS
wmi_p2p_listen(struct wmi_t *wmip, A_UINT32 timeout);

A_STATUS
wmi_p2p_go_neg_start(struct wmi_t *wmip, WMI_P2P_GO_NEG_START_CMD *go_param);

A_STATUS
wmi_p2p_set_config(struct wmi_t *wmip, WMI_P2P_FW_SET_CONFIG_CMD *config);

A_STATUS
wmi_p2p_set_cmd(struct wmi_t *wmip, WMI_P2P_SET_CMD *buf);

void
p2p_go_neg_complete_rx(void *ctx, const A_UINT8 *datap, A_UINT8 len);

A_STATUS
wmi_wps_set_config(struct wmi_t *wmip, WMI_WPS_SET_CONFIG_CMD *wps_config);

A_STATUS
wmi_p2p_prov_disc_req_cmd(struct wmi_t *wmip,WMI_P2P_FW_PROV_DISC_REQ_CMD *buf);

A_STATUS
wmi_p2p_invite_cmd(struct wmi_t *wmip, WMI_P2P_INVITE_CMD *buf);

A_STATUS
wmi_p2p_grp_init_cmd(struct wmi_t *wmip, WMI_P2P_GRP_INIT_CMD *buf);

A_STATUS
wmi_p2p_set_profile(struct wmi_t *wmip, WMI_P2P_SET_PROFILE_CMD *buf);

A_STATUS
wmi_p2p_set_noa(struct wmi_t *wmip, WMI_NOA_INFO_STRUCT *buf);

A_STATUS
wmi_p2p_set_oppps(struct wmi_t *wmip, WMI_OPPPS_INFO_STRUCT *pOpp);

A_STATUS wmi_p2p_invite_req_rsp_cmd(struct wmi_t *wmip,WMI_P2P_FW_INVITE_REQ_RSP_CMD *buf);
A_STATUS wmi_sdpd_send_cmd(struct wmi_t *wmip, WMI_P2P_SDPD_TX_CMD *buf);
#endif

#if ENABLE_AP_MODE
A_STATUS
wmi_ap_set_param(struct wmi_t *wmip, A_VOID *data);
#endif


#ifdef __cplusplus
}
#endif

#endif /* _WMI_API_H_ */
