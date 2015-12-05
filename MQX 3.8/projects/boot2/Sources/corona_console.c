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
//! \todo Implement a console that is more thread-driven, instead of polling.
//! \note To make this code (and any GPIO code for that matter) tighter, you could
//! use GPIO_READ and GPIO_WRITE macros rather than using all the GPIO LDD's.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <stdarg.h>
#include <stdlib.h>
#include "corona_console.h"
#include "cfg_factory.h"
#include "PE_Types.h"
#include "ADC0_Vsense.h"
#include "wson_flash.h"
#include "corona_gpio.h"
#include "corona_ext_flash_layout.h"
#include "parse_fwu_pkg.h"
#include "sasha.h"
#include "sherlock.h"
#include "pmem.h"
#include "adc_sense.h"
#include "bu26507_display.h"

//#include "PORTA_GPIO.h"
//#include "PORTB_GPIO_LDD.h"
//#include "PORTC_GPIO_LDD.h"
//#include "PORTD_GPIO_LDD.h"
//#include "PORTE_GPIO_LDD.h"

#include "boot2_mgr.h"

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////
typedef const char * cc_cmd_str_t;
typedef const char * cc_cmd_summary_t;
typedef const char * cc_io_cmd_t;
typedef corona_err_t (*cc_cmd_callback_t)(cc_handle_t *pHandle);

typedef struct corona_console_command
{
	cc_cmd_str_t       cmd;  	   // command name.
	cc_cmd_summary_t   sum;      // brief summary for the command
	uint16_t           minArgs;  // minimum number of arguments including the command itself.
	cc_manual_t        man;  	   // string presented to the user when a 'man' page is requested.
	cc_cmd_callback_t  callback; // function pointer callback to call when command is requested.
}cc_cmd_t;

#if 0
typedef enum
{
  APP_IMAGE_1,
  B2A_IMAGE_1,
  B2B_IMAGE_1,
  APP_IMAGE_2,
  B2A_IMAGE_2,
  B2B_IMAGE_2,
  SPI_MAP_END
}spi_image_idx_t;

typedef struct spi_flash_stuff
{
  char*           name;         // image name
  spi_image_idx_t image;        // image ID
  uint32_t        start_addrs;  // beginning address for image
  uint8_t         num_sectors;  // number of sectors for this image
}spi_image_t;

spi_image_t spi_map[] =
{
  {"app1",  APP_IMAGE_1, 0x000000, 6},
  {"b2a1",  B2A_IMAGE_1, 0x060000, 1},
  {"b2b1",  B2B_IMAGE_1, 0x070000, 1},
  {"app2",  APP_IMAGE_2, 0x080000, 6},
  {"b2a2",  B2A_IMAGE_2, 0x0E0000, 1},
  {"b2b2",  B2B_IMAGE_2, 0x0F0000, 1},
  {"end",   SPI_MAP_END, 0x000000, 0}
};
#endif // 0

typedef struct update_flag
{
        char*       flag_name;
        uint32_t    flag_addrs;
}update_flag_t;

update_flag_t flag_loc[] =
{
        {"app",  APP_HDR + HDR_UPDATE},
        {"b2a",  B2A_HDR + HDR_UPDATE},
        {"b2b",  B2B_HDR + HDR_UPDATE},
};


////////////////////////////////////////////////////////////////////////////////
// External Variables
////////////////////////////////////////////////////////////////////////////////
// extern volatile bool g_UART5_DataTransmittedFlg;   // needed for letting us know when hardware has TX'd data.
extern void         delay (int dcount);
extern void         ck_app_update(void);
extern void         calc_crc_int (void);
extern uint32       calc_crc_pkg (int spi_loc, int pkg_size);
extern void         read_headers(void);
extern corona_err_t b2_load_pkg (int spi_loc);
extern uint_8       _usb_device_deinit(void);
extern void         set_flag(uint8_t *flagp);
extern void         goto_app();
extern cc_handle_t  g_cc_handle;
extern int usbinit;
extern void usb_reset(void);

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static uint8_t  g_mem_pool[8192];
static uint32_t g_mem_pool_idx = 0;
uint8_t g_sherlock_in_progress = 0;

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static void cc_split_str(cc_handle_t *pHandle, char *pCmd);
static void printchar(cc_handle_t *pHandle, char **str, int c);
static void trim_whitesp(char * pStr);
static int  print(cc_handle_t *pHandle, char **out, const char *format, va_list args );
static int  printi(cc_handle_t *pHandle, char **out, int i, int b, int sg, int width, int pad, int letbase);
static int  prints(cc_handle_t *pHandle, char **out, const char *string, int width, int pad);
static uint32_t mask(uint32_t msb, uint32_t lsb);
//static corona_err_t cc_process_io(cc_handle_t *pHandle, cc_io_dir_t direction);
static void usb_printchar (uint8_t* oneChar);


/*
 *    Command Line Callbacks
 */
#if 0 // don't need these compiled into boot2
static corona_err_t cc_process_about(cc_handle_t *pHandle);
static corona_err_t cc_process_acc(cc_handle_t *pHandle);
static corona_err_t cc_process_bfr(cc_handle_t *pHandle);
static corona_err_t cc_process_bfw(cc_handle_t *pHandle);
static corona_err_t cc_process_dis(cc_handle_t *pHandle);
static corona_err_t cc_process_ior(cc_handle_t *pHandle);
static corona_err_t cc_process_iow(cc_handle_t *pHandle);
static corona_err_t cc_process_intc(cc_handle_t *pHandle);
static corona_err_t cc_process_intr(cc_handle_t *pHandle);
static corona_err_t cc_process_key(cc_handle_t *pHandle);
static corona_err_t cc_process_memw(cc_handle_t *pHandle);
static corona_err_t cc_process_memr(cc_handle_t *pHandle);
static corona_err_t cc_process_pwm(cc_handle_t *pHandle);
static corona_err_t cc_process_test_console(cc_handle_t *pHandle);
#endif

static corona_err_t cc_process_adc(cc_handle_t *pHandle);
static corona_err_t cc_process_ccfg(cc_handle_t *pHandle);
static corona_err_t cc_process_cheos(cc_handle_t *pHandle);
static corona_err_t cc_process_factoryreset(cc_handle_t *pHandle);
static corona_err_t cc_process_fle(cc_handle_t *pHandle);
static corona_err_t cc_process_flr(cc_handle_t *pHandle);
static corona_err_t cc_process_flw(cc_handle_t *pHandle);
static corona_err_t cc_process_help(cc_handle_t *pHandle);
static corona_err_t cc_process_man(cc_handle_t *pHandle);
static corona_err_t cc_process_memr(cc_handle_t *pHandle);
static corona_err_t cc_process_fwa (cc_handle_t *pHandle);
static corona_err_t cc_process_fws (cc_handle_t *pHandle);
static corona_err_t cc_process_rpkg (cc_handle_t *pHandle);
static corona_err_t cc_process_lpkg (cc_handle_t *pHandle);
static corona_err_t cc_process_rhdr (cc_handle_t *pHandle);
static corona_err_t cc_process_reset(cc_handle_t *pHandle);
static corona_err_t cc_process_pcfg(cc_handle_t *pHandle);
static corona_err_t cc_process_scfg(cc_handle_t *pHandle);
static corona_err_t cc_process_sherlock(cc_handle_t *pHandle);
static corona_err_t cc_process_state(cc_handle_t *pHandle);
static corona_err_t cc_process_gto (cc_handle_t *pHandle);
static corona_err_t cc_process_setupd (cc_handle_t *pHandle);
static corona_err_t cc_process_pwr(cc_handle_t *pHandle);
static corona_err_t cc_process_uid(cc_handle_t *pHandle);
static corona_err_t cc_process_who(cc_handle_t *pHandle);

/*
 *    GPIO Callbacks
 */
