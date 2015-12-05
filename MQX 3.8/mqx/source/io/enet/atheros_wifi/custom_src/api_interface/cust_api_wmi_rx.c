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
#include <a_config.h>
#include <a_types.h>
#include <a_osapi.h>
#include <common_api.h>
#include <custom_api.h>
#include <driver_cxt.h>
#include <wmi_host.h>
#include "mqx.h"
#include "bsp.h"
#include "enet.h"
#include "enetprv.h"
#include "enet_wifi.h"

#if ENABLE_P2P_MODE
#include "p2p.h"
#include "wmi.h"
#include "wmi_api.h"
#include "wmi_host.h"
#endif

#include <atheros_wifi_api.h>

extern A_CONST WMI_SCAN_PARAMS_CMD default_scan_param;
extern A_CONST A_UINT8 max_performance_power_param;

#if 1
static A_CONST A_INT32 wmi_rateTable[][2] = {
  //{W/O SGI, with SGI}
    {1000, 1000},
    {2000, 2000},
    {5500, 5500},
    {11000, 11000},
    {6000, 6000},
    {9000, 9000},
    {12000, 12000},
    {18000, 18000},
    {24000, 24000},
    {36000, 36000},
    {48000, 48000},
    {54000, 54000},
    {6500, 7200},
    {13000, 14400},
    {19500, 21700},
    {26000, 28900},
    {39000, 43300},
    {52000, 57800},
    {58500, 65000},
    {65000, 72200},
    {13500,  15000},
    {27500,  30000},
    {40500,  45000},
    {54000,  60000},
    {81500,  90000},
    {108000, 120000},
    {121500, 135000},
    {135000, 150000}
    };
#endif

static A_VOID
fill_scan_info(A_VOID *pCxt, ENET_SCAN_INFO *pScanInfo, WMI_BSS_INFO_HDR *bih, A_INT32 len)
{
	A_SCAN_SUMMARY scan_summary;


	if(A_OK == Api_ParseInfoElem(pCxt, bih, len, &scan_summary)){
		pScanInfo->channel = scan_summary.channel;
		pScanInfo->rssi = scan_summary.rssi;
		A_MEMCPY(pScanInfo->bssid, scan_summary.bssid, ATH_MAC_LEN);
		pScanInfo->ssid_len = scan_summary.ssid_len;

		A_MEMCPY(pScanInfo->ssid, &(scan_summary.ssid[0]), scan_summary.ssid_len);
		pScanInfo->security_enabled = (A_UINT8)((scan_summary.caps & IEEE80211_CAPINFO_PRIVACY)? 1:0);
		pScanInfo->preamble = (A_UINT8)((scan_summary.caps & IEEE80211_CAPINFO_SHORT_PREAMBLE)? 1:0);

		if((scan_summary.caps & (IEEE80211_CAPINFO_ESS|IEEE80211_CAPINFO_IBSS)) == IEEE80211_CAPINFO_ESS){
	    	pScanInfo->bss_type = ENET_MEDIACTL_MODE_INFRA;
	    }else if((scan_summary.caps & (IEEE80211_CAPINFO_ESS|IEEE80211_CAPINFO_IBSS)) == IEEE80211_CAPINFO_IBSS){
	    	pScanInfo->bss_type = ENET_MEDIACTL_MODE_ADHOC;
	    }else{
	    	/* error condition report it as ad-hoc in this case*/
	    	pScanInfo->bss_type = ENET_MEDIACTL_MODE_ADHOC;
	    }

	    pScanInfo->beacon_period = scan_summary.beacon_period;
	}
}

