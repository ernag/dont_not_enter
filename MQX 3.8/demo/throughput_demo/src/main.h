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
* $FileName: main.h$
* $Version : 1.0$
* $Date    : 
*
* Comments:
*   The header file for Atheros wmiconfig tool.
*
*END************************************************************************/

#ifndef _main_h_
#define _main_h_

#include <mqx.h>
#include <bsp.h>
#include <lwevent.h>
#include <message.h>
#include "atheros_stack_offload.h"
#include "athstartpack.h"
#if MQX_KERNEL_LOGGING
#include <klog.h>
#endif

#define ENABLE_STACK_OFFLOAD 1
#define FLASH_APP 0 // 1 == use flash agent. 0 = use throughput demo

#define DEMOCFG_ENABLE_SERIAL_SHELL    !FLASH_APP   /* enable shell task for serial console */
#define DEMOCFG_ENABLE_SWITCH_TASK     0   /* enable button sensing task (otherwise keys are polled) */
#define DEMOCFG_ENABLE_AUTO_LOGGING    0   /* enable logging to serial console (or USB drive) */
#define DEMOCFG_ENABLE_USB_FILESYSTEM  0   /* enable USB mass storage */
#define DEMOCFG_ENABLE_RTCS            0   /* enable RTCS operation */
#define DEMOCFG_ENABLE_FTP_SERVER      0   /* enable ftp server */
#define DEMOCFG_ENABLE_TELNET_SERVER   0   /* enable telnet server */
#define DEMOCFG_ENABLE_KLOG            0   /* enable kernel logging */
#define DEMOCFG_USE_POOLS              0   /* enable external memory pools for USB and RTCS */
#define DEMOCFG_USE_WIFI               1  /* USE WIFI Interface */
/* default addresses for external memory pools and klog */
#if BSP_M52259EVB
    #define DEMOCFG_RTCS_POOL_ADDR  (uint_32)(BSP_EXTERNAL_MRAM_RAM_BASE)
    #define DEMOCFG_RTCS_POOL_SIZE  0x0000A000
    #define DEMOCFG_MFS_POOL_ADDR   (uint_32)(DEMOCFG_RTCS_POOL_ADDR + DEMOCFG_RTCS_POOL_SIZE)
    #define DEMOCFG_MFS_POOL_SIZE   0x00002000
    #define DEMOCFG_KLOG_ADDR       (uint_32)(DEMOCFG_MFS_POOL_ADDR + DEMOCFG_MFS_POOL_SIZE)
    #define DEMOCFG_KLOG_SIZE       4000
#elif DEMOCFG_USE_POOLS
    #warning External pools will not be used on this board.
#endif

#if DEMOCFG_ENABLE_RTCS
   #include <rtcs.h>
   #include <ftpc.h> 
   #include <ftpd.h> 

   
   extern void HVAC_initialize_networking(void);

   #ifndef ENET_IPADDR
   //   #define ENET_IPADDR  IPADDR(15,15,103,25) 
     #define ENET_IPADDR  IPADDR(192,168,1,90)
   //#define ENET_IPADDR  IPADDR(172,16,83,25)
   #endif

   #ifndef ENET_IPMASK
      #define ENET_IPMASK  IPADDR(255,255,255,0) 
   #endif

   #ifndef ENET_IPGATEWAY
    //  #define ENET_IPGATEWAY  IPADDR(15,15,103,10) 
     #define ENET_IPGATEWAY  IPADDR(192,168,1,1)
   #endif
#endif
#if DEMOCFG_USE_WIFI
   #include "iwcfg.h"
   
   #define DEMOCFG_SSID            "iot"
   //Possible Values managed or adhoc
   #define DEMOCFG_NW_MODE         "managed" 
   #define DEMOCFG_AP_MODE         "softap"
   //Possible vales 
   // 1. "wep"
   // 2. "wpa"
   // 3. "wpa2"
   // 4. "none"
   #define DEMOCFG_SECURITY        "none"
   #define DEMOCFG_PASSPHRASE      NULL
   #define DEMOCFG_WEP_KEY         "abcde"
   //Possible values 1,2,3,4
   #define DEMOCFG_WEP_KEY_INDEX   1
   #define DEMOCFG_DEFAULT_DEVICE  ATHEROS_WIFI_DEFAULT_ENET_DEVICE
