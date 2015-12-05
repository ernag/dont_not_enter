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


#ifndef __main_h_
#define __main_h_

#ifdef EFM_UTF
#include <atheros_stack_offload.h>
#include <atheros_wifi.h>
#include <atheros_wifi_api.h>
#else
#include <mqx.h>
#include <bsp.h>
#include <lwevent.h>
#include <message.h>
#include <rtcs.h>
#include "iwcfg.h"
#endif
#include "a_osapi.h"
#include "athstartpack.h"
enum 
{
   MAIN_TASK = 1,
   WRITE_TASK
};

#define ENET_DEVICE                  1
#define ENET_IPADDR  IPADDR(192,168,0,200) 


#define ATH_ART_TEST                 1
#define VERSION                     (2)    //Tool Version

/*
 * define of serial aganet protocol 
 */ 
#define DEF_SERIAL_AGENT_START			0xcc
#define DEF_SERIAL_AGENT_READ_START		0xaa
#define DEF_SERIAL_AGENT_LARGE_READ_START	0xa1
#define DEF_SERIAL_AGENT_WRITE_START		0xbb
#define DEF_SERIAL_AGENT_LARGE_WRITE_START	0xb1
#define DEF_IOT_ART_PROGRAMMER_START            0xdd  

/* 
 * define of buffer size
 */
#if 1 //Dhana
#define PLATFORM_UART_BUF_SIZE             128
#else
#define PLATFORM_UART_BUF_SIZE             126
#endif
#define LINE_ARRAY_SIZE                    2100
#define READ_BUF_SIZE                      PLATFORM_UART_BUF_SIZE
#define WRITE_BUF_SIZE                     PLATFORM_UART_BUF_SIZE
#define TC_CMDS_SIZE_MAX                   255

#define APP_DELAY_MSEC (500) /* MSEC to delay between activity */


/* 
 * define of LED
 */
#define int_32            A_INT32
#define uint_32           A_UINT32
#define uint_16           A_UINT16
#define uint_8            A_UINT8
#define int_8             A_INT8
#define char_ptr          A_INT8*
#define boolean           A_UINT8



/*
 ===========================================================================================
 */
typedef struct _ART_READ {
	uint_8  cmd;
	uint_32 len;
} ART_READ;


#include "atheros_wifi_api.h"

enum ath_private_ioctl_cmd
{
	ATH_PROGRAM_FLASH_ART=ATH_CMD_LAST,			/*input- buffer, length, start address*/
	ATH_EXECUTE_FLASH_ART,				/*input- execute address*/	
	ATH_HTC_RAW_OPEN,
	ATH_HTC_RAW_CLOSE,
	ATH_HTC_RAW_READ,
	ATH_HTC_RAW_WRITE,
	ATH_WRITE_TCMD,
        ATH_REG_LOCAL_PTR,
	ATH_BLOCK_FOR_TCMD_RESP,
	ATH_READ_TCMD_BUF,
	ATH_SET_RESP,
        ATH_CHECK_WRITE_DONE
};

enum cmd_id{
	PROGRAM_BOARD_DATA = 0,
	PROGRAM_RESULT,
	PROGRAM_OTP,
	EXECUTE,
	EXECUTE_RESULT,
	PROGRAM_ART,
	CRC_ERROR,
	VERSION_NUM,
	UNKNOWN_CMD
};

typedef enum {
    TCMD_CONT_TX_ID,
    TCMD_CONT_RX_ID,
    TCMD_PM_ID,
    TC_CMDS_ID,
    TCMD_SET_REG_ID,
    INVALID_CMD_ID=255
}TCMD_ID;

#pragma pack(1)
#pragma pack(4)

typedef PREPACK struct {
    unsigned int   testCmdId FIELD_PACKED;
    unsigned int   act FIELD_PACKED;
    PREPACK union {
        unsigned int  enANI FIELD_PACKED;    // to be identical to CONT_RX struct
        struct PREPACK {
            unsigned short   length FIELD_PACKED;
            unsigned char    version FIELD_PACKED;
            unsigned char    bufLen FIELD_PACKED;
        } POSTPACK parm;
    } POSTPACK u;
} POSTPACK TC_CMDS_HDR;

typedef PREPACK struct {
    TC_CMDS_HDR  hdr FIELD_PACKED;
    unsigned char      buf[TC_CMDS_SIZE_MAX+1] FIELD_PACKED;
} POSTPACK TC_CMDS;


/*
 * PROTOTYPES
 */

uint_32  write_serial_port(uint_32 length);
uint_32  read_serial_port (uint_32 lineIndex);
void     close_serial_port();
uint_32  send_serial_ack();
void     read_serial_ack();
void     wait_for_reply();
void     initialize_IO (void);
void     initialize_networking(void);
void     set_led_state(uint_16 state);
void     WifiConnected(int value);
void     Main_task(uint_32);
void write_task(uint_32);
void atheros_driver_setup_mediactl(void);
void tcmd_update_com_len(int Sendlen);

#endif /* __main_h_ */