static A_VOID
fill_ext_scan_info(A_VOID *pCxt, ATH_SCAN_EXT *pExtScanInfo, WMI_BSS_INFO_HDR *bih, A_INT32 len)
{
	A_SCAN_SUMMARY scan_summary;


	if(A_OK == Api_ParseInfoElem(pCxt, bih, len, &scan_summary)){
		A_MEMZERO(pExtScanInfo, sizeof(ATH_SCAN_EXT));
		pExtScanInfo->channel = scan_summary.channel;
		pExtScanInfo->rssi = scan_summary.rssi;
		A_MEMCPY(pExtScanInfo->bssid, scan_summary.bssid, ATH_MAC_LEN);
		pExtScanInfo->ssid_len = scan_summary.ssid_len;

		A_MEMCPY(pExtScanInfo->ssid, &(scan_summary.ssid[0]), scan_summary.ssid_len);
		pExtScanInfo->security_enabled = (A_UINT8)((scan_summary.caps & IEEE80211_CAPINFO_PRIVACY)? 1:0);
		pExtScanInfo->preamble = (A_UINT8)((scan_summary.caps & IEEE80211_CAPINFO_SHORT_PREAMBLE)? 1:0);

		if((scan_summary.caps & (IEEE80211_CAPINFO_ESS|IEEE80211_CAPINFO_IBSS)) == IEEE80211_CAPINFO_ESS){
	    	pExtScanInfo->bss_type = ENET_MEDIACTL_MODE_INFRA;
	    }else if((scan_summary.caps & (IEEE80211_CAPINFO_ESS|IEEE80211_CAPINFO_IBSS)) == IEEE80211_CAPINFO_IBSS){
	    	pExtScanInfo->bss_type = ENET_MEDIACTL_MODE_ADHOC;
	    }else{
	    	/* error condition report it as ad-hoc in this case*/
	    	pExtScanInfo->bss_type = ENET_MEDIACTL_MODE_ADHOC;
	    }

	    pExtScanInfo->beacon_period = scan_summary.beacon_period;

	    if(scan_summary.rsn_cipher & TKIP_CRYPT){
	    	pExtScanInfo->rsn_cipher |= ATH_CIPHER_TYPE_TKIP;
	    }

	    if(scan_summary.rsn_cipher & AES_CRYPT){
	    	pExtScanInfo->rsn_cipher |= ATH_CIPHER_TYPE_CCMP;
	    }

	    if(scan_summary.rsn_cipher & WEP_CRYPT){
	    	pExtScanInfo->rsn_cipher |= ATH_CIPHER_TYPE_WEP;
	    }

	    if(scan_summary.wpa_cipher & TKIP_CRYPT){
	    	pExtScanInfo->wpa_cipher |= ATH_CIPHER_TYPE_TKIP;
	    }

	    if(scan_summary.wpa_cipher & AES_CRYPT){
	    	pExtScanInfo->wpa_cipher |= ATH_CIPHER_TYPE_CCMP;
	    }

	    if(scan_summary.wpa_cipher & WEP_CRYPT){
	    	pExtScanInfo->wpa_cipher |= ATH_CIPHER_TYPE_WEP;
	    }

	    if(scan_summary.rsn_auth & WPA2_AUTH){
	    	pExtScanInfo->rsn_auth |= SECURITY_AUTH_1X;
	    }

	    if(scan_summary.rsn_auth & WPA2_PSK_AUTH){
	    	pExtScanInfo->rsn_auth |= SECURITY_AUTH_PSK;
	    }

	    if(scan_summary.wpa_auth & WPA_AUTH){
	    	pExtScanInfo->wpa_auth |= SECURITY_AUTH_1X;
	    }

	    if(scan_summary.wpa_auth & WPA_PSK_AUTH){
	    	pExtScanInfo->wpa_auth |= SECURITY_AUTH_PSK;
	    }
	}
}

