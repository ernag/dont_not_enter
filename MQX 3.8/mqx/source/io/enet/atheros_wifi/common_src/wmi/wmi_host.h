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
#ifndef _WMI_HOST_H_
#define _WMI_HOST_H_

#ifdef __cplusplus
extern "C" {
#endif

struct wmi_stats {
    A_UINT32    cmd_len_err;
    A_UINT32    cmd_id_err;
};


#define BEACON_PERIOD_IE_INDEX 8
#define CAPS_IE_INDEX 10
#define IE_INDEX 12

/*
 * These constants are used with A_WLAN_BAND_SET.
 */
#define A_BAND_24GHZ           0
#define A_BAND_5GHZ            1
#define A_NUM_BANDS            2

struct wmi_t {
    A_BOOL                          wmi_numQoSStream;
    A_UINT16                        wmi_streamExistsForAC[WMM_NUM_AC];
    A_UINT8                         wmi_fatPipeExists;
    void                           *wmi_devt;
    struct wmi_stats                wmi_stats;
    A_MUTEX_T                       wmi_lock;
    HTC_ENDPOINT_ID                wmi_endpoint_id;
    void                            *persistentProfile;
#if ENABLE_P2P_MODE
    WMI_P2P_PERSISTENT_PROFILE_CMD	persistentNode;
    WMI_SET_PASSPHRASE_CMD  		apPassPhrase;
#endif

};

#define LOCK_WMI(w)     A_MUTEX_LOCK(&(w)->wmi_lock);
#define UNLOCK_WMI(w)   A_MUTEX_UNLOCK(&(w)->wmi_lock);

#ifdef __cplusplus
}
#endif

#endif /* _WMI_HOST_H_ */
