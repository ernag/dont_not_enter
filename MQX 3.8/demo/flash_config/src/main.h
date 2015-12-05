#ifndef _demo_h_
#define _demo_h_
/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
* Copyright (c) 2004-2008 Embedded Access Inc.;
* All Rights Reserved
*
*************************************************************************** 
*
* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
* THE POSSIBILITY OF SUCH DAMAGE.
*
**************************************************************************
*
* $FileName: HVAC.h$
* $Version : 3.5.21.0$
* $Date    : Mar-25-2010$
*
* Comments:
*   The main configuration file for HVAC demo
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>
#include <lwevent.h>
#include <message.h>
#include "athstartpack.h"
#if MQX_KERNEL_LOGGING
#include <klog.h>
#endif

#define DEMOCFG_ENABLE_SERIAL_SHELL    0   /* enable shell task for serial console */
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

#include <rtcs.h>
extern void HVAC_initialize_networking(void);
   
#if DEMOCFG_ENABLE_RTCS
   #include <rtcs.h>
   #include <ftpc.h> 
   #include <ftpd.h> 

   #ifndef ENET_IPADDR
   //   #define ENET_IPADDR  IPADDR(15,15,103,25) 
     #define ENET_IPADDR  IPADDR(192,168,1,90)
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
   
#endif
#define DEMOCFG_DEFAULT_DEVICE  0
#ifndef DEMOCFG_DEFAULT_DEVICE
   #define DEMOCFG_DEFAULT_DEVICE   BSP_DEFAULT_ENET_DEVICE
#endif
   


#include <shell.h>
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



enum task{
   FLASH_AGENT_TASK = 1

};

#pragma pack(1)

typedef struct _version {
    unsigned int        host_ver;
    unsigned int        target_ver;
    unsigned int        wlan_ver;
    unsigned int        abi_ver;
}version_t;

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


#include "atheros_wifi_api.h"

#if 0
enum ath_private_ioctl_cmd
{
	ATH_PROGRAM_FLASH=ATH_CMD_LAST,			/*input- buffer, length, start address*/
	ATH_EXECUTE_FLASH				/*input- execute address*/	
};




typedef struct ath_program_flash
{
	uint_8* buffer;
	uint_32 load_addr;
	uint_16 length;
	uint_32 result;	
} ATH_PROGRAM_FLASH_STRUCT;
#endif

void atheros_driver_setup_mediactl(void);
void atheros_driver_setup_init(void);

#endif