A_VOID
Custom_Api_BssInfoEvent(A_VOID *pCxt, A_UINT8 *datap, A_INT32 len)
{
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    WMI_BSS_INFO_HDR *bih = (WMI_BSS_INFO_HDR *)datap;
	ENET_SCAN_INFO_PTR pScanInfo;
	ATH_SCAN_EXT *pExtScanInfo;
	A_UINT8 i,worst_snr_idx;
	A_UINT8 worst_snr = 0xff;
    A_UINT16 scanCount;

	/* add/replace entry in application scan results */
	if(GET_DRIVER_CXT(pCxt)->extended_scan){
		pExtScanInfo = (ATH_SCAN_EXT*)(GET_DRIVER_CXT(pCxt)->pScanOut);
		scanCount = GET_DRIVER_CXT(pCxt)->scanOutCount;
		//look for previous match
    	for(i=0 ; i<scanCount ; i++){
    		if(0==A_MEMCMP(bih->bssid, pExtScanInfo[i].bssid, ATH_MAC_LEN)){
    			fill_ext_scan_info(pCxt, &pExtScanInfo[i], bih, len);
    			break;
    		}
    		/* keep worst rssi entry for optional use below */
    		if(worst_snr > pExtScanInfo[i].rssi){
    			worst_snr = pExtScanInfo[i].rssi;
    			worst_snr_idx = i;
    		}
    	}

    	if(i >= scanCount){
    		if(GET_DRIVER_CXT(pCxt)->scanOutSize <= scanCount){
    			/* replace other non-matching entry based on rssi */
    			if(bih->snr > worst_snr){
    				fill_ext_scan_info(pCxt, &pExtScanInfo[worst_snr_idx], bih, len);
    			}
    		}else{
    			/* populate new entry */
    			fill_ext_scan_info(pCxt, &pExtScanInfo[scanCount], bih, len);
    			scanCount++;
                GET_DRIVER_CXT(pCxt)->scanOutCount = scanCount;
    		}
    	}
	}else{
    	pScanInfo = (ENET_SCAN_INFO_PTR)(GET_DRIVER_CXT(pCxt)->pScanOut);
        scanCount = GET_DRIVER_CXT(pCxt)->scanOutCount;
    	//look for previous match
    	for(i=0 ; i<scanCount ; i++){
    		if(0==A_MEMCMP(bih->bssid, pScanInfo[i].bssid, ATH_MAC_LEN)){
    			fill_scan_info(pCxt, &pScanInfo[i], bih, len);
    			break;
    		}
    		/* keep worst rssi entry for optional use below */
    		if(worst_snr > pScanInfo[i].rssi){
    			worst_snr = pScanInfo[i].rssi;
    			worst_snr_idx = i;
    		}
    	}

    	if(i >= scanCount){
    		if(GET_DRIVER_CXT(pCxt)->scanOutSize <= scanCount){
    			/* replace other non-matching entry based on rssi */
    			if(bih->snr > worst_snr){
    				fill_scan_info(pCxt, &pScanInfo[worst_snr_idx], bih, len);
    			}
    		}else{
    			/* populate new entry */
    			fill_scan_info(pCxt, &pScanInfo[scanCount], bih, len);
    			scanCount++;
                GET_DRIVER_CXT(pCxt)->scanOutCount = scanCount;
    		}
    	}
    }
}

A_VOID
Custom_Api_ConnectEvent(A_VOID *pCxt)
{
	ATH_CONNECT_CB cb = NULL;

    /* Update connect & link status atomically */
    DRIVER_SHARED_RESOURCE_ACCESS_ACQUIRE(pCxt);
    {
        if(GET_DRIVER_CXT(pCxt)->connectStateCB != NULL){
		    cb = (ATH_CONNECT_CB)GET_DRIVER_CXT(pCxt)->connectStateCB;
		    /* call this later from outside the spinlock */
	    }
    }
    DRIVER_SHARED_RESOURCE_ACCESS_RELEASE(pCxt);

    CUSTOM_DRIVER_WAKE_USER(pCxt);

    /* call the callback function provided by application to
     * indicate connection state */
	if(cb != NULL){
		cb(A_TRUE);
	}
}


A_VOID
Custom_Api_DisconnectEvent(A_VOID *pCxt, A_UINT8 reason, A_UINT8 *bssid,
                        A_UINT8 assocRespLen, A_UINT8 *assocInfo, A_UINT16 protocolReasonStatus)
{
    ATH_CONNECT_CB cb = NULL;


    /* Update connect & link status atomically */
    DRIVER_SHARED_RESOURCE_ACCESS_ACQUIRE(pCxt);
    if(GET_DRIVER_CXT(pCxt)->connectStateCB != NULL){
		cb = (ATH_CONNECT_CB)GET_DRIVER_CXT(pCxt)->connectStateCB;
		/* call this later from outside the spinlock */
	}
    DRIVER_SHARED_RESOURCE_ACCESS_RELEASE(pCxt);
	/* call the callback function provided by application to
     * indicate connection state */
	if(cb != NULL){
		if(reason == INVALID_PROFILE)
		{
			cb(INVALID_PROFILE);
		}
		else
		{
		cb(A_FALSE);
	}
}
}


