/*
 * wifi_mgr.h
 *
 *  Created on: Mar 13, 2013
 *      Author: Chris
 */

#ifndef WIFI_MGR_H_
#define WIFI_MGR_H_

#include <atheros_wifi_api.h>
#include <wmi.h>
#include "ar4100p_main.h"
#include "app_errors.h"
#include "whistlemessage.pb.h"

// Note: The max. response that can come back from the server in one response 
// needs to be determined so that this buffer size limit can be optimized.
// Also, use of CFG_PACKET_SIZE_MAX_RX assumes this is the largest the packet can be
//   but this may include overhead for Atheros headers.
#define MAX_WIFI_RX_BUFFER_SIZE      CFG_PACKET_SIZE_MAX_RX

#define WIFI_RECEIVE_CONTINUOUS_AGGREGATE_RETRY_TIMEOUT 	50 //# t_select timeout for continuous RX loop
#define WIFI_RECEIVE_TSELECT_ERROR_RETRY_MAX_COUNT			2
#define WIFI_RECEIVE_TSELECT_ERROR_RETRY_TIMEOUT			3*1000
#define WIFI_RECEIVE_TSELECT_TOTAL_RETRY_TIME				WIFI_RECEIVE_TSELECT_ERROR_RETRY_TIMEOUT*WIFI_RECEIVE_TSELECT_ERROR_RETRY_MAX_COUNT

extern const char WPA_PARAM_VERSION_WPA[];
extern const char WPA_PARAM_VERSION_WPA2[];

typedef struct wpa_parameters_s
{
    char        *version;    // "wpa", "wpa2" - referenced per WPA_PARAM_VERSION_WPA and WPA_PARAM_VERSION_WPA2
    char        *passphrase; // ascii
    A_UINT32    ucipher;     // atheros_wifi_api.h: ATH_CIPHER_TYPE_*
    CRYPTO_TYPE mcipher;     // wmi.h: CRYPTO_TYPE
} wpa_parameters_t;

typedef struct wep_parameters_s
{
    char   *key[MAX_NUM_WEP_KEYS];
    uint_8 default_key_index; // 1, 2, 3, 4 (1-based index in wmiconfig.c, not this key[]
} wep_parameters_t;


typedef struct security_parameters_s
{
    int type; // ar4100p_main.h: SEC_MODE_*
    union parameters_u
    {
        wpa_parameters_t wpa_parameters;
        wep_parameters_t wep_parameters;
    }   parameters;
} security_parameters_t;

typedef enum  {
    WIFIMGR_SECURITY_FLUSH_NEVER,
    WIFIMGR_SECURITY_FLUSH_LATER,
    WIFIMGR_SECURITY_FLUSH_NOW
} WIFIMGR_SECURITY_FLUSH_TYPE_e;

uint8_t a4100_wifi_comm_ok(void);
void WIFIMGR_init(void);
void WIFIMGR_shut_down(void);
err_t WIFIMGR_set_security(char *ssid, security_parameters_t *security_parameters, WIFIMGR_SECURITY_FLUSH_TYPE_e flush);
err_t WIFIMGR_clear_security(void);
err_t WIFIMGR_connect(char *ssid, security_parameters_t *security_parameters, boolean has_ever_connected);
err_t WIFIMGR_disconnect(void);
void WIFIMGR_notify_disconnect(void);
err_t WIFIMGR_open(char *host, uint_16 port, int *sock, enum SOCKET_TYPE sock_type);
err_t WIFIMGR_close(int sock);
err_t WIFIMGR_send(int sock, uint_8 *data, uint_32 length, int_32 *bytes_sent);
err_t WIFIMGR_receive(int sock, void *buf, uint_32 buf_len, uint_32 ms, int_32 *pbytes_received, boolean continuous_aggregate);
err_t WIFIMGR_zero_copy_free(void *buf);
err_t WIFIMGR_scan(ENET_SCAN_LIST *scan_list);
err_t WIFIMGR_ext_scan(ATH_GET_SCAN *ext_scan_list, boolean deep_scan);
err_t WIFIMGR_query_next_ssid(boolean firstQuery, char *ssid);
err_t WIFIMGR_remove(char *ssid);
boolean WIFIMGR_is_configured(char *ssid);
boolean WIFIMGR_wifi_is_on(void);
uint8_t WIFIMGR_get_wifi_fw_version(uint32_t *pFwVersion);
err_t WIFIMGR_lmm_network_encode_w_sized_strings_to_security_param(LmWiFiNetwork *pLmWiFiNetwork, security_parameters_t *security_param);
err_t WIFIMGR_ATH_GET_SCAN_to_security_param(ATH_SCAN_EXT *scan_list,
        char* wpa_passphrase, char* wep_key, 
        security_parameters_t *out_security_parameters);
err_t WIFIMGR_set_RX_buf(int8_t bufCount);
void WIFIMGR_list(void);
err_t WIFIMGR_get_max_seg(int sock);
#endif /* WIFI_MGR_H_ */