#endif

#ifndef DEMOCFG_DEFAULT_DEVICE
   #define DEMOCFG_DEFAULT_DEVICE   BSP_DEFAULT_ENET_DEVICE
#endif
   
#if DEMOCFG_ENABLE_USB_FILESYSTEM
   #include <mfs.h>
   #include <part_mgr.h>
   #include <usbmfs.h>

   #define LOG_FILE "c:\\hvac_log.txt"
#endif

#if DEMOCFG_ENABLE_WEBSERVER
   #include "httpd.h"
#endif

#include <shell.h>


typedef struct {
    uint_8 ssid[32];
    uint_8 macaddress[6];
    uint_16 channel;
} wps_scan_params;


#pragma pack(1)

typedef struct _version {
    unsigned int        host_ver;
    unsigned int        target_ver;
    unsigned int        wlan_ver;
    unsigned int        abi_ver;
}version_t;


#define VERSION (4)

/******************************************/
/* firmware input param value definitions */
/******************************************/
/* IOTFLASHOTP_PARAM_SKIP_OTP - instructs firmware to skip OTP operations */
#define IOTFLASHOTP_PARAM_SKIP_OTP                  (0x00000001)
/* IOTFLASHOTP_PARAM_SKIP_FLASH - instructs firmware to skip Flash operations */
#define IOTFLASHOTP_PARAM_SKIP_FLASH                (0x00000002)
/* IOTFLASHOTP_PARAM_USE_NVRAM_CONFIG_FROM_OTP - instructs firmware to use NVRAM config found in OTP
 *  to access flash. */
#define IOTFLASHOTP_PARAM_USE_NVRAM_CONFIG_FROM_OTP (0x00000004)
/* IOTFLASHOTP_PARAM_FORCE_FLASH_REWRITE - instructs firmware to ignore any build_number protection
 * when deciding whether or not to re-write the flash. normally, firmware does not re-write flash
 * if the build_number of the new image is less then the build_number of the image currently in 
 * flash. */
#define IOTFLASHOTP_PARAM_FORCE_FLASH_REWRITE       (0x00000008)
/* IOTFLASHOTP_PARAM_FORCE_MAC_OVERRIDE - instructs firmware to ignore any existing mac address 
 * in OTP and instead write the new mac address to otp. normally, if any mac address is found 
 * in OTP the firmware will avoid writing a new mac address to OTP. */
#define IOTFLASHOTP_PARAM_FORCE_MAC_OVERRIDE        (0x00000010)
/*************************************/
/* firmware return value definitions */
/*************************************/
#define IOTFLASHOTP_RESULT_OTP_SUCCESS              (0x00000001)
#define IOTFLASHOTP_RESULT_OTP_FAILED               (0x00000002)
#define IOTFLASHOTP_RESULT_OTP_NOT_WRITTEN          (0x00000004)
#define IOTFLASHOTP_RESULT_OTP_SKIPPED              (0x00000008)
#define IOTFLASHOTP_RESULT_OTP_POS_MASK             (0x0000000f)
#define IOTFLASHOTP_RESULT_OTP_POS_SHIFT            (8)
#define IOTFLASHOTP_RESULT_OTP_MASK                 (0x0000ffff)

#define IOTFLASHOTP_RESULT_FLASH_SUCCESS            (0x00010000)
#define IOTFLASHOTP_RESULT_FLASH_FAILED             (0x00020000)
#define IOTFLASHOTP_RESULT_FLASH_VALIDATE_FAILED    (0x00040000)
#define IOTFLASHOTP_RESULT_FLASH_SKIPPED            (0x00080000)
#define IOTFLASHOTP_RESULT_FLASH_OLDER_VERSION      (0x00100000) /* flash was not written because a newer version is already present. */
#define IOTFLASHOTP_RESULT_FLASH_MASK               (0xffff0000)