A_VOID
Custom_Api_ReadyEvent(A_VOID *pCxt, A_UINT8 *datap, A_UINT8 phyCap, A_UINT32 sw_ver, A_UINT32 abi_ver)
{
    ENET_CONTEXT_STRUCT_PTR enet_ptr = (ENET_CONTEXT_STRUCT_PTR)GET_DRIVER_CXT(pCxt)->pUpperCxt;
    /* this custom implementation sets an event after setting CXT_WMI_READY
     * so as to allow the blocked user thread to wake up. */
    A_MEMCPY(enet_ptr->ADDRESS, datap, ATH_MAC_LEN);
    CUSTOM_DRIVER_WAKE_USER(pCxt);
    A_UNUSED_ARGUMENT(phyCap);
    A_UNUSED_ARGUMENT(sw_ver);
    A_UNUSED_ARGUMENT(abi_ver);
}

A_VOID
Custom_Api_RSNASuccessEvent(A_VOID *pCxt, A_UINT8 code)
{

	 ATH_CONNECT_CB  cb = NULL;
	/* this is the event that the customer has to use to send a callback
	 * to the application so that it will print the success event */

    /* get the callback handler from the device context */
    DRIVER_SHARED_RESOURCE_ACCESS_ACQUIRE(pCxt);
    if(GET_DRIVER_CXT(pCxt)->connectStateCB != NULL){
		cb = (ATH_CONNECT_CB)GET_DRIVER_CXT(pCxt)->connectStateCB;
		/* call this later from outside the spinlock */
	}
    DRIVER_SHARED_RESOURCE_ACCESS_RELEASE(pCxt);
	/* call the callback function provided by application to
     * indicate 4 way handshake status */
	if(cb != NULL){

		cb(code);
	}

}


A_VOID
Custom_Api_BitRateEvent_tx(A_VOID *pCxt,A_INT8 rateIndex)
{
	 ATH_CONNECT_CB  cb = NULL;
	/* the driver will get the index of the last tx rate from chip
	 * based on this index we get the rate tx from the array */

    /* get the callback handler from the device context */
    DRIVER_SHARED_RESOURCE_ACCESS_ACQUIRE(pCxt);
    if(GET_DRIVER_CXT(pCxt)->connectStateCB != NULL){
		cb = (ATH_CONNECT_CB)GET_DRIVER_CXT(pCxt)->connectStateCB;
		/* call this later from outside the spinlock */
	}
    DRIVER_SHARED_RESOURCE_ACCESS_RELEASE(pCxt);
	/* call the callback function provided by application to
     * indicate last transmitted rate */

	if(cb != NULL){

		cb(wmi_rateTable[rateIndex][0]);
	}
}

