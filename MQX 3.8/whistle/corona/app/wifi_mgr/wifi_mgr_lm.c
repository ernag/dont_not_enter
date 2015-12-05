/*
 * wifi_mgr_lm.c
 *
 *  Created on: Aug 20, 2013
 *      Author: Kevin
 */

#include <custom_stack_offload.h>
#include <athdefs.h>
#include <mqx.h>
#include "wassert.h"
#include "ar4100p_main.h"
#include "wifi_mgr.h"
#include "app_errors.h"
#include "whistlemessage.pb.h"
#include "pb_helper.h"

err_t WIFIMGR_lmm_network_encode_w_sized_strings_to_security_param(LmWiFiNetwork *pLmWiFiNetworkIn, security_parameters_t *pSecParamOut)
{
    WMP_sized_string_cb_args_t *sized_str_arg;
    if (!pLmWiFiNetworkIn->has_type)
        return ERR_GENERIC;

    switch(pLmWiFiNetworkIn->type)
    {
        case LmWiFiNetworkType_WIFI_NETWORK_TYPE_WPA:
        case LmWiFiNetworkType_WIFI_NETWORK_TYPE_WPA2:
            pSecParamOut->type = SEC_MODE_WPA;
            if (pLmWiFiNetworkIn->type == LmWiFiNetworkType_WIFI_NETWORK_TYPE_WPA)
                pSecParamOut->parameters.wpa_parameters.version = (char*) WPA_PARAM_VERSION_WPA;
            else if (pLmWiFiNetworkIn->type == LmWiFiNetworkType_WIFI_NETWORK_TYPE_WPA2)
                pSecParamOut->parameters.wpa_parameters.version = (char*) WPA_PARAM_VERSION_WPA2;

            if (!pLmWiFiNetworkIn->has_uCipher)
                return ERR_GENERIC;
            switch(pLmWiFiNetworkIn->uCipher)
            {
                case LmWiFiNetworkuCipher_WIFI_NETWORK_UCIPHER_TKIP:
                    pSecParamOut->parameters.wpa_parameters.ucipher = ATH_CIPHER_TYPE_TKIP; 
                    break;
                case LmWiFiNetworkuCipher_WIFI_NETWORK_UCIPHER_CCMP:
                    pSecParamOut->parameters.wpa_parameters.ucipher = ATH_CIPHER_TYPE_CCMP; 
                    break;
                default:
                    return ERR_GENERIC;
            }
            
            if (!pLmWiFiNetworkIn->has_mCipher)
                return ERR_GENERIC;
            switch(pLmWiFiNetworkIn->mCipher)
            {
                case LmWiFiNetworkmCipher_WIFI_NETWORK_MCIPHER_TKIP:
                    pSecParamOut->parameters.wpa_parameters.mcipher = TKIP_CRYPT; 
                    break;
                case LmWiFiNetworkmCipher_WIFI_NETWORK_MCIPHER_AES:
                    pSecParamOut->parameters.wpa_parameters.mcipher = AES_CRYPT; 
                    break;
                default:
                    return ERR_GENERIC;
            }

            sized_str_arg = pLmWiFiNetworkIn->password.arg;
            if (!sized_str_arg->exists)
                return ERR_GENERIC;
            pSecParamOut->parameters.wpa_parameters.passphrase = sized_str_arg->pPayload;
            
            break;
        case LmWiFiNetworkType_WIFI_NETWORK_TYPE_WEP:
            pSecParamOut->type = SEC_MODE_WEP;

            sized_str_arg = pLmWiFiNetworkIn->password.arg;
            if (!sized_str_arg->exists)
                return ERR_GENERIC;
            pSecParamOut->parameters.wep_parameters.key[0] = sized_str_arg->pPayload;
            pSecParamOut->parameters.wep_parameters.default_key_index = 1;
            
            break;
        case LmWiFiNetworkType_WIFI_NETWORK_TYPE_NONE:
            pSecParamOut->type = SEC_MODE_OPEN;
            break;
        default:
            return ERR_GENERIC;
    }
    return ERR_OK;
}

err_t WIFIMGR_security_param_to_lmm_network_encode(char *ssid, security_parameters_t *security_param,
                            LmWiFiNetwork *pOutLmmNetwork)
{
    if (NULL != ssid)
    {
        pOutLmmNetwork->ssid.arg = (void *) ssid;
        pOutLmmNetwork->ssid.funcs.encode = encode_whistle_str;
    }
    switch(security_param->type)
    {
        case SEC_MODE_WPA:
            pOutLmmNetwork->has_type = true;
            if (0 == strcmp(security_param->parameters.wpa_parameters.version, WPA_PARAM_VERSION_WPA))
                pOutLmmNetwork->type = LmWiFiNetworkType_WIFI_NETWORK_TYPE_WPA;
            else if (0 == strcmp(security_param->parameters.wpa_parameters.version, WPA_PARAM_VERSION_WPA2))
                pOutLmmNetwork->type = LmWiFiNetworkType_WIFI_NETWORK_TYPE_WPA2;
            else
                return -1;
            
            pOutLmmNetwork->has_uCipher = true;
            switch(security_param->parameters.wpa_parameters.ucipher)
            {
                case ATH_CIPHER_TYPE_TKIP:
                    pOutLmmNetwork->uCipher = LmWiFiNetworkuCipher_WIFI_NETWORK_UCIPHER_TKIP;
                    break;
                case ATH_CIPHER_TYPE_CCMP:
                    pOutLmmNetwork->uCipher = LmWiFiNetworkuCipher_WIFI_NETWORK_UCIPHER_CCMP;
                    break;
                default:
                    return ERR_GENERIC;
            }
            
            pOutLmmNetwork->has_mCipher = true;
            switch(security_param->parameters.wpa_parameters.mcipher)
            {
                case TKIP_CRYPT:
                    pOutLmmNetwork->mCipher = LmWiFiNetworkmCipher_WIFI_NETWORK_MCIPHER_TKIP;
                    break;
                case AES_CRYPT:
                    pOutLmmNetwork->mCipher = LmWiFiNetworkmCipher_WIFI_NETWORK_MCIPHER_AES;
                    break;
                default:
                    return ERR_GENERIC;
            }

            if (NULL != security_param->parameters.wpa_parameters.passphrase)
            {
                pOutLmmNetwork->password.arg = (void *) security_param->parameters.wpa_parameters.passphrase;
                pOutLmmNetwork->password.funcs.encode = encode_whistle_str;
            }
            break;
        case SEC_MODE_WEP:
            pOutLmmNetwork->has_type = true;
            pOutLmmNetwork->type = LmWiFiNetworkType_WIFI_NETWORK_TYPE_WEP;

            if (NULL != security_param->parameters.wep_parameters.key[0])
            {
                pOutLmmNetwork->password.arg = security_param->parameters.wep_parameters.key[0];
                pOutLmmNetwork->password.funcs.encode = encode_whistle_str;
            }
            break;
        case SEC_MODE_OPEN:
            pOutLmmNetwork->has_type = true;
            pOutLmmNetwork->type = LmWiFiNetworkType_WIFI_NETWORK_TYPE_NONE;
            if (NULL != ssid)
            {
                pOutLmmNetwork->ssid.arg = (void *) ssid;
                pOutLmmNetwork->ssid.funcs.encode = encode_whistle_str;
            }
            break;
            
        default:
            WPRINT_ERR("dec ntwk");
            return -4;
    }
    
    return ERR_OK;
}

