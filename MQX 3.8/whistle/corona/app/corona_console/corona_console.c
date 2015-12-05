////////////////////////////////////////////////////////////////////////////////
//! \addtogroup application
//! \ingroup corona
//! @{
//
// Copyright (c) 2013 Whistle Labs
//
// Whistle Labs
// Proprietary and Confidential
//
// This source code and the algorithms implemented therein constitute
// confidential information and may comprise trade secrets of Whistle Labs
// or its associates.
//
//! \file corona_console.c
//! \brief Corona Console application
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
//!
//! \detail This is the Corona Console application or "diags" application, used to
//! communicate with a serial command line to access basic features and tests of Corona.
//! \note To make this code (and any GPIO code for that matter) tighter, you could
//! use GPIO_READ and GPIO_WRITE macros rather than using all the GPIO LDD's.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include <watchdog.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include "corona_console.h"
#include "ext_flash.h"
#include "evt_mgr.h"
#include "bt_mgr.h"
#include "cfg_mgr.h"
#include "cfg_factory.h"
#include "con_mgr.h"
#include "fwu_mgr.h"
#include "pwr_mgr.h"
#include "min_act.h"
#include "acc_mgr.h"
#include "wifi_mgr.h"
#include "SS1BTVS.h"
#include "ar4100p_main.h"
#include "user_config.h"
#include "cfg_mgr.h"
#include "rf_switch.h"
#include "wassert.h"
#include "parse_fwu_pkg.h"
#include "time_util.h"
#include "persist_var.h"
#include "corona_ext_flash_layout.h"
#include "led_hal.h"
#include "accel_hal.h"
#include "dis_mgr.h"
#include "prx_mgr.h"
#include <custom_stack_offload.h>
#include "wmi.h"
#include "adc_driver.h"
#include "rtc_alarm.h"
#include "cormem.h"
#include "whistlemessage.pb.h"
#include "wmutex.h"


#if 0
#include "corona_gpio.h"
#include "corona_isr_util.h"
#include "bu26507_display.h"
#include "lis3dh_accel.h"
#include "pwr_mgr.h"
#include "ADC0_Vsense.h"
#include "PE_Types.h"
#include "PORTA_GPIO.h"
#include "PORTB_GPIO_LDD.h"
#include "PORTC_GPIO_LDD.h"
#include "PORTD_GPIO_LDD.h"
#include "PORTE_GPIO_LDD.h"
#include "UART5_DBG.h"
#endif


////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////
typedef const char * cc_cmd_str_t;
typedef const char * cc_cmd_summary_t;
typedef const char * cc_io_cmd_t;
typedef err_t (*cc_cmd_callback_t)(cc_handle_t *pHandle);

typedef struct corona_console_command
{
    cc_cmd_str_t      cmd;         // command name.
    cc_cmd_summary_t  sum;         // brief summary for the command
    uint_16           minArgs;     // minimum number of arguments including the command itself.
    cc_manual_t       man;         // string presented to the user when a 'man' page is requested.
    cc_cmd_callback_t callback;    // function pointer callback to call when command is requested.
} cc_cmd_t;

typedef struct sherlock_queue_type
{
    QUEUE_ELEMENT_STRUCT HEADER;   // standard queue element header.
    int             len;           // length of the raw data
    char            pData[];       // pointer to raw data.
}shlk_qu_elem_t, *shlk_qu_elem_ptr_t;

////////////////////////////////////////////////////////////////////////////////
// Externs
////////////////////////////////////////////////////////////////////////////////
extern err_t _EVTMGR_flush_evt_blk_ptrs(uint_32 num_bytes);
extern int BTPSAPI HCITR_COMWrite(unsigned int HCITransportID, unsigned int Length, unsigned char *Buffer);

extern matrix_info_t rgb_data;  // For the process_rgb cmd
extern void init_rgb_data();

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static cc_handle_t  g_handle;
static LWEVENT_STRUCT g_console_lwevent;
static cc_cmd_summary_t UNSUPPORTED_COMMAND = "Unsupported\n";
static cc_cmd_summary_t BAD_ARGUMENT = "bad arg\n";
static uint8_t g_console_is_up = 0;
static RTC_ALARM_t g_sher_flush_alarm = 0;
static _task_id g_console_task_print = 0;

/*
 *   Sherlock stuff.
 */
#define     MAX_SHER_STR        127
#define     MAX_SHER_QU_ELEMS    50

/*
 * Timeout for flushing sherlock if data remains in the page buffer
 * Note: for best efficiecy during accel logging, the timeout should be
 * greater than 7 seconds
 */
#define SHERLOCK_FLUSH_TIMEOUT      10

static      QUEUE_STRUCT g_shlk_qu;


////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static void cc_split_str(cc_handle_t *pHandle, char *pCmd);
static void trim_whitesp(char *pStr);

//static err_t cc_process_io(cc_handle_t *pHandle, cc_io_dir_t direction);
static err_t cc_apply_int_setting(cc_handle_t *pHandle, uint_8 bEnable);

/*
 *    Command Line Callbacks
 */
static err_t cc_process_about(cc_handle_t *pHandle);
static err_t cc_process_acc(cc_handle_t *pHandle);
static err_t cc_process_act(cc_handle_t *pHandle);
static err_t cc_process_adc(cc_handle_t *pHandle);
static err_t cc_process_bfr(cc_handle_t *pHandle);
static err_t cc_process_bfw(cc_handle_t *pHandle);
static err_t cc_process_btcon(cc_handle_t *pHandle);
static err_t cc_process_btdebug(cc_handle_t *pHandle);
static err_t cc_process_btkey(cc_handle_t *pHandle);
static err_t cc_process_ccfg(cc_handle_t *pHandle);
static err_t cc_process_cfgclear(cc_handle_t *pHandle);
static err_t cc_process_check_in(cc_handle_t *pHandle);
static err_t cc_process_console(cc_handle_t *pHandle);
static err_t cc_process_print_package(cc_handle_t *pHandle);
static err_t cc_process_data(cc_handle_t *pHandle);
static err_t cc_process_dis(cc_handle_t *pHandle);
static err_t cc_process_rohm(cc_handle_t *pHandle);
static err_t cc_process_fle(cc_handle_t *pHandle);
static err_t cc_process_flr(cc_handle_t *pHandle);
static err_t cc_process_flw(cc_handle_t *pHandle);
static err_t cc_process_help(cc_handle_t *pHandle);
static err_t cc_process_intc(cc_handle_t *pHandle);
static err_t cc_process_intd(cc_handle_t *pHandle);
static err_t cc_process_inte(cc_handle_t *pHandle);
static err_t cc_process_intr(cc_handle_t *pHandle);
static err_t cc_process_ints(cc_handle_t *pHandle);
static err_t cc_process_int_log_dump(cc_handle_t *pHandle);
static err_t cc_process_ior(cc_handle_t *pHandle);
static err_t cc_process_iow(cc_handle_t *pHandle);
//static err_t cc_process_key(cc_handle_t *pHandle);
static err_t cc_process_log(cc_handle_t *pHandle);
static err_t cc_process_ledtest(cc_handle_t *pHandle);
static err_t cc_process_man(cc_handle_t *pHandle);
static err_t cc_process_memw(cc_handle_t *pHandle);
static err_t cc_process_memr(cc_handle_t *pHandle);
static err_t cc_process_persist(cc_handle_t *pHandle);
static err_t cc_process_pcfg(cc_handle_t *pHandle);
static err_t cc_process_pwm(cc_handle_t *pHandle);
static err_t cc_process_pwr(cc_handle_t *pHandle);
static err_t cc_process_prx(cc_handle_t *pHandle);
static err_t cc_process_rgb(cc_handle_t *pHandle);
static err_t cc_process_rfswitch(cc_handle_t *pHandle);
static err_t cc_process_rtcdump(cc_handle_t *pHandle);
static err_t cc_process_scfg(cc_handle_t *pHandle);
static err_t cc_process_sherlock(cc_handle_t *pHandle);
static err_t cc_process_sol(cc_handle_t *pHandle);
static err_t cc_process_testbrd(cc_handle_t *pHandle);
static err_t cc_process_test_console(cc_handle_t *pHandle);
static err_t cc_process_time(cc_handle_t *pHandle);
static err_t cc_process_uid(cc_handle_t *pHandle);
static err_t cc_process_ver(cc_handle_t *pHandle);
static err_t cc_process_wcfg(cc_handle_t *pHandle);
static err_t cc_process_woff(cc_handle_t *pHandle);
static err_t cc_process_wcon(cc_handle_t *pHandle);
static err_t cc_process_wcsen(cc_handle_t *pHandle);
static err_t cc_process_wdis(cc_handle_t *pHandle);
static err_t cc_process_wlist(cc_handle_t *pHandle);
static err_t cc_process_wmac(cc_handle_t *pHandle);
static err_t cc_process_wmiconfig(cc_handle_t *pHandle);
static err_t cc_process_wmon(cc_handle_t *pHandle);
static err_t cc_process_wscan(cc_handle_t *pHandle);
static err_t cc_process_wsend(cc_handle_t *pHandle);
static err_t cc_process_wping(cc_handle_t *pHandle);


#ifdef JIM_LED_TEST
static err_t cc_process_scroll(cc_handle_t *pHandle);
static err_t cc_process_slope(cc_handle_t *pHandle);
#endif  // JIM_LED_TEST

/*
 *    GPIO Callbacks
 */