#if ENABLE_P2P_MODE
A_VOID
Custom_Api_p2p_go_neg_event(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len,WMI_P2P_PROV_INFO *wps_info)
{
    WMI_P2P_GO_NEG_RESULT_EVENT *ev;
    void *wmip;
    WMI_WPS_START_CMD p;
    WMI_P2P_GRP_INIT_CMD p2pgo;
    WMI_CONNECT_CMD ap;
    WMI_SET_PASSPHRASE_CMD pp;
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
	WMI_SCAN_PARAMS_CMD scan_param_cmd;

    ev = (WMI_P2P_GO_NEG_RESULT_EVENT *)datap;
    wmip = (struct wmi_t *)pCxt;


	printf(" role_go : %d \n", ev->role_go);

 	A_MEMCPY(((struct wmi_t *)wmip)->persistentNode.credential.mac_addr, ev->peer_device_addr, ATH_MAC_LEN);
	A_MEMCPY(((struct wmi_t *)wmip)->persistentNode.credential.ssid,ev->ssid,ev->ssid_len);
	((struct wmi_t *)wmip)->persistentNode.credential.ssid_len = ev->ssid_len;
	((struct wmi_t *)wmip)->persistentNode.credential.ap_channel = ev->freq;

    if((ev->status == 0) && (ev->role_go == 0))
    {

        A_MEMZERO(&p, sizeof(p));

        p.role = WPS_ENROLLEE_ROLE;
        if (ev->wps_method == WPS_PBC)
        {
            p.config_mode = WPS_PBC_MODE;
        }
        else
        {
            p.config_mode = WPS_PIN_MODE;
            A_MEMCPY(p.wps_pin.pin,wps_info->wps_pin, P2P_WPS_PIN_LEN);
            p.wps_pin.pin_length = 8;
        }

        p.ssid_info.channel = A_LE2CPU16(ev->freq);
        p.ssid_info.ssid_len = ev->ssid_len;
        A_MEMCPY(p.ssid_info.ssid,ev->ssid,ev->ssid_len);
        A_MEMCPY(p.ssid_info.macaddress,ev->peer_interface_addr,ATH_MAC_LEN);

        p.timeout = 30;
        p.ctl_flag = 1;
		wmi_cmd_start(wmip, &max_performance_power_param, WMI_SET_POWER_MODE_CMDID, sizeof(WMI_POWER_MODE_CMD));
        /* prevent background scan during WPS */
		A_MEMCPY(&scan_param_cmd, &default_scan_param, sizeof(WMI_SCAN_PARAMS_CMD));		   
		scan_param_cmd.bg_period        = A_CPU2LE16(0xffff);		 
	    wmi_cmd_start(wmip, &scan_param_cmd, WMI_SET_SCAN_PARAMS_CMDID, sizeof(WMI_SCAN_PARAMS_CMD)); 

        if(A_OK != wmi_cmd_start(wmip, &p, WMI_WPS_START_CMDID, sizeof(WMI_WPS_START_CMD))){		

        }
    }
    else if((ev->status == 0) && (ev->role_go == 1))
    {
        A_MEMZERO(&p2pgo, sizeof(p2pgo));
        A_MEMZERO(&ap, sizeof(ap));
        A_MEMZERO(&p, sizeof(p));
        A_MEMZERO(&pp,sizeof(pp));

        p2pgo.group_formation  = 1;
        p2pgo.persistent_group = 1;


        A_MEMCPY(pp.passphrase,((struct wmi_t *)wmip)->apPassPhrase.passphrase, ((struct wmi_t *)wmip)->apPassPhrase.passphrase_len);
        pp.passphrase_len = ((struct wmi_t *)wmip)->apPassPhrase.passphrase_len;

        A_MEMCPY(pp.ssid,ev->ssid,ev->ssid_len);
        pp.ssid_len = ev->ssid_len;

        printf("ssid %s, passphrase %s \n",pp.ssid,pp.passphrase);
        A_MEMCPY (ap.ssid,ev->ssid,ev->ssid_len);

        ap.ssidLength          = ev->ssid_len;
        ap.channel             = ev->freq;
        ap.networkType         = AP_NETWORK;
        ap.dot11AuthMode       = OPEN_AUTH;
        ap.authMode            = WPA2_PSK_AUTH;
        ap.pairwiseCryptoType  = AES_CRYPT;
        ap.pairwiseCryptoLen   = AES_KEY_SIZE_BYTES;
        ap.groupCryptoType     = AES_CRYPT;
        ap.groupCryptoLen      = AES_KEY_SIZE_BYTES;
        ap.ctrl_flags          = A_CPU2LE32(CONNECT_DO_WPA_OFFLOAD | CONNECT_WPS_FLAG);

        if(A_OK != wmi_cmd_start(wmip, &pp, 
                        WMI_SET_PASSPHRASE_CMDID, sizeof(WMI_SET_PASSPHRASE_CMD))){			
			printf("\n P2P GO Init command Failed \n");
            return;
        }

        if(wmi_p2p_grp_init_cmd(wmip, &p2pgo) != A_OK)
        {
            return;
        }

        if(A_OK != wmi_cmd_start(wmip, (A_VOID*)&ap, WMI_AP_CONFIG_COMMIT_CMDID, sizeof(WMI_CONNECT_CMD)))       
        {
            return;
        }

        p.role = WPS_REGISTRAR_ROLE;

        if (ev->wps_method == WPS_PBC)
        {
            p.config_mode = WPS_PBC_MODE;
        }
        else
        {
            p.config_mode = WPS_PIN_MODE;
            A_MEMCPY(p.wps_pin.pin,wps_info->wps_pin, P2P_WPS_PIN_LEN);
            p.wps_pin.pin_length = 8;
        }
        p.ssid_info.channel    = ev->freq;
        p.ssid_info.ssid_len   = ev->ssid_len;
        A_MEMCPY(p.ssid_info.ssid,ev->ssid,ev->ssid_len);
        A_MEMCPY(p.ssid_info.macaddress,ev->peer_interface_addr,ATH_MAC_LEN);

        p.timeout  = 30;
        p.ctl_flag = 1;

		wmi_cmd_start(wmip, &max_performance_power_param, WMI_SET_POWER_MODE_CMDID, sizeof(WMI_POWER_MODE_CMD));

        if(A_OK != wmi_cmd_start(wmip, &p, WMI_WPS_START_CMDID, sizeof(WMI_WPS_START_CMD))){		
             return;	
        }
    }
    else
    {
        return;
    }
    return;
}


