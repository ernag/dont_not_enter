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

/*
 * $FileName: ar4100p_main.h$
 * $Version : 1.0$
 * $Date    :
 *
 * Comments:
 *   The header file for Atheros wmiconfig tool.
 *
 * END************************************************************************/

#ifndef _ar4100_main_h_
#define _ar4100_main_h_

#include <mqx.h>
#include <bsp.h>
#include <lwevent.h>
#include <message.h>
#include <string.h>

#include "rtcs.h"
#include "ipcfg.h"
#include "atheros_wifi_api.h"
#include "atheros_wifi.h"
#include "atheros_stack_offload.h"
#include "enet_wifi.h"
#include "iwcfg.h"

#if MQX_KERNEL_LOGGING
#include <klog.h>
#endif

/* define MACROS required by common code. */
#define USE_ATH_CHANGES           1              // Macro to control Atheros specific changes to FNET tools
#define ATH_STRCMP(_a, _b)         strcmp((char const *)(_a), (const char *)(_b))
#define ATH_TIME_DELAY_MSEC(_a)    _time_delay((_a))
#define CFG_PACKET_SIZE_MAX_TX    (1576)  /* Defines size of Application and Socket TX&RX buffers.*/
#define CFG_PACKET_SIZE_MAX_RX    (1556)  /* Defines size of Application and Socket TX&RX buffers.*/
#define DEMOCFG_USE_WIFI          1       /* USE WIFI Interface */
#define DEMOCFG_DEFAULT_DEVICE    ATHEROS_WIFI_DEFAULT_ENET_DEVICE

typedef struct
{
    uint_8 ssid[32];
    uint_8 macaddress[6];
    uint_16 channel;
} wps_scan_params;

typedef struct version
{
    uint_32 host_ver;
    uint_32 target_ver;
    uint_32 wlan_ver;
    uint_32 abi_ver;
} version_t;

typedef struct key
{
    char key[30];
    uint_8 key_index;
} key_t;

enum wps_mode_e
{
    PUSH_BUTTON = 1, PIN
};
enum p2p_mode_e
{
    CONNECT = 1,
    FIND,
    JOIN,
    LISTEN,
    CANCEL,
    STOP,
    CONNECT_CLIENT,
    NODE_LIST,
    SET_CONFIG,
    WPS_CONFIG,
    AUTH,
    DISC_REQ,
    SET,
    P2P_INVITE_AUTH,
    PERSISTENT,
    P2P_INVITE,
    INV_CONNECT,
    JOIN_PROFILE,
    AP_MODE,
    AP_MODE_PP,
    P2P_ON_OFF,
    P2P_SET_NOA,
    P2P_SET_OPPPS
};

typedef enum mode
{
    MODE_STATION = 0, MODE_AP, MODE_ADHOC, MODE_MAXIMUM
} mode_t;

typedef struct
{
    union
    {
        struct
        {
            A_UINT8 hidden_ssid;
        } wmi_ap_hidden_ssid_cmd;
        
        struct
        {
            A_UINT32 period;
        } wmi_ap_conn_inact_cmd;
        
        struct
        {
            A_UINT8 countryCode[3];
        } wmi_ap_set_country_cmd;
        
        struct
        {
            A_UINT16 beaconInterval;
        } wmi_beacon_int_cmd;
        
        struct
        {
            A_UINT8 dtimval;
        } wmi_ap_dtim_cmd;
        
        struct
        {
            A_UINT8 ps_val[2];
        } wmi_ap_ps_buf_cmd;
    } u;
} AP_CFG_CMD;

#include "atheros_wifi_api.h"

enum ath_private_ioctl_cmd
{
    ATH_REG_QUERY = ATH_CMD_LAST, ATH_MEM_QUERY
};

typedef struct
{
    uint_32 address;
    uint_32 value;
    uint_32 mask;
    uint_32 size;
    uint_32 operation;
} ATH_REGQUERY;

typedef struct
{
    uint_32 literals;
    uint_32 rodata;
    uint_32 data;
    uint_32 bss;
    uint_32 text;
    uint_32 heap;
} ATH_MEMQUERY;