#if 0
static err_t cc_io_button(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_bt_pwdn(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_mfi_rst(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_rf_sw_vers(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_data_wp(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_data_hold(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_acc_int1(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_acc_int2(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_chg(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_pgood(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_26mhz(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_led(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_wifi_pd(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_wifi_int(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
static err_t cc_io_32khz(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir);
#endif

/*
** ===================================================================
**     Global Array      :  g_cc_cmds
**
**     Description :
**         Table of structures which define each command supported in
**          Corona Console.  This is the place to add/delete commands.
** ===================================================================
*/
static const cc_cmd_t g_cc_cmds[] = \
{
/*
 *   COMMAND       SUMMARY                              MIN ARGS    MANUAL        CALLBACK
 */
    { "help",      "",                                   1, CC_MAN_HELP,             cc_process_help         },
    { "h",         "help",                               1, CC_MAN_HELP,             cc_process_help         },
    { "?",         "help",                               1, CC_MAN_HELP,             cc_process_help         },
    { "about",     "",                                   1, CC_MAN_ABT,              cc_process_about        },
    { "acc",       "",                                   2, CC_MAN_ACC,              cc_process_acc          },
//  { "act",       "activity",                           1, CC_MAN_ACT,              cc_process_act          },
//  { "adc",       "sample ADC",                         4, NULL,                    cc_process_adc          },
//  { "bfr",       "bit field read",                     4, NULL,                    cc_process_bfr          },
//  { "bfw",       "bit field write",                    5, NULL,                    cc_process_bfw          },
//    { "btcon",     "BT Conn",                            2, CC_MAN_BTCON,            cc_process_btcon        },
    { "btdebug",   "",                                   2, CC_MAN_BTDEBUG,          cc_process_btdebug      },
    { "btkey",     "",                                   2, CC_MAN_BTKEY,            cc_process_btkey        },
    { "ccfg",      "",                                   2, CC_MAN_CCFG,             cc_process_ccfg         },
    { "cfgclear",  "",                                   1, CC_MAN_CFGCLEAR,         cc_process_cfgclear     },
    { "checkin",   "",                                   1, CC_MAN_CHECKIN,          cc_process_check_in     },
    { "console",   "",                                   1, CC_MAN_CONSOLE,          cc_process_console     },
    { "data",      "",                                   2, CC_MAN_DATA,             cc_process_data         },
//    { "dis",       "display",                            2, CC_MAN_DIS,              cc_process_dis          },
//    { "rohm",      "ROHM",                               3, CC_MAN_ROHM,             cc_process_rohm         },
//  { "fle",       "erase sector",                       2, NULL,                    cc_process_fle          },
//  { "flr",       "read from SPI Flash mem",            3, NULL,                    cc_process_flr          },
//  { "flw",       "write to SPI Flash mem",             3, NULL,                    cc_process_flw          },
//    { "ints",      "Status of all GPIO interrupts",    1, NULL,                    cc_process_ints         },
    { "ild",      "int log",                             1, CC_MAN_ILD,              cc_process_int_log_dump },
//  { "ledtest",   "led test",                           1, NULL,                    cc_process_ledtest      },
    { "log",       "",                                   1, CC_MAN_LOG,              cc_process_log          },
    { "man",       "",                                   2, CC_MAN_MAN,              cc_process_man          },
//  { "memr",      "read from memory",                   3, NULL,                    cc_process_memr         },
//  { "memw",      "write to memory",                    3, NULL,                    cc_process_memw         },
    { "persist",   "",                                   1, CC_MAN_PERSIST,          cc_process_persist      },
    { "pcfg",      "",                                   1, CC_MAN_PCFG,             cc_process_pcfg         },
    { "printpackage","",                                 1, CC_MAN_PRINT_PACKAGE,    cc_process_print_package},
//  { "pwm",       "change PWM",                         2, NULL,                    cc_process_pwm          },
    { "pwr",       "",                                   1, CC_MAN_PWR,              cc_process_pwr          },
    { "prx",       "",                                   1, CC_MAN_PRX,              cc_process_prx          },
//    { "rgb",       "colors",                             4, CC_MAN_RGB,              cc_process_rgb          },
//    { "rfswitch",  "",                                   3, CC_MAN_RFSWITCH,         cc_process_rfswitch     },
//    { "rtcd",      "",                                   1, CC_MAN_RTCDUMP,          cc_process_rtcdump      },
    { "scfg",      "",                                   3, CC_MAN_SCFG,             cc_process_scfg         },
    { "sher",      "",                                   1, "",                      cc_process_sherlock     },
//    { "sol",       "sign of life",                       1, CC_MAN_SOL,              cc_process_sol          },
    { "testbrd",   "",                                   1, CC_MAN_TESTB,            cc_process_testbrd      },
//    { "time",      "",                                   1, CC_MAN_TIME,             cc_process_time         },
	{ "uid",       "",                                   1, CC_MAN_UID,              cc_process_uid          },
    { "ver",       "",                                   1, CC_MAN_VER,              cc_process_ver          },
    { "wcfg",      "",                                   1, CC_MAN_WCFG,             cc_process_wcfg         },
    { "wcon",      "",                                   1, CC_MAN_WCON,             cc_process_wcon         },
    { "wdis",      "",                                   1, CC_MAN_WDIS,             cc_process_wdis         },
    { "woff",      "",                                   1, CC_MAN_WOFF,             cc_process_woff         },
    { "wlist",     "",                                   1, CC_MAN_WLIST,            cc_process_wlist        },
	//{ "wmon",      "",                                   2, CC_MAN_WMON,             cc_process_wmon         },
#if __ATHEROS_WMICONFIG_SUPPORT__
    { "wmiconfig", "Atheros",                            2, CC_MAN_WMICONFIG,        cc_process_wmiconfig    },
#endif
    { "wscan",     "WiFi",                               1, CC_MAN_WSCAN,            cc_process_wscan        },
//	{ "wping",     "WiFi ping",                          2, CC_MAN_WPING,            cc_process_wping        },
#ifdef JIM_LED_TEST
	{ "scroll",    "",                                   1, CC_MAN_SCROLL,    cc_process_scroll       },
	{ "slope",     "",                                   1, CC_MAN_SLOPE,     cc_process_slope        },
#endif  // JIM_LED_TEST
};

/*
** ===================================================================
**     Global Array      :  g_cc_io_entries
**
**     Description :
**         Table of structures which define each GPIO and its callback.
** ===================================================================
*/
#if 0
static const cc_io_entry_t g_cc_io_entries[] = \
{
/*
 *            TYPE          COMMAND         CALLBACK
 */
    { CC_IO_RO_TYPE, "button",     cc_io_button     },
    { CC_IO_RW_TYPE, "bt_pwdn",    cc_io_bt_pwdn    },
    { CC_IO_RW_TYPE, "mfi_rst",    cc_io_mfi_rst    },
    { CC_IO_RW_TYPE, "rf_sw_vers", cc_io_rf_sw_vers },
    { CC_IO_RW_TYPE, "data_wp",    cc_io_data_wp    },
    { CC_IO_RW_TYPE, "data_hold",  cc_io_data_hold  },
    { CC_IO_RO_TYPE, "acc_int1",   cc_io_acc_int1   },
    { CC_IO_RO_TYPE, "acc_int2",   cc_io_acc_int2   },
    { CC_IO_RO_TYPE, "chg",        cc_io_chg        },
    { CC_IO_RO_TYPE, "pgood",      cc_io_pgood      },
    { CC_IO_RW_TYPE, "26mhz",      cc_io_26mhz      },
    { CC_IO_RW_TYPE, "led",        cc_io_led        },
    { CC_IO_RW_TYPE, "wifi_pd",    cc_io_wifi_pd    },
    { CC_IO_RO_TYPE, "wifi_int",   cc_io_wifi_int   },
    { CC_IO_RW_TYPE, "32khz",      cc_io_32khz      },
};
#endif

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define NUM_CC_COMMANDS           sizeof(g_cc_cmds) / sizeof(cc_cmd_t)
#define NUM_IO_COMMANDS           sizeof(g_cc_io_entries) / sizeof(cc_io_entry_t)
#define CC_SECURITY_0_RESET       "reset"
#define CC_SECURITY_1_PASSWORD    "castle"
#define CC_SECURITY_2_NUM_REQ     "magic"
#define CC_SECURITY_2_KEY         0xFDB97531
#define PAD_RIGHT                 1
#define PAD_ZERO                  2
#define PRINT_BUF_LEN             12 // should be enough for 32 bit int
#define NUM_LBUFS                 8  // Number of line buffers
#define LBUF_SIZE                 32 // Size of each line buffer
#define STDIN_CONSOLE_BUF_SZ      64

#define GPIO_READ(PORT, PIN)    (PORT & mask(PIN, PIN)) >> PIN;
#define GPIO_WRITE(PORT, PIN)   (PORT = mask(PIN, PIN))

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! \brief Provide a mask given bit range.
//!
//! \fntype Reentrant Function
//!
//! \detail Give a mask corresponding to msb:lsb.
//!
//! \return 32-bit mask corresponding to msb:lsb.
//!
///////////////////////////////////////////////////////////////////////////////
uint_32 mask(uint_32 msb, uint_32 lsb)
{
    uint_32 ret     = 0;
    uint_32 numOnes = msb - lsb + 1;

    if (msb < lsb)
    {
        return 0;
    }

    while (numOnes-- > 0)
    {
        ret = 1 | (ret << 1);
    }

    return(ret << lsb);
}

/*
 *   Initialize the console.
 */
void CONSOLE_init(void)
{
    _lwevent_create( &g_console_lwevent, LWEVENT_AUTO_CLEAR);
    _queue_init(&g_shlk_qu, MAX_SHER_QU_ELEMS);

     cc_init(&g_handle, NULL, CC_TYPE_JTAG);
}

/*
 *   Copy a string into the caller's buffer, not to exceed max_len.
 */
static sh_u8_t _shlk_output_time_string(char *pStr, sh_u32_t max_len)
{
    DATE_STRUCT     time_date;
    TIME_STRUCT     time_sec_ms;
    int             ret;

    UTIL_utc_if_possible_time_struct(&time_sec_ms);
    _time_to_date(&time_sec_ms, &time_date);

	ret = snprintf( pStr, (size_t)max_len, "\n\n\tT:\t%02d/%02d/%4d %02d:%02d:%02d\n\n",
										   time_date.MONTH,
										   time_date.DAY,
										   time_date.YEAR,
										   time_date.HOUR,
										   time_date.MINUTE,
										   time_date.SECOND );
	
	if( (sh_u32_t)ret > max_len )
	{
		return 0;
	}
	else if( ret < 0 )
	{
		return 0;
	}
	else
	{
		return (sh_u8_t)ret;
	}
}

/* 
 * Callback for the sherlock flush timer
 */
static void _shlk_flush_timeout_callback( void )
{
    // Invalidate the timer handle
    g_sher_flush_alarm = 0;
    // Don't VoteForON here since it can hold things up during shutdown
    //PWRMGR_VoteForON( PWRMGR_SHERLOCK_VOTE );  // here guarantees task runs
    _lwevent_set(&g_console_lwevent, EVENT_CONSOLE_SHERLOCK_FLUSH);
}

/*
 *   Kill the Sherlock flush timer
 */
static void _shlk_kill_flush_timer( void )
{
	RTC_cancel_alarm( &g_sher_flush_alarm );
}   

/*
 *   Start a timer that will flush Sherlock when it expires
 */
static void _shlk_start_flush_timer( void )
{
    // If the timer is already running, kill it now 
    _shlk_kill_flush_timer();
    
    // Restart the timer
    g_sher_flush_alarm = RTC_add_alarm_in_seconds( SHERLOCK_FLUSH_TIMEOUT, (rtc_alarm_cbk_t) _shlk_flush_timeout_callback );
    wassert( g_sher_flush_alarm );
}

/*
 *   Handle flushing a sherlock qu element out to a console of some sort,
 *      depending on configuration.
 */
static void _print_qu_elem(shlk_qu_elem_ptr_t pQuElem)
{
    if( !g_st_cfg.cc_st.enable )
    {
        /*
         *   Only exception will be if the console is the one doing the printing.
         *     This will only be true if console_task_print is non-zero.
         */
        if( g_console_task_print && (_task_get_id() == g_console_task_print) ) 
        {
        }
        else
        {
            return;    // printing is disabled and this is not the console, so don't do it !
        }
    }

    _io_write(stdout, pQuElem->pData, pQuElem->len);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Low priority offload task for writing strings to Sherlock.
///////////////////////////////////////////////////////////////////////////////
void SHLK_tsk(uint_32 dummy)
{
    _mqx_uint mask;
    shlk_qu_elem_ptr_t pQuElem = NULL;
    char pageBuffer[EXT_FLASH_PAGE_SIZE];
    shlk_err_t shlk_err;
    static int numPageBytes = 0;
    
    while(1)
    {
        /*
         *   Queue must be empty, so we can vote for sleep now.
         */
    	PWRMGR_VoteForOFF( PWRMGR_SHERLOCK_VOTE );
        PWRMGR_VoteForSleep( PWRMGR_SHERLOCK_VOTE );
        
        _watchdog_stop();
        
        _lwevent_wait_ticks(   &g_console_lwevent, 
                               EVENT_CONSOLE_SHERLOCK_NODE_AVAILABLE |
                               EVENT_CONSOLE_SHERLOCK_FLUSH,
                               FALSE, 
                               0);
        
        /*
         *   Make sure idle loop doesn't cause us to go to sleep in the middle
         *     of trying to use SPI Flash.
         */
        PWRMGR_VoteForAwake( PWRMGR_SHERLOCK_VOTE );
        PWRMGR_VoteForON( PWRMGR_SHERLOCK_VOTE );
               
        mask = _lwevent_get_signalled();
        
        if ( mask & EVENT_CONSOLE_SHERLOCK_NODE_AVAILABLE )
        {
            while( NULL != (pQuElem = (shlk_qu_elem_ptr_t) _queue_dequeue(&g_shlk_qu)) )
            {
                // Copy bytes from the queue to the page buffer            
                _watchdog_start(60*1000);

                if ( (numPageBytes > 0) && (numPageBytes + pQuElem->len > EXT_FLASH_PAGE_SIZE) )
                {
                     // Time to flush the page buffer to flash
                    _shlk_kill_flush_timer();
                    shlk_err = SHLK_log(&(g_handle.shlk_hdl), (char*) pageBuffer, numPageBytes);
                    if(SHLK_ERR_OK != shlk_err)
                    {
                        PROCESS_NINFO_NO_PRINT(ERR_CC_SHERLOCK_LOG_FAIL, "%i", shlk_err);
                    }
                    numPageBytes = 0;
                }

                // Copy queue bytes to the page buffer
                wassert( (numPageBytes+pQuElem->len) <= EXT_FLASH_PAGE_SIZE ); // make sure we do not go out of bounds on this memcpy.
                
                _print_qu_elem(pQuElem);
                
                memcpy( &pageBuffer[numPageBytes],  (char *) pQuElem->pData, pQuElem->len );  
                numPageBytes += pQuElem->len;
                _lwmem_free(pQuElem);
            }
            
            /*
             *  Start the flush timer if there are bytes left in the page buffer
             *    and the timer hasn't already started 
             */ 
            if ( (numPageBytes > 0) && (g_sher_flush_alarm == 0))
            {
                 _shlk_start_flush_timer();                
            }
        }
        
        if ( mask & EVENT_CONSOLE_SHERLOCK_FLUSH )
        {
            /*
             *  If the page buffer isn't empty, flush it to flash 
             */
            if ( numPageBytes > 0 )
            {
                _shlk_kill_flush_timer();
                shlk_err = SHLK_log(&(g_handle.shlk_hdl), (char*) pageBuffer, numPageBytes);
                if( SHLK_ERR_OK != shlk_err )
                {
                    PROCESS_NINFO_NO_PRINT(ERR_CC_SHERLOCK_LOG_FAIL, "%i", shlk_err);
                }
                numPageBytes = 0;
            }
        }
    }
}

/*
 *   Convert arbitrary stream of bytes to a string of hex characters that is NULL terminated.
 *     Note:  You must provide a buffer that is at least (2x + 1) the length in bytes of your data array.
 */
void bytes_to_string(const void *pData, const unsigned int num_bytes, char *pOutput)
{
    const unsigned char *pByte = pData;
    unsigned int i;
    
    memset(pOutput, 0, num_bytes*2 + 1);
    
    for(i=0; i<num_bytes; i++)
    {
        sprintf(&pOutput[i*2], "%.2x", pByte[i]);
    }
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Main entry point for launching the USB console.
///////////////////////////////////////////////////////////////////////////////
void CNSL_tsk(uint_32 dummy)
{
    char        buf[STDIN_CONSOLE_BUF_SZ];
    uint32_t    idx = 0;

    while(1)
    {   
        memset(buf, 0, STDIN_CONSOLE_BUF_SZ);
        printf("]");
        gets((char *)buf);
        
        /*
         * On any input, disable LLS deep sleep for the boot session to allow console operation 
         */
        PWRMGR_VoteForAwake( PWRMGR_CONSOLE_VOTE );
        
        cc_process_cmd(&g_handle, buf);
        
        if( ! g_st_cfg.cc_st.enable )
        {   
            printf("DISABLED Use 'console 1'\t");
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Initializes a handle for a Corona Console.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function initializes a cc_handle_t for usage in managing a Corona console instance.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
err_t cc_init(cc_handle_t *pHandle, void *pDevice, console_type_t type)
{
    pHandle->type = (uint_8)type;
    
    switch (type)
    {
        case CC_TYPE_USB:
        case CC_TYPE_JTAG:  // USB and JTAG don't need special handles
            break;
            
        default:
            return ERR_GENERIC;
    }
    
    /*
     *   Init Sherlock, and check to see if it's enabled.
     */
    SHLK_init(      &(pHandle->shlk_hdl),
                    wson_read,
                    wson_write,
                    shlk_printf,
                    EXT_FLASH_SHLK_ASSDUMP_ADDR,
                    EXT_FLASH_SHLK_LOG_ADDR,
                    (sh_u8_t)EXT_FLASH_SHLK_LOG_NUM_SECTORS      );
    
    SHLK_set_time_cbk( &(pHandle->shlk_hdl), _shlk_output_time_string, 4 );

    g_console_is_up = 1;
    corona_print("SHLK en: %u\n", SHLK_is_enabled(&(pHandle->shlk_hdl), EXT_FLASH_SHLK_EN_ADDR) );
    
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Process a command in the Corona Console and provide necessary feedback.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Given a handle and a character string from the user, this API will take care of
//! piping things to the appropriate command for processing, as well as outputting results to the user.
//! Other than cc_init(), this is the main API clients interact with.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
err_t cc_process_cmd(cc_handle_t *pHandle, char *pCmd)
{
    int i;
    err_t err = ERR_OK;

    // Break up the commands into substrings and populate our structures.
    cc_split_str(pHandle, pCmd);

    if (pHandle->argc == 0)
    {
        err = ERR_GENERIC; // no arguments given.
        goto DONE;
    }

    for (i = 0; i < pHandle->argc; i++)
    {
        trim_whitesp(pHandle->argv[i]); // get rid of any newlines or garbage that could screw up our number/string conversions.
    }

    printf("\n");
    
    // Is the string something we can understand?  If so, call it's callback, return status.
    for (i = 0; i < NUM_CC_COMMANDS; i++)
    {
        if (!strcmp(pHandle->argv[0], g_cc_cmds[i].cmd))
        {
            // Make sure the correct number or minimum arguments were passed in for this command.
            if (pHandle->argc < g_cc_cmds[i].minArgs)
            {
                printf("\nmore args\n\n");
                err = ERR_GENERIC;
                goto DONE;
            }
            
            /*
             *   If this is non-zero, we'll check the task ID to assure we are only printing stuff that originates on
             *      this task's context, particularly for the case when console == 0.
             */
            g_console_task_print = _task_get_id();
            
            err = g_cc_cmds[i].callback(pHandle);
            
            g_console_task_print = 0;
            
            printf("\n");
            goto DONE;
        }
    }
    
    printf(UNSUPPORTED_COMMAND);
    
DONE:
    return err;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Split user string into argv and argc style for processing.
//!
//! \fntype Reentrant Function
//!
///////////////////////////////////////////////////////////////////////////////
static void cc_split_str(cc_handle_t *pHandle, char *pCmd)
{
    char *pScan = pCmd;
    int  i;

    pHandle->argc = 0;

    for (i = 0; i < CC_MAX_ARGS; i++)
    {
        // Skip leading whitespace
        while (isspace(*pScan))
        {
            pScan++;
        }

        if (*pScan != '\0')
        {
            pHandle->argv[pHandle->argc++] = pScan;
        }
        else
        {
            pHandle->argv[pHandle->argc] = 0;
            break;
        }

        // Scan over arg.
        while (*pScan != '\0' && !isspace(*pScan))
        {
            pScan++;
        }

        // Terminate arg.
        if ((*pScan != '\0') && (i < CC_MAX_ARGS - 1))
        {
            *pScan++ = '\0';
        }
    }
}

/*
 *   Callback to use for Sherlock to print to.
 *     The sherlock callback is purely a string, so no need to support the overhead
 *     of va_args, tokens, and the like.  Just print the string directly.
 */
void shlk_printf(const char *pStr, uint_32 len)
{
    _io_fputs(pStr, stdout);
}


/*
 *   SHLK flush is a public function to flush the sherlock page buffer to flash
 */
void shlk_flush( void )
{
    // Don't VoteForON here since it can hold things up during shutdown
    //PWRMGR_VoteForON( PWRMGR_SHERLOCK_VOTE );  // here guarantees task runs
    _lwevent_set(&g_console_lwevent, EVENT_CONSOLE_SHERLOCK_FLUSH);
}


/*
 *   Corona Print pipes output to different places, depending on RELEASE/BUILD configs.
 */
err_t _corona_print(const char *format, va_list ap)
{    
    shlk_qu_elem_ptr_t pQuElem;
    char sherlock_buf[MAX_SHER_STR+1];
    int temp_len; 
    
    /*
     *    Process and queue up the formatted string,
     *       to be logged and flushed to the UART in Sherlock's context.
     */
    temp_len = vsnprintf(sherlock_buf, MAX_SHER_STR, format, ap);
    if(temp_len <= 0)
    {
        return ERR_VSNPRINTF; //PROCESS_NINFO_NO_PRINT(ERR_VSNPRINTF, NULL);
    }
    
    temp_len = ( temp_len >= MAX_SHER_STR ) ? MAX_SHER_STR : temp_len; // limit the size of a Sherlock element.
    
    /*
     *   Capture this output, queue it up, and log it to Sherlock in our background task.
     */
    pQuElem = _lwmem_alloc_system( sizeof(shlk_qu_elem_t) + temp_len + 1 );
    if( NULL == pQuElem )
    {
        return ERR_MEM_ALLOC;
    }
    
    pQuElem->len = temp_len;
    
    memcpy(pQuElem->pData, sherlock_buf, pQuElem->len);
    pQuElem->pData[pQuElem->len] = 0;  // ensure NULL termination.

    if( TRUE == _queue_enqueue(&g_shlk_qu, (QUEUE_ELEMENT_STRUCT_PTR) pQuElem) )
    {
        // Don't VoteForON here since it can hold things up during shutdown
        //PWRMGR_VoteForON( PWRMGR_SHERLOCK_VOTE );  // here guarantees task runs
        _lwevent_set(&g_console_lwevent, EVENT_CONSOLE_SHERLOCK_NODE_AVAILABLE);
        return ERR_OK;
    }

    /*
     *   Oops. The Sherlock qu is full, go ahead and stop here, so we don't overflow
     *            the system with a bunch of Sherlock queue elements building up.
     */
    _lwmem_free(pQuElem);
    return ERR_CC_SHERLOCK_QU_FULL; //PROCESS_NINFO_NO_PRINT(ERR_CC_SHERLOCK_QU_FULL, NULL);
}

/*
 *   The grand kahuna.
 */
err_t corona_print(const char *format, ...)
{   
    va_list ap;
    err_t err;
    
#if PWRMGR_BREADCRUMB_ENABLED > 1
    return ERR_OK;
#endif
    
    if( 0 == g_console_is_up )
    {
        return ERR_OK;
    }
    
    va_start(ap, format);
    err = _corona_print(format, ap);
    va_end(ap);

	return err;
}

#if PWRMGR_BREADCRUMB_ENABLED
err_t pwr_corona_print(const char *format, ...)
{   
    va_list ap;
    err_t err;

    va_start(ap, format);
    err = _corona_print(format, ap);
    va_end(ap);

	return err;
}
#endif

#ifdef USB_ENABLED
static int print(cc_handle_t *pHandle, char **out, const char *format, va_list args)
{
    register int width, pad;
    register int pc = 0;
    char         scr[2];
    
    for ( ; *format != 0; ++format)
    {
        if (*format == '%')
        {
            ++format;
            width = pad = 0;
            if (*format == '\0')
            {
                break;
            }
            if (*format == '%')
            {
                goto out;
            }
            if (*format == '-')
            {
                ++format;
                pad = PAD_RIGHT;
            }
            while (*format == '0')
            {
                ++format;
                pad |= PAD_ZERO;
            }
            for ( ; *format >= '0' && *format <= '9'; ++format)
            {
                width *= 10;
                width += *format - '0';
            }
            if (*format == 's')
            {
                register char *s = (char *)va_arg(args, int);
                pc += prints(pHandle, out, s ? s : "(null)", width, pad);
                continue;
            }
            if ( (*format == 'd') || (*format == 'i') )
            {
                pc += printi(pHandle, out, va_arg(args, int), 10, 1, width, pad, 'a');
                continue;
            }
            if (*format == 'h')
            {
                pc += printi(pHandle, out, va_arg(args, int_8), 10, 1, width, pad, 'a');
                continue;
            }
            if (*format == 'x')
            {
                pc += printi(pHandle, out, va_arg(args, int), 16, 0, width, pad, 'a');
                continue;
            }
            if (*format == 'X')
            {
                pc += printi(pHandle, out, va_arg(args, int), 16, 0, width, pad, 'A');
                continue;
            }
            if (*format == 'u')
            {
                pc += printi(pHandle, out, va_arg(args, unsigned long), 10, 0, width, pad, 'a');
                continue;
            }
            if (*format == 'c')
            {
                /* char are converted to int then pushed on the stack */
                scr[0] = (char)va_arg(args, int);
                scr[1] = '\0';
                pc    += prints(pHandle, out, scr, width, pad);
                continue;
            }
        }
        else
        {
out:
            printchar(pHandle, out, *format);
            if(*format == '\n')
            {
                // Don't make everyone have to use '\r' all over the place in the code.
                //  Instead, just put a line feed next to the new line, so we start fresh.
                printchar(pHandle, out, '\r');
            }
            ++pc;
        }
    }
    if (out)
    {
        **out = '\0';
    }
    va_end(args);
    return pc;
}
#endif

#ifdef USB_ENABLED
static void printchar(cc_handle_t *pHandle, char **str, int c)
{
    if (str)
    {
        **str = c;
        ++(*str);
    }
    else
    {
        /*
         *    NOTE:  This is the one part of the Corona Console that is assuming some underlying hardware.
         *           Thus, is the portion that would need to change depending on what behavior is needed.
         */
        uint_8 aChar = c;

        switch (pHandle->type)
        {
            case CC_TYPE_USB:
                usb_printchar(&aChar);
                break;
    
            default:
                wassert(0);
        }
    }
}
#endif

#ifdef USB_ENABLED
// Collect printchar's output
static void usb_printchar(uint_8 *oneChar)
{
    static uint_8 lbuf[NUM_LBUFS][LBUF_SIZE];
    static uint_8 *active_lptr      = &lbuf[0][0];
    static int    active_lbuf_index = 0;
    static int    line_idx          = 0;
    uint_8        err;
    int           c;
    int           retries = 5;
    static uint32_t g_usb_busy_debug = 0; // global to see where we're at with DEVICE_BUSY's..

    c = *oneChar;
    active_lptr[line_idx++] = c;

    if( (c == (int)'\n') || (line_idx == LBUF_SIZE) )
    {
        do
        {
            _time_delay(1); // very stupid delay, should never actually be needed, but USB_BUSY seems to be screw-ee.
            err = usb_write(&active_lptr[0], line_idx);
            if ((err == 0) || (retries-- == 0))
            {
                break;
            }

            if (err == 0xC1 /*USBERR_DEVICE_BUSY*/)
            {
                g_usb_busy_debug++;
                _time_delay(1);
            }
        } while (err != 0 /*USB_OK*/);

        if (err != 0 /*USB_OK*/)
        {
            printf("ERR (%x)! - Bytes TX'd did != line_idx!", err);
        }
        line_idx = 0;
        active_lbuf_index++;
        active_lbuf_index %= NUM_LBUFS;
        active_lptr        = &lbuf[active_lbuf_index][0];
    }
}
#endif

#ifdef USB_ENABLED
static int prints(cc_handle_t *pHandle, char **out, const char *string, int width, int pad)
{
    register int pc = 0, padchar = ' ';

    if (width > 0)
    {
        register int        len = 0;
        register const char *ptr;
        for (ptr = string; *ptr; ++ptr)
        {
            ++len;
        }
        if (len >= width)
        {
            width = 0;
        }
        else
        {
            width -= len;
        }
        if (pad & PAD_ZERO)
        {
            padchar = '0';
        }
    }
    if (!(pad & PAD_RIGHT))
    {
        for ( ; width > 0; --width)
        {
            printchar(pHandle, out, padchar);
            ++pc;
        }
    }
    for ( ; *string; ++string)
    {
        printchar(pHandle, out, *string);
        ++pc;
    }
    for ( ; width > 0; --width)
    {
        printchar(pHandle, out, padchar);
        ++pc;
    }

    return pc;
}
#endif

#ifdef USB_ENABLED
static int printi(cc_handle_t *pHandle, char **out, int i, int b, int sg, int width, int pad, int letbase)
{
    char                  print_buf[PRINT_BUF_LEN];
    register char         *s;
    register int          t, neg = 0, pc = 0;
    register unsigned int u = i;

    if (i == 0)
    {
        print_buf[0] = '0';
        print_buf[1] = '\0';
        return prints(pHandle, out, print_buf, width, pad);
    }

    if (sg && (b == 10) && (i < 0))
    {
        neg = 1;
        u   = -i;
    }

    s  = print_buf + PRINT_BUF_LEN - 1;
    *s = '\0';

    while (u)
    {
        t = u % b;
        if (t >= 10)
        {
            t += letbase - '0' - 10;
        }
        *--s = t + '0';
        u   /= b;
    }

    if (neg)
    {
        if (width && (pad & PAD_ZERO))
        {
            printchar(pHandle, out, '-');
            ++pc;
            --width;
        }
        else
        {
            *--s = '-';
        }
    }

    return pc + prints(pHandle, out, s, width, pad);
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'intc' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Clear the IRQ count for a given IRQ.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_intc(cc_handle_t *pHandle)
{
#if 0
    if (ciu_clr_str(pHandle->argv[1]) != ERR_OK)
    {
        corona_print((pHandle, "ERR: %s is !valid IRQ name\n\r", pHandle->argv[1]);
        return CC_IRQ_UNSUPPORTED;
    }

    return corona_print( "%s IRQ Count: 0\n\r", pHandle->argv[1]);
#else
    return ERR_GENERIC;
#endif
}

static err_t cc_process_int_log_dump(cc_handle_t *pHandle)
{
#if INT_LOGGING
	wint_dump();
	return ERR_OK;
#else
	corona_print("Disabled. #define INT_LOGGING 1 in user_config.h\n");
    return ERR_GENERIC;
#endif
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Process enabling/disabling of one or all GPIO interrupts.
//!
//! \fntype Reentrant Function
//!
//! \detail Handle processing of enabling/disable a GPIO interrupt or all of them.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_apply_int_setting(cc_handle_t *pHandle, uint_8 bEnable)
{
#if 0
    isr_uentry_t        *pEntry;
    volatile isr_irqc_t irqc;

    if (!strcmp(pHandle->argv[1], "all"))
    {
        int i = 0;

        do
        {
            if (bEnable)
            {
                irqc = g_uEntries[i].irqc;
            }
            else
            {
                irqc = IRQC_DISABLED;
            }

            ciu_status(&(g_uEntries[i]), &irqc);
            corona_print( "(%s)\tstatus:\t%x\t(%s)\n\r", g_uEntries[i].pName, irqc, ciu_status_str(&irqc));
        } while (g_uEntries[++i].idx < INT_Reserved119);

        corona_print( "\n\r");
    }
    else
    {
        pEntry = get_uentry_str(pHandle->argv[1]);
        if (!pEntry)
        {
            return corona_print("ERR: %s is not a valid ISR\n", pHandle->argv[1]);
        }

        if (bEnable)
        {
            irqc = pEntry->irqc;
        }
        else
        {
            irqc = IRQC_DISABLED;
        }
        ciu_status(pEntry, &irqc);
        corona_print("(%s)\tstatus:\t%x\t(%s)\n\r", pEntry->pName, irqc, ciu_status_str(&irqc));
    }
#else
    corona_print(UNSUPPORTED_COMMAND);
#endif
    return ERR_OK;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'intd' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Disable a GPIO interrupt.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_intd(cc_handle_t *pHandle)
{
#if 0
    return cc_apply_int_setting(pHandle, FALSE);
#else
    return ERR_GENERIC;
#endif
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'inte' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Enable a GPIO interrupt.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_inte(cc_handle_t *pHandle)
{
#if 0
    return cc_apply_int_setting(pHandle, TRUE);
#else
    return ERR_GENERIC;
#endif
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'intr' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read the IRQ count for a given IRQ.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_intr(cc_handle_t *pHandle)
{
#if 0
    uint_32 count;
    if (ciu_cnt_str(pHandle->argv[1], &count) != ERR_OK)
    {
        corona_print("ERR: %s is !valid IRQ name\n\r", pHandle->argv[1]);
        return CC_IRQ_UNSUPPORTED;
    }

    return corona_print("%s IRQ Count: %d\n\r", pHandle->argv[1], count);
#else
    return ERR_GENERIC;
#endif
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'ints' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Print status of all GPIO interrupts.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_ints(cc_handle_t *pHandle)
{
#if 0
    int                 i = 0;
    volatile isr_irqc_t irqc;

    do
    {
        irqc = IRQC_REQUEST_STATUS;

        ciu_status(&(g_uEntries[i]), &irqc);
        corona_print("(%s)\tstatus:\t%x\t(%s)\n\r", g_uEntries[i].pName, irqc, ciu_status_str(&irqc));
    } while (g_uEntries[++i].idx < INT_Reserved119);

    corona_print("\n\r");
#else
    return ERR_GENERIC;
#endif
    return ERR_OK;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'key' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Process handling of the 'key' command, which is used to change
//! the security level for this console session.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_key(cc_handle_t *pHandle)
{
    static volatile uint_32 randomNumber = 0xDEADBEEF;

    if (!strcmp(pHandle->argv[1], CC_SECURITY_1_PASSWORD))
    {
        corona_print("Security 1 Granted\n\r");
        pHandle->security = CC_SECURITY_1;
    }
    else if (!strcmp(pHandle->argv[1], CC_SECURITY_2_NUM_REQ))
    {
        corona_print("0x%d\n", (randomNumber ^ (uint_32)pHandle));
    }
    else if (atoi(pHandle->argv[1]) == (randomNumber ^ CC_SECURITY_2_KEY))
    {
        corona_print("Security 2 Granted\n\r", randomNumber);
        pHandle->security = CC_SECURITY_2;
    }
    else if (!strcmp(pHandle->argv[1], CC_SECURITY_0_RESET))
    {
        corona_print("Reset to Security 0\n\r");
        pHandle->security = CC_SECURITY_0;
    }
    else
    {
        corona_print(BAD_ARGUMENT);
    }

    randomNumber ^= (uint_32)pHandle; // make number more random each time.
    
    return ERR_OK;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'ledtest' command.
//!
//! \detail Cycle through the LED colors
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_ledtest(cc_handle_t *pHandle)
{
#if 0
    corona_print("***LED Test***\n  Red\n");
    led_run_pattern (LED_PATTERN_1);
    _time_delay(1000);
    
    corona_print("  White\n");
    led_run_pattern (LED_PATTERN_2);
    _time_delay(1000);
    
    corona_print("  Blue\n");
    led_run_pattern (LED_PATTERN_3);
    _time_delay(1000);
    
    corona_print("  Green\n");
    led_run_pattern (LED_PATTERN_4);
    _time_delay(1000);
    
    corona_print("  Yellow\n");
    led_run_pattern (LED_PATTERN_5);
    _time_delay(1000);
    
    corona_print("  [OFF]\n");
    led_run_pattern (LED_PATTERN_OFF);
#else
    corona_print(UNSUPPORTED_COMMAND);
#endif
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'log' command.
//!
//! \detail Control the logging output
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_log(cc_handle_t *pHandle)
{
    if (!strcmp(pHandle->argv[1], "wkup"))
    {
        if (pHandle->argc < 3)
        {
            return corona_print("%d\n", g_st_cfg.cc_st.log_wakeups);
        }
        g_st_cfg.cc_st.log_wakeups = (uint_8)strtoul(pHandle->argv[2], NULL, 10);
        corona_print("%d\n", g_st_cfg.cc_st.log_wakeups);
        CFGMGR_flush( CFG_FLUSH_STATIC );
    } else
    {
        return corona_print(BAD_ARGUMENT);
    }
    return ERR_OK;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'sol' command.
//!
//! \detail Sign of Life: Flash LEDs periodically
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_sol(cc_handle_t *pHandle)
{
    uint32_t period = 10000;
    
    if (pHandle->argc == 2)
    {
        period = (atoi(pHandle->argv[1]) * 1000);
    }
    while(1)
    {
        led_run_pattern (LED_PATTERN_4);    // Green
        _time_delay(1000);
        led_run_pattern (LED_PATTERN_OFF);
        _time_delay(period);
    }

    return ERR_OK;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'pwr' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Process handling of the 'pwr' command, which is used to change
//! the power state of the system.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_pwr(cc_handle_t *pHandle)
{
	int retval;
	
	// For now args, just display the current power mode
	if ( pHandle->argc == 1 )
	{
		if ( 4 == SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK )
		{
			corona_print("VLPR\n");
		}
		else
		{
			corona_print("RUNM\n");
		}
	    return ERR_OK;
	}
	    
    // \note Only SHIP mode is needed in the bootloader diags.
    if (!strcmp(pHandle->argv[1], "ship"))
    {
        PWRMGR_Ship( FALSE );
    }
    else if (!strcmp(pHandle->argv[1], "reboot"))
    {
        if (pHandle->argc < 3)
        {
            return corona_print(BAD_ARGUMENT);
        }   
        
        PWRMGR_Reboot( atoi( pHandle->argv[2]), PV_REBOOT_REASON_USER_REQUESTED, TRUE );
    }
    else if (!strcmp(pHandle->argv[1], "vlpr"))
    {
    	if ( PWRMGR_VoteForVLPR( PWRMGR_CONSOLE_VOTE ) == ERR_OK )
    	{
    	    corona_print("VLPR\n");
    	}
    	else
    	{
    	    WPRINT_ERR(NULL);
    	}
    }
    else if (!strcmp(pHandle->argv[1], "runm"))
    {  
    	if ( PWRMGR_VoteForRUNM( PWRMGR_CONSOLE_VOTE ) == ERR_OK )
    	{
    	    corona_print("RUNM\n");
    	}
    	else
    	{
    	    WPRINT_ERR("->RUNM");
    	}
    }
    else if (!strcmp(pHandle->argv[1], "rfoff"))
    {
        _time_delay(100);
        PWRMGR_radios_OFF();
    }
    else if (!strcmp(pHandle->argv[1], "rfon"))
    {
        _time_delay(100);
        PWRMGR_radios_ON();
    }
    else if (!strcmp(pHandle->argv[1], "votes"))
    {
    	uint32_t awake, on, runm;
    	
    	PWRMGR_get_pwr_votes(&awake, &on, &runm);
        corona_print("Awk 0x%x On 0x%x RUNM 0x%x\n", awake, on, runm);
    }
    else if (!strcmp(pHandle->argv[1], "use_lls"))
    {
        if ( pHandle->argc < 3 )
        {
            return corona_print("use_lls = %d\n", g_st_cfg.pwr_st.enable_lls_deep_sleep );
        }
        if ( strtoul(pHandle->argv[2], NULL, 10) )
        {                
            g_st_cfg.pwr_st.enable_lls_deep_sleep = 1;
            CFGMGR_flush( CFG_FLUSH_STATIC );
            PWRMGR_VoteForSleep( PWRMGR_CONSOLE_VOTE );
            PWRMGR_VoteForSleep( PWRMGR_PWRMGR_VOTE );
            corona_print("LLS ON\n");
            
            // Wait for LLS to be entered in the idle loop
            _time_delay(100);
        }
        else 
        {
            g_st_cfg.pwr_st.enable_lls_deep_sleep = 0;
            CFGMGR_flush( CFG_FLUSH_STATIC );
            PWRMGR_VoteForAwake( PWRMGR_CONSOLE_VOTE );
            corona_print("LLS OFF\n");
            _time_delay(100);
        }     
    }
    else
    {
        WPRINT_ERR(BAD_ARGUMENT);
    }
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'prx' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Process handling of the 'prx' command, which is used to change
//! the proximity timeout.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_prx(cc_handle_t *pHandle)
{
    if(pHandle->argc < 2)
    {
        goto DONE;
    }

    PERMGR_set_timeout(strtoul(pHandle->argv[1], NULL, 10));
    
DONE:
    corona_print("PRX TO: %d\n", g_st_cfg.per_st.poll_interval);
    return ERR_OK;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'rfswitch' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Set which radio has access to the antenna via the rf switch.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_rfswitch(cc_handle_t *pHandle)
{
	boolean on = FALSE;
	
	if(!strcmp(pHandle->argv[2], "on"))
	{
		on = TRUE;
	}
	
    if(!strcmp(pHandle->argv[1], "bt"))
    {
    	rf_switch(RF_SWITCH_BT, on);
        corona_print("Set rf switch to pass BT to the antenna\n");
    }
    else if (!strcmp(pHandle->argv[1], "wifi"))
    {
    	rf_switch(RF_SWITCH_WIFI, on);
        corona_print("Set rf switch to pass WiFi to the antenna\n");
    }
    else
    {
        corona_print("%s: %s bad\n", ERR_STRING, pHandle->argv[1]);
    }
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'rtcdump' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Dump the RTC queue
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_rtcdump(cc_handle_t *pHandle)
{
	int period;
	int cycles;
	int i;
	
    if (pHandle->argc < 2)
    {
    	RTC_alarm_queue_debug();
    	goto DONE;
    }
    else if (pHandle->argc != 3)
    {
    	corona_print(CC_MAN_RTCDUMP);
    	goto DONE;
    }
    
    period = strtoul(pHandle->argv[1], NULL, 10);
    cycles = strtoul(pHandle->argv[2], NULL, 10);

	RTC_alarm_queue_debug();

	for (i = 1; i < cycles; ++i)
	{
    	_time_delay(1000*period);
    	RTC_alarm_queue_debug();
	}
	
DONE:
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'cc_process_time' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Display system time info.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_time(cc_handle_t *pHandle)
{
    RTC_TIME_STRUCT rtc_time;
    TIME_STRUCT mqx_time;
    
    _rtc_get_time(&rtc_time);
    _time_get(&mqx_time);
    
    corona_print("RTC Time: \t%d s\n", rtc_time.seconds);
    corona_print("MQX Time: \t%d s\t %d\n", mqx_time.SECONDS, mqx_time.MILLISECONDS);
    
    
    UTIL_time_show_rtc_time();
    UTIL_time_show_mqx_time();
    
    return ERR_OK;
}
#endif

#if 1
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'uid' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Print out the counts value for the UID pin and what we think it is.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_uid(cc_handle_t *pHandle)
{
	int i = 0;
	uint32_t average = 0;
	cable_t cable;
	
	while(i++ < 3)
    {
		average += adc_sample_uid();
    	_time_delay(10);
    }
	
	average = average / 3;
	corona_print("Counts: %u\n", average);
	cable = adc_get_cable(average);
	
	corona_print("Cable: ");
	switch(cable)
	{
		case CABLE_BURNIN:
			corona_print("burn in\n");
			break;
			
		case CABLE_FACTORY_DBG:
			corona_print("factory dbg\n");
			break;
			
		case CABLE_CUSTOMER:
			corona_print("customer\n");
			break;
			
		default:
			corona_print("ERR\n");
	}
	
	return ERR_OK;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'man' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Present the manual for the current command associated with pHandle.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_man(cc_handle_t *pHandle)
{
    int i;

    for (i = 0; i < NUM_CC_COMMANDS; i++)
    {
        if (!strcmp(pHandle->argv[1], g_cc_cmds[i].cmd))
        {
            corona_print(g_cc_cmds[i].man);
            return ERR_OK;
        }
    }
    corona_print("\nno manual\n");
    return ERR_GENERIC;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'bfr' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read a bitfield from a specific 32-bit address anywhere in memory.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_bfr(cc_handle_t *pHandle)
{
#if 0
    uint_32 *pAddr;
    uint_32 bitfield;
    int     idx = 0;
    int     lsb, msb;

    pAddr = (uint_32 *)strtoul(pHandle->argv[1], NULL, 16);
    if (pAddr == NULL)
    {
        return corona_print("ERR: fail addr->int\n");
    }

    msb      = atoi(pHandle->argv[2]);
    lsb      = atoi(pHandle->argv[3]);
    bitfield = (mask(msb, lsb) & *pAddr) >> lsb;

    corona_print("ADDR: %x\n", pAddr);
    corona_print("Value <31:0>:\t%x\n", *pAddr);
    corona_print("Value <%d:%d>:\t%x\n\n", msb, lsb, bitfield);
#else
    corona_print(UNSUPPORTED_COMMAND);
#endif
    return ERR_OK;
}


#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'ior' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read GPIO(s).
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_ior(cc_handle_t *pHandle)
{
    return cc_process_io(pHandle, CC_IO_READ);
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'pwm' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Set the PWM duty cycle from 0-100%
//!
//! \return err_t error code
//! \todo add PWM by ms and us ?
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_pwm(cc_handle_t *pHandle)
{
#if 0
    err_t   err;
    uint_16 percentage = atoi(pHandle->argv[1]);

    err = bu26507_pwm(percentage);
    if (err != ERR_OK)
    {
        corona_print("PWM ERR! (%x)\n", err);
    }
    return err;
#else
    return ERR_GENERIC;
#endif
}


#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'iow' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Write GPIO(s).
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_iow(cc_handle_t *pHandle)
{
    return cc_process_io(pHandle, CC_IO_WRITE);
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Process a GPIO request.
//!
//! \fntype Reentrant Function
//!
//! \detail Read or Write a GPIO.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_io(cc_handle_t *pHandle, cc_io_dir_t direction)
{
    int     i;
    uint_32 data;
    uint_32 numSeconds = 0;
    uint_32 numLoops   = 0;

    for (i = 0; i < NUM_IO_COMMANDS; i++)
    {
        if (!strcmp(pHandle->argv[1], g_cc_io_entries[i].cmd))
        {
            switch (direction)
            {
            case CC_IO_WRITE:
                if (g_cc_io_entries[i].type != CC_IO_RW_TYPE)
                {
                    corona_print("ERR: This GPIO is !writable\n\r");
                    return CC_UNWRITABLE_GPIO;
                }
                data = (uint_32)strtoul(pHandle->argv[2], NULL, 16);
                return g_cc_io_entries[i].callback(pHandle, &data, direction);

            case CC_IO_READ:
                if (pHandle->argc > 2)
                {
                    // User wants us to poll/print the signal for N seconds, so handle that case.
                    numSeconds = atoi(pHandle->argv[2]);
                    numLoops   = (numSeconds * 1000) / 100;
                }

                do
                {
                    g_cc_io_entries[i].callback(pHandle, &data, direction);
                    corona_print("%s: %x                   \r", g_cc_io_entries[i].cmd, data);
                    if (numLoops != 0)
                    {
                        sleep(100);
                    }
                } while (numLoops-- > 0);
                corona_print("\n\r");
                return ERR_OK;

            default:
                corona_print("ERR: Read or Write !specified\n\r");
                return ERR_GENERIC;
            }
        }
    }
    corona_print("ERR: (%s) is !valid GPIO value\n\r", pHandle->argv[1]);

    return ERR_GENERIC;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'bfw' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Write a bitfield to a specific 32-bit address anywhere in memory.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_bfw(cc_handle_t *pHandle)
{
#if 0
    uint_32 *pAddr;
    uint_32 val;
    uint_32 contents;
    int     idx = 0;
    int     lsb, msb;

    pAddr = (uint_32 *)strtoul(pHandle->argv[1], NULL, 16);
    if (pAddr == NULL)
    {
        return corona_print("ERR: Could ! convert address to integer\n\r");
    }

    msb   = atoi(pHandle->argv[2]);
    lsb   = atoi(pHandle->argv[3]);
    val   = (uint_32)strtoul(pHandle->argv[4], NULL, 16);
    val <<= lsb; // the value given by user is right justified, so shift it into position.
    if (val > mask(msb, lsb))
    {
        return corona_print("ERR: value passed is too big to fit into bitfield\n\r");
    }

    corona_print("Writ'g %x to <%d:%d> at %x \n\r", val, msb, lsb, pAddr);
    contents  = *pAddr;          // contents BEFORE the write.
    contents &= ~mask(msb, lsb); // now we need to create a mask in the range of the bitfield specified
    contents |= val;
    *pAddr    = contents;        // perform the final write to memory location chosen by user.

    corona_print("%x: %x\n\n\r", pAddr, *pAddr);
#else
    corona_print(UNSUPPORTED_COMMAND);
#endif
    return ERR_OK;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Disconnect/Connect BT.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_btcon(cc_handle_t *pHandle)
{
	boolean connect;
	BD_ADDR_t ANY_ADDR;
	err_t err;
	LmMobileStatus lmMobileStatus;
		
	if (2 != pHandle->argc)
	{
		corona_print("Usg: btcon <0|1>\n");
		return ERR_GENERIC;
	}
	
	connect = (boolean)strtoul(pHandle->argv[1], NULL, 10);
	
	if (connect)
	{
		// TODO: Someone go ahead and rewrite this to accept a specific BDADDR
	    ASSIGN_BD_ADDR(ANY_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); // NULL means any
		BTMGR_request();
		err = BTMGR_connect(ANY_ADDR, &lmMobileStatus);
	}
	else
	{
		BTMGR_disconnect();
		BTMGR_release();
	}
	
	return ERR_OK;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief BT Link Key Management
//!
//! \fntype Non-Reentrant Function
//!
//! \detail.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_btkey(cc_handle_t *pHandle)
{
	boolean connect;
	BD_ADDR_t ANY_ADDR;
	err_t err;
		
	if (strcmp(pHandle->argv[1], "delete") == 0)
	{
		if (strcmp(pHandle->argv[2], "all") == 0)
		{
			BD_ADDR_t null_addr;
		    ASSIGN_BD_ADDR(null_addr, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

			DeleteLinkKey(null_addr);
			memset(&g_dy_cfg.bt_dy.LinkKeyInfo, 0, sizeof(g_dy_cfg.bt_dy.LinkKeyInfo));
		}
		else
		{
			int idx = atoi(pHandle->argv[2]);
			
			if (idx >= 0 && idx < MAX_BT_REMOTES)
			{
				BTMGR_delete_key(idx);
			}
		}
	}
	else if (strcmp(pHandle->argv[1], "list") == 0)
	{
		int i;
		
		corona_print("index: BD_ADDR key usage_count pom\n");

		for (i = 0; i < MAX_BT_REMOTES; ++i)
		{
			corona_print("%d: %02x:%02x:%02x:%02x:%02x:%02x 0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x %d %d\n", i,
		    		g_dy_cfg.bt_dy.LinkKeyInfo[i].BD_ADDR.BD_ADDR5, g_dy_cfg.bt_dy.LinkKeyInfo[i].BD_ADDR.BD_ADDR4, g_dy_cfg.bt_dy.LinkKeyInfo[i].BD_ADDR.BD_ADDR3,
		    		g_dy_cfg.bt_dy.LinkKeyInfo[i].BD_ADDR.BD_ADDR2, g_dy_cfg.bt_dy.LinkKeyInfo[i].BD_ADDR.BD_ADDR1, g_dy_cfg.bt_dy.LinkKeyInfo[i].BD_ADDR.BD_ADDR0,
		    		g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key15, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key14, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key13, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key12, 
		    		g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key11, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key10, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key9, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key8, 
		    		g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key7, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key6, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key5, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key4, 
		    		g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key3, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key2, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key1, g_dy_cfg.bt_dy.LinkKeyInfo[i].LinkKey.Link_Key0,
		    		(int)g_dy_cfg.bt_dy.LinkKeyInfo[i].usage_count, (int)g_dy_cfg.bt_dy.LinkKeyInfo[i].pom);
		}
	}
	
	return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Enable/Disable BT debug.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_btdebug(cc_handle_t *pHandle)
{
	boolean hci_debug_enable, ssp_debug_enable = 0;
	
	if (2 == pHandle->argc)
	{
		goto SET;
	}

	if (3 != pHandle->argc)
	{
		corona_print("Usg: btdebug 0|1 [0|1]\n");
		return ERR_GENERIC;
	}
	
	ssp_debug_enable = (boolean)strtoul(pHandle->argv[2], NULL, 10);

SET:
	hci_debug_enable = (boolean)strtoul(pHandle->argv[1], NULL, 10);
	
	BTMGR_set_debug(hci_debug_enable, ssp_debug_enable);
	
	return ERR_OK;
}



///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'pcfg' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Print out all factory configs.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_pcfg(cc_handle_t *pHandle)
{
    fcfg_tag_t tag;
    fcfg_handle_t handle;
    fcfg_err_t ferr;
    fcfg_len_t len;
    char *pDesc;
    
    wassert( FCFG_OK == FCFG_init(&handle,
                                  EXT_FLASH_FACTORY_CFG_ADDR,
                                  &wson_read,
                                  &wson_write,
                                  &_lwmem_alloc_system_zero,
                                  &_lwmem_free) );
    
    corona_print("BRD\t%u\n", FCFG_board(&handle));
    corona_print("LED\t%u\n", FCFG_led(&handle));
    corona_print("SIL\t%u\n", FCFG_silego(&handle));
    corona_print("RF\t%u\n",  FCFG_rfswitch(&handle));
    
    corona_print("\nFACTORY CFGS\n\n");
    for(tag = CFG_FACTORY_FIRST_FCFG; tag <= CFG_FACTORY_LAST_FCFG; tag++)
    {
        ferr = FCFG_getdescriptor(tag, &pDesc);
        if( FCFG_OK != ferr )
        {
            return WPRINT_ERR("des %i, %i", ferr, tag);
        }
        corona_print("TAG: %i\tDESC: %s\t", tag, pDesc);
        
        ferr = FCFG_getlen(&handle, tag, &len);
        if( FCFG_VAL_UNSET == ferr )
        {
            corona_print("(unset)\n");
            continue;
        }
        else if( FCFG_OK != ferr )
        {
            return WPRINT_ERR("glen %i, %i", ferr, tag);
        }
        corona_print("LEN: %i\tVALUE: ", len);
        wassert(0 != len);
        
        if( FCFG_INT_SZ == len )
        {
            /*
             *   Assume this length is a number, since we disallow setting strings to a length of 4.
             */
            uint32_t val;
            
            ferr = FCFG_get(&handle, tag, &val);
            if( FCFG_OK != ferr )
            {
                return WPRINT_ERR("fget %i, %i", ferr, tag);
            }
            corona_print("hex:%x dec:%i\n", val, val);
        }
        else
        {
            /*
             *   Must be a string, print it as such.
             */
            char *pStr;
            
            pStr = walloc(len+1);
            
            ferr = FCFG_get(&handle, tag, pStr);
            if( FCFG_OK != ferr )
            {
                wfree(pStr);
                return WPRINT_ERR("fget %i, %i", ferr, tag);
            }
            corona_print("%s\n", pStr);
            wfree(pStr);
        }
    }
    
    corona_print("\n\n");
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'scfg' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Set a factory config.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_scfg(cc_handle_t *pHandle)
{
    fcfg_handle_t handle;
    fcfg_err_t ferr;
    fcfg_tag_t tag;
    uint32_t val;
    
    wassert( FCFG_OK == FCFG_init(&handle,
                                  EXT_FLASH_FACTORY_CFG_ADDR,
                                  &wson_read,
                                  &wson_write,
                                  &_lwmem_alloc_system_zero,
                                  &_lwmem_free) );
    
    ferr = FCFG_gettagfromdescriptor(pHandle->argv[1], &tag);
    if( FCFG_DESC_UNKNOWN == ferr )
    {
        /*
         *   Descriptor is unknown, so maybe they passed in a number.
         */
        tag = (fcfg_tag_t)strtoul(pHandle->argv[1], NULL, 10);
    }
    else if( FCFG_OK != ferr )
    {
        return WPRINT_ERR("gtag %i, %i", ferr, tag);
    }
    
    /*
     *   String or 32-bit number ?
     */
    if( '$' == pHandle->argv[2][0])
    {
        // String
        
        fcfg_len_t len = strlen( &(pHandle->argv[2][1]) ) + 1;
        char maybe[6];
        
        corona_print("TAG %i %s\n", tag, &(pHandle->argv[2][1]));
        
        if( FCFG_INT_SZ == len )
        {
            /*
             *   4 is forbidden for strings, since this is how we differentiate things.
             *     Just use 6 arbitrarily as the size of the str to get around that.
             */
            memset(maybe, 0, FCFG_NOT_INT_SZ);
            memcpy(maybe, &(pHandle->argv[2][1]), FCFG_INT_SZ);
            ferr = FCFG_set(&handle, tag, FCFG_NOT_INT_SZ, maybe);
        }
        else
        {
            ferr = FCFG_set(&handle, tag, len, &(pHandle->argv[2][1]));
        }
        
        if( FCFG_OK != ferr )
        {
            return WPRINT_ERR("fset %i, %i", ferr, tag);
        }
    }
    else
    {
        // 32-bit unsigned
        
        val = strtoul(pHandle->argv[2], NULL, 10);
        corona_print("TAG %i %i\n", tag, val);
        ferr = FCFG_set(&handle, tag, sizeof(uint32_t), &val);
        if( FCFG_OK != ferr )
        {
            return WPRINT_ERR("fset %i, %i", ferr, tag);
        }
    }
    
    CFGMGR_load_fcfgs(TRUE);
    CFGMGR_flush( CFG_FLUSH_STATIC );
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'sherlock' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Log detective.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_sherlock(cc_handle_t *pHandle)
{
    shlk_err_t err;
    sh_u8_t en;
    uint32_t num_pages;
    
    if(1 == pHandle->argc)
    {
        goto shlk_is_enabled;
    }
    else if (!strcmp(pHandle->argv[1], "on"))
    {
        en = 1;
        goto enable_sherlock;
    }
    else if (!strcmp(pHandle->argv[1], "off"))
    {
        en = 0;
        goto enable_sherlock;
    }
    else if (!strcmp(pHandle->argv[1], "reset"))
    {
        err = SHLK_reset(&(pHandle->shlk_hdl));
        if( SHLK_ERR_OK != err )
        {
            return WPRINT_ERR("shlk %i", err);
        }
    }
#if 0
    else if (!strcmp(pHandle->argv[1], "serv"))
    {
        /*
         *   Dump data to server next time.
         */
        return corona_print(   "%u pgs\n", 
                               SHLK_serv_pages( &(pHandle->shlk_hdl), 
                                                EXT_FLASH_SHLK_SERV_ADDR, 
                                                (pHandle->argc < 3)? 0:(uint32_t)strtoul(pHandle->argv[2], NULL, 10), 
                                                (sh_u8_t)(pHandle->argc < 3) ) );
    }
    else if (!strcmp(pHandle->argv[1], "assdump"))
    {
        g_sherlock_in_progress = 1;
        err = SHLK_assdump(&(pHandle->shlk_hdl));
        g_sherlock_in_progress = 0;
        if( SHLK_ERR_OK != err )
        {
            return WPRINT_ERR("shlk %i", err);
        }
    }
    else if (!strcmp(pHandle->argv[1], "dump"))
    {
        if(pHandle->argc < 3)
        {
            return corona_print(BAD_ARGUMENT);
        }
        else if(!strcmp(pHandle->argv[2], "*"))
        {
            g_sherlock_in_progress = 1;
            err = SHLK_dump(&(pHandle->shlk_hdl), 0);
            g_sherlock_in_progress = 0;
            if( SHLK_ERR_OK != err )
            {
                 return WPRINT_ERR("shlk %i", err);
            }
            return ERR_OK;
        }
        else
        {
            num_pages = (uint32_t)strtoul(pHandle->argv[2], NULL, 10);
            
            g_sherlock_in_progress = 1;
            err = SHLK_dump(&(pHandle->shlk_hdl), num_pages);
            g_sherlock_in_progress = 0;
            if( SHLK_ERR_OK != err )
            {
                return WPRINT_ERR("shlk %i", err);
            }
            return ERR_OK;
        }
#endif

    return ERR_OK;
    
enable_sherlock:
    SHLK_enable(&(pHandle->shlk_hdl), EXT_FLASH_SHLK_EN_ADDR, en);
    
shlk_is_enabled:
    return corona_print("SHLK en %u\n", SHLK_is_enabled(&(pHandle->shlk_hdl), EXT_FLASH_SHLK_EN_ADDR) );
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'ccfg' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read a factory config.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_ccfg(cc_handle_t *pHandle)
{
    fcfg_handle_t handle;
    fcfg_err_t ferr;
    fcfg_tag_t tag;
    
    wassert( FCFG_OK == FCFG_init(&handle,
                                  EXT_FLASH_FACTORY_CFG_ADDR,
                                  &wson_read,
                                  &wson_write,
                                  &_lwmem_alloc_system_zero,
                                  &_lwmem_free) );
    
    if (!strcmp(pHandle->argv[1], "*"))
    {
        wassert( FCFG_OK == FCFG_clearall(&handle) );
        return ERR_OK;
    }
    
    ferr = FCFG_gettagfromdescriptor(pHandle->argv[1], &tag);
    if( FCFG_DESC_UNKNOWN == ferr )
    {
        /*
         *   Descriptor is unknown, so maybe they passed in a number.
         */
        tag = (fcfg_tag_t)strtoul(pHandle->argv[1], NULL, 10);
    }
    else if( FCFG_OK != ferr )
    {
        return WPRINT_ERR("gtag %i, %i", ferr, tag);
    }
    
    ferr = FCFG_clear(&handle, tag);
    if( FCFG_OK != ferr )
    {
        return WPRINT_ERR("fclr %i, %i", ferr, tag);
    }
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'cfgclear' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Clear configuration settings.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_cfgclear(cc_handle_t *pHandle)
{
    /*
     *   The user is explicitly requesting to hard reset to factory defaults,
     *     so load __and_flush__ in this case.
     */
    CFGMGR_load_factory_defaults( 1 );
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'checkin' command.
//!
//! \fntype Reentrant Function
//!
//! \detail send check-in command to server
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_check_in(cc_handle_t *pHandle)
{
    FWUMGR_Send_Checkin();
    return(ERR_OK);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'console' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Control console configuration.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_console(cc_handle_t *pHandle)
{
    if (pHandle->argc < 2)
    {
        goto DONE;
    }
    
    g_st_cfg.cc_st.enable = (0 == (uint_8)strtoul(pHandle->argv[1], NULL, 10))?0:1;
    CFGMGR_flush( CFG_FLUSH_STATIC );
    
 DONE:
    printf("EN %d\n", g_st_cfg.cc_st.enable);  // use printf() to make sure this goes out.
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'print_package' command.
//!
//! \fntype Reentrant Function
//!
//! \detail send check-in command to server
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_print_package(cc_handle_t *pHandle)
{
    FWUMGR_Print_Package();
    return(ERR_OK);
}



///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'memr' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read from memory.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_memr(cc_handle_t *pHandle)
{
#if 0
    uint_32 *pAddr;
    int     numWordsToRead;
    int     idx = 0;

    numWordsToRead = atoi(pHandle->argv[2]);
    pAddr          = (uint_32 *)strtoul(pHandle->argv[1], NULL, 16);
    if (pAddr == NULL)
    {
        return corona_print("ERR: Couldn't convert address to integer\n\r");
    }

    corona_print("%x:", pAddr);

    while (numWordsToRead-- > 0)
    {
        corona_print("\t\t%x", pAddr[idx++]);
        if ((idx % 4) == 0)
        {
            corona_print("\n\r\t"); // 4 words per line, just for prettiness.
        }
    }
    corona_print("\n\r");
#else
    corona_print(UNSUPPORTED_COMMAND);
#endif

    return ERR_OK;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'memw' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Write to memory.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_memw(cc_handle_t *pHandle)
{
#if 0
    uint_32 *pAddr;
    int     numWordsToWrite = pHandle->argc - 2; // minus 2 for memr command and addr.
    int     idx             = 2;

    pAddr = (uint_32 *)strtoul(pHandle->argv[1], NULL, 16);
    if (pAddr == NULL)
    {
        return corona_print("ERR: Couldn't convert address to integer\n\r");
    }

    while (numWordsToWrite-- > 0)
    {
        *pAddr = (uint_32)strtoul(pHandle->argv[idx++], NULL, 16);
        pAddr++;
    }
#else
    corona_print(UNSUPPORTED_COMMAND);
#endif
    return ERR_OK;
}



///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'cc_process_persist' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Write to persistent variable space.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_persist(cc_handle_t *pHandle)
{
    size_t written;
    uint8_t boot_sequence;
    uint16_t server_millis;
    uint32_t server_seconds;
    
    if (pHandle->argc == 1)
    {
        persist_print_array();
        print_persist_data();
    }
//    else if (pHandle->argc == 2)
//    {
//        written = _set_persist_array((uint8_t*) pHandle->argv[1], strlen(pHandle->argv[1]));
//        corona_print("Wrote string array to persistent data as raw rarray (%d bytes)\r\n", written);
//    }
    else if (pHandle->argc == 4)
    {
        boot_sequence = strtoul(pHandle->argv[1], NULL, 10);
        server_millis = strtoul(pHandle->argv[2], NULL, 10);
        server_seconds = strtoul(pHandle->argv[3], NULL, 10);
        
        corona_print("Seq %u\nOffst %d:%d\n", boot_sequence, server_millis, server_seconds);
        PMEM_CLEAR();
        __SAVED_ACROSS_BOOT.boot_sequence = boot_sequence;
        __SAVED_ACROSS_BOOT.server_offset_millis = server_millis;
        __SAVED_ACROSS_BOOT.server_offset_seconds = server_seconds;
        PMEM_UPDATE_CKSUM();
        persist_print_array();
    }
    else
    {
        corona_print(BAD_ARGUMENT);
        return ERR_GENERIC;
    }

    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'about' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Print the about string.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_about(cc_handle_t *pHandle)
{
    corona_print("%s APP Corona\n", CC_MAN_ABT);

    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'acc' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Process a command related to the accelerometer.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_acc(cc_handle_t *pHandle)
{
    uint16_t xx, yy, zz;
    uint8_t  xyz;
    int numReads;
    
#ifdef ACC_DEV
    if (!strcmp(pHandle->argv[1], "fs"))
    {
        uint_8 fs;
        
        if (pHandle->argc < 3)
        {
            return corona_print("%d G\n", g_st_cfg.acc_st.full_scale);
        }
        
        fs = (uint_8)strtoul(pHandle->argv[2], NULL, 10);
        
        if(fs <= FULLSCALE_16G)
        {
            g_st_cfg.acc_st.full_scale = fs;
            ACCMGR_apply_configs();
        }
        else
        {
            return corona_print(BAD_ARGBAD_ARGUMENT);
        }
    }
    else if (!strcmp(pHandle->argv[1], "sec"))
    {
        if (pHandle->argc < 3)
        {
            return corona_print("%d\n", g_st_cfg.acc_st.seconds_to_wait_before_sleeping);
        }   
        g_st_cfg.acc_st.seconds_to_wait_before_sleeping = (uint_8)strtoul(pHandle->argv[2], NULL, 10);
        ACCMGR_apply_configs();
    }
    else if (!strcmp(pHandle->argv[1], "thresh"))
    {
        if (pHandle->argc < 3)
        {
            return corona_print("%d\n", g_st_cfg.acc_st.any_motion_thresh);
        }
        g_st_cfg.acc_st.any_motion_thresh = (uint_8)strtoul(pHandle->argv[2], NULL, 10);
        ACCMGR_apply_configs();
    }
    else 
#endif
    if (!strcmp(pHandle->argv[1], "cont"))
    {
        if (pHandle->argc < 3)
        {
            return corona_print("%d\n", g_st_cfg.acc_st.continuous_logging_mode);
        }
        g_st_cfg.acc_st.continuous_logging_mode = (uint_8)strtoul(pHandle->argv[2], NULL, 10);
        corona_print("%d\n", g_st_cfg.acc_st.continuous_logging_mode);
        CFGMGR_flush( CFG_FLUSH_STATIC );
    }
    else if (!strcmp(pHandle->argv[1],"fake"))
    {
        if ( pHandle->argc < 3)
        {
            return corona_print("%d\n", g_st_cfg.acc_st.use_fake_data);
        }
        g_st_cfg.acc_st.use_fake_data = (uint_8)strtoul(pHandle->argv[2], NULL, 10);
        corona_print("%d\n", g_st_cfg.acc_st.use_fake_data);
        CFGMGR_flush( CFG_FLUSH_STATIC );
    }
    else if (!strcmp(pHandle->argv[1],"reset"))
    {
        //corona_print("reset'g\n");
        ACCMGR_Reset();
        CFGMGR_flush( CFG_FLUSH_STATIC );
    }   
    else if (!strcmp(pHandle->argv[1],"shutdown"))
    {
        //corona_print("Shutt'g down\n");
        ACCMGR_shut_down( false );
        CFGMGR_flush( CFG_FLUSH_STATIC );
    }    
    else if (!strcmp(pHandle->argv[1],"startup"))
    {
        //corona_print("Start'g up\n");
        ACCMGR_start_up();
        CFGMGR_flush( CFG_FLUSH_STATIC );
    }    
    else if (!strcmp(pHandle->argv[1],"mot"))
    {
        if (pHandle->argc < 3)
        {
            // No arg: print motion detect status flag
            corona_print("%d\n", accel_poll_int2_status());
        }
        else
        {
            // Turn motion detect latch on/off
            accel_enable_fifo( 0, (uint_8)strtoul(pHandle->argv[2], NULL, 10) );
        }
    }
    else if (!strcmp(pHandle->argv[1],"read"))
    {
#ifdef ACC_DEV
        // Shutdown accel in case it's busy
        // corona_print("Shutt'g down ACCMGR\n");
        ACCMGR_shut_down( false );
        while (!ACCMGR_is_shutdown() )
        {
            _time_delay(150);
        }
        // corona_print("ACCMGR is shut down\n");
        CFGMGR_flush( CFG_FLUSH_STATIC );
        _time_delay(100);
        
        //corona_print("Read'g ACC\n");
        
        numReads = 10;
        while (numReads-- > 0)
        {
            xx = yy = zz = 0x0000;
            // err_t accel_read_register (uint_8 cmd, uint_8 *pData, uint_16 numBytes);
            // Problem trying to read more than one byte at a time so read one at a time.
            accel_read_register(SPI_STLIS3DH_OUT_X_L, &xyz, 1);
            _time_delay(1);
            accel_read_register(SPI_STLIS3DH_OUT_X_H, (uint8_t*)&xx, 1);
            xx = (xx << 8) | xyz;
            
            _time_delay(1);
            accel_read_register(SPI_STLIS3DH_OUT_Y_L, &xyz, 1);
            _time_delay(1);
            accel_read_register(SPI_STLIS3DH_OUT_Y_H, (uint8_t*)&yy, 1);
            yy = (yy << 8) | xyz;
            
            _time_delay(1);
            accel_read_register(SPI_STLIS3DH_OUT_Z_L, &xyz, 1);
            _time_delay(1);
            accel_read_register(SPI_STLIS3DH_OUT_Z_H, (uint8_t*)&zz, 1);
            zz = (zz << 8) | xyz;

             corona_print("X:%5u\tY:%5u\tZ:%5u\n", xx, yy, zz);
            _time_delay(100);
        }
        corona_print("\n\n");
#else
        corona_print(UNSUPPORTED_COMMAND);
#endif
        return ERR_OK;
    }
    
#if 0
    if (!strcmp(pHandle->argv[1], "dump"))
    {
        int_16 x, y, z;

        if (pHandle->argc == 2)
        {
            // dump a single X/Y/Z sample to the console.
            if (lis3dh_get_vals(&x, &y, &z) != ERR_OK)
            {
                return corona_print("Err: couldn't read values!\n");
            }
            return corona_print("X: %h\t\tY: %h\t\tZ: %h\n", x, y, z);
        }
        else
        {
            // keep dumping accel data to console for N seconds.
            uint_32 numSeconds            = atoi(pHandle->argv[2]);
            uint_32 delayMsBetweenSamples = 300; // milliseconds
            uint_32 numLoops;

            if (pHandle->argc > 3)
            {
                delayMsBetweenSamples = atoi(pHandle->argv[3]); // the optional 4th argument is sample rate in milliseconds
            }

            numLoops = (numSeconds * 1000) / delayMsBetweenSamples;

            while (numLoops-- > 0)
            {
                if (lis3dh_get_vals(&x, &y, &z) != ERR_OK)
                {
                    return corona_print("Err: couldn't read values!\n");
                }
                corona_print("X: %h        Y: %h          Z: %h                                       \r",
                         x, y, z);
                //printf("%hi\t%hi\t%hi\n", x, y, z);
                sleep(delayMsBetweenSamples);
            }
            corona_print("\n\n");
            return ERR_OK;
        }
    }
    else if (!strcmp(pHandle->argv[1], "set"))
    {
        uint_8 reg, val;

        if (pHandle->argc != 4)
        {
            return corona_print("ERR: acc set requires exactly 4 args. %d given\n", pHandle->argc);
        }

        reg = (uint_8)strtoul(pHandle->argv[2], NULL, 16);
        val = (uint_8)strtoul(pHandle->argv[3], NULL, 16);

        if (lis3dh_write_reg(reg, val) != ERR_OK)
        {
            return corona_print("ERR: LIS3DH write returned an error!\n");
        }
        return ERR_OK;
    }
    else if (!strcmp(pHandle->argv[1], "get"))
    {
        uint_8 reg, val;

        if (pHandle->argc != 3)
        {
            return corona_print("ERR: acc get requires exactly 3 args. %d given\n", pHandle->argc);
        }

        reg = (uint_8)strtoul(pHandle->argv[2], NULL, 16);

        if (lis3dh_read_reg(reg, &val) != ERR_OK)
        {
            return corona_print("ERR: LIS3DH write returned an error!\n");
        }

        return corona_print("Value: %x\n\n", val);
    }
#endif
    else
    {
        return corona_print("bad cmd\n");
    }
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'act' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Dump activity metrics.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_act(cc_handle_t *pHandle)
{
#if (CALCULATE_MINUTES_ACTIVE)
    if (pHandle->argc > 1)
    {
        if (!strcmp(pHandle->argv[1], "on"))
        {
            corona_print("ACT ON\n");
            g_st_cfg.acc_st.calc_activity_metric = 1;
        }
        else if (!strcmp(pHandle->argv[1], "off"))
        {
            corona_print("ACT OFF\n");
            g_st_cfg.acc_st.calc_activity_metric = 0;
        }
        else
        {
            return corona_print("%s %s\n", pHandle->argv[1], BAD_ARGUMENT);
        }
        
        CFGMGR_flush(CFG_FLUSH_STATIC);
    }
    
    if( g_st_cfg.acc_st.calc_activity_metric )
    {
        MIN_ACT_print_vals();
    }
    else
    {
        corona_print("ACT OFF. <<act on>> to enable\n");
    }
#else
    corona_print(UNSUPPORTED_COMMAND);
#endif
    return ERR_OK;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'adc' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Write a register to the ROHM chip.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_adc(cc_handle_t *pHandle)
{
#if 0
    uint_32 numSamples;
    uint_32 msDelay;
    uint_32 i;
    uint_32 channelIdx;
    uint_16 raw, natural;

    // adc  <pin> <numSamples> <ms delay between samples>
    if (!strcmp(pHandle->argv[1], "uid"))
    {
        channelIdx = ADC0_Vsense_Corona_UID;
        corona_print("\nPIN\t\tRAW COUNTS\tOHMS\n");
    }
    else if (!strcmp(pHandle->argv[1], "bat"))
    {
        channelIdx = ADC0_Vsense_Corona_VBat;
        corona_print("\nPIN\t\tRAW COUNTS\tMILLIVOLTS\n");
    }
    else
    {
        return corona_print("ERR: %s is !valid pin!\n", pHandle->argv[1]);
    }

    numSamples = (uint_32)strtoul(pHandle->argv[2], NULL, 16);
    msDelay    = (uint_32)strtoul(pHandle->argv[3], NULL, 16);

    for (i = 0; i < numSamples; i++)
    {
        if (adc_sense(channelIdx, &raw, &natural) != ERR_OK)
        {
            corona_print("ERR: ADC could !sense at sample (%d)", i);
            return CC_ERR_ADC;
        }
        corona_print("%s\t\t%d\t\t%d\n", pHandle->argv[1], raw, natural);
        sleep(msDelay);
    }
#else
    corona_print(UNSUPPORTED_COMMAND);
#endif
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'data' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Flush or reset event data.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
extern void _EVTMGR_lock(void);
extern void _EVTMGR_unlock(void);
static err_t cc_process_data(cc_handle_t *pHandle)
{
    if(!strcmp(pHandle->argv[1], "flush"))
    {
        //corona_print("Flush data\n");
        
        _EVTMGR_lock();
        _EVTMGR_flush_evt_blk_ptrs( 0 );
        _EVTMGR_unlock();
    }
    else if(!strcmp(pHandle->argv[1], "reset"))
    {
        //corona_print("Reset data\n");
        EVTMGR_reset();
    }
    else if(!strcmp(pHandle->argv[1], "duty"))
    {
        /*
         *   Update duty cycle of data dumps.
         */
        
        if(pHandle->argc < 3)
        {
            corona_print("%ds\n", g_st_cfg.evt_st.data_dump_seconds);
            return ERR_GENERIC;
        }

        EVTMGR_upload_dutycycle( (uint_32)strtoul(pHandle->argv[2], NULL, 10) );
    }
    else if(!strcmp(pHandle->argv[1], "holdread"))
    {
        /*
         *   Update duty cycle of data dumps.
         */
        
        if(pHandle->argc < 3)
        {
            corona_print("%d\n", g_st_cfg.evt_st.debug_hold_evt_rd_ptr);
            return ERR_GENERIC;
        }

        if ((uint_32)strtoul(pHandle->argv[2], NULL, 10) == 1)
        {
        	g_st_cfg.evt_st.debug_hold_evt_rd_ptr = 1;
        }else if ((uint_32)strtoul(pHandle->argv[2], NULL, 10) == 0) {
        	g_st_cfg.evt_st.debug_hold_evt_rd_ptr = 0;
        }
        else
        {
        	corona_print("ERR\n");
        	return ERR_GENERIC;
        }
        CFGMGR_flush( CFG_FLUSH_STATIC );
    }
    else if (!strcmp(pHandle->argv[1], "ichk"))
    {
        if ( pHandle->argc < 3)
        {
            return corona_print("%d\n", g_st_cfg.evt_st.ignore_checkin_status);
        }
        g_st_cfg.evt_st.ignore_checkin_status = (uint_8)strtoul(pHandle->argv[2], NULL, 10);
        corona_print("%d\n", g_st_cfg.evt_st.ignore_checkin_status);
        CFGMGR_flush( CFG_FLUSH_STATIC );
    }
    else if (!strcmp(pHandle->argv[1], "spam"))
    {
        int i = 0;
        char *dumb;

        dumb = walloc(1100);
        
        while(i++ <= 2056)
        {
            /*
             *   Fragment the payloads to EVTMGR for maximum stressi-ness.
             */
            if( i % 3 )
            {
                dumb[24] = 0;
            }
            else
            {
                memset(dumb, 0x12345673, 1096);
            }
            
            WMP_post_fw_info( ERR_SPAM, "fake data", "spam!", dumb, 666 );
            
            if( 0 == i%4 )
            {
                _time_delay(100);  // give time to breathe and flush the buffers.
            }
        }
        
        wfree(dumb);
        corona_print("\n\n");
    }
    else
    {
        corona_print(BAD_ARGUMENT);
    }
       
    return ERR_OK;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'rohm' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Write a register to the ROHM chip.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_rohm(cc_handle_t *pHandle)
{
    uint_8 reg;
    uint_8 data;

    reg  = (uint_8)strtoul(pHandle->argv[1], NULL, 16);
    data = (uint_8)strtoul(pHandle->argv[2], NULL, 16);

    if (bu26507_write(reg, data) != ERR_OK)
    {
        corona_print("ERR: writing\n");
        return ERR_GENERIC;
    }

    return corona_print("Wrote -> reg(%x): %x\n\n", reg, data);
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'dis' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Write a register to the ROHM chip.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_dis(cc_handle_t *pHandle)
{
    int pattern;

    pattern  = (uint_8)strtoul(pHandle->argv[1], NULL, 10);

    if (DISMGR_run_pattern(pattern) != ERR_OK)
    {
        corona_print("Failed\n");
        return ERR_GENERIC;
    }
    
    return ERR_OK;
}
#endif

#if 0
/*
 *   Callback used for dumping data to a server.
 */
err_t evtmgr_server_dump_cbk(err_t err, uint8_t *pData, uint32_t len, uint8_t last_block)
{
    
    if(err == ERR_EVTMGR_NO_DATA_TO_DUMP)
    {
        corona_print("\n NO DATA TO DUMP, or Event Block Pointers need to be flushed first... \n\n");
        g_last_dump = TRUE;
        return err;
    }
    
    if(err != ERR_OK)
    {
        corona_print("\n\n*** ERROR (%x) during EVTMGR Dumping! ***\n", err);
        g_last_dump = TRUE;
        return err;
    }
    
    if(len > 0)
    {
        // Send data to server...
        int_32 send_result;
        err_t cm_err;
        
        /*
         *   Open a 'socket'
         */
        cm_err = CONMGR_open(g_conmgr_handle, HOST_TO_UPLOAD_DATA_TO, g_st_cfg.con_st.port);
        if(cm_err != ERR_OK)
        {
            corona_print("CONMGR_open failed with code %x\n", cm_err);
            last_block = TRUE;
            err = ERR_EVTMGR_CONMGR_SEND;
            goto end_server_dump;
        }
        
        cm_err = CONMGR_send(g_conmgr_handle, pData, len, &send_result);
        if (send_result == len)
        {
            corona_print("\n");
        }
        else
        {
            corona_print("CONMGR_send failed with code %x\n", cm_err);
            last_block = TRUE;
            err = ERR_EVTMGR_CONMGR_SEND;
            goto end_server_dump;
        }
    }

end_server_dump:

    if(last_block)
    {
        corona_print("End Serv Dump\n");
        wassert(ERR_OK == CONMGR_release(g_conmgr_handle));
        g_last_dump = last_block;
    }
    return err;
}

static void con_mgr_connected(CONMGR_handle_t handle, boolean success)
{
    if(!success)
    {
        corona_print("con_mgr_connected callback returned an error!\n\n");
        g_last_dump = TRUE;
    }
    wassert(handle == g_conmgr_handle);
    g_waiting_for_connection = FALSE;
}

static void con_mgr_disconnected(CONMGR_handle_t handle)
{
    corona_print("CONMGR lost its connection!\n");
    g_last_dump = TRUE;
    g_waiting_for_connection = TRUE;
}

/*
 *   Callback used for dumping data to the console.
 */
#define NUM_BYTES_PER_NEWLINE   8

err_t evtmgr_console_dump_cbk(err_t err, uint8_t *pData, uint32_t len, uint8_t last_block)
{
    uint32_t i = 0;
    
    if(err == ERR_EVTMGR_NO_DATA_TO_DUMP)
    {
        corona_print("\n NO DATA TO DUMP, or Event Block Pointers need to be flushed first... \n\n");
        g_last_dump = TRUE;
        return err;
    }
    
    if(err != ERR_OK)
    {
        corona_print("\n\n*** ERROR (%x) during EVTMGR Dumping! ***\n", err);
        g_last_dump = TRUE;
        return err;
    }
    
    while(i < len)
    {
        corona_print("%x\t", pData[i++]);
        if( (i % NUM_BYTES_PER_NEWLINE) == 0 )
        {
            corona_print("\n");
        }
    }
    
    corona_print("\n");
    
    g_last_dump = last_block;
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'dump' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Dumps event blocks to a target.
//!
//! eturn err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_dump(cc_handle_t *pHandle)
{
    err_t err = ERR_OK;
    uint_32 max_blocks = 0;
    
    
    if(!strcmp(pHandle->argv[1], "here"))
    {
        if(pHandle->argc > 2)
        {
            max_blocks = (uint_32)strtoul(pHandle->argv[2], NULL, 10);
        }
        
        corona_print("\n\n");
        err = EVTMGR_dump(max_blocks, evtmgr_console_dump_cbk, FALSE);
    }
    else if(!strcmp(pHandle->argv[1], "serv"))
    {   
        if(pHandle->argc < 4)
        {
            return corona_print("ERR: server dump requires server and port args\n");
        }
        
        // Configure the Server and Port to what the user passed in.
        memset(HOST_TO_UPLOAD_DATA_TO, 0, MAX_SERVER_STRING_SIZE);
        strcpy(HOST_TO_UPLOAD_DATA_TO, pHandle->argv[2]);
        g_st_cfg.con_st.port = (uint_32)strtoul(pHandle->argv[3], NULL, 10);
        corona_print("Waiting for connection @ %s : %d\n\n", HOST_TO_UPLOAD_DATA_TO, g_st_cfg.con_st.port);
        
        wassert(ERR_OK == CONMGR_register_cbk(con_mgr_connected, con_mgr_disconnected, 0, 0,
                    CONMGR_CONNECTION_TYPE_ANY, CONMGR_CLIENT_ID_CONSOLE, &g_conmgr_handle));
        
        if(pHandle->argc > 4)
        {
            max_blocks = (uint_32)strtoul(pHandle->argv[4], NULL, 10);
        }
        
        while(g_waiting_for_connection)
        {
            _time_delay(50);
        }
        
        err = EVTMGR_dump(max_blocks, evtmgr_server_dump_cbk, FALSE);
    }
    else
    {
        return corona_print("ERR: %s is !valid dump subcommand\n", pHandle->argv[1]);
    }
    
    if(err != ERR_OK)
    {
        goto done_processing_dump;
    }
    
    while(g_last_dump == FALSE)
    {
        _time_delay(500);
    }
    
done_processing_dump:

    g_last_dump = FALSE;
    g_waiting_for_connection = TRUE;
    if(err != ERR_OK)
    {
        corona_print("ERR: EVTMGR_dump() returned an error to console: (%x)\n", err);
    }
    return err;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'fle' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Erase a sector of the SPI Flash
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_fle(cc_handle_t *pHandle)
{

    uint_32 address;

    address = (uint_32)strtoul(pHandle->argv[1], NULL, 16);

    wson_erase(address);

    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'flr' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Read from SPI Flash memory.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_flr(cc_handle_t *pHandle)
{
    uint_32 address;
    uint_32 numBytes;
    uint_8  *pData;
    int     i;


    address  = (uint_32)strtoul(pHandle->argv[1], NULL, 16);
    numBytes = atoi(pHandle->argv[2]);

    pData = _lwmem_alloc(numBytes);
    wassert(pData);

    wson_read(address, pData, numBytes);

    for (i = 0; i < numBytes; i++)
    {
        if (((i % 4) == 0) /*&& (i > 0)*/)
        {
            corona_print("\t");
        }
        if (((i % 20) == 0) /* && (i > 0)*/)
        {
            corona_print("\n");
        }
        corona_print("%02x", pData[i]);
    }
	
done_flr:
    wfree(pData);
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'flw' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Write to SPI Flash memory.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_flw(cc_handle_t *pHandle)
{
    uint_32 address;
    uint_32 numBytes;
    uint_8  *pData;
    int     i;

    address  = (uint_32)strtoul(pHandle->argv[1], NULL, 16);
    numBytes = pHandle->argc - 2;

    pData = _lwmem_alloc(numBytes);
    wassert(pData);

    for (i = 0; i < numBytes; i++)
    {
        pData[i] = (uint_8)strtoul(pHandle->argv[i + 2], NULL, 16);
    }

    wson_write(address, pData, numBytes, FALSE);

done_flw:
    wfree(pData);
    return ERR_OK;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Remove trailing white space from the end of a string.
//!
//! \fntype Reentrant Function
//!
///////////////////////////////////////////////////////////////////////////////
static void trim_whitesp(char *pStr)
{
    while (*pStr)
    {
        if ((*pStr == '\n') || (*pStr == '\r') || (*pStr == '\t') || (*pStr == ' '))
        {
            *pStr = 0;
            return;
        }
        pStr++;
    }
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for '?' and 'h' and 'help' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Print out all available commands and their summaries.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_help(cc_handle_t *pHandle)
{
    int i;
    char *pRow;
    
    pRow = walloc(128);

    //corona_print("Corona Commands\n===============\n");
    
    for (i = 0; i < NUM_CC_COMMANDS; i++)
    {
        memset(pRow, 0, 128);
        strcat(pRow, g_cc_cmds[i].cmd);
        strcat(pRow, "\t\t\t:");
        strcat(pRow, g_cc_cmds[i].sum);
        strcat(pRow, "\n");
        corona_print("%s", pRow);
        _time_delay(10);
    }
    wfree(pRow);
    return ERR_OK;
}


/*
 *   Run testbrd, return number of errors.
 *   
 *     Its best to sort these in order of least-likely error, to most-likely.
 */
err_t testbrd(void)
{
    err_t err = ERR_OK;
    
    PMEM_FLAG_SET( PV_FLAG_BURNIN_TESTBRD_IN_PROGRESS, 1 );
    
    corona_print("\n *testbrd*\n\n");
    
    /*
     *   ACCELEROMETER
     */
    corona_print("\nACC\n");
    if (! lis3dh_comm_ok())
    {
        err = PROCESS_NINFO( ERR_BURNIN_TESTBRD_ACCEL_FAIL, "ACC" );
        goto done_testbrd;
    }

    /*
     *    SPI FLASH
     */
    corona_print("\nSPI Flash\n");
    if (! wson_comm_ok())
    {
        err = PROCESS_NINFO( ERR_BURNIN_TESTBRD_WSON_FAIL, "WSON" );
        goto done_testbrd;
    }
    
    /*
     *   MFI 
     */
    corona_print("\nMFI\n");
    if (! BTMGR_mfi_comm_ok())
    {
        err = PROCESS_NINFO( ERR_BURNIN_TESTBRD_MFI_FAIL, "MFI" );
        goto done_testbrd;
    }
    
    /*
     *   BLUETOOTH
     */
    corona_print("BT\n");
    BTMGR_request();
    if( (whistle_get_BluetoothStackID() < 1) || (!BTMGR_uart_comm_ok()) )
    {
        err = PROCESS_NINFO( ERR_BURNIN_TESTBRD_BT_FAIL, "BT" );
        BTMGR_release();
        goto done_testbrd;
    }

    /*
     *   WIFI
     */
    corona_print("\nWIFI\n");
    if (! a4100_wifi_comm_ok())
    {
        err = PROCESS_NINFO( ERR_BURNIN_TESTBRD_WIFI_FAIL, "WIFI" );
        goto done_testbrd;
    }
    
    corona_print("\nOK PASS\n");

done_testbrd:
    PMEM_FLAG_SET( PV_FLAG_BURNIN_TESTBRD_IN_PROGRESS, 0 );
    return err;
    
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'testbrd' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Tests all the board level tests in one command:
//!         Bluetooth communication, WIFI communication, Accelerometer communication,
//!         MFI communication, and External NOR Flash communication.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_testbrd(cc_handle_t *pHandle)
{
    unsigned long i = 0;
    unsigned long num_runs = 1;
    
    if(pHandle->argc > 1)
    {
        num_runs = (uint32_t)strtoul(pHandle->argv[1], NULL, 10);
    }
    
    while(i < num_runs)
    {
        err_t err;
        
        corona_print("\n\n\rTest %u\n\r", i++);
        err = testbrd();
        
        if(ERR_OK != err)
        {
            return ERR_OK;
        }
    }
    
    corona_print("%u Runs OK\n\r", num_runs);
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for wscan command.
//!
//! \fntype Reentrant Function
//!
//! \detail Print out all available WIFI networks and their info.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_ver(cc_handle_t *pHandle)
{
    print_header_info();
    return ERR_OK;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for wmon command.
//!
//! \detail Monitor WiFi networks displaying signal strength via LEDs
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////

#define STRONG_RSSI     25
#define MEDIUM_RSSI     15
#define WEAK_RSSI       3

err_t cc_process_wmon(cc_handle_t *pHandle)
{
    ATH_GET_SCAN          ext_scan_list;
    char*                 ssid = NULL;
    int                   result;
    int                   i;
    int                   rssi = 0;
    int                   max_rssi = 0;
    uint32_t              pgood;
    uint32_t              loop_count = 0;
    boolean               deep_scan;
    
    ssid = (char*)pHandle->argv[1];
    deep_scan = atoi(pHandle->argv[2]);
    
    while (1)
    {
        result = WIFIMGR_ext_scan(&ext_scan_list, deep_scan);
        if (result != 0)
        {
            corona_print("WM scan fail %d\n", result);
            return -1;
        }
    
        for (i = 0; i < ext_scan_list.num_entries; i++)
        {
            char temp_ssid[MAX_SSID_LENGTH];
        	memcpy(temp_ssid, ext_scan_list.scan_list[i].ssid, ext_scan_list.scan_list[i].ssid_len);
        	temp_ssid[ext_scan_list.scan_list[i].ssid_len] = 0;

            if ((ssid != NULL) && (strcmp(ssid, temp_ssid) != 0))
            {
                continue;
            }
            rssi = ext_scan_list.scan_list[i].rssi;
            if (rssi > max_rssi)
            {
                max_rssi = rssi;
            }
        }
        if (max_rssi >= STRONG_RSSI)
        {
            led_run_pattern (LED_PATTERN_4);    // Green
            _time_delay(1000);
            led_run_pattern (LED_PATTERN_OFF);
            // corona_print("STRONG_RSSI\n");

        }
        else if (max_rssi >= MEDIUM_RSSI)
        {
            led_run_pattern (LED_PATTERN_5);    // Yellow/Amber
            _time_delay(1000);
            led_run_pattern (LED_PATTERN_OFF);
            // corona_print("MEDIUM_RSSI\n");
        }
        else if (max_rssi >= WEAK_RSSI)
        {
            led_run_pattern (LED_PATTERN_1);    // Red
            _time_delay(1000);
            led_run_pattern (LED_PATTERN_OFF);
            // corona_print("WEAK_RSSI\n");
        }
        else
        {
            led_run_pattern (LED_PATTERN_1);    // Red
            _time_delay(300);
            led_run_pattern (LED_PATTERN_OFF);
            _time_delay(300);
            led_run_pattern (LED_PATTERN_1);    // Red
            _time_delay(300);
            led_run_pattern (LED_PATTERN_OFF);
            _time_delay(300);
            led_run_pattern (LED_PATTERN_1);    // Red
            _time_delay(300);
            led_run_pattern (LED_PATTERN_OFF);
            // corona_print("NO_RSSI\n");
        }
        _time_delay(3000);
        pgood = GPIO_READ(GPIOE_PDIR, 2);       // Ck if usb cable is plugged in
        if ((loop_count++ > 10) && (!pgood))
        {
            break;
        }
    }
    return ERR_OK;
}
#endif

/*
 *   Take user's passed in wifi credentials, and extract the SSID and PASSWORD.
 *   Format is Sasha$ID walle~12$PW
 *   
 *   Return starting index of first argument following SSID.
 */
static int cc_extract_wifi_args(cc_handle_t *pHandle, char *pSSID, char *pPassword)
{
    int index = 1; // this is where the SSID starts.
    int found = 0;

    /*
     *   First search for and construct the SSID.
     */
    
    while( index < pHandle->argc )
    {
        int len;
        
        strcat(pSSID, pHandle->argv[index++]);
        len = strlen(pSSID);
        
        if( (len >= 3) && (!strcmp(pSSID+len-3,"$ID")) )
        {
            char *pToken;
            
            /*
             *   Now just null terminate at the last occurence of '$'.
             */
            
            pToken = strrchr(pSSID, '$');
            wassert(pToken);
            *pToken = 0;
            
            found++;
            break;
        }   
        else
        {
            pSSID[ strlen(pSSID) ] = ' ';  // since not terminated with "$ID", assume there is a space here.
        }
    }
    
    if( found < 1 )
    {
        corona_print("bad tok\n");
        return -1;
    }
    
    
    /*
     *   Now search for and construct the Password.
     */
    while( index < pHandle->argc )
    {
        int len;
        
        strcat(pPassword, pHandle->argv[index++]);
        len = strlen(pPassword);
        
        if( (len >= 3) && (!strcmp(pPassword+len-3,"$PW")) )
        {
            char *pToken;
            
            /*
             *   Now just null terminate at the last occurence of '$'.
             */
            
            pToken = strrchr(pPassword, '$');
            wassert(pToken);
            *pToken = 0;
            
            found++;
            break;
        }   
        else
        {
            pPassword[ strlen(pPassword) ] = ' ';  // since not terminated with "$ID", assume there is a space here.
        }
    }
    
    if( found < 2 )
    {
        corona_print("bad tok\n");
        return -2;
    }
    
    return index;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for wcfg command.
//!
//! \fntype Reentrant Function
//!
//! \detail Configure a WI-FI network, i.e. set its security.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_wcfg(cc_handle_t *pHandle)
{
    int status;
    security_parameters_t sec_params;
    boolean doScan = FALSE;
    char *pSSID = NULL;
    char *pPassword = NULL;
    
    if (!strcmp(pHandle->argv[1],"clear"))
    {
        status = WIFIMGR_clear_security();
    }
    else
    {
        
        /*
         *   We need to find:
         *   
         *   (A)  SSID
         *   (B)  Password
         *   (C)  Index of the first argument after the password.  This is necessary, since
         *        ID's and passwords can have spaces, which will yield an irregular amount of
         *        parameters.  The index after password will tell us where to start looking for type/cipher.
         */
        
        int  idx_first_arg_after_pswd;
        
        pSSID = walloc( MAX_SSID_LENGTH );
        pPassword = walloc( MAX_PASSPHRASE_SIZE+1 );
        
        idx_first_arg_after_pswd = cc_extract_wifi_args(pHandle, pSSID, pPassword);
        
        if(idx_first_arg_after_pswd < 0)
        {
            corona_print(BAD_ARGUMENT);
            goto done_process_wcfg;
        }
        
        corona_print("SSID\t%s\n", pSSID);
        corona_print("PW\t%s\n", pPassword);
        
        if (pHandle->argc == idx_first_arg_after_pswd)
        {
            // Only ssid and password provided. Scan for others.
            sec_params.parameters.wpa_parameters.passphrase = pPassword; // in case it's WPA/WPA2
            sec_params.parameters.wep_parameters.key[0] = pPassword; // in case it's WEP
            doScan = TRUE;
        }
        else
        {
            if (0 == strcmp("open", pHandle->argv[idx_first_arg_after_pswd]))
            {
                sec_params.type = SEC_MODE_OPEN;   
                
                if ((pHandle->argc > (idx_first_arg_after_pswd + 1)))
                {
                    corona_print(BAD_ARGUMENT);
                    goto done_process_wcfg;
                }
                else
                {
                    goto wcfg_set_security;
                }
            }
            else if (0 == strcmp("wep", pHandle->argv[idx_first_arg_after_pswd]))
            {
                sec_params.type = SEC_MODE_WEP;
                sec_params.parameters.wep_parameters.key[0] = pPassword;
                sec_params.parameters.wep_parameters.key[1] = "";
                sec_params.parameters.wep_parameters.key[2] = "";
                sec_params.parameters.wep_parameters.key[3] = "";
                sec_params.parameters.wep_parameters.default_key_index = 1; // 1 is key[0]
                
                if ((pHandle->argc > (idx_first_arg_after_pswd + 1)))
                {
                    corona_print(BAD_ARGUMENT);
                    goto done_process_wcfg;
                }
                else
                {
                    goto wcfg_set_security;
                }
            }
            else if (0 == strcmp(WPA_PARAM_VERSION_WPA, pHandle->argv[idx_first_arg_after_pswd]))
            {
                sec_params.type = SEC_MODE_WPA;
                sec_params.parameters.wpa_parameters.version = (char*) WPA_PARAM_VERSION_WPA;
                sec_params.parameters.wpa_parameters.passphrase = pPassword;
                
                if ((pHandle->argc != (idx_first_arg_after_pswd + 3)))
                {
                    corona_print(BAD_ARGUMENT);
                    goto done_process_wcfg;
                }                
            }
            else if (0 == strcmp("wpa2", pHandle->argv[idx_first_arg_after_pswd]))
            {
                sec_params.type = SEC_MODE_WPA;
                sec_params.parameters.wpa_parameters.version = (char*) WPA_PARAM_VERSION_WPA2;
                sec_params.parameters.wpa_parameters.passphrase = pPassword;

                if ((pHandle->argc != (idx_first_arg_after_pswd + 3)))
                {
                    corona_print(BAD_ARGUMENT);
                    goto done_process_wcfg;
                }
            }
            else
            {
                corona_print("%s %s\n", BAD_ARGUMENT, pHandle->argv[idx_first_arg_after_pswd]);
                goto done_process_wcfg;
            }
            
            if (0 == strcmp("none", pHandle->argv[idx_first_arg_after_pswd + 1]))
            {
                sec_params.parameters.wpa_parameters.mcipher = NONE_CRYPT;
            }
            else if (0 == strcmp("wep", pHandle->argv[idx_first_arg_after_pswd + 1]))
            {
                sec_params.parameters.wpa_parameters.mcipher = WEP_CRYPT;
            }
            else if (0 == strcmp("tkip", pHandle->argv[idx_first_arg_after_pswd + 1]))
            {
                sec_params.parameters.wpa_parameters.mcipher = TKIP_CRYPT;
            }
            else if (0 == strcmp("aes", pHandle->argv[idx_first_arg_after_pswd + 1]))
            {
                sec_params.parameters.wpa_parameters.mcipher = AES_CRYPT;
            }
            else
            {
                corona_print("%s %s\n", BAD_ARGUMENT, pHandle->argv[idx_first_arg_after_pswd + 1]);
                goto done_process_wcfg;
            }
            
            if (0 == strcmp("wep", pHandle->argv[idx_first_arg_after_pswd + 2]))
            {
                sec_params.parameters.wpa_parameters.ucipher = ATH_CIPHER_TYPE_WEP;
            }
            else if (0 == strcmp("tkip", pHandle->argv[idx_first_arg_after_pswd + 2]))
            {
                sec_params.parameters.wpa_parameters.ucipher = ATH_CIPHER_TYPE_TKIP;
            }
            else if (0 == strcmp("ccmp", pHandle->argv[idx_first_arg_after_pswd + 2]))
            {
                sec_params.parameters.wpa_parameters.ucipher = ATH_CIPHER_TYPE_CCMP;
            }
            else if (0 != strcmp("none", pHandle->argv[idx_first_arg_after_pswd + 2]))
            {
                corona_print("%s %s\n", BAD_ARGUMENT, pHandle->argv[idx_first_arg_after_pswd + 2]);
                goto done_process_wcfg;
            }
        }
        
wcfg_set_security:

        status = LMMGR_set_security(pSSID, doScan, &sec_params);            
    }
    
    if (0 == status)
    {
        corona_print("OK\n\n");
    }
    else
    {
        WPRINT_ERR("%d", status);
    }
    
done_process_wcfg:

    if(pSSID)
    {
        wfree(pSSID);
    }
    
    if(pPassword)
    {
        wfree(pPassword);
    }
    
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for wcon command.
//!
//! \fntype Reentrant Function
//!
//! \detail Connect to a WI-FI network.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_wcon(cc_handle_t *pHandle)
{
    return WIFIMGR_connect(NULL, NULL, TRUE);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for woff command.
//!
//! \fntype Reentrant Function
//!
//! \detail Turn off Wi-Fi adapter.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_woff(cc_handle_t *pHandle)
{
    LWGPIO_STRUCT wifipwrdn;
    int status;
//
//    whistle_ShutDownWIFI();
//    
//    corona_print("shutdown wi-fi driver\n");    

    wassert( lwgpio_init(&wifipwrdn, BSP_GPIO_WIFI_PWDN_B_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE) );
    lwgpio_set_functionality(&wifipwrdn, BSP_GPIO_WIFI_PWDN_B_MUX_GPIO);
    lwgpio_set_value(&wifipwrdn, LWGPIO_VALUE_LOW);
    

    GPIO_PDOR_REG( PTE_BASE_PTR ) &= ~(1 << 4); // power down WiFi for now

    corona_print("Shutdown\n");
    
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for wdis command.
//!
//! \fntype Reentrant Function
//!
//! \detail Disconnect WI-FI network.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_wdis(cc_handle_t *pHandle)
{
    int status;
    
    status = LMMGR_disconnect();
    
    if (0 == status)
    {
        corona_print("OK\n\n");
    }
    else
    {
        corona_print("ERR %d\n\n", status);
    }
    return ERR_OK;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for wping command.
//!
//! \detail WiFi Ping <IP address>
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_wping(cc_handle_t *pHandle)
{
    A_INT32 result;
    
    result = whistle_wmi_ping(pHandle->argv[1], 1, 64);
    if (result != ERR_OK)
    {
        corona_print("FAIL\n");
    }
    else
    {
        corona_print("OK\n");
    }
    return ERR_OK;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for wlist command.
//!
//! \fntype Reentrant Function
//!
//! \detail List available WI-FI networks.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_wlist(cc_handle_t *pHandle)
{
	WIFIMGR_list();
	return ERR_OK;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for wmiconfig command.
//!
//! \fntype Reentrant Function
//!
//! \detail Pipe into the Atheros wmiconfig command line.
//!         See Atheros Release \doc\ThroughPut Demo document for details on commands.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_wmiconfig(cc_handle_t *pHandle)
{   
#if __ATHEROS_WMICONFIG_SUPPORT__
    A_INT32 result = wmiconfig_handler(pHandle->argc, pHandle->argv);
    corona_print("\n*** WMICONFIG RESULT: (%d) ***\n\n", result);
#else
    corona_print("ERR\n");
#endif
    return ERR_OK;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for wscan command.
//!
//! \fntype Reentrant Function
//!
//! \detail Print out all available WIFI networks and their info.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_wscan(cc_handle_t *pHandle)
{
    uint8_t max_rssi;
    
    LMMGR_printscan( &max_rssi, 0 );
    corona_print("MAX RSSI %d\n", max_rssi);
    
    return ERR_OK;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
// cc_process_rgb
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_rgb(cc_handle_t *pHandle)
{
    matrix_info_t* p_rgb_data   = &rgb_data;
    int err = 0;

//    if (!inited)
//    {
//        inited++;
//        memset (&rgb_data, 0, sizeof(rgb_data));
//    }
    if ((p_rgb_data->r_level = atoi(pHandle->argv[1])) > 15)
    {
        err++;
    }
    if ((p_rgb_data->g_level = atoi(pHandle->argv[2])) > 15)
    {
        err++;
    }
    if ((p_rgb_data->b_level = atoi(pHandle->argv[3])) > 15)
    {
        err++;
    }
    if (err)
    {
        corona_print ("Invalid option\n");
        return ERR_OK;
    }
    
    // DISMGR_run_pattern( DISMGR_PATTERN_OFF );
    // _time_delay(50);

    DISMGR_run_pattern(DISMGR_PATTERN_RGB_DISPLAY);

    return ERR_OK;
}
#endif

#ifdef JIM_LED_TEST

///////////////////////////////////////////////////////////////////////////////
// todo: cc_process_scroll
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_scroll(cc_handle_t *pHandle)
{
    scroll_setup_t* ss2p     = &scroll_2;
    int idx;
    
    if (pHandle->argc > 1)
    {
        idx = 1;
        while (idx < pHandle->argc)
        {
//            corona_print ("1:%s 2:%s argc: %d\n", pHandle->argv[1],pHandle->argv[2],pHandle->argc);
//            _time_delay(500);
            if (strcmp("amask", pHandle->argv[idx]) == 0) // A matrix led mask (hex)
            {
                idx++;
                ss2p->a_led_mask = ((uint_16)strtoul(pHandle->argv[idx], NULL, 16) & 0x3ff);
                idx++;
                continue;
            }
            else if (strcmp("bmask", pHandle->argv[idx]) == 0) // A matrix led mask (hex)
            {
                idx++;
                ss2p->b_led_mask = ((uint_16)strtoul(pHandle->argv[idx], NULL, 16) & 0x3ff);
                idx++;
                continue;
            }
            else if (strcmp("acolor", pHandle->argv[idx]) == 0) // A matrix led mask (hex)
            {
                idx++;
                ss2p->a_color_mask = ((uint_8)strtoul(pHandle->argv[idx], NULL, 16) & 0x03);
                idx++;
                continue;
            }
            else if (strcmp("bcolor", pHandle->argv[idx]) == 0) // A matrix led mask (hex)
            {
                idx++;
                ss2p->b_color_mask = ((uint_8)strtoul(pHandle->argv[idx], NULL, 16) & 0x3ff);
                idx++;
                continue;
            }
            else if (strcmp("speed", pHandle->argv[idx]) == 0) // A matrix led mask (hex)
            {
                idx++;
                ss2p->scroll_speed = atoi(pHandle->argv[idx]) & 0x07;
                idx++;
                continue;
            }
            else if (strcmp("dir", pHandle->argv[idx]) == 0) // A matrix led mask (hex)
            {
                idx++;
                ss2p->scroll_direction = ((uint_8)strtoul(pHandle->argv[idx], NULL, 16) & 0x0f);
                idx++;
                continue;
            }
            else if (strcmp("timer", pHandle->argv[idx]) == 0) // A matrix led mask (hex)
            {
                idx++;
                jdelay = atoi(pHandle->argv[idx]);
                idx++;
                continue;
            }
            else if (strcmp("print", pHandle->argv[idx]) == 0)
            {
                corona_print("amask:%x bmask:%x acolor:%d bcolor:%d speed:%d dir:%d timer: %d\n",
                        ss2p->a_led_mask,ss2p->b_led_mask,ss2p->a_color_mask,ss2p->b_color_mask,ss2p->scroll_speed,ss2p->scroll_direction,jdelay);
                return ERR_OK;
            }
            else
            {
                corona_print (BAD_ARGUMENT);
                return ERR_OK;
            }
        }
    }

    DISMGR_run_pattern( DISMGR_PATTERN_OFF );
    _time_delay(50);

    DISMGR_run_pattern(DISMGR_PATTERN_RUN_SCROLL);

    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
// todo: cc_process_slope
///////////////////////////////////////////////////////////////////////////////
static err_t cc_process_slope(cc_handle_t *pHandle)
{
    matrix_info_t*  slp2p    = &slope_2;
    int idx;
    
    if (pHandle->argc > 1)
    {
        idx = 1;
        while (idx < pHandle->argc)
        {
            if (strcmp("mask", pHandle->argv[idx]) == 0) // A matrix led mask (hex)
            {
                idx++;
                slp2p->led_mask = ((uint_16)strtoul(pHandle->argv[idx], NULL, 16) & 0x3ff);
                idx++;
                continue;
            }
            else if (strcmp("color", pHandle->argv[idx]) == 0) // led mask (hex)
            {
                idx++;
                slp2p->color_mask = ((uint_8)strtoul(pHandle->argv[idx], NULL, 16) & 0x03);
                idx++;
                continue;
            }
            else if (strcmp("delay", pHandle->argv[idx]) == 0)
            {
                idx++;
                slp2p->slope_dly = atoi(pHandle->argv[idx]);
                idx++;
                continue;
            }
            else if (strcmp("cycle", pHandle->argv[idx]) == 0)
            {
                idx++;
                slp2p->slope_cyc = atoi(pHandle->argv[idx]);
                idx++;
                continue;
            }
            else if (strcmp("timer", pHandle->argv[idx]) == 0)
            {
                idx++;
                jdelay = atoi(pHandle->argv[idx]);
                idx++;
                continue;
            }
            else if (strcmp("print", pHandle->argv[idx]) == 0)
            {
                corona_print("mask:%x color:%d delay:%d cycle:%d timer: %d\n",
                        slp2p->led_mask,slp2p->color_mask,slp2p->slope_dly,slp2p->slope_cyc,jdelay);
                return ERR_OK;
            }
            else
            {
                corona_print ("Invalid option\n");
                return ERR_OK;
            }
        }
    }
            
    DISMGR_run_pattern( DISMGR_PATTERN_OFF );
    _time_delay(50);

    DISMGR_run_pattern(DISMGR_PATTERN_RUN_SLOPE);
    
    return ERR_OK;
}

#endif  // JIM_LED_TEST

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading button signal(PTA19).  Not writable.
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_button(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = GPIO_READ(GPIOE_PDIR, 1);
        return ERR_OK;
    }
    return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing BT_PWDN signal (PTB0).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_bt_pwdn(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = PORTB_GPIO_LDD_GetFieldValue(NULL, BT_PWDN_B);
    }
    else if (ioDir == CC_IO_WRITE)
    {
        PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, *pData);
    }
    else
    {
        return ERR_GENERIC;
    }
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing MFI_RST_B signal (PTB1).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_mfi_rst(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = PORTB_GPIO_LDD_GetFieldValue(NULL, MFI_RST_B);
    }
    else if (ioDir == CC_IO_WRITE)
    {
        PORTB_GPIO_LDD_SetFieldValue(NULL, MFI_RST_B, *pData);
    }
    else
    {
        return ERR_GENERIC;
    }
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing RF_SW_V1 signal (PTB2:PTB3).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_rf_sw_vers(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = PORTB_GPIO_LDD_GetFieldValue(NULL, RF_SW_VERSION);
    }
    else if (ioDir == CC_IO_WRITE)
    {
        PORTB_GPIO_LDD_SetFieldValue(NULL, RF_SW_VERSION, *pData);
    }
    else
    {
        return ERR_GENERIC;
    }
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing DATA_SPI_WP_B signal (PTB18).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_data_wp(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = PORTB_GPIO_LDD_GetFieldValue(NULL, DATA_SPI_WP_B);
    }
    else if (ioDir == CC_IO_WRITE)
    {
        PORTB_GPIO_LDD_SetFieldValue(NULL, DATA_SPI_WP_B, *pData);
    }
    else
    {
        return ERR_GENERIC;
    }
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing DATA_SPI_HOLD_B signal (PTB19).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_data_hold(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = PORTB_GPIO_LDD_GetFieldValue(NULL, DATA_SPI_HOLD_B);
    }
    else if (ioDir == CC_IO_WRITE)
    {
        PORTB_GPIO_LDD_SetFieldValue(NULL, DATA_SPI_HOLD_B, *pData);
    }
    else
    {
        return ERR_GENERIC;
    }
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading ACC_INT1 signal (PTC12).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_acc_int1(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = GPIO_READ(GPIOA_PDIR, 13);
        return ERR_OK;
    }
    return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading ACC_INT2 signal (PTC13).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_acc_int2(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = GPIO_READ(GPIOC_PDIR, 13);
        return ERR_OK;
    }
    return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading CHG_B signal (PTC14).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_chg(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = GPIO_READ(GPIOD_PDIR, 0);
        return ERR_OK;
    }
    return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading PGOOD_B signal (PTC15).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_pgood(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = GPIO_READ(GPIOE_PDIR, 2);
        return ERR_OK;
    }
    return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing 26MHZ_MCU_EN signal (PTC19).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_26mhz(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = PORTC_GPIO_LDD_GetFieldValue(NULL, MCU_26MHz_EN);
    }
    else if (ioDir == CC_IO_WRITE)
    {
        PORTC_GPIO_LDD_SetFieldValue(NULL, MCU_26MHz_EN, *pData);
    }
    else
    {
        return ERR_GENERIC;
    }
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing 4 Debug LED signals (PTD12:PTD15).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_led(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = PORTD_GPIO_LDD_GetFieldValue(NULL, LED_DBG);
    }
    else if (ioDir == CC_IO_WRITE)
    {
        PORTD_GPIO_LDD_SetFieldValue(NULL, LED_DBG, *pData);
    }
    else
    {
        return ERR_GENERIC;
    }
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing WIFI_PD_B signal (PTE4).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_wifi_pd(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = PORTE_GPIO_LDD_GetFieldValue(NULL, WIFI_PD_B);
    }
    else if (ioDir == CC_IO_WRITE)
    {
        PORTE_GPIO_LDD_SetFieldValue(NULL, WIFI_PD_B, *pData);
    }
    else
    {
        return ERR_GENERIC;
    }
    return ERR_OK;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading WIFI_INT signal (PTE5).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_wifi_int(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = GPIO_READ(GPIOE_PDIR, 5);
        return ERR_OK;
    }
    return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing 32K_B_ENABLE signal (PTE6).
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static err_t cc_io_32khz(cc_handle_t *pHandle, uint_32 *pData, cc_io_dir_t ioDir)
{
    if (ioDir == CC_IO_READ)
    {
        *pData = PORTE_GPIO_LDD_GetFieldValue(NULL, BT_32KHz_EN);
    }
    else if (ioDir == CC_IO_WRITE)
    {
        PORTE_GPIO_LDD_SetFieldValue(NULL, BT_32KHz_EN, *pData);
    }
    else
    {
        return ERR_GENERIC;
    }
    return ERR_OK;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