A_VOID
Custom_Api_p2p_node_list_event(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len)
{
    A_UINT8  index, temp_val, *temp_ptr = NULL;
    A_UINT16 config_method;
    temp_val = ((WMI_P2P_NODE_LIST_EVENT *)datap)->num_p2p_dev;
    temp_ptr = datap;
    datap += sizeof(A_UINT8);
    if (temp_val > 0)
    {
        for (index = 0; index < temp_val; index++)
        {
            config_method = A_LE2CPU16(((struct p2p_device *)datap)->config_methods);
            printf("\n p2p_device_name       : %s \n",                        ((struct p2p_device *)datap)->device_name);
            printf("\t p2p_primary_dev_type  : %s \n",                        ((struct p2p_device *)datap)->pri_dev_type);
            printf("\t p2p_interface_addr    : %x:%x:%x:%x:%x:%x \n",         ((struct p2p_device *)datap)->interface_addr[0],
                    ((struct p2p_device *)datap)->interface_addr[1],
                    ((struct p2p_device *)datap)->interface_addr[2],
                    ((struct p2p_device *)datap)->interface_addr[3],
                    ((struct p2p_device *)datap)->interface_addr[4],
                    ((struct p2p_device *)datap)->interface_addr[5]);
            printf("\t p2p_device_addr       : %x:%x:%x:%x:%x:%x \n",         ((struct p2p_device *)datap)->p2p_device_addr[0],
                    ((struct p2p_device *)datap)->p2p_device_addr[1],
                    ((struct p2p_device *)datap)->p2p_device_addr[2],
                    ((struct p2p_device *)datap)->p2p_device_addr[3],
                    ((struct p2p_device *)datap)->p2p_device_addr[4],
                    ((struct p2p_device *)datap)->p2p_device_addr[5]);
            printf("\t p2p_device_capability : %x \n",                        ((struct p2p_device *)datap)->dev_capab);
            printf("\t p2p_group_capability  : %x \n",                        ((struct p2p_device *)datap)->group_capab);
            printf("\t p2p_config_method     : %x \n",                        config_method);
            printf("\t p2p_wps_method        : %x \n",                        ((struct p2p_device *)datap)->wps_method);
            printf("\t Peer Listen channel   : %d \n",                        A_LE2CPU16(((struct p2p_device *)datap)->listen_freq));
            printf("\t Peer Oper   channel   : %d \n",                        A_LE2CPU16(((struct p2p_device *)datap)->oper_freq));


            datap += sizeof(struct p2p_device);
        }

    }
    datap = temp_ptr;

}