typedef struct _request{
	unsigned short cmdId FIELD_PACKED; 
	unsigned char buffer[1024] FIELD_PACKED;
	unsigned int addr FIELD_PACKED;
	unsigned int option FIELD_PACKED;
	unsigned short length FIELD_PACKED;		
	unsigned short crc FIELD_PACKED;
}  request_t;


typedef struct _response{
	unsigned short cmdId FIELD_PACKED;
	unsigned int result FIELD_PACKED;
	version_t version FIELD_PACKED;
} response_t;
#pragma pack(4)

enum cmd_id{
	PROGRAM = 0,
	PROGRAM_RESULT,
	EXECUTE,
	EXECUTE_RESULT,
	CRC_ERROR,
	VERSION_NUM,
	FW_VERSION,
	UNKNOWN_CMD
};

void Flash_Agent_Task(uint_32);
void WifiConnected(int val);
void reset_atheros_driver_setup_init(void);
typedef struct key
{
	char key[30];
	uint_8 key_index;
} key_t;



enum wps_mode_e 
{
	PUSH_BUTTON = 1,
	PIN
};

typedef enum mode {

	MODE_STATION = 0,
	MODE_AP,
	MODE_ADHOC,
	MODE_MAXIMUM

}mode_t;

typedef struct {
	union {

		struct{
			A_UINT8 hidden_ssid;
		} wmi_ap_hidden_ssid_cmd;

		struct {
			A_UINT32 period;
		} wmi_ap_conn_inact_cmd;

		struct {
			A_UINT8 countryCode[3];
		} wmi_ap_set_country_cmd;

		struct {
			A_UINT16 beaconInterval;
		} wmi_beacon_int_cmd;

	}u;
}AP_CFG_CMD;

#include "atheros_wifi_api.h"

enum ath_private_ioctl_cmd
{
	ATH_REG_QUERY=ATH_CMD_LAST,
	ATH_MEM_QUERY
};



typedef struct
{
	uint_32 address;
	uint_32 value;
	uint_32 mask;
	uint_32 size;
	uint_32 operation;
}ATH_REGQUERY;

typedef struct
{
	uint_32 literals;
	uint_32 rodata;
	uint_32 data;
	uint_32 bss;
	uint_32 text;
	uint_32 heap;
}ATH_MEMQUERY;


typedef struct {
	uint_8 wps_in_progress;
	uint_8 connect_flag;
}wps_context_t;


#define ATH_REG_OP_READ 			(1)
#define ATH_REG_OP_WRITE 			(2)
#define ATH_REG_OP_RMW 				(3)
#define MAX_ACK_RETRY  			   (20)	 //Max number of times ACK issent to Peer with stats
#define MAX_SSID_LENGTH 		 (32+1)

#define MIN_WEP_KEY_SIZE			10
#define MAX_WEP_KEY_SIZE			26
#define MAX_PASSPHRASE_SIZE         64
#define MAX_NUM_WEP_KEYS             4
#define MAX_WPS_PIN_SIZE            32

 
/********************** Function Prototypes **************************************/

int_32 wmi_iwconfig(int_32 argc, char_ptr argv[]);

void Shell_Task(uint_32);
void atheros_driver_setup(void);
int_32 wmi_ping(int_32 argc, char_ptr argv[] );
int_32 wmi_ping6(int_32 argc, char_ptr argv[] );
extern int_32 wps_query(uint_8 block);
int_32 ipconfig_query(int_32 argc, char_ptr argv[]);
void resetTarget();
int_32 wmiconfig_handler(int_32 argc, char_ptr argv[] );
void atheros_driver_setup_mediactl(void);
void atheros_driver_setup_init(void);



#if ENABLE_STACK_OFFLOAD
char *inet_ntoa( A_UINT32 addr, char *res_str );
char * inet6_ntoa(char* addr, char * str);
void ath_udp_echo (int_32 argc, char_ptr argv[]);
#endif
/*********************** Extern Variables ******************************************/
extern uint_8 access_category;
extern uint_8 wifi_connected_flag;     //Flag to indicate WIFI connectivity status
extern uint_8 bench_quit;
extern _enet_handle    whistle_handle;

#endif