typedef struct
{
    uint_8 wps_in_progress;
    uint_8 connect_flag;
} wps_context_t;

#define ATH_REG_OP_READ        (1)
#define ATH_REG_OP_WRITE       (2)
#define ATH_REG_OP_RMW         (3)
#define MAX_ACK_RETRY          (20)      //Max number of times ACK issent to Peer with stats
#define MAX_SSID_LENGTH        (32 + 1)

#define MIN_WEP_KEY_SIZE       10
#define MAX_WEP_KEY_SIZE       26
#define WIFI_MAX_WPA_VER_SIZE  8
#define MAX_PASSPHRASE_SIZE    64
#define MAX_NUM_WEP_KEYS       4
#define MAX_WPA_VER_SIZE       8
#define MAX_WPS_PIN_SIZE       32

#define DRIVER_UP              (0)
#define DRIVER_DOWN            (1)
#define READ_STATUS            (2)

/* Security modes */
#define SEC_MODE_OPEN          (0)
#define SEC_MODE_WEP           (1)
#define SEC_MODE_WPA           (2)

#define getByte(n, data)                          ((data >> (8 * n)) & 0xFF)

#define IS_DRIVER_READY    is_driver_initialized()
#define HANDLE_IOCTL(inout_param)                 handle_ioctl(inout_param)
#define ATH_COMMIT(device)                        ath_commit(device)
#define ATH_SET_MODE(device, mode)                ath_set_mode(device, mode)
#define ATH_SET_ESSID(device, essid)              ath_set_essid(device, essid)
#define ATH_SET_SEC_TYPE(device, sectype)         ath_set_sec_type(device, sectype)
#define ATH_SET_PASSPHRASE(device, passphrase)    ath_set_passphrase(device, passphrase)
#define ATH_SEND_WEP_PARAM(wep_param)             ath_send_wep_param(wep_param)
#define ATH_GET_ESSID(device, essid)              ath_get_essid(device, essid)
#define ATH_NET_CONNECT(pNetparams)               NetConnect(pNetparams)
#define SET_POWER_MODE(pwr_mode)                  set_power_mode(pwr_mode)
#define TURN_LED_ON
#define TURN_LED_OFF
#if __TEST_ATHEROS_FLASH_SUPPORT__
#define TEST_FLASH    test_flash()
#endif
#define ATH_MEM_QUERY(argc, argv)    ath_mem_query(argc, argv)
#define ATH_REG_QUERY(argc, argv)    ath_reg_query(argc, argv)

/********************** Function Prototypes **************************************/

A_STATUS is_driver_initialized();
void SystemReset();
A_UINT32 ext_scan();
A_STATUS handle_ioctl(ATH_IOCTL_PARAM_STRUCT *inout_param);
A_INT32 set_power_mode(char *pwr_mode);
#if __TEST_ATHEROS_FLASH_SUPPORT__
A_INT32 test_flash(void);
#endif
A_INT32 ath_mem_query(A_INT32 argc, char *argv[]);
A_INT32 ath_reg_query(A_INT32 argc, char *argv[]);
A_INT32 wmi_set_scan(A_UINT32 dev_num);
A_INT32 ath_driver_state(A_INT32 argc, char *argv[]);
A_STATUS ath_set_essid(A_UINT32 device, char *essid);
A_STATUS ath_set_sec_type(A_UINT32 device, char *sec_type);
A_STATUS ath_set_passphrase(A_UINT32 device, char *passphrase);
A_STATUS ath_get_essid(A_UINT32 device, char *essid);
A_INT32 get_power_mode(char *pwr_mode);
A_STATUS ath_commit(A_UINT32 device);
A_INT32 set_callback();
A_STATUS ath_set_mode(A_UINT32 device, char *mode);
A_UINT8 driver_status(A_UINT8 val);
A_STATUS ath_send_wep_param(ENET_WEPKEYS *wep_param);
A_INT32 CompleteWPS(ATH_NETPARAMS *pNetparams, A_UINT8 block, mode_t mode_flag);
A_INT32 NetConnect(ATH_NETPARAMS *pNetparams);
int handle_pmk(char *pmk);
int_32 wmi_iwconfig(int_32 argc, char_ptr argv[]);
A_STATUS is_driver_initialized();
A_STATUS handle_ioctl(ATH_IOCTL_PARAM_STRUCT *inout_param);