A_VOID
Custom_Api_p2p_req_auth_event(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len)
{
    printf("\n source addr : %x:%x:%x:%x:%x:%x \n",    ((WMI_P2P_REQ_TO_AUTH_EVENT *)datap)->sa[0],
                                                       ((WMI_P2P_REQ_TO_AUTH_EVENT *)datap)->sa[1],
                                                       ((WMI_P2P_REQ_TO_AUTH_EVENT *)datap)->sa[2],
                                                       ((WMI_P2P_REQ_TO_AUTH_EVENT *)datap)->sa[3],
                                                       ((WMI_P2P_REQ_TO_AUTH_EVENT *)datap)->sa[4],
                                                       ((WMI_P2P_REQ_TO_AUTH_EVENT *)datap)->sa[5]);

    printf("\n dev_password_id : %x \n",               ((WMI_P2P_REQ_TO_AUTH_EVENT *)datap)->dev_password_id);

}


A_VOID
Custom_Api_get_p2p_ctx(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len)
{
    printf("\n peer addr : %x:%x:%x:%x:%x:%x \n",      ((WMI_P2P_PROV_DISC_RESP_EVENT *)datap)->peer[0],
            ((WMI_P2P_PROV_DISC_RESP_EVENT *)datap)->peer[1],
            ((WMI_P2P_PROV_DISC_RESP_EVENT *)datap)->peer[2],
            ((WMI_P2P_PROV_DISC_RESP_EVENT *)datap)->peer[3],
            ((WMI_P2P_PROV_DISC_RESP_EVENT *)datap)->peer[4],
            ((WMI_P2P_PROV_DISC_RESP_EVENT *)datap)->peer[5]);

    printf("\n config_methods : %x \n",                A_LE2CPU16(((WMI_P2P_PROV_DISC_RESP_EVENT *)datap)->config_methods));
}


A_VOID
Custom_Api_p2p_prov_disc_req(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len)
{
    A_UINT16 wps_method;
    printf("\n source addr : %x:%x:%x:%x:%x:%x \n",    ((WMI_P2P_PROV_DISC_REQ_EVENT *)datap)->sa[0],
            ((WMI_P2P_PROV_DISC_REQ_EVENT *)datap)->sa[1],
            ((WMI_P2P_PROV_DISC_REQ_EVENT *)datap)->sa[2],
            ((WMI_P2P_PROV_DISC_REQ_EVENT *)datap)->sa[3],
            ((WMI_P2P_PROV_DISC_REQ_EVENT *)datap)->sa[4],
            ((WMI_P2P_PROV_DISC_REQ_EVENT *)datap)->sa[5]);

    printf("\n wps_config_method : %x \n",             A_LE2CPU16(((WMI_P2P_PROV_DISC_REQ_EVENT *)datap)->wps_config_method));

    wps_method = A_LE2CPU16(((WMI_P2P_PROV_DISC_REQ_EVENT *)datap)->wps_config_method);

    if(WPS_CONFIG_DISPLAY == wps_method)
    {
        printf("Random num: 48409797");
    }

}