#ifdef INCLUDE_ALL
static corona_err_t cc_io_button(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_bt_pwdn(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_mfi_rst(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_rf_sw_vers(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_data_wp(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_data_hold(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_acc_int1(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_acc_int2(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_chg(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_pgood(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_26mhz(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_led(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_wifi_pd(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_wifi_int(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
static corona_err_t cc_io_32khz(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);
#endif

/*
 *   Array of sectors to erase when doing a factory reset.
 */
static const uint32_t g_factoryreset_sectors[] = \
{
    EXT_FLASH_CFG_STATIC_ADDR,
    EXT_FLASH_CFG_SERIALIZED1_ADDR,
    EXT_FLASH_CFG_SERIALIZED2_ADDR,
    EXT_FLASH_EVTMGR_INFO_ADDR,
    EXT_FLASH_EVTMGR_INFO_ADDR_DEP,
    EXT_FLASH_FWUPDATE_INFO_ADDR,
    EXT_FLASH_FTA_CFG_ADDR,
};

#define NUM_FACTORYRESET_SECTORS    ( sizeof(g_factoryreset_sectors) / sizeof(uint32_t) )

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
 *   COMMAND          SUMMARY                      MIN ARGS    MANUAL        CALLBACK
 */
	  {"help", "help",                                 1,  CC_MAN_HELP, cc_process_help},
	  {"h",    "^",                                    1,  CC_MAN_HELP, cc_process_help},
	  {"?",    "^",                                    1,  CC_MAN_HELP, cc_process_help},
#if 0
	  {"about","print about menu",                	   1,  CC_MAN_ABT,  cc_process_about},
	  {"acc",  "accelerometer control",                2,  CC_MAN_ACC,  cc_process_acc},
	  {"adc",  "sample UID or VBatt voltage",          4,  CC_MAN_ADC,  cc_process_adc},
	  {"bfr",  "bit field read",                       4,  CC_MAN_BFR,  cc_process_bfr},
	  {"bfw",  "bit field write",                      5,  CC_MAN_BFW,  cc_process_bfw},
	  {"dis", "display write, ROHM register",          3,  CC_MAN_DIS,  cc_process_dis},
#endif
	  {"ccfg",   "clr factory cfgs",                   2,  CC_MAN_CCFG, cc_process_ccfg},
	  {"cheos",  "pure cheos",                         1,  "cheotic\n",   cc_process_cheos},
	  {"factoryreset", "",                             1,  CC_MAN_FACTRST,  cc_process_factoryreset},
	  {"fle", "",                                      2,  CC_MAN_FLE,  cc_process_fle},
	  {"flr", "", 		                               3,  CC_MAN_FLR,  cc_process_flr},
#if 0
	  {"flw", "write to SPI Flash memory",             3,  CC_MAN_FLW,  cc_process_flw},
#endif
	  {"fwa",  "",                                     3,  CC_MAN_FWA,  cc_process_fwa},
      {"fws",  "",                                     2,  CC_MAN_FWS,  cc_process_fws},
      {"rpkg", "read fw pkg",                          1,  CC_MAN_RPKG, cc_process_rpkg},
      {"lpkg", "load fw pkg",                          1,  CC_MAN_LPKG, cc_process_lpkg},
      {"rhdr", "read hdrs",                            1,  CC_MAN_RHDR, cc_process_rhdr},
      {"gto",  "go to app",                            1,  CC_MAN_GTO,  cc_process_gto},
      {"setupd","",                                    2,  CC_MAN_SETUPD,cc_process_setupd},
#if 0
	  {"intc", "Clear an IRQ count",                   2,  CC_MAN_INTC, cc_process_intc},
	  {"intr", "Read an IRQ count",                    2,  CC_MAN_INTR, cc_process_intr},
	  {"ior",  "read GPIO(s)",                         2,  CC_MAN_IOR,  cc_process_ior},
	  {"iow",  "write GPIO(s)",                        3,  CC_MAN_IOW,  cc_process_iow},
	  {"key",  "change security",                      2,  CC_MAN_KEY,  cc_process_key},
#endif
	  {"man",  "manual",                               2,  CC_MAN_MAN,  cc_process_man},
	  //{"memr", "read from memory",                     3,  CC_MAN_MEMR, cc_process_memr},
	  {"reset","reset device",                         1,  CC_MAN_RESET,cc_process_reset},
#if 0
	  {"memw", "write to memory",                      3,  CC_MAN_MEMW, cc_process_memw},
	  {"pwm",  "change PWM",                           2,  CC_MAN_PWM,  cc_process_pwm},
	  {"test", "test console argv",                    1,  CC_MAN_TEST, cc_process_test_console},
#endif
	  {"pwr",  "",                                     2,  CC_MAN_PWR,  cc_process_pwr},
	  {"pcfg", "print factory cfgs",                   1,  CC_MAN_PCFG, cc_process_pcfg},
	  {"scfg", "set factory cfg",                      3,  CC_MAN_SCFG, cc_process_scfg},
	  {"sher", "",                                     1,  CC_MAN_SHERLOCK, cc_process_sherlock},
	  {"state","get state",                            1,  CC_MAN_STATE, cc_process_state},
	  {"uid",  "cable",                                1,  CC_MAN_UID,  cc_process_uid},
	  {"who",  "boot2",                                1,  CC_MAN_WHO,  cc_process_who},
};

/*
** ===================================================================
**     Global Array      :  g_cc_io_entries
**
**     Description :
**         Table of structures which define each GPIO and its callback.
** ===================================================================
*/
#ifdef INCLUDE_ALL

static const cc_io_entry_t g_cc_io_entries[] = \
{
/*
 *  	      TYPE          COMMAND         CALLBACK
 */
		{  CC_IO_RO_TYPE,  "button",      cc_io_button    },
		{  CC_IO_RW_TYPE,  "bt_pwdn",     cc_io_bt_pwdn   },
		{  CC_IO_RW_TYPE,  "mfi_rst",     cc_io_mfi_rst   },
		{  CC_IO_RW_TYPE,  "rf_sw_vers",  cc_io_rf_sw_vers},
		{  CC_IO_RW_TYPE,  "data_wp",     cc_io_data_wp   },
		{  CC_IO_RW_TYPE,  "data_hold",   cc_io_data_hold },
		{  CC_IO_RO_TYPE,  "acc_int1",    cc_io_acc_int1  },
		{  CC_IO_RO_TYPE,  "acc_int2",    cc_io_acc_int2  },
		{  CC_IO_RO_TYPE,  "chg",         cc_io_chg       },
		{  CC_IO_RO_TYPE,  "pgood",       cc_io_pgood     },
		{  CC_IO_RW_TYPE,  "26mhz",       cc_io_26mhz     },
		{  CC_IO_RW_TYPE,  "led",         cc_io_led       },
		{  CC_IO_RW_TYPE,  "wifi_pd",     cc_io_wifi_pd   },
		{  CC_IO_RO_TYPE,  "wifi_int",    cc_io_wifi_int  },
		{  CC_IO_RW_TYPE,  "32khz",       cc_io_32khz     },
};
#endif

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define NUM_CC_COMMANDS	sizeof(g_cc_cmds) / sizeof(cc_cmd_t)
#define PAD_RIGHT 1
#define PAD_ZERO 2
#define PRINT_BUF_LEN 12   // should be enough for 32 bit int

// Factory Config (strictly reserved sector --  CANNOT change!)
#define EXT_FLASH_FACTORY_CFG_ADDR           0xC000

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

static corona_err_t _print_abort(cc_handle_t *pHandle)
{
    cc_print (pHandle, "Aborting\n");
    return CC_INVALID_CMD;
}

// Wrapper function for fcfg's free format.
unsigned long fcfg_free(void *pAddr)
{
    return 0;
}

// Wrapper function for malloc.
void *fcfg_malloc(unsigned long size)
{
    void *pReturn = NULL;

    if( (size + g_mem_pool_idx + 1) > 8192 )
    {
        /*
         *   Wraps around, so start from the beginning.
         */
        g_mem_pool_idx = size;
        pReturn = (void *) &( g_mem_pool[0] );
    }
    else
    {
        g_mem_pool_idx += size;
        pReturn = (void *) &( g_mem_pool[g_mem_pool_idx - size] );
    }

    return pReturn;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Initializes a handle for a Corona Console.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function initializes a cc_handle_t for usage in managing a Corona console instance.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t cc_init(cc_handle_t *pHandle, void *pDevice, console_type_t type)
{
	//pHandle->security = CC_SECURITY_0; // default is strictest security level until proven more lenient.
	pHandle->type = type;
	pHandle->pUart = NULL;

	switch(type)
	{
		case CC_TYPE_UART:
			pHandle->pUart = (LDD_TDeviceData *)pDevice; // UART pointer we can use to write serial data to the K60 UART.
			break;

		case CC_TYPE_USB:
		case CC_TYPE_JTAG:  // USB and JTAG don't need special handles
			break;

		default:
			return CC_ERR_INVALID_CONSOLE_TYPE;
	}
	
    /*
     *   Check to see if Sherlock is enabled.
     */
    SHLK_init(      &(pHandle->shlk_hdl),
                    wson_read,
                    wson_write,
                    shlk_printf,
                    EXT_FLASH_SHLK_ASSDUMP_ADDR,
                    EXT_FLASH_SHLK_LOG_ADDR,
                    (sh_u8_t)EXT_FLASH_SHLK_LOG_NUM_SECTORS      );
	
	return SUCCESS;
}

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
static corona_err_t cc_process_uid(cc_handle_t *pHandle)
{
    int i = 0;
    uint16_t raw, natural;
    cable_t cable;
    
    adc_sense(ADC0_Vsense_Corona_UID, &raw, &natural);
    
    cc_print(pHandle, "Counts: %u\n", raw);
    cable = adc_get_cable((unsigned long) raw);
    
    cc_print(pHandle, "Cable: ");
    switch(cable)
    {
        case CABLE_BURNIN:
            cc_print(pHandle, "burn in\n");
            break;
            
        case CABLE_FACTORY_DBG:
            cc_print(pHandle, "factory dbg\n");
            break;
            
        case CABLE_CUSTOMER:
            cc_print(pHandle, "customer\n");
            break;
            
        default:
            cc_print(pHandle, "ERR\n");
    }
    
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'who' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Print out which app.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_who(cc_handle_t *pHandle)
{
    cc_print(pHandle, "boot2\n");
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
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t cc_process_cmd(cc_handle_t *pHandle, char *pCmd)
{
	int i;

	// Break up the commands into substrings and populate our structures.
	cc_split_str(pHandle, pCmd);

	if(pHandle->argc == 0)
	{
		return CC_INVALID_CMD; // no arguments given.
	}

	for(i = 0; i < pHandle->argc; i++)
	{
		trim_whitesp(pHandle->argv[i]); // get rid of any newlines or garbage that could screw up our number/string conversions.
	}

	cc_print(pHandle, "\n\r");

	// Is the string something we can understand?  If so, call it's callback, return status.
	for(i = 0; i < NUM_CC_COMMANDS; i++)
	{
		if(!strcmp(pHandle->argv[0], g_cc_cmds[i].cmd))
		{
			// Make sure the correct number or minimum arguments were passed in for this command.
			if(pHandle->argc < g_cc_cmds[i].minArgs)
			{
				cc_print(pHandle, "\nNeed more args\n");
				return CC_NOT_ENOUGH_ARGS;
			}
			return g_cc_cmds[i].callback(pHandle);
		}
	}
	cc_print(pHandle, "Unsupported\n");
	return CC_INVALID_CMD;
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
	int i;

	pHandle->argc = 0;

	for(i = 0; i < CC_MAX_ARGS; i++)
	{
		// Skip leading whitespace
		while(isspace(*pScan))
			pScan++;

		if(*pScan != '\0')
			pHandle->argv[pHandle->argc++] = pScan;
		else {
			pHandle->argv[pHandle->argc] = 0;
			break;
		}

		// Scan over arg.
		while(*pScan != '\0' && !isspace(*pScan))
			pScan++;

		// Terminate arg.
		if(*pScan != '\0' && i < CC_MAX_ARGS-1)
			*pScan++ = '\0';
	}
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Print Something to the console.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This functions sends the payload to the UART5 console on Corona.
//! It sends the data, then blocks until the hardware provides confirmation data was TX'd.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t cc_print(cc_handle_t *pHandle, const char *format, ...)
{
    va_list args;

    va_start( args, format );

    if(pHandle->type == CC_TYPE_JTAG)
    {
    	printf(format, args);  // \todo this is not printing things properly, needs fixing.
    }
    else
    {
        if (usbinit)
        {
            print( pHandle, 0, format, args );
        }
    }

	return SUCCESS;
}

static int print(cc_handle_t *pHandle, char **out, const char *format, va_list args )
{
	register int width, pad;
	register int pc = 0;
	char scr[2];

	for (; *format != 0; ++format) {
		if (*format == '%') {
			++format;
			width = pad = 0;
			if (*format == '\0') break;
			if (*format == '%') goto out;
			if (*format == '-') {
				++format;
				pad = PAD_RIGHT;
			}
			while (*format == '0') {
				++format;
				pad |= PAD_ZERO;
			}
			for ( ; *format >= '0' && *format <= '9'; ++format) {
				width *= 10;
				width += *format - '0';
			}
			if( *format == 's' ) {
				register char *s = (char *)va_arg( args, int );
				pc += prints (pHandle, out, s?s:"(null)", width, pad);
				continue;
			}
            if ( (*format == 'd') || (*format == 'i') ) {
				pc += printi (pHandle, out, va_arg( args, int ), 10, 1, width, pad, 'a');
				continue;
			}
			if( *format == 'h' ) {
                pc += printi(pHandle, out, va_arg(args, signed char), 10, 1, width, pad, 'a');
				continue;
			}
			if( *format == 'x' ) {
				pc += printi (pHandle, out, va_arg( args, int ), 16, 0, width, pad, 'a');
				continue;
			}
			if( *format == 'X' ) {
				pc += printi (pHandle, out, va_arg( args, int ), 16, 0, width, pad, 'A');
				continue;
			}
			if( *format == 'u' ) {
				pc += printi (pHandle, out, va_arg( args, unsigned long ), 10, 0, width, pad, 'a');
				continue;
			}
			if( *format == 'c' ) {
				/* char are converted to int then pushed on the stack */
				scr[0] = (char)va_arg( args, int );
				scr[1] = '\0';
				pc += prints (pHandle, out, scr, width, pad);
				continue;
			}
		}
        else
        {
out:
            printchar(pHandle, out, *format);
            ++pc;
        }
	}
	if (out) **out = '\0';
	va_end( args );
	return pc;
}

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
		//LDD_TError Error;
		//uint8_t status;
		uint8_t aChar = c;

		switch(pHandle->type)
		{
#if 0
			case CC_TYPE_UART:
//				Error = UART5_DBG_SendBlock(pHandle->pUart, (void *)&c, 1);
//				while (!g_UART5_DataTransmittedFlg){}
//				g_UART5_DataTransmittedFlg = FALSE;
				break;
#endif
			case CC_TYPE_USB:
			  usb_printchar (&aChar);
				break;

			case CC_TYPE_JTAG:
			default:
				printf("JTAG sucks\n");
				while(1){}
		}
	}
}


// Collect printchar's output

#define NUM_LBUFS 8   // Number of line buffers
#define LBUF_SIZE 32  // Size of each line buffer

static void usb_printchar (uint8_t* oneChar)
{
  static uint8_t   lbuf[NUM_LBUFS][LBUF_SIZE];
  static uint8_t*  active_lptr = &lbuf[0][0];
  static int      active_lbuf_index = 0;
  static int      line_idx = 0;
  int status;
  int c;

  c = *oneChar;
  active_lptr[line_idx++] = c;
  
  if( c == (int)'\n')
  {
      /*
       *   Insert carriage return character.
       */
      active_lptr[line_idx++] = '\r';
  }

  if ((c == (int)'\n') || (c == (int)"\r") || (line_idx == LBUF_SIZE))
  {
    status = b2_send_data (&active_lptr[0], line_idx);
    if (status) // != USB_OK
    {
      /* Send Data Error Handling Code goes here */
      //printf("USB Send Error! (0x%x)\n", status);
    }
    sleep(10);
    line_idx = 0;
    active_lbuf_index++;
    active_lbuf_index %= NUM_LBUFS;
    active_lptr = &lbuf[active_lbuf_index][0];
  }
}

static int prints(cc_handle_t *pHandle, char **out, const char *string, int width, int pad)
{
	register int pc = 0, padchar = ' ';

	if (width > 0) {
		register int len = 0;
		register const char *ptr;
		for (ptr = string; *ptr; ++ptr) ++len;
		if (len >= width) width = 0;
		else width -= len;
		if (pad & PAD_ZERO) padchar = '0';
	}
	if (!(pad & PAD_RIGHT)) {
		for ( ; width > 0; --width) {
			printchar (pHandle, out, padchar);
			++pc;
		}
	}
	for ( ; *string ; ++string) {
		printchar (pHandle, out, *string);
		++pc;
	}
	for ( ; width > 0; --width) {
		printchar (pHandle, out, padchar);
		++pc;
	}

	return pc;
}

static int printi(cc_handle_t *pHandle, char **out, int i, int b, int sg, int width, int pad, int letbase)
{
	char print_buf[PRINT_BUF_LEN];
	register char *s;
	register int t, neg = 0, pc = 0;
	register unsigned int u = i;

	if (i == 0) {
		print_buf[0] = '0';
		print_buf[1] = '\0';
		return prints (pHandle, out, print_buf, width, pad);
	}

	if (sg && b == 10 && i < 0) {
		neg = 1;
		u = -i;
	}

	s = print_buf + PRINT_BUF_LEN-1;
	*s = '\0';

	while (u) {
		t = u % b;
		if( t >= 10 )
			t += letbase - '0' - 10;
		*--s = t + '0';
		u /= b;
	}

	if (neg) {
		if( width && (pad & PAD_ZERO) ) {
			printchar (pHandle, out, '-');
			++pc;
			--width;
		}
		else {
			*--s = '-';
		}
	}

	return pc + prints (pHandle, out, s, width, pad);
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'intc' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Clear the IRQ count for a given IRQ.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_intc(cc_handle_t *pHandle)
{
	if(ciu_clr_str(pHandle->argv[1]) != SUCCESS)
	{
		cc_print(pHandle, "Error: %s is not a valid IRQ name.\n", pHandle->argv[1]);
		return CC_IRQ_UNSUPPORTED;
	}

	return cc_print(pHandle, "%s IRQ Count: 0\n", pHandle->argv[1]);
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'intr' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read the IRQ count for a given IRQ.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_intr(cc_handle_t *pHandle)
{
	uint32_t count;
	if(ciu_cnt_str(pHandle->argv[1], &count) != SUCCESS)
	{
		cc_print(pHandle, "Error: %s is not a valid IRQ name.\n", pHandle->argv[1]);
		return CC_IRQ_UNSUPPORTED;
	}

	return cc_print(pHandle, "%s IRQ Count: %d\n", pHandle->argv[1], count);
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'key' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Process handling of the 'key' command, which is used to change
//! the security level for this console session.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_key(cc_handle_t *pHandle)
{
	static volatile uint32_t randomNumber = 0xDEADBEEF;

	if(!strcmp(pHandle->argv[1], CC_SECURITY_1_PASSWORD))
	{
		cc_print(pHandle, "Security 1 Granted.\n");
		pHandle->security = CC_SECURITY_1;
	}
	else if(!strcmp(pHandle->argv[1], CC_SECURITY_2_NUM_REQ))
	{
		cc_print(pHandle, "0x%d\n", (randomNumber ^ (uint32_t)pHandle));
	}
	else if(atoi(pHandle->argv[1]) == (randomNumber ^ CC_SECURITY_2_KEY) )
	{
		cc_print(pHandle, "Security 2 Granted.\n", randomNumber);
		pHandle->security = CC_SECURITY_2;
	}
	else if(!strcmp(pHandle->argv[1], CC_SECURITY_0_RESET))
	{
		cc_print(pHandle, "Reset to Security 0.\n");
		pHandle->security = CC_SECURITY_0;
	}
	else
	{
		cc_print(pHandle, "Argument not recognized.\n");
	}

	randomNumber ^= (uint32_t)pHandle; // make number more random each time.
}
#endif

/*
 *   Reboot.
 */
void wassert_reboot(void)
{
    uint32_t read_value;
    
    usb_reset();
    
    read_value = SCB_AIRCR;
    
    // Issue a Kinetis Software System Reset
    read_value &= ~SCB_AIRCR_VECTKEY_MASK;
    read_value |= SCB_AIRCR_VECTKEY(0x05FA);
    read_value |= SCB_AIRCR_SYSRESETREQ_MASK;

    __DI(); // disable interrupts
    
    SCB_AIRCR = read_value;
    
    while (1)
    {
    }
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'reset' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Wait some number of milliseconds, then reboot.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_reset(cc_handle_t *pHandle)
{
    cc_print(pHandle, "Resetting device in 3 seconds (close terminal now!).\n");
    sleep( 3 * 1000 );

    wassert_reboot();

    return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'pwr' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Process handling of the 'pwr' command, which is used to change
//! the power state of the system.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_pwr(cc_handle_t *pHandle)
{
/*
 *   NOTE:  There is no guarantee that pmem is valid here.
 *          But we can assume that if it is not valid, it will at least
 *          all be cleared.  Since APP_MAGIC_INIT_VAL will never be set by boot2,
 *          this is OK, since the app is responsible for initializing itself, if that's not set
 *          to the corresponding magic number.
 */
    
	if(!strcmp(pHandle->argv[1], "ship"))
	{
        pmem_flag_set(PV_FLAG_GO_TO_SHIP_MODE, TRUE);   // Changed per FW-714
        return SUCCESS;
	}
	else
	{
		cc_print(pHandle, "ERR %s\n", pHandle->argv[1]);
	}
	
	return CC_ERR_INVALID_PWR_STATE;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'test' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Just print back argc/argv to test things out.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_test_console(cc_handle_t *pHandle)
{
	int i;
	cc_print(pHandle, "\nTesting console.\n");

	// Print out argv/argc.
	for(i = 0; i < pHandle->argc; i++)
	{
		cc_print(pHandle, pHandle->argv[i]);
		cc_print(pHandle, "\n");
	}

	cc_print(pHandle, "Printing decimal int: %d\n", 123456789);
	cc_print(pHandle, "Printing hexadecimal int: 0x%x\n", 0xDEADBEEF);
	cc_print(pHandle, "Printing single char: %c\n", 'q');
	cc_print(pHandle, "Printing string: %s\n", "String of Test, this is... fool.!@#$^&*()");
	cc_print(pHandle, "Printing floating point number: %.3f. Doubt that worked...?\n", 123.456);
	cc_print(pHandle, "table_test address: 0x%x\n", g_table_test);
	cc_print(pHandle, "mask 17:13: 0x%x\n", mask(17, 13));
	cc_print(pHandle, "mask 31:0: 0x%x\n", mask(31, 0));
	cc_print(pHandle, "mask 1:0: 0x%x\n", mask(1, 0));
	cc_print(pHandle, "mask 0:0: 0x%x\n", mask(0, 0));
	cc_print(pHandle, "mask 31:31: 0x%x\n", mask(31, 31));
	cc_print(pHandle, "mask 31:30: 0x%x\n", mask(31, 30));
	cc_print(pHandle, "mask 29:11: 0x%x\n", mask(29, 11));
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'man' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Present the manual for the current command associated with pHandle.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_man(cc_handle_t *pHandle)
{
	int i;
	for(i = 0; i < NUM_CC_COMMANDS; i++)
	{
		if(!strcmp(pHandle->argv[1], g_cc_cmds[i].cmd))
		{
			cc_print(pHandle, g_cc_cmds[i].man);
			return SUCCESS;
		}
	}
	cc_print(pHandle, "\nno manual\n");
	return CC_NO_MAN;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'bfr' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read a bitfield from a specific 32-bit address anywhere in memory.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_bfr(cc_handle_t *pHandle)
{
	uint32_t *pAddr;
	uint32_t bitfield;
	int idx = 0;
	int lsb, msb;

	pAddr = (uint32_t *)strtoul(pHandle->argv[1], NULL, 16);
	if(pAddr == NULL)
	{
		return cc_print(pHandle, "Error: Could not convert address to integer.\n");
	}

	msb = atoi(pHandle->argv[2]);
	lsb = atoi(pHandle->argv[3]);
	bitfield = (mask(msb, lsb) & *pAddr) >> lsb;

	cc_print(pHandle, "Address: 0x%x\n", pAddr);
	cc_print(pHandle, "Value <31:0>:\t0x%x\n", *pAddr);
	cc_print(pHandle, "Value <%d:%d>:\t0x%x\n\n", msb, lsb, bitfield);
	return SUCCESS;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'ior' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read GPIO(s).
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_ior(cc_handle_t *pHandle)
{
	return cc_process_io(pHandle, CC_IO_READ);
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'pwm' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Set the PWM duty cycle from 0-100%
//!
//! \return corona_err_t error code
//! \todo add PWM by ms and us ?
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_pwm(cc_handle_t *pHandle)
{
	corona_err_t err;
	uint16_t percentage = atoi(pHandle->argv[1]);

	err = bu26507_pwm(percentage);
	if(err != SUCCESS)
	{
		cc_print(pHandle, "PWM Error! (0x%x)\n", err);
	}
	return err;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'iow' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Write GPIO(s).
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_iow(cc_handle_t *pHandle)
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
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_io(cc_handle_t *pHandle, cc_io_dir_t direction)
{
	int i;
	uint32_t data;
	uint32_t numSeconds = 0;
	uint32_t numLoops = 0;

	for(i = 0; i < NUM_IO_COMMANDS; i++)
	{
		if(!strcmp(pHandle->argv[1], g_cc_io_entries[i].cmd))
		{
			switch(direction)
			{
				case CC_IO_WRITE:
					if(g_cc_io_entries[i].type != CC_IO_RW_TYPE)
					{
						cc_print(pHandle, "Error: This GPIO is not writable.\n");
						return CC_UNWRITABLE_GPIO;
					}
					data = (uint32_t)strtoul(pHandle->argv[2], NULL, 16);
					return g_cc_io_entries[i].callback(pHandle, &data, direction);

				case CC_IO_READ:
					if(pHandle->argc > 2)
					{
						// User wants us to poll/print the signal for N seconds, so handle that case.
						numSeconds = atoi(pHandle->argv[2]);
						numLoops = (numSeconds*1000) / 100;
					}

					do
					{
						g_cc_io_entries[i].callback(pHandle, &data, direction);
						cc_print(pHandle, "%s: 0x%x                   \r", g_cc_io_entries[i].cmd, data);
						if(numLoops != 0)
						{
							sleep(100);
						}
					}while(numLoops-- > 0);
					cc_print(pHandle, "\n");
					return SUCCESS;

				default:
					cc_print(pHandle, "Error: Read or Write not specified.\n");
					return CC_ERROR_GENERIC;
			}
		}
	}
	cc_print(pHandle, "Error: (%s) is not a valid GPIO value.\n", pHandle->argv[1]);
	return CC_ERROR_GENERIC;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'bfw' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Write a bitfield to a specific 32-bit address anywhere in memory.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_bfw(cc_handle_t *pHandle)
{
	uint32_t *pAddr;
	uint32_t val;
	uint32_t contents;
	int idx = 0;
	int lsb, msb;

	pAddr = (uint32_t *)strtoul(pHandle->argv[1], NULL, 16);
	if(pAddr == NULL)
	{
		return cc_print(pHandle, "Error: Could not convert address to integer.\n");
	}

	msb = atoi(pHandle->argv[2]);
	lsb = atoi(pHandle->argv[3]);
	val = (uint32_t)strtoul(pHandle->argv[4], NULL, 16);
	val <<= lsb; // the value given by user is right justified, so shift it into position.
	if(val > mask(msb, lsb))
	{
		return cc_print(pHandle, "Error: value passed is too big to fit into bitfield.\n");
	}

	cc_print(pHandle, "Writing 0x%x to <%d:%d> at 0x%x ...\n", val, msb, lsb, pAddr);
	contents = *pAddr; // contents BEFORE the write.
	contents &= ~mask(msb, lsb); // now we need to create a mask in the range of the bitfield specified
	contents |= val;
	*pAddr = contents; // perform the final write to memory location chosen by user.

	cc_print(pHandle, "0x%x: 0x%x\n\n", pAddr, *pAddr);
	return SUCCESS;
}
#endif

#if 0
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
static uint32_t mask(uint32_t msb, uint32_t lsb)
{
	uint32_t ret = 0;
	uint32_t numOnes = msb - lsb + 1;
	if(msb < lsb)
	{
		return 0;
	}

	while(numOnes-- > 0)
	{
		ret = 1 | (ret << 1);
	}

	return (ret << lsb);
}
#endif

///////////////////////////////////////////////////////////////////////////////
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
static corona_err_t cc_process_pcfg(cc_handle_t *pHandle)
{
    fcfg_tag_t tag;
    fcfg_handle_t handle;
    fcfg_err_t ferr;
    fcfg_len_t len;
    char *pDesc;

    if( FCFG_OK != FCFG_init(&handle,
                                  EXT_FLASH_FACTORY_CFG_ADDR,
                                  &wson_read,
                                  &wson_write,
                                  &fcfg_malloc,
                                  &fcfg_free) )
    {
        return cc_print(pHandle, "ERR init FCFG!\n");
    }

    cc_print(pHandle, "BOARD\t%u\n",  FCFG_board(&handle));
    cc_print(pHandle, "LED\t%u\n",    FCFG_led(&handle));
    cc_print(pHandle, "SILEGO\t%u\n", FCFG_silego(&handle));
    cc_print(pHandle, "RF\t%u\n",     FCFG_rfswitch(&handle));
    
    cc_print(pHandle, "FACTORY CONFIGS:\n\n");
    for(tag = CFG_FACTORY_FIRST_FCFG; tag <= CFG_FACTORY_LAST_FCFG; tag++)
    {
        ferr = FCFG_getdescriptor(tag, &pDesc);
        if( FCFG_OK != ferr )
        {
            return cc_print(pHandle, "ERR: %i getdesc TAG: %i\n", ferr, tag);
        }
        cc_print(pHandle, "TAG: %i\tDESC: %s\t", tag, pDesc);

        ferr = FCFG_getlen(&handle, tag, &len);
        if( FCFG_VAL_UNSET == ferr )
        {
            cc_print(pHandle, "(unset)\n");
            continue;
        }
        else if( FCFG_OK != ferr )
        {
            return cc_print(pHandle, "ERR: %i getlen TAG: %i\n", ferr, tag);
        }
        cc_print(pHandle, "LEN: %i\tVALUE: ", len);
        if(0 == len)
        {
            return cc_print(pHandle, "ERR: 0 len\n");
        }

        if( FCFG_INT_SZ == len )
        {
            /*
             *   Assume this length is a number, since we disallow setting strings to a length of 4.
             */
            uint32_t val;

            ferr = FCFG_get(&handle, tag, &val);
            if( FCFG_OK != ferr )
            {
                return cc_print(pHandle, "ERR: %i get TAG: %i\n", ferr, tag);
            }
            cc_print(pHandle, "hex:0x%x dec:%i\n", val, val);
        }
        else
        {
            /*
             *   Must be a string, print it as such.
             */
            char *pStr;

            pStr = fcfg_malloc(len+1);
            if( NULL == pStr )
            {
                return cc_print(pHandle, "ERR: alloc\n");
            }

            ferr = FCFG_get(&handle, tag, pStr);
            if( FCFG_OK != ferr )
            {
                fcfg_free(pStr);
                return cc_print(pHandle, "ERR: %i get TAG: %i\n", ferr, tag);
            }
            cc_print(pHandle, "%s\n", pStr);
            fcfg_free(pStr);
        }
    }

    cc_print(pHandle, "\n\n");
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
static corona_err_t cc_process_scfg(cc_handle_t *pHandle)
{
    fcfg_handle_t handle;
    fcfg_err_t ferr;
    fcfg_tag_t tag;
    uint32_t val;

    if( FCFG_OK != FCFG_init(&handle,
                                  EXT_FLASH_FACTORY_CFG_ADDR,
                                  &wson_read,
                                  &wson_write,
                                  &fcfg_malloc,
                                  &fcfg_free) )
    {
        return cc_print(pHandle, "ERR init\n");
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
        return cc_print(pHandle, "ERR %i get tag from desc\n", ferr);
    }

    /*
     *   String or 32-bit number ?
     */
    if( '$' == pHandle->argv[2][0])
    {
        // String

        fcfg_len_t len = strlen( &(pHandle->argv[2][1]) ) + 1;
        char maybe[6];

        cc_print(pHandle, "Set TAG: %i to str: %s\n", tag, &(pHandle->argv[2][1]));

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
            cc_print(pHandle, "ERR %i set TAG %i\n", ferr, tag);
        }
        return ERR_OK;
    }
    else
    {
        // 32-bit unsigned

        val = strtoul(pHandle->argv[2], NULL, 10);
        cc_print(pHandle, "Setting TAG: %i to value %i\n", tag, val);
        ferr = FCFG_set(&handle, tag, sizeof(uint32_t), &val);
        if( FCFG_OK != ferr )
        {
            cc_print(pHandle, "ERR %i set TAG %i\n", ferr, tag);
        }
        return ERR_OK;
    }
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'cheos' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Pure ChEOS
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_cheos(cc_handle_t *pHandle)
{
     cc_print(pHandle, "\n\n%s\n\n", SASHA_PHOTO);
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
static corona_err_t cc_process_ccfg(cc_handle_t *pHandle)
{
    fcfg_handle_t handle;
    fcfg_err_t ferr;
    fcfg_tag_t tag;

    if( FCFG_OK != FCFG_init(&handle,
                                  EXT_FLASH_FACTORY_CFG_ADDR,
                                  &wson_read,
                                  &wson_write,
                                  &fcfg_malloc,
                                  &fcfg_free) )
    {
        return cc_print(pHandle, "ERR init\n");
    }

    if (!strcmp(pHandle->argv[1], "*"))
    {
        cc_print(pHandle, "Clearing all factory configs!\n");
        if( FCFG_OK != FCFG_clearall(&handle) )
        {
            return cc_print(pHandle, "ERR clr\n");
        }
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
        return cc_print(pHandle, "ERR %i get tag from desc!\n", ferr);
    }

    cc_print(pHandle, "Clearing TAG: %i ...\n", tag);
    ferr = FCFG_clear(&handle, tag);
    if( FCFG_OK != ferr )
    {
        return cc_print(pHandle, "ERR %i clr TAG %i\n", ferr, tag);
    }
    return ERR_OK;
}

void shlk_printf(const char *pStr, uint32_t len)
{
    virtual_com_send(pStr, len);
    sleep(1);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'state' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Print any firmware state the user cares about.
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_state(cc_handle_t *pHandle)
{
    uint32_t ship;
    
    if( !pmem_is_data_valid() )
    {
        ship = 0;  // how could we be waking from ship if RAM is bogus?
    }
    else
    {
        ship = (PV_REBOOT_REASON_SHIP_MODE == __SAVED_ACROSS_BOOT.reboot_reason)?1:0;
    }
    
	cc_print(pHandle, "ship:\t%i\n", ship);
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
static corona_err_t cc_process_sherlock(cc_handle_t *pHandle)
{
    shlk_err_t err;
    sh_u8_t en;
    
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
    else if (!strcmp(pHandle->argv[1], "serv"))
    {
        /*
         *   Dump data to server next time.
         */
        return cc_print( pHandle, "%u pgs\n", 
                                   SHLK_serv_pages( &(pHandle->shlk_hdl), 
                                                    EXT_FLASH_SHLK_SERV_ADDR, 
                                                    (pHandle->argc < 3)? 0:(uint32_t)strtoul(pHandle->argv[2], NULL, 10), 
                                                    (sh_u8_t)(pHandle->argc < 3) ) );
    }
    else if (!strcmp(pHandle->argv[1], "reset"))
    {
        g_sherlock_in_progress = 1;
        err = SHLK_reset(&(pHandle->shlk_hdl));
        g_sherlock_in_progress = 0;
        if( SHLK_ERR_OK != err )
        {
            return cc_print(pHandle, "assdump ERR %i\n", err);
        }
    }
    else if (!strcmp(pHandle->argv[1], "assdump"))
    {
        g_sherlock_in_progress = 1;
        err = SHLK_assdump(&(pHandle->shlk_hdl));
        g_sherlock_in_progress = 0;
        if( SHLK_ERR_OK != err )
        {
            return cc_print(pHandle, "assdump ERR %i\n", err);
        }
    }
    else if (!strcmp(pHandle->argv[1], "dump"))
    {
#if 0
        if(pHandle->argc < 3)
        {
            return cc_print(pHandle, "need more args\n");
        }
        else if(!strcmp(pHandle->argv[2], "*"))
#endif
        {
            g_sherlock_in_progress = 1;
            err = SHLK_dump(&(pHandle->shlk_hdl), 0);
            g_sherlock_in_progress = 0;
            if( SHLK_ERR_OK != err )
            {
                return cc_print(pHandle, "dump ERR %i\n", err);
            }
            return ERR_OK;
        }
#if 0
        else
        {
            uint32_t num_pages = (uint32_t)strtoul(pHandle->argv[2], NULL, 10);
            
            g_sherlock_in_progress = 1;
            err = SHLK_dump(&(pHandle->shlk_hdl), num_pages);
            g_sherlock_in_progress = 0;
            if( SHLK_ERR_OK != err )
            {
                return cc_print(pHandle, "dump ERR %i\n", err);
            }
            return ERR_OK;
        }
#endif
    }
    else
    {
        cc_print(pHandle, "bad sub-cmd\n");
    }

    return ERR_OK;
    
enable_sherlock:
    err = SHLK_enable(&(pHandle->shlk_hdl), EXT_FLASH_SHLK_EN_ADDR, en);
    if( SHLK_ERR_OK != err )
    {
        return cc_print(pHandle, "ERR %i\n", err);
    }
    
shlk_is_enabled:
    return cc_print(pHandle, "SHLK en: %u\n", SHLK_is_enabled(&(pHandle->shlk_hdl), EXT_FLASH_SHLK_EN_ADDR) );
}


// #if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'memr' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read from memory.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_memr(cc_handle_t *pHandle)
{
#if 0
	uint32_t *pAddr;
	int numWordsToRead;  // input bytes
	int idx = 0;

	numWordsToRead = atoi(pHandle->argv[2]);
	numWordsToRead /= 4;

	pAddr = (uint32_t *)strtoul(pHandle->argv[1], NULL, 16);
	if(pAddr == NULL)
	{
		return cc_print(pHandle, "ERR: Could not convert address to integer.\n");
	}

    cc_print(pHandle, "%08X:\t", pAddr);
	while(numWordsToRead-- > 0)
	{
		cc_print(pHandle, "%08X\t", pAddr[idx++]);
		if ((idx % 4) == 0)
		{
			cc_print(pHandle, "\n"); // 4 words per line, just for easy reading...
	        cc_print(pHandle, "%08X:\t", pAddr + idx);
		}
	}
	cc_print(pHandle, "\n");
#endif
	return SUCCESS;
}
// #endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'memw' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Write to memory.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_memw(cc_handle_t *pHandle)
{
	uint32_t *pAddr;
	int numWordsToWrite = pHandle->argc - 2;  // minus 2 for memr command and addr.
	int idx = 2;

	pAddr = (uint32_t *)strtoul(pHandle->argv[1], NULL, 16);
	if(pAddr == NULL)
	{
		return cc_print(pHandle, "Error: Could not convert address to integer.\n");
	}

	while(numWordsToWrite-- > 0)
	{
		*pAddr = (uint32_t)strtoul(pHandle->argv[idx++], NULL, 16);
		pAddr++;
	}

	return SUCCESS;
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'about' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Print the about string.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_about(cc_handle_t *pHandle)
{
	return cc_print(pHandle, "%s", CC_MAN_ABT);
}
#endif

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'acc' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Process a command related to the accelerometer.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_acc(cc_handle_t *pHandle)
{
	if(!strcmp(pHandle->argv[1], "dump"))
	{
		int16_t x, y, z;

		if(pHandle->argc == 2)
		{
			// dump a single X/Y/Z sample to the console.
			if( lis3dh_get_vals(&x, &y, &z) != SUCCESS )
			{
				return cc_print(pHandle, "Error: could not read values!\n");
			}
			return cc_print(pHandle, "X: %h\t\tY: %h\t\tZ: %h\n", x, y, z);
		}
		else
		{
			// keep dumping accel data to console for N seconds.
			uint32_t numSeconds = atoi(pHandle->argv[2]);
			uint32_t delayMsBetweenSamples = 300; // milliseconds
			uint32_t numLoops;

			if(pHandle->argc > 3)
			{
				delayMsBetweenSamples = atoi(pHandle->argv[3]); // the optional 4th argument is sample rate in milliseconds
			}

			numLoops = (numSeconds*1000) / delayMsBetweenSamples;

			while(numLoops-- > 0)
			{
				if( lis3dh_get_vals(&x, &y, &z) != SUCCESS )
				{
					return cc_print(pHandle, "Error: could not read values!\n");
				}
				cc_print(pHandle, "X: %h        Y: %h          Z: %h                                       \r",
						x, y, z);
				//printf("%hi\t%hi\t%hi\n", x, y, z);
				sleep(delayMsBetweenSamples);
			}
			cc_print(pHandle, "\n\n");
			return SUCCESS;
		}
	}
	else if(!strcmp(pHandle->argv[1], "set"))
	{
		uint8_t reg, val;

		if(pHandle->argc != 4)
		{
			return cc_print(pHandle, "Error: acc set requires exactly 4 args. %d given.\n", pHandle->argc);
		}

		reg = (uint8_t)strtoul(pHandle->argv[2], NULL, 16);
		val = (uint8_t)strtoul(pHandle->argv[3], NULL, 16);

		if( lis3dh_write_reg(reg, val) != SUCCESS)
		{
			return cc_print(pHandle, "Error: LIS3DH write returned some error!\n");
		}
		return SUCCESS;
	}
	else if(!strcmp(pHandle->argv[1], "get"))
	{
		uint8_t reg, val;

		if(pHandle->argc != 3)
		{
			return cc_print(pHandle, "Error: acc get requires exactly 3 args. %d given.\n", pHandle->argc);
		}

		reg = (uint8_t)strtoul(pHandle->argv[2], NULL, 16);

		if( lis3dh_read_reg(reg, &val) != SUCCESS)
		{
			return cc_print(pHandle, "Error: LIS3DH write returned some error!\n");
		}

		return cc_print(pHandle, "Value: 0x%x\n\n", val);
	}
	else
	{
		return cc_print(pHandle, "Error: %s is not a supported sub-command of acc.\n", pHandle->argv[1]);
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'adc' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Write a register to the ROHM chip.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_adc(cc_handle_t *pHandle)
{
#if 0
	uint32_t numSamples;
	uint32_t msDelay;
	uint32_t i;
	uint32_t channelIdx;
	uint16_t raw, natural;

	// adc  <pin> <numSamples> <ms delay between samples>
	if(!strcmp(pHandle->argv[1], "uid"))
	{
		channelIdx = ADC0_Vsense_Corona_UID;
		cc_print(pHandle, "\nPIN\t\tRAW COUNTS\tOHMS\n");
	}
	else if(!strcmp(pHandle->argv[1], "bat"))
	{
		channelIdx = ADC0_Vsense_Corona_VBat;
		cc_print(pHandle, "\nPIN\t\tRAW COUNTS\tMILLIVOLTS\n");
	}
	else
	{
		return cc_print(pHandle, "Error: %s is not a valid pin!\n", pHandle->argv[1]);
	}

	numSamples = (uint32_t)strtoul(pHandle->argv[2], NULL, 16);
	msDelay    = (uint32_t)strtoul(pHandle->argv[3], NULL, 16);

	for(i = 0; i < numSamples; i++)
	{
		if( adc_sense(channelIdx, &raw, &natural) != SUCCESS )
		{
			cc_print(pHandle, "ERROR: ADC could not sense at sample (%d)", i);
			return CC_ERR_ADC;
		}
		cc_print(pHandle, "%s\t\t%d\t\t%d\n", pHandle->argv[1], raw, natural);
		sleep(msDelay);
	}
#endif
	return SUCCESS;
}

#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'dis' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Write a register to the ROHM chip.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_dis(cc_handle_t *pHandle)
{
	uint8_t reg;
	uint8_t data;

	reg  = (uint8_t)strtoul(pHandle->argv[1], NULL, 16);
	data = (uint8_t)strtoul(pHandle->argv[2], NULL, 16);

	if(bu26507_write(reg, data) != SUCCESS)
	{
		cc_print(pHandle, "Error: Could not write to BU26507!\n");
		return CC_ERROR_GENERIC;
	}

	cc_print(pHandle, "Wrote --> reg(0x%x): 0x%x\n\n", reg, data);
	return SUCCESS;
}
#endif

/*
 *   Erase all the SPI Flash sectors important for a factory reset.
 */
int factoryreset(cc_handle_t *pHandle)
{
    uint8_t i;

    for(i=0; i < NUM_FACTORYRESET_SECTORS; i++)
    {
        if(pHandle)
        {
            cc_print(pHandle, "Erasing 0x%x\n", g_factoryreset_sectors[i]);
        }
        
        if(wson_erase(g_factoryreset_sectors[i]) != SUCCESS)
        {
            return 1;
        }
    }
    
    return 0; // success.
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'factoryreset' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail installs all the factory defaults.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_factoryreset(cc_handle_t *pHandle)
{

    if( 0 != factoryreset(pHandle) )
    {
        return cc_print(pHandle, "ERR\n");
    }
    cc_print(pHandle, "Done OK\n");
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'fle' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Erase a sector of the SPI Flash
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_fle(cc_handle_t *pHandle)
{
#if 1
	uint32_t address;

	address = (uint32_t)strtoul(pHandle->argv[1], NULL, 16);

	if(wson_erase(address) != SUCCESS)
	{
		cc_print(pHandle, "Error: Could not erase sector!\n");
		return CC_ERROR_GENERIC;
	}
#endif
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'flr' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Read from SPI Flash memory.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
#define USB_CHUNK_SEND  32
static corona_err_t cc_process_flr(cc_handle_t *pHandle)
{
	uint32_t  address;
    uint32_t  numBytes;
	const uint32_t  size = (USB_CHUNK_SEND * (EXT_FLASH_PAGE_SIZE/USB_CHUNK_SEND)) ; //(EXT_FLASH_PAGE_SIZE);
	uint8_t  pBuf[size];
	uint8_t *pPtr;

	address  = (uint32_t)strtoul(pHandle->argv[1], NULL, 16);
	numBytes = (uint32_t)strtoul(pHandle->argv[2], NULL, 10);
	
	cc_print(pHandle, "\n\r\t * ADDR: 0x%x *\n\r", address);
	cc_print(pHandle, "\t * SIZE: %u *\n\r", numBytes);
	sleep(400);  // sleep for a bit to give the user time to take a peak.
	
	while( 1 )
	{
	    uint32_t j;
	    
	    pPtr = pBuf;
	    
		if( wson_read( address, pPtr, size ) != SUCCESS )
		{
			cc_print(pHandle, "ERR\n");
            bu26507_display_pattern( BU_PATTERN_ABORT );
            return CC_ERROR_GENERIC;
		}

		for(j = 0; j < size; j += USB_CHUNK_SEND)
		{
	        if( 0 != virtual_com_send(pPtr, USB_CHUNK_SEND) )
	        {
	            bu26507_display_pattern( BU_PATTERN_ABORT );
	            return CC_ERROR_GENERIC;
	        }
	        
	        pPtr += USB_CHUNK_SEND;
	        sleep(1);
		}
		
		if( size >= numBytes )
		{
		    break;
		}
		
	    address += size;
	    numBytes -= size;
	}
	
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'flw' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Write to SPI Flash memory.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_flw(cc_handle_t *pHandle)
{
#if 0
	uint32_t address;
	uint32_t numBytes;
	uint8_t *pData;
	int i;

	address  = (uint32_t)strtoul(pHandle->argv[1], NULL, 16);
	numBytes = pHandle->argc - 2;

	pData = malloc(numBytes);
	if(!pData)
	{
		cc_print(pHandle, "Error: Could not malloc enough memory!\n");
		return CC_ERR_MEM_ALLOC;
	}

	for(i = 0; i < numBytes; i++)
	{
		pData[i] = (uint8_t)strtoul(pHandle->argv[i+2], NULL, 16);
		//cc_print(pHandle, "remove me: 0x%x\n", pData[i]);
	}

	if(wson_write(address, pData, numBytes, FALSE) != SUCCESS)
	{
		cc_print(pHandle, "Error: Could not write to memory!\n");
		free(pData);
		return CC_ERROR_GENERIC;
	}

	free(pData);
#endif
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'fwa' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Load App FW image.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////

static corona_err_t cc_process_fwa (cc_handle_t *pHandle)
{
  uint32_t address;
  uint32_t numBytes;
  corona_err_t status;

  if (pHandle->argc > 3) // abort... no way for user to correct mistakes
  {
      return _print_abort(pHandle);
  }
  address  = (uint32_t)strtoul(pHandle->argv[1], NULL, 16);
  numBytes = atoi(pHandle->argv[2]);

  cc_print (pHandle, "addr:\t0x%x\n", address);
  cc_print (pHandle, "numBytes:\t%d\n", numBytes);

  status = b2_load_app_fw (address, numBytes);

  return status;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'fws' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Load FW package to SPI flash.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////

static corona_err_t cc_process_fws (cc_handle_t *pHandle)
{
  uint32_t        start_addrs = EXT_FLASH_APP_IMG_ADDR;
  uint32_t        image_size;
  corona_err_t    status = SUCCESS;

  if (pHandle->argc > 2) // abort... no way for user to correct mistakes
  {
      return _print_abort(pHandle);
  }

  image_size = atoi(pHandle->argv[1]);

  cc_print (pHandle, "start addr: 0x%x\n", start_addrs);
  cc_print (pHandle, "sz: %d\n", image_size);

  status = b2_load_spi (start_addrs, image_size);

  return status;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'rpkg' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Read FWU package in SPI flash.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////

static corona_err_t cc_process_rpkg (cc_handle_t *pHandle)
{
    fwu_pkg_t    package;
    int          pkg;
    corona_err_t status = SUCCESS;
    uint32_t     pkg_addrs;
    uint32_t     pkg_size;
    uint32_t     pkg_crc;

    memset(&package, 0, sizeof(fwu_pkg_t));
    if (pHandle->argc > 2) // abort... no way for user to correct mistakes
    {
        return _print_abort(pHandle);
    }
    // if no arg, default pkg is 0.
    if (pHandle->argc == 1)
    {
        pkg = 0;
    }
    else
    {
        pkg = atoi(pHandle->argv[1]);
    }
    if (pkg == 0)
    {
        pkg_addrs = EXT_FLASH_APP_IMG_ADDR;
    }
    else if (pkg == 1)
    {
        pkg_addrs = EXT_FLASH_APP_IMG_BKP_ADDR;
    }
    else
    {
        cc_print (pHandle, "bad pkg num %d, 0 or 1 OK\n", pkg);
        return CC_INVALID_CMD;
    }

    cc_print (pHandle, "rd pkg %d\n", pkg);
    status = b2_read_pkg (&package, pkg);
    print_package(&package);
    pkg_size = package.pkg_info.pkg_size;
    pkg_crc = calc_crc_pkg(pkg_addrs, pkg_size);
    cc_print (pHandle, "\npkg CRC: 0x%X\n", pkg_crc);
    return status;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'lpkg' command.
//!
//! \detail Load FWU package to SPI flash location 0 or 1
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////

static corona_err_t cc_process_lpkg (cc_handle_t *pHandle)
{
    int          pkg;
    corona_err_t status = SUCCESS;

    if (pHandle->argc > 2) // abort... no way for user to correct mistakes
    {
        return _print_abort(pHandle);
    }
    // if no arg, default pkg is 0.
    if (pHandle->argc == 1)
    {
        pkg = 0;
    }
    else
    {
        pkg = atoi(pHandle->argv[1]);
    }
    if ((pkg != 0) && (pkg != 1))
    {
        cc_print (pHandle, "bad pkg num %d, 0 or 1 OK\n", pkg);
        return CC_INVALID_CMD;
    }
    status = b2_load_pkg (pkg);

    return status;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'rhdr' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Read the boot2 and app headers
//!
//! \return corona_err_t error code // Won't be returning
//!
///////////////////////////////////////////////////////////////////////////////

static corona_err_t cc_process_rhdr (cc_handle_t *pHandle)
{
  if (pHandle->argc > 1) // abort...
  {
      return _print_abort(pHandle);
  }
  read_headers();

  return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'gto' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Go to an address.
//!
//! \return corona_err_t error code // Won't be returning
//!
///////////////////////////////////////////////////////////////////////////////

static corona_err_t cc_process_gto (cc_handle_t *pHandle)
{
  uint32_t address;
  corona_err_t status;
  int (*gtoApp)(void);
  int* gtoPtr;
  int i;

  if (pHandle->argc > 2) // abort... no way for user to correct mistakes
  {
      return _print_abort(pHandle);
  }
  if (pHandle->argc == 2)
  {
      address  = (uint32_t)strtoul(pHandle->argv[1], NULL, 16);
  }
  else
  {
      address = 0x22000;
  }
  cc_print (pHandle, "addr: 0x%x\n", address);

  gtoApp = (int(*)())*(int*)(address+4); // get the entry address
  cc_print (pHandle, "goto: 0x%x\n**Close term\n", gtoApp);

  delay(500000);
  _usb_device_deinit();     // Disable usb

  for (i = 0; i < 20; i++)
  {
      delay(500000);
  }
  if (address == 0x22000)
  {
      goto_app();   // will reset if boot2 was updated
  }
  else
  {
      gtoApp();                          // Jump to the app
  }
  return SUCCESS;  // should never get here...
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'setupd' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Sets the update flag for the named app or boot2
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////

static corona_err_t cc_process_setupd (cc_handle_t *pHandle)
{
    int i;

    if (pHandle->argc > 2) // abort... no way for user to correct mistakes
    {
        return _print_abort(pHandle);
    }

    for (i = 0; i < 3; i++)
    {
        if(!strcmp(pHandle->argv[1], flag_loc[i].flag_name))
        {
            cc_print (pHandle, "Flag addr: 0x%X\n", flag_loc[i].flag_addrs);
            set_flag ((uint8_t*) flag_loc[i].flag_addrs);
            return SUCCESS;
        }
    }
    cc_print (pHandle, "bad flag name\n");
    return CC_INVALID_CMD;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Remove trailing white space from the end of a string.
//!
//! \fntype Reentrant Function
//!
///////////////////////////////////////////////////////////////////////////////
static void trim_whitesp(char * pStr)
{
	while(*pStr)
	{
		if( (*pStr == '\n') || (*pStr == '\r') || (*pStr == '\t') || (*pStr == ' ') )
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
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_help(cc_handle_t *pHandle)
{
	int i;

	cc_print(pHandle, "Commands\n========\n");
	for(i = 0; i < NUM_CC_COMMANDS; i++)
	{
		cc_print(pHandle, g_cc_cmds[i].cmd);
		cc_print(pHandle, "\t\t:");
		cc_print(pHandle, g_cc_cmds[i].sum);
		cc_print(pHandle, "\r\n");
	}
}

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
uint32_t mask(uint32_t msb, uint32_t lsb)
{
  uint32_t ret = 0;
  uint32_t numOnes = msb - lsb + 1;
  if(msb < lsb)
  {
    return 0;
  }

  while(numOnes-- > 0)
  {
    ret = 1 | (ret << 1);
  }

  return (ret << lsb);
}


#if 0
///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading button signal(PTA19).  Not writable.
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_button(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = GPIO_READ(GPIOA_PDIR, 29);
		return SUCCESS;
	}
	return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing BT_PWDN signal (PTB0).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_bt_pwdn(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = PORTB_GPIO_LDD_GetFieldValue(NULL, BT_PWDN_B);
	}
	else if(ioDir == CC_IO_WRITE)
	{
		PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, *pData);
	}
	else
	{
		return CC_ERROR_GENERIC;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing MFI_RST_B signal (PTB1).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_mfi_rst(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = PORTB_GPIO_LDD_GetFieldValue(NULL, MFI_RST_B);
	}
	else if(ioDir == CC_IO_WRITE)
	{
		PORTB_GPIO_LDD_SetFieldValue(NULL, MFI_RST_B, *pData);
	}
	else
	{
		return CC_ERROR_GENERIC;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing RF_SW_V1 signal (PTB2:PTB3).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_rf_sw_vers(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = PORTB_GPIO_LDD_GetFieldValue(NULL, RF_SW_VERSION);
	}
	else if(ioDir == CC_IO_WRITE)
	{
		PORTB_GPIO_LDD_SetFieldValue(NULL, RF_SW_VERSION, *pData);
	}
	else
	{
		return CC_ERROR_GENERIC;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing DATA_SPI_WP_B signal (PTB18).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_data_wp(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = PORTB_GPIO_LDD_GetFieldValue(NULL, DATA_SPI_WP_B);
	}
	else if(ioDir == CC_IO_WRITE)
	{
		PORTB_GPIO_LDD_SetFieldValue(NULL, DATA_SPI_WP_B, *pData);
	}
	else
	{
		return CC_ERROR_GENERIC;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing DATA_SPI_HOLD_B signal (PTB19).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_data_hold(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = PORTB_GPIO_LDD_GetFieldValue(NULL, DATA_SPI_HOLD_B);
	}
	else if(ioDir == CC_IO_WRITE)
	{
		PORTB_GPIO_LDD_SetFieldValue(NULL, DATA_SPI_HOLD_B, *pData);
	}
	else
	{
		return CC_ERROR_GENERIC;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading ACC_INT1 signal (PTC12).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_acc_int1(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = GPIO_READ(GPIOC_PDIR, 12);
		return SUCCESS;
	}
	return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading ACC_INT2 signal (PTC13).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_acc_int2(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = GPIO_READ(GPIOC_PDIR, 13);
		return SUCCESS;
	}
	return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading CHG_B signal (PTC14).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_chg(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = GPIO_READ(GPIOC_PDIR, 14);
		return SUCCESS;
	}
	return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading PGOOD_B signal (PTC15).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_pgood(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = GPIO_READ(GPIOC_PDIR, 15);
		return SUCCESS;
	}
	return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing 26MHZ_MCU_EN signal (PTC19).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_26mhz(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = PORTC_GPIO_LDD_GetFieldValue(NULL, MCU_26MHz_EN);
	}
	else if(ioDir == CC_IO_WRITE)
	{
		PORTC_GPIO_LDD_SetFieldValue(NULL, MCU_26MHz_EN, *pData);
	}
	else
	{
		return CC_ERROR_GENERIC;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing 4 Debug LED signals (PTD12:PTD15).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_led(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = PORTD_GPIO_LDD_GetFieldValue(NULL, LED_DBG);
	}
	else if(ioDir == CC_IO_WRITE)
	{
		PORTD_GPIO_LDD_SetFieldValue(NULL, LED_DBG, *pData);
	}
	else
	{
		return CC_ERROR_GENERIC;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing WIFI_PD_B signal (PTE4).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_wifi_pd(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = PORTE_GPIO_LDD_GetFieldValue(NULL, WIFI_PD_B);
	}
	else if(ioDir == CC_IO_WRITE)
	{
		PORTE_GPIO_LDD_SetFieldValue(NULL, WIFI_PD_B, *pData);
	}
	else
	{
		return CC_ERROR_GENERIC;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading WIFI_INT signal (PTE5).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_wifi_int(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = GPIO_READ(GPIOE_PDIR, 5);
		return SUCCESS;
	}
	return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing 32K_B_ENABLE signal (PTE6).
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_32khz(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
#if 0
	if(ioDir == CC_IO_READ)
	{
		*pData = PORTE_GPIO_LDD_GetFieldValue(NULL, BT_32KHz_EN);
	}
	else if(ioDir == CC_IO_WRITE)
	{
		PORTE_GPIO_LDD_SetFieldValue(NULL, BT_32KHz_EN, *pData);
	}
	else
	{
		return CC_ERROR_GENERIC;
	}
#endif
	return SUCCESS;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
