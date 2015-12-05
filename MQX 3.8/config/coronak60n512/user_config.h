/**HEADER**********************************************************************
*
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
*******************************************************************************
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
*******************************************************************************
*
* $FileName: user_config.h$
* $Version : 3.8.B.3$
* $Date    : Dec-9-2011$
*
* Comments:
*
*   User configuration for MQX components
*
*END**************************************************************************/

#ifndef __user_config_h__
#define __user_config_h__

/* mandatory CPU identification */
#define MQX_CPU                 PSP_CPU_MK60DN512

#pragma readonly_strings on
#pragma longlong on
/*
** BSP settings - for defaults see mqx\source\bsp\<board_name>\<board_name>.h
*/

/* Log power state changes */
/* 0 or undefined = disabled.
 * 1              = enabled. Keep all other corona_prints.
 * 2              = enabled. Disable all other corona_prints except power state logging.
 */
//#define PWRMGR_BREADCRUMB_ENABLED 1

#define WHISTLE_UTF_AGENT        0 /*  ENABLES UTF AGENT (WIFI FCC TESTING) TO WORK    */
#if WHISTLE_UTF_AGENT
    #define MANUFACTURING_SUPPORT    1    // This is only needed for the utf_agent_corona project (Atheros FCC).
    //#ifndef USB_ENABLED
    //    #error "USB is now required for WIFI FCC TESING!"
    //#endif
#endif

/* Enables reboot after each upload to server */
#define WIFI_REBOOT_HACK
#define BT_REBOOT_HACK

/* Enable interrupt enable/disable logging */
/* Comment these out for ship! */
#define INT_LOGGING              0

#if !INT_LOGGING
#define wint_enable _int_enable
#define wint_disable _int_disable
#endif

/* Enable walloc/wfree logging */
//#define CORMEM_DEBUG 1

#define BSPCFG_ENABLE_ATHEROS_WIFI 1
#define BSPCFG_ENABLE_TTYA       0
#define BSPCFG_ENABLE_ITTYA      0 /* BT taken over completely by Bluetopia, tho it would be here */
#define BSPCFG_ENABLE_TTYB       0
#define BSPCFG_ENABLE_ITTYB      0
#define BSPCFG_ENABLE_TTYC       0
#define BSPCFG_ENABLE_ITTYC      0
#define BSPCFG_ENABLE_TTYD       1 /* WiFi debug */
#define BSPCFG_ENABLE_ITTYD      0
#define BSPCFG_ENABLE_TTYE       0
#define BSPCFG_ENABLE_ITTYE      0

#ifdef USB_ENABLED
    #define BSPCFG_ENABLE_ITTYF      0 /* UART debug */
#else
    #define BSPCFG_ENABLE_ITTYF      1 /* UART debug */
#endif

#define BSPCFG_ENABLE_I2C0       0
#define BSPCFG_ENABLE_II2C0      0

/* 
 *   COR-293:  BT MFI - Requires Polled Driver
 */
#define BSPCFG_ENABLE_I2C1       1 
#define BSPCFG_ENABLE_II2C1      0

#define BSPCFG_ENABLE_SPI0       0
#define BSPCFG_ENABLE_ISPI0      1 /* WiFi */
#define BSPCFG_ENABLE_SPI1       0
#define BSPCFG_ENABLE_ISPI1      0 /* Not Used - Custom driver is used for Accel */
#define BSPCFG_ENABLE_SPI2       0
#define BSPCFG_ENABLE_ISPI2      1 /* SPI Flash */
#define BSPCFG_ENABLE_GPIODEV    0 /* We use lwgpio, so can't use this */
#define BSPCFG_ENABLE_RTCDEV     1
#define BSPCFG_ENABLE_PCFLASH    0
#define BSPCFG_ENABLE_ADC0       0 /* Not Used - Custom driver is used for ADC0 */
#define BSPCFG_ENABLE_ADC1       0
#define BSPCFG_ENABLE_FLASHX     0
#define BSPCFG_ENABLE_ESDHC      0

#define BSPCFG_ENABLE_II2S0      0
#define BSPCFG_ENABLE_CRC        0

/*
** board-specific MQX settings - see for defaults mqx\source\include\mqx_cnfg.h
*/

#define MQX_USE_IDLE_TASK        1
#define MQX_HAS_TIME_SLICE       1
#define MQX_USE_TIMER            1
//#define MQX_ENABLE_LOW_POWER     0

/*
** board-specific RTCS settings - see for defaults rtcs\source\include\rtcscfg.h
*/

#define RTCSCFG_ENABLE_ICMP      1
#define RTCSCFG_STACK_SIZE       512

#define MQX_TASK_DESTRUCTION     1


/*
 *
 * Whistle Settings
 *
 */

/*
 *   COR-275 (special app that reports WIFI RSSI)
 *   
 *        NOTE:  This app will disable other features in the system,
 *               related to displaying battery life, and uploading data.
 *            
 *        This will app will report on the display on any button press,
 *           the WIFI RSSI (signal strength).  See JIRA for more details.
 *           
 */ 
#define WIFI_RSSI_TEST_APP        0

/*
 *   (FTA) Functional Test Application
 *   
 *   Setting this to non-zero will compile-in the FTA hooks.
 *   Otherwise, set this to 0.
 */
#define FTA_WHISTLE               0

/*
 * USB_ENABLED
 * If USB is disabled then console and logging all goes to the serial port (less power consumption).
 * If USB is enabled then console and logging goes to JTAG until USB is plugged in,
 *   then it is switched to USB. 
 */

#ifdef DEBUG_WHISTLE
    #define MQX_KERNEL_LOGGING  1
	//#define MQX_USE_LOGS		1
#else
    #define MQX_KERNEL_LOGGING  0
#endif

#if defined(DEBUG_WHISTLE) // && !defined(WHISTLE_UTF_AGENT)
    #define BSPCFG_ENABLE_IODEBUG    1
#else
    #define BSPCFG_ENABLE_IODEBUG    0
#endif

#define MQX_USE_SW_WATCHDOGS 1

/*
** include common settings
*/
#define PSP_HAS_SUPPORT_STRUCT  1

/* use the rest of defaults from small-RAM-device profile */
#include "small_ram_config.h"

/* and enable verification checks in kernel */
#include "verif_enabled_config.h"

#endif /* __user_config_h__ */