A_VOID
Custom_Api_p2p_serv_disc_req(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len)
{
    void *wmip;
    wmip = (struct wmi_t *)pCxt;

	printf("Custom_Api_p2p_serv_disc_req event \n");
	printf("type : %d   frag id : %x \n", ((WMI_P2P_SDPD_RX_EVENT *)datap)->type, ((WMI_P2P_SDPD_RX_EVENT *)datap)->frag_id);
	printf("transaction_status : %x \n", ((WMI_P2P_SDPD_RX_EVENT *)datap)->transaction_status);
	printf("freq : %d status_code : %d comeback_delay : %d tlv_length : %d update_indic : %d \n",
												  A_LE2CPU16(((WMI_P2P_SDPD_RX_EVENT *)datap)->freq),
												  A_LE2CPU16(((WMI_P2P_SDPD_RX_EVENT *)datap)->status_code),
												  A_LE2CPU16(((WMI_P2P_SDPD_RX_EVENT *)datap)->comeback_delay),
												  A_LE2CPU16(((WMI_P2P_SDPD_RX_EVENT *)datap)->tlv_length),
												  A_LE2CPU16(((WMI_P2P_SDPD_RX_EVENT *)datap)->update_indic));
	printf("source addr : %x:%x:%x:%x:%x:%x \n", ((WMI_P2P_SDPD_RX_EVENT *)datap)->peer_addr[0],
												 ((WMI_P2P_SDPD_RX_EVENT *)datap)->peer_addr[1],
												 ((WMI_P2P_SDPD_RX_EVENT *)datap)->peer_addr[2],
												 ((WMI_P2P_SDPD_RX_EVENT *)datap)->peer_addr[3],
												 ((WMI_P2P_SDPD_RX_EVENT *)datap)->peer_addr[4],
												 ((WMI_P2P_SDPD_RX_EVENT *)datap)->peer_addr[5]);


}

A_VOID
Custom_Api_p2p_invite_req(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len)
{
    WMI_P2P_FW_INVITE_REQ_EVENT *pEv = (WMI_P2P_FW_INVITE_REQ_EVENT *)datap;
    int i,j;
    printf("Invitation Req From : \n");
    for (i=0;i<ATH_MAC_LEN;i++)
        printf(" [%x] ",pEv->go_dev_addr[i]);
}

A_VOID
Custom_Api_p2p_invite_rcvd_result(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len)
{
    int i,j;
    WMI_P2P_INVITE_RCVD_RESULT_EVENT *pEv = (WMI_P2P_INVITE_RCVD_RESULT_EVENT  *)datap;

    printf("Invite Result Status : %x \n",pEv->status);

    for (i=0;i<ATH_MAC_LEN;i++)
        printf(" [%x] ",pEv->sa[i]);
    printf("Start P2P Join Process to connect to P2P GO\n");

}

A_VOID
Custom_Api_p2p_invite_send_result(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len)
{
    A_DRIVER_CONTEXT *pDCxt = GET_DRIVER_COMMON(pCxt);
    struct wmi_t *pWmi = (struct wmi_t *)pCxt;
    WMI_P2P_INVITE_SENT_RESULT_EVENT *invSendResult = (WMI_P2P_INVITE_SENT_RESULT_EVENT *)datap;
    WMI_P2P_PERSISTENT_PROFILE_CMD  *persistentProfile = (WMI_P2P_PERSISTENT_PROFILE_CMD *) pWmi->persistentProfile;

    printf("WPS Credentials Invite result \n");

    printf("SSID %s \n",persistentProfile->credential.ssid);
    printf("KEY  %s \n",persistentProfile->credential.key);
    printf("KEY Len %d \n",persistentProfile->credential.key_len);

    wmi_wps_credential_tx(pDCxt->pWmiCxt, (WMI_P2P_PERSISTENT_PROFILE_CMD *)pWmi->persistentProfile);
    printf("Invitation Result %d\n",invSendResult->status);
}

A_VOID
Custom_Api_wps_profile_event_rx(A_VOID *pCxt, A_UINT8 *datap, A_INT32 len, A_VOID *pReq)
{
    WMI_WPS_PROFILE_EVENT *pEv = (WMI_WPS_PROFILE_EVENT *)datap;
    struct wmi_t *pWmi = (struct wmi_t *)pCxt;

    /* TODO: flag it for only P2P Enabled case !*/
    if(pEv->status == WPS_STATUS_FAILURE){
       wmi_p2p_cancel(pWmi);
    }

}
#endif
#if MANUFACTURING_SUPPORT
A_VOID
Custom_Api_Test_Cmd_Event(A_VOID *pCxt, A_UINT8 *datap, A_UINT32 len)
{
  A_UNUSED_ARGUMENT(len);
  A_MEMCPY(GET_DRIVER_CXT(pCxt)->pScanOut,datap,len);
  GET_DRIVER_CXT(pCxt)->testCmdRespBufLen = len;
}
#endif
/* EOF */