void Shell_Task(uint_32);
void atheros_driver_setup(void);
int_32 wmi_ping(int_32 argc, char_ptr argv[]);
int_32 wmi_ping6(int_32 argc, char_ptr argv[]);
extern int_32 wps_query(uint_8 block);
int_32 ipconfig_query(int_32 argc, char_ptr argv[]);
void resetTarget();
A_INT32 set_callback();
#if __ATHEROS_WMICONFIG_SUPPORT__
int_32 wmiconfig_handler(int_32 argc, char_ptr argv[]);
#endif
A_INT32 set_wep_keys(A_UINT32 dev_num, char *wep_key1, char *wep_key2,
        char *wep_key3, char *wep_key4, A_UINT32 key_len, A_UINT32 key_index);

char *inet_ntoa(A_UINT32 addr, char *res_str);
char *inet6_ntoa(char *addr, char *str);
void ath_udp_echo(int_32 argc, char_ptr argv[]);
char *print_ip6(IP6_ADDR_T *addr, char *str);
#if __ATHEROS_IPCONFIG_STATIC_SUPPORT__
A_INT32 ipconfig_static(A_INT32 argc, char *argv[]);
#endif
A_INT32 ipconfig_dhcp(A_INT32 argc, char *argv[]);
A_INT32 wmi_ping(A_INT32 argc, char *argv[]);
A_INT32 wmi_ping6(A_INT32 argc, char *argv[]);
uint_32 mystrtoul(const char *arg, const char *endptr, int base);
void application_frame_cb(A_VOID *ptr);
A_INT32 get_tx_status();
A_INT32 StartWPS(ATH_NETPARAMS *pNetparams, A_UINT8 use_pinmode,
        wps_scan_params *wpsScan_p);

/* Whistle adds */
_mqx_uint whistle_atheros_init(_enet_handle *handle);
_enet_handle whistle_atheros_get_handle(void);
void whistle_atheros_get_enet_address(_enet_address addr);
A_INT32 whistle_set_passphrase(char *passphrase);
A_INT32 whistle_set_wep_key(uint_8 key_idx, char *key_val);
A_INT32 whistle_connect_handler(const char *ssid_str);
A_INT32 whistle_set_wpa(const char *version, A_UINT32 ucipher, A_UINT32 mcipher);
A_INT32 whistle_set_wep(int key_idx);
void whistle_display_mac_addr(void);
A_INT32 whistle_scan_control(A_BOOL deep_scan);
A_INT32 whistle_ipconfig_dhcp(void);
A_INT32 whistle_wmi_ping(const char *hostaddr_str, A_UINT32 count,
        A_UINT32 size);
A_INT32 whistle_mac_store(char *mac);
A_INT32 whistle_wmi_iwconfig(ENET_SCAN_LIST *scan_list);
A_INT32 whistle_wmi_set_scan(A_UINT32 dev_num, ENET_SCAN_LIST *scan_list);
A_UINT32 whistle_ext_scan(ATH_GET_SCAN *ext_scan_list);
void whistle_set_callback_with_driver(void(*cb)(int));
A_INT32 whistle_set_callback(void);
A_UINT8 *whistle_get_connected_ssid(void);
A_INT32 whistle_set_tx_power(A_UINT8 dBm);
void whistle_ShutDownWIFI();
A_INT32 whistle_get_rssi();
A_INT32 whistle_get_channel();

#if DRIVER_CONFIG_INCLUDE_BMI
extern void Flash_Agent_Task(uint_32 val);
#endif

/*********************** Extern Variables ******************************************/
extern uint_8 access_category;
extern A_UINT32 wifi_connected_flag; //Flag to indicate WIFI connectivity status
extern uint_8 bench_quit;
extern _enet_handle _whistle_handle;
extern A_UINT8 original_ssid[];
#endif
