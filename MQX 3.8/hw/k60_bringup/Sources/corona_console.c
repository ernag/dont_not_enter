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
#include "adc_sense.h"
#include "corona_console.h"
#include "cfg_factory.h"
#include "cfg_mgr.h"
#include "corona_gpio.h"
#include "corona_isr_util.h"
#include "bu26507_display.h"
#include "lis3dh_accel.h"
#include "pwr_mgr.h"
#include "ADC0_Vsense.h"
#include "wson_flash.h"
#include "PE_Types.h"
#include "PORTA_GPIO.h"
#include "PORTB_GPIO_LDD.h"
#include "PORTC_GPIO_LDD.h"
#include "PORTD_GPIO_LDD.h"
#include "PORTE_GPIO_LDD.h"
#include "UART0_BT.h"
#include "UART5_DBG.h"

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
	cc_cmd_summary_t   sum;        // brief summary for the command
	uint16_t           minArgs;    // minimum number of arguments including the command itself.
	cc_manual_t        man;  	   // string presented to the user when a 'man' page is requested.
	cc_cmd_callback_t  callback;   // function pointer callback to call when command is requested.
}cc_cmd_t;

typedef enum corona_console_io_type
{
	/*
	 *  READ TYPE
	 *  Can only be used with ior
	 */
	CC_IO_RO_TYPE = 0x1,

	/*
	 *  READ/WRITE TYPE
	 *  Can be used for read or writes
	 */
	CC_IO_RW_TYPE = 0x3
} cc_io_type_t;

typedef corona_err_t (*cc_io_callback_t)(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);

typedef struct corona_console_io_entry
{
	cc_io_type_t     type;
	cc_io_cmd_t      cmd;
	cc_io_callback_t callback;
}cc_io_entry_t;

////////////////////////////////////////////////////////////////////////////////
// Externs
////////////////////////////////////////////////////////////////////////////////
extern volatile bool g_UART5_DataTransmittedFlg;   // needed for letting us know when hardware has TX'd data.
extern uint8_t USB_Class_CDC_Send_Data(uint8_t controller_ID,   /* [IN] Controller ID */
	    uint8_t ep_num,          /* [IN] Endpoint Number */
	    uint8_t * app_buff,    /* Pointer to Application Buffer */
	    uint16_t/*USB_PACKET_SIZE*/ size    /* Size of Application Buffer */);
extern isr_uentry_t g_uEntries[]; // Table of supported uEntries.
extern void usb_reset(void);

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static const uint32_t g_table_test[10] = {0xFFFFFFFF, 0, 0xDEADBEEF, 0xDEAD, 0xBEEF, 0x9999, 0x1234fdec, 8888, 9876, 999999};
unsigned long g_32khz_test_flags = 0;

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static void cc_split_str(cc_handle_t *pHandle, char *pCmd);
static void printchar(cc_handle_t *pHandle, char **str, int c);
static void trim_whitesp(char * pStr);
static int  print(cc_handle_t *pHandle, char **out, const char *format, va_list args );
static int  printi(cc_handle_t *pHandle, char **out, int i, int b, int sg, int width, int pad, int letbase);
static int  prints(cc_handle_t *pHandle, char **out, const char *string, int width, int pad);
static corona_err_t cc_process_io(cc_handle_t *pHandle, cc_io_dir_t direction);
static corona_err_t cc_apply_int_setting(cc_handle_t *pHandle, uint8_t bEnable);

/*
 *    Command Line Callbacks
 */
static corona_err_t cc_process_32k(cc_handle_t *pHandle);
static corona_err_t cc_process_about(cc_handle_t *pHandle);
static corona_err_t cc_process_acc(cc_handle_t *pHandle);
static corona_err_t cc_process_adc(cc_handle_t *pHandle);
static corona_err_t cc_process_btmac(cc_handle_t *pHandle);
static corona_err_t cc_process_btstress(cc_handle_t *pHandle);
static corona_err_t cc_process_bfr(cc_handle_t *pHandle);
static corona_err_t cc_process_bfw(cc_handle_t *pHandle);
static corona_err_t cc_process_ccfg(cc_handle_t *pHandle);
static corona_err_t cc_process_dis(cc_handle_t *pHandle);
static corona_err_t cc_process_fle(cc_handle_t *pHandle);
static corona_err_t cc_process_flr(cc_handle_t *pHandle);
static corona_err_t cc_process_flw(cc_handle_t *pHandle);
static corona_err_t cc_process_help(cc_handle_t *pHandle);
static corona_err_t cc_process_intc(cc_handle_t *pHandle);
static corona_err_t cc_process_intd(cc_handle_t *pHandle);
static corona_err_t cc_process_inte(cc_handle_t *pHandle);
static corona_err_t cc_process_intr(cc_handle_t *pHandle);
static corona_err_t cc_process_ints(cc_handle_t *pHandle);
static corona_err_t cc_process_ior(cc_handle_t *pHandle);
static corona_err_t cc_process_iow(cc_handle_t *pHandle);
static corona_err_t cc_process_key(cc_handle_t *pHandle);
static corona_err_t cc_process_led(cc_handle_t *pHandle);
static corona_err_t cc_process_man(cc_handle_t *pHandle);
static corona_err_t cc_process_memw(cc_handle_t *pHandle);
static corona_err_t cc_process_memr(cc_handle_t *pHandle);
static corona_err_t cc_process_pcfg(cc_handle_t *pHandle);
static corona_err_t cc_process_pwm(cc_handle_t *pHandle);
static corona_err_t cc_process_pwr(cc_handle_t *pHandle);
static corona_err_t cc_process_reset(cc_handle_t *pHandle);
static corona_err_t cc_process_scfg(cc_handle_t *pHandle);
static corona_err_t cc_process_test_console(cc_handle_t *pHandle);
static corona_err_t cc_process_testbrd(cc_handle_t *pHandle);
static corona_err_t cc_process_uid(cc_handle_t *pHandle);
static corona_err_t cc_process_who(cc_handle_t *pHandle);


/*
 *    GPIO Callbacks
 */
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
	  {"help", "print help menu",                      1,  CC_MAN_HELP, cc_process_help},
	  {"h",    "print help menu",                      1,  CC_MAN_HELP, cc_process_help},
	  {"?",    "print help menu",                      1,  CC_MAN_HELP, cc_process_help},
	  {"32k",  "32KHz test status",                    1,  CC_MAN_32K,  cc_process_32k},
	  {"about","print about menu",                	   1,  CC_MAN_ABT,  cc_process_about},
	  {"acc",  "accelerometer control",                2,  CC_MAN_ACC,  cc_process_acc},
	  {"adc",  "sample UID or VBatt voltage",          4,  CC_MAN_ADC,  cc_process_adc},
	  {"btmac","Read BD Address",                      1,  CC_MAN_BTMAC,cc_process_btmac},
	  //{"btstress", "BT stress test",                   1,  CC_MAN_BTSTR,cc_process_btstress},
	  {"bfr",  "bit field read",                       4,  CC_MAN_BFR,  cc_process_bfr},
	  {"bfw",  "bit field write",                      5,  CC_MAN_BFW,  cc_process_bfw},
	  {"ccfg", "clear factory config(s)",              2,  CC_MAN_CCFG, cc_process_ccfg},
	  {"dis", "display write, ROHM register",          3,  CC_MAN_DIS,  cc_process_dis},
	  {"fle", "erase sector corresponding to address", 2,  CC_MAN_FLE,  cc_process_fle},
	  {"flr", "read from SPI Flash memory", 		   3,  CC_MAN_FLR,  cc_process_flr},
	  {"flw", "write to SPI Flash memory",             3,  CC_MAN_FLW,  cc_process_flw},
	  {"intc", "Clear an IRQ count",                   2,  CC_MAN_INTC, cc_process_intc},
	  {"intd", "Disable a GPIO interrupt",             2,  CC_MAN_INTD, cc_process_intd},
	  {"inte", "Enable a GPIO interrupt",              2,  CC_MAN_INTE, cc_process_inte},
	  {"intr", "Read an IRQ count",                    2,  CC_MAN_INTR, cc_process_intr},
	  {"ints", "Get status of all GPIO interrupts",    1,  CC_MAN_INTS, cc_process_ints},
	  {"ior",  "read GPIO(s)",                         2,  CC_MAN_IOR,  cc_process_ior},
	  {"iow",  "write GPIO(s)",                        3,  CC_MAN_IOW,  cc_process_iow},
	  {"key",  "change security",                      2,  CC_MAN_KEY,  cc_process_key},
	  {"led",  "toggle LEDs on/off",                   1,  CC_MAN_LED,  cc_process_led},
	  {"man",  "present manual for command of choice", 2,  CC_MAN_MAN,  cc_process_man},
	  {"memr", "read from memory",                     3,  CC_MAN_MEMR, cc_process_memr},
	  {"memw", "write to memory",                      3,  CC_MAN_MEMW, cc_process_memw},
	  {"pcfg", "print all factory configs",            1,  CC_MAN_PCFG, cc_process_pcfg},
	  {"pwm",  "change PWM",                           2,  CC_MAN_PWM,  cc_process_pwm},
	  {"pwr",  "change power state",                   2,  CC_MAN_PWR,  cc_process_pwr},
	  {"reset","reset the device",                     1,  CC_MAN_RESET,cc_process_reset},
	  {"scfg", "set factory config",                   3,  CC_MAN_SCFG, cc_process_scfg},
	  {"test", "test console argv",                    1,  CC_MAN_TEST, cc_process_test_console},
	  {"testbrd", "runs board level tests",            1,  CC_MAN_TESTB, cc_process_testbrd},
	  {"uid",  "sample cable",                         1,  CC_MAN_UID,  cc_process_uid},
	  {"who",  "pcba",                                 1,  CC_MAN_WHO,  cc_process_who},
};

/*
** ===================================================================
**     Global Array      :  g_cc_io_entries
**
**     Description :
**         Table of structures which define each GPIO and its callback.
** ===================================================================
*/
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

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define NUM_CC_COMMANDS	sizeof(g_cc_cmds) / sizeof(cc_cmd_t)
#define NUM_IO_COMMANDS sizeof(g_cc_io_entries) / sizeof(cc_io_entry_t)
#define CC_SECURITY_0_RESET		"reset"
#define CC_SECURITY_1_PASSWORD	"castle"
#define CC_SECURITY_2_NUM_REQ	"magic"
#define CC_SECURITY_2_KEY		0xFDB97531
#define PAD_RIGHT 1
#define PAD_ZERO 2
#define PRINT_BUF_LEN 12   // should be enough for 32 bit int

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

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
	pHandle->security = CC_SECURITY_0; // default is strictest security level until proven more lenient.
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
	return SUCCESS;
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
				cc_print(pHandle, "\nCommand requires more arguments.\n\r\n");
				return CC_NOT_ENOUGH_ARGS;
			}
			return g_cc_cmds[i].callback(pHandle);
		}
	}
	cc_print(pHandle, "Unsupported Command\n\r");
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
    corona_err_t err = CC_ERR_INVALID_CONSOLE_TYPE;
    
    if( !pHandle )
    {
        return 1;
    }

    va_start( args, format );

    if( (pHandle->type == CC_TYPE_USB) || (pHandle->type == CC_TYPE_UART) )  // USB is most common case, so knock that out first.
    {
    	print( pHandle, 0, format, args );
    	err = SUCCESS;
    	goto cc_print_end;
    }

    {
    	uint8_t buffer[128];
    	uint32_t pgood;

		vsnprintf(buffer, sizeof(buffer)-1, format, args);

		if(pHandle->type == CC_TYPE_JTAG)
		{
			goto cc_print_printf;
		}

		cc_io_pgood(pHandle, &pgood, CC_IO_READ); // handle case where there is no USB, so we'd want to print to CW console.

		if( (pHandle->type == CC_TYPE_USB) && (pgood == 1) )  // \todo - Need to actually test that this functionality works as expected.
		{
	    	goto cc_print_printf;
		}

		// printf("Error: cc_print() has no valid print methods!\n");
		goto cc_print_end;

cc_print_printf:
		printf("%s", buffer);  // \todo this is not printing things properly, needs fixing.
		err = SUCCESS;
		goto cc_print_end;
    }

cc_print_end:
    va_end(args);
    return err;
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
		else {
		out:
			printchar (pHandle, out, *format);
			++pc;
		}
	}
	if (out) **out = '\0';
	va_end( args );
	return pc;
}

static void usb_printchar (uint8_t* oneChar);

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
		LDD_TError Error;
		uint8_t status;
		uint8_t aChar = c;
		uint8_t usb_retries = 10;

		switch(pHandle->type)
		{
			case CC_TYPE_UART:
				Error = UART5_DBG_SendBlock(pHandle->pUart, (void *)&c, 1);
				while (!g_UART5_DataTransmittedFlg){}
				g_UART5_DataTransmittedFlg = FALSE;
				break;

			case CC_TYPE_USB:
			  usb_printchar (&aChar);
				break;

			case CC_TYPE_JTAG:
			default:
				printf("Don't use cc_print() to pipe via JTAG to printchar(), too slow!\n");
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

  if ((c == (int)'\n') || (c == (int)"\r") || (line_idx == LBUF_SIZE))
  {
    status = virtual_com_send(&active_lptr[0], line_idx);
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
		cc_print(pHandle, "Error: %s is not a valid IRQ name.\n\r", pHandle->argv[1]);
		return CC_IRQ_UNSUPPORTED;
	}

	return cc_print(pHandle, "%s IRQ Count: 0\n\r", pHandle->argv[1]);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Process enabling/disabling of one or all GPIO interrupts.
//!
//! \fntype Reentrant Function
//!
//! \detail Handle processing of enabling/disable a GPIO interrupt or all of them.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_apply_int_setting(cc_handle_t *pHandle, uint8_t bEnable)
{
	isr_uentry_t * pEntry;
	volatile isr_irqc_t irqc;

	if(!strcmp(pHandle->argv[1], "all"))
	{
		int i = 0;

		do
		{
			if(bEnable)
			{
				irqc = g_uEntries[i].irqc;
			}
			else
			{
				irqc = IRQC_DISABLED;
			}

			ciu_status(&(g_uEntries[i]), &irqc);
			cc_print(pHandle, "(%s)\tstatus:\t0x%x\t(%s)\n\r", g_uEntries[i].pName, irqc, ciu_status_str(&irqc));
		} while(g_uEntries[++i].idx < INT_Reserved119);

		cc_print(pHandle, "\n\r");
	}
	else
	{
		pEntry = get_uentry_str(pHandle->argv[1]);
		if(!pEntry)
		{
			return cc_print(pHandle, "Error: %s is not a valid ISR.\n", pHandle->argv[1]);
		}

		if(bEnable)
		{
			irqc = pEntry->irqc;
		}
		else
		{
			irqc = IRQC_DISABLED;
		}
		ciu_status(pEntry, &irqc);
		cc_print(pHandle, "(%s)\tstatus:\t0x%x\t(%s)\n\r", pEntry->pName, irqc, ciu_status_str(&irqc));
	}

	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'intd' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Disable a GPIO interrupt.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_intd(cc_handle_t *pHandle)
{
	return cc_apply_int_setting(pHandle, FALSE);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'inte' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Enable a GPIO interrupt.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_inte(cc_handle_t *pHandle)
{
	return cc_apply_int_setting(pHandle, TRUE);
}

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
		cc_print(pHandle, "Error: %s is not a valid IRQ name.\n\r", pHandle->argv[1]);
		return CC_IRQ_UNSUPPORTED;
	}

	return cc_print(pHandle, "%s IRQ Count: %d\n\r", pHandle->argv[1], count);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'ints' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Print status of all GPIO interrupts.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_ints(cc_handle_t *pHandle)
{
	int i = 0;
	volatile isr_irqc_t irqc;

	do
	{
		irqc = IRQC_REQUEST_STATUS;

		ciu_status(&(g_uEntries[i]), &irqc);
		cc_print(pHandle, "(%s)\tstatus:\t0x%x\t(%s)\n\r", g_uEntries[i].pName, irqc, ciu_status_str(&irqc));
	} while(g_uEntries[++i].idx < INT_Reserved119);

	cc_print(pHandle, "\n\r");
	return SUCCESS;
}

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
		cc_print(pHandle, "Security 1 Granted.\n\r");
		pHandle->security = CC_SECURITY_1;
	}
	else if(!strcmp(pHandle->argv[1], CC_SECURITY_2_NUM_REQ))
	{
		cc_print(pHandle, "0x%d\n", (randomNumber ^ (uint32_t)pHandle));
	}
	else if(atoi(pHandle->argv[1]) == (randomNumber ^ CC_SECURITY_2_KEY) )
	{
		cc_print(pHandle, "Security 2 Granted.\n\r", randomNumber);
		pHandle->security = CC_SECURITY_2;
	}
	else if(!strcmp(pHandle->argv[1], CC_SECURITY_0_RESET))
	{
		cc_print(pHandle, "Reset to Security 0.\n\r");
		pHandle->security = CC_SECURITY_0;
	}
	else
	{
		cc_print(pHandle, "Argument not recognized.\n\r");
	}

	randomNumber ^= (uint32_t)pHandle; // make number more random each time.
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'led' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Toggle LED's on/off.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_led(cc_handle_t *pHandle)
{
    static uint32_t toggle = 0;
    
    if( toggle++ % 2 )
    {
        bu26507_write(0x11, 0);  // all off.
    }
    else
    {
        bu26507_display_test();  // all on (white)
    }
    return SUCCESS;
}

/*
 *   Reboot.
 */
void wassert_reboot(void)
{
    uint32_t read_value;
    
    PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 0);   // only do this for the 26MHz image!
    
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
    //uint32_t *pReboot = 0;
    
    cc_print(pHandle, "Reset in 3s. Close term\n");

    sleep( 3 * 1000 );   
    wassert_reboot();
    //*pReboot = 0xDEADBEEF;  // this will trigger a data abort.
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
	corona_err_t err = CC_ERR_INVALID_PWR_STATE;
/*
	if(!strcmp(pHandle->argv[1], "standby"))
	{
		err = pwr_mgr_state(PWR_STATE_STANDBY);
	}
	else
*/
	// \note Only SHIP mode is needed in the bootloader diags.
	if(!strcmp(pHandle->argv[1], "ship"))
	{
	    cc_print(pHandle, "Start ship mode in 3s\n\rClose TERM\n\r");
	    sleep( 3 * 1000 );
	    
		err = pwr_mgr_state(PWR_STATE_SHIP);
		while(1){}
	}
/*
	else if(!strcmp(pHandle->argv[1], "normal"))
	{
		err = pwr_mgr_state(PWR_STATE_NORMAL);
	}
*/
	else
	{
		cc_print(pHandle, "Error: %s is not a valid power state.\n\r", pHandle->argv[1]);
	}
	
	return err;
}

static void _usb_reset_test(cc_handle_t *pHandle)
{
    cc_print(pHandle, "Reset USB 2 sec. Close TERM!\n\r");
    sleep( 2 * 1000 );
    
    wassert_reboot();
}

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
    _usb_reset_test(pHandle);
    
#if 0
	int i;
	cc_print(pHandle, "\nTesting console.\n\r");

	// Print out argv/argc.
	for(i = 0; i < pHandle->argc; i++)
	{
		cc_print(pHandle, pHandle->argv[i]);
		cc_print(pHandle, "\n\r");
	}

	cc_print(pHandle, "Printing decimal int: %d\n\r", 123456789);
	cc_print(pHandle, "Printing hexadecimal int: 0x%x\n\r", 0xDEADBEEF);
	cc_print(pHandle, "Printing single char: %c\n\r", 'q');
	cc_print(pHandle, "Printing string: %s\n\r", "String of Test, this is... fool.!@#$^&*()");
	cc_print(pHandle, "Printing floating point number: %.3f. Doubt that worked...?\n\r", 123.456);
	cc_print(pHandle, "table_test address: 0x%x\n\r", g_table_test);
	cc_print(pHandle, "mask 17:13: 0x%x\n\r", mask(17, 13));
	cc_print(pHandle, "mask 31:0: 0x%x\n\r", mask(31, 0));
	cc_print(pHandle, "mask 1:0: 0x%x\n\r", mask(1, 0));
	cc_print(pHandle, "mask 0:0: 0x%x\n\r", mask(0, 0));
	cc_print(pHandle, "mask 31:31: 0x%x\n\r", mask(31, 31));
	cc_print(pHandle, "mask 31:30: 0x%x\n\r", mask(31, 30));
	cc_print(pHandle, "mask 29:11: 0x%x\n\r", mask(29, 11));
#endif
}

/*
 *   Run testbrd, return number of errors.
 */
unsigned char testbrd(cc_handle_t *pHandle)
{
    unsigned char num_errors = 0;

    cc_print(pHandle, "Testing board level\n\r");
    
    cc_print(pHandle, "Turning on LED's (White)\n\r");
    if( SUCCESS != bu26507_display_test() )
    {
        bu26507_color( BU_COLOR_RED );
        cc_print(pHandle, "\r*** ERR ROHM\n\r");
        num_errors++;
    }
    else
    {
        cc_print(pHandle, "\rbu26507 comm OK   :-)\n\r");
    }
    
    if( 0 == num_errors)
    {
        bu26507_color( BU_COLOR_BLUE );
    }
    cc_print(pHandle, "\r\n --- BLUETOOTH ---\n\r");
    
    if(bt_uart_comm_ok(BT_BAUD_DEFAULT))
    {
        bu26507_color( BU_COLOR_RED );
        cc_print(pHandle, "\r*** ERR BT comm!\n\r");
        num_errors++;
    }
    else
    {
        cc_print(pHandle, "\rBT comm OK   :-)\n\r");
    }

    cc_print(pHandle, "\r\n --- WIFI ---\n\r");
    if( 0 == num_errors)
    {
        bu26507_color( BU_COLOR_GREEN );
    }
    if(a4100_wifi_comm_ok())
    {
        bu26507_color( BU_COLOR_RED );
        cc_print(pHandle, "\r*** ERR: WIFI comm!\n\r");
        num_errors++;
    }
    else
    {
        cc_print(pHandle, "\rWIFI comm OK   :-)\n\r");
    }

    cc_print(pHandle, "\r\n --- ACCELEROMETER ---\n\r");
    if( 0 == num_errors)
    {
        bu26507_color( BU_COLOR_YELLOW );
    }
    if(lis3dh_comm_ok())
    {
        bu26507_color( BU_COLOR_RED );
        cc_print(pHandle, "\r*** ERR: Accel comm!\n\r");
        num_errors++;
    }
    else
    {
        cc_print(pHandle, "\rAccel comm OK   :-)\n\r");
    }

    cc_print(pHandle, "\r\n --- MFI ---\n\r");
    if( 0 == num_errors)
    {
        bu26507_color( BU_COLOR_WHITE );
    }
    if(mfi_comm_ok())
    {
        bu26507_color( BU_COLOR_RED );
        cc_print(pHandle, "\r*** ERR: MFI comm!\n\r");
        num_errors++;
    }
    else
    {
        cc_print(pHandle, "\rMFI comm OK   :-)\n\r");
    }

    cc_print(pHandle, "\r\n --- SPI FLASH ---\n\r");
    if( 0 == num_errors)
    {
        bu26507_color( BU_COLOR_BLUE );
    }
    if(wson_comm_ok())
    {
        bu26507_color( BU_COLOR_RED );
        cc_print(pHandle, "\r*** ERR: SPI Flash comm!\n\r");
        num_errors++;
    }
    else
    {
        cc_print(pHandle, "\rSPI Flash comm OK   :-)\n\r");
    }
    
    cc_print(pHandle, "\r\n --- ADC ---\n\r");
    if( 0 == num_errors)
    {
        bu26507_color( BU_COLOR_YELLOW );
    }
    if(adc_ok())
    {
        bu26507_color( BU_COLOR_RED );
        cc_print(pHandle, "\r*** ERR: ADC!\n\r");
        num_errors++;
    }
    else
    {
        cc_print(pHandle, "\rADC OK   :-)\n\r");
    }
    
    if( 0 != num_errors )
    {
        bu26507_color( BU_COLOR_RED );
    }
    else
    {
        bu26507_color( BU_COLOR_GREEN );
    }

    cc_print(pHandle, "\n\r --- END TEST ---\n\rNumber of Errors: %d\n\r", num_errors);
    
    return num_errors;
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
static corona_err_t cc_process_testbrd(cc_handle_t *pHandle)
{
    unsigned long i = 0;
    unsigned long num_runs = 1;
    unsigned char num_err;
    
    if(pHandle->argc > 1)
    {
        num_runs = (uint32_t)strtoul(pHandle->argv[1], NULL, 10);
    }
    
    // Init BT here, so we don't have to do it N times.
#if 0
    PORTE_GPIO_LDD_SetFieldValue(NULL, BT_32KHz_EN, 1);
    PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 0);
    sleep(200);
#endif
    PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 1);
    sleep(10);
    
    while(i < num_runs)
    {
        cc_print(pHandle, "\n\n\rTest %u\n\r", i++);
        num_err = testbrd(pHandle);
        
        if(num_err)
        {
            return ERR_OK;
        }
    }
    
    cc_print(pHandle, "%u Runs OK\n\r", num_runs);
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
    cc_print(pHandle, "pcba\n");
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
    uint32_t pgood = GPIO_READ(GPIOE_PDIR, 2);
    
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
    
    if( pgood )
    {
        cc_print(pHandle, "PGOOD NOT asserted\n");
    }
    else
    {
        cc_print(pHandle, "PGOOD OK\n");
    }
    
    return ERR_OK;
}

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
	cc_print(pHandle, "\n\rManual not found.\n\r");
	return CC_NO_MAN;
}

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
		return cc_print(pHandle, "Error: Could not convert address to integer.\n\r");
	}

	msb = atoi(pHandle->argv[2]);
	lsb = atoi(pHandle->argv[3]);
	bitfield = (mask(msb, lsb) & *pAddr) >> lsb;

	cc_print(pHandle, "Address: 0x%x\n\r", pAddr);
	cc_print(pHandle, "Value <31:0>:\t0x%x\n\r", *pAddr);
	cc_print(pHandle, "Value <%d:%d>:\t0x%x\n\n\r", msb, lsb, bitfield);
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'btmac' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Read a bitfield from a specific 32-bit address anywhere in memory.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_btmac(cc_handle_t *pHandle)
{
    char BD_ADDR[32];
    
    memset(BD_ADDR, 0, 32);
    
    if( SUCCESS != bt_getmac( BD_ADDR ) )
    {
        cc_print(pHandle, "Error retrieving BT Mac\n\r");
    }
    else
    {
        cc_print(pHandle, "%s\n\r", BD_ADDR);
    }
}

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
						cc_print(pHandle, "Error: This GPIO is not writable.\n\r");
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
					cc_print(pHandle, "\n\r");
					return SUCCESS;

				default:
					cc_print(pHandle, "Error: Read or Write not specified.\n\r");
					return CC_ERROR_GENERIC;
			}
		}
	}
	cc_print(pHandle, "Error: (%s) is not a valid GPIO value.\n\r", pHandle->argv[1]);
	return CC_ERROR_GENERIC;
}

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
		return cc_print(pHandle, "Error: Could not convert address to integer.\n\r");
	}

	msb = atoi(pHandle->argv[2]);
	lsb = atoi(pHandle->argv[3]);
	val = (uint32_t)strtoul(pHandle->argv[4], NULL, 16);
	val <<= lsb; // the value given by user is right justified, so shift it into position.
	if(val > mask(msb, lsb))
	{
		return cc_print(pHandle, "Error: value passed is too big to fit into bitfield.\n\r");
	}

	cc_print(pHandle, "Writing 0x%x to <%d:%d> at 0x%x ...\n\r", val, msb, lsb, pAddr);
	contents = *pAddr; // contents BEFORE the write.
	contents &= ~mask(msb, lsb); // now we need to create a mask in the range of the bitfield specified
	contents |= val;
	*pAddr = contents; // perform the final write to memory location chosen by user.

	cc_print(pHandle, "0x%x: 0x%x\n\n\r", pAddr, *pAddr);
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'btstress' command.
//!
//! \fntype Reentrant Function
//!
//! \detail Performs a BT stress test.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_btstress(cc_handle_t *pHandle)
{
	if( bt_stress() )
	{
		cc_print(pHandle, "\n\n\rXXXXX\tERR: BT stress FAIL\tXXXXX\n\n\r");
	}
}

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
        return cc_print(pHandle, "ERROR initializing FCFG!\n\r");
    }
    
    cc_print(pHandle, "FACTORY CONFIGS:\n\n\r");
    
    for(tag = CFG_FACTORY_FIRST_FCFG; tag <= CFG_FACTORY_LAST_FCFG; tag++)
    {   
        ferr = FCFG_getdescriptor(tag, &pDesc);
        if( FCFG_OK != ferr )
        {
            return cc_print(pHandle, "\n\rERROR: %i getdescriptor(), TAG: %i\n\r", ferr, tag);
        }
        cc_print(pHandle, "TAG: %i\tDESC: %s\t", tag, pDesc);
        
        ferr = FCFG_getlen(&handle, tag, &len);
        if( FCFG_VAL_UNSET == ferr )
        {
            cc_print(pHandle, "(unset)\n\r");
            continue;
        }
        else if( FCFG_OK != ferr )
        {
            return cc_print(pHandle, "\nERROR: %i getlen(), TAG: %i\n\r", ferr, tag);
        }
        cc_print(pHandle, "LEN: %i\tVALUE: ", len);
        if(0 == len)
        {
            return cc_print(pHandle, "ZERO LENGTH ERROR!\n\r");
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
                return cc_print(pHandle, "\nERROR: %i get(), TAG: %i\n\r", ferr, tag);
            }
            cc_print(pHandle, "hex:0x%x dec:%i\n\r", val, val);
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
                return cc_print(pHandle, "MALLOC ERROR!\n\r");
            }
            
            ferr = FCFG_get(&handle, tag, pStr);
            if( FCFG_OK != ferr )
            {
                fcfg_free(pStr);
                return cc_print(pHandle, "\nERROR: %i get(), TAG: %i\n\r", ferr, tag);
            }
            cc_print(pHandle, "%s\n\r", pStr);
            fcfg_free(pStr);
        }
    }
    
    cc_print(pHandle, "\n\n\r");
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
        return cc_print(pHandle, "ERROR initializing FCFG!\n\r");
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
        return cc_print(pHandle, "ERROR %i getting tag from desc!\n\r", ferr);
    }
    
    /*
     *   String or 32-bit number ?
     */
    if( '$' == pHandle->argv[2][0])
    {
        // String
        
        fcfg_len_t len = strlen( &(pHandle->argv[2][1]) ) + 1;
        char maybe[6];
        
        cc_print(pHandle, "Setting TAG: %i to string: %s\n\r", tag, &(pHandle->argv[2][1]));
        
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
            cc_print(pHandle, "ERROR %i setting TAG %i\n\r", ferr, tag);
        }
        return ERR_OK;
    }
    else
    {
        // 32-bit unsigned
        
        val = strtoul(pHandle->argv[2], NULL, 10);
        cc_print(pHandle, "Setting TAG: %i to value %i\n\r", tag, val);
        ferr = FCFG_set(&handle, tag, sizeof(uint32_t), &val);
        if( FCFG_OK != ferr )
        {
            cc_print(pHandle, "ERROR %i setting TAG %i\n\r", ferr, tag);
        }
        return ERR_OK;
    }
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
        return cc_print(pHandle, "ERROR initializing FCFG!\n\r");
    }
    
    if (!strcmp(pHandle->argv[1], "*"))
    {
        cc_print(pHandle, "Clearing all factory configs!\n\r");
        if( FCFG_OK != FCFG_clearall(&handle) )
        {
            return cc_print(pHandle, "ERROR trying to clear all!\n\r");
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
        return cc_print(pHandle, "ERROR %i getting tag from desc!\n\r", ferr);
    }
    
    cc_print(pHandle, "Clearing TAG: %i ...\n\r", tag);
    ferr = FCFG_clear(&handle, tag);
    if( FCFG_OK != ferr )
    {
        return cc_print(pHandle, "ERROR %i trying to clear TAG %i\n\r", ferr, tag);
    }
    return ERR_OK;
}

//////////////////////////////////////////////////////////////////////////////
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
	uint32_t *pAddr;
	int numWordsToRead;
	int idx = 0;

	numWordsToRead = atoi(pHandle->argv[2]);
	pAddr = (uint32_t *)strtoul(pHandle->argv[1], NULL, 16);
	if(pAddr == NULL)
	{
		return cc_print(pHandle, "Error: Could not convert address to integer.\n\r");
	}

	cc_print(pHandle, "0x%x:", pAddr);

	while(numWordsToRead-- > 0)
	{
		cc_print(pHandle, "\t\t%x", pAddr[idx++]);
		if( (idx % 4) == 0)
		{
			cc_print(pHandle, "\n\r\t"); // 4 words per line, just for prettiness.
		}
	}
	cc_print(pHandle, "\n\r");

	return SUCCESS;
}

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
		return cc_print(pHandle, "Error: Could not convert address to integer.\n\r");
	}

	while(numWordsToWrite-- > 0)
	{
		*pAddr = (uint32_t)strtoul(pHandle->argv[idx++], NULL, 16);
		pAddr++;
	}

	return SUCCESS;
}

#if 0
/*
 *     READING IS NOT SUPPORTED WITH THE DISPLAY DEVICE !
 */
///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for 'disr' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Read a register from ROHM chip.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_disr(cc_handle_t *pHandle)
{
	uint8_t reg;
	uint8_t data;

	reg = (uint8_t)strtoul(pHandle->argv[1], NULL, 16);

	if(bu26507_read(reg, &data) != SUCCESS)
	{
		cc_print(pHandle, "Error: Could not read from BU26507!\n\r");
		return CC_ERROR_GENERIC;
	}

	cc_print(pHandle, "reg(0x%x): 0x%x\n\n\r", reg, data);

	return SUCCESS;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//! \brief Callback for '32k' command.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Print out status of 32KHz testing
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_process_32k(cc_handle_t *pHandle)
{
    cc_print(pHandle, "32KHz : %s : 0x%x\n\r", g_32khz_test_flags?"FAIL":"PASS", g_32khz_test_flags);
    
    if(g_32khz_test_flags & TEST_32KHz_FLAG_BT)
    {
        cc_print(pHandle, "BT FAIL\n\r");
    }
    
    if(g_32khz_test_flags & TEST_32KHz_FLAG_BT_STRESS)
    {
        cc_print(pHandle, "BT STRESS FAIL\n\r");
    }
    
    if(g_32khz_test_flags & TEST_32KHz_FLAG_MFI)
    {
        cc_print(pHandle, "MFI FAIL\n\r");
    }
    
    if(g_32khz_test_flags & TEST_32KHz_FLAG_ACC)
    {
        cc_print(pHandle, "ACC FAIL\n\r");
    }
    
    if(g_32khz_test_flags & TEST_32KHz_FLAG_WIFI)
    {
        cc_print(pHandle, "WIFI FAIL\n\r");
    }
    
    if(g_32khz_test_flags & TEST_32KHz_FLAG_FLASH)
    {
        cc_print(pHandle, "FLASH FAIL\n\r");
    }
}

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
	if(!strcmp(pHandle->argv[1], "read"))
	{
		int16_t x, y, z;
		int num_times = 10;
		
		cc_print(pHandle, "Reading accelerometer... \n\r");

		if(pHandle->argc == 2)
		{
		    while( num_times != 0 )
		    {
	            // dump a single X/Y/Z sample to the console.
	            if( lis3dh_get_vals(&x, &y, &z) != SUCCESS )
	            {
	                return cc_print(pHandle, "Error: could not read values!\n\r");
	            }
	            
	            cc_print(pHandle, "X:%i\tY:%i\tZ:%i\n\r", x/1024, y/1024, z/1024);
	            sleep(150);
	            num_times--;
		    }
		    
		    return;
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
					return cc_print(pHandle, "Error: could not read values!\n\r");
				}
				cc_print(pHandle, "X: %i        Y: %i          Z: %i                                       \r",
						x/1024, y/1024, z/1024);
				//printf("%hi\t%hi\t%hi\n", x, y, z);i
				sleep(delayMsBetweenSamples);
			}
			cc_print(pHandle, "\n\n\r");
			return SUCCESS;
		}
	}
	else if(!strcmp(pHandle->argv[1], "set"))
	{
		uint8_t reg, val;

		if(pHandle->argc != 4)
		{
			return cc_print(pHandle, "Error: acc set requires exactly 4 args. %d given.\n\r", pHandle->argc);
		}

		reg = (uint8_t)strtoul(pHandle->argv[2], NULL, 16);
		val = (uint8_t)strtoul(pHandle->argv[3], NULL, 16);

		if( lis3dh_write_reg(reg, val) != SUCCESS)
		{
			return cc_print(pHandle, "Error: LIS3DH write returned some error!\n\r");
		}
		return SUCCESS;
	}
	else if(!strcmp(pHandle->argv[1], "get"))
	{
		uint8_t reg, val;

		if(pHandle->argc != 3)
		{
			return cc_print(pHandle, "Error: acc get requires exactly 3 args. %d given.\n\r", pHandle->argc);
		}

		reg = (uint8_t)strtoul(pHandle->argv[2], NULL, 16);

		if( lis3dh_read_reg(reg, &val) != SUCCESS)
		{
			return cc_print(pHandle, "Error: LIS3DH write returned some error!\n\r");
		}

		return cc_print(pHandle, "Value: 0x%x\n\n\r", val);
	}
	else
	{
		return cc_print(pHandle, "Error: %s is not a supported sub-command of acc.\n\r", pHandle->argv[1]);
	}
}

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
	uint32_t numSamples;
	uint32_t msDelay;
	uint32_t i;
	uint32_t channelIdx;
	uint16_t raw, natural;

	// adc  <pin> <numSamples> <ms delay between samples>
	if(!strcmp(pHandle->argv[1], "uid"))
	{
		channelIdx = ADC0_Vsense_Corona_UID;
		cc_print(pHandle, "\nPIN\t\tRAW COUNTS\tOHMS\n\r");
	}
	else if(!strcmp(pHandle->argv[1], "bat"))
	{
		channelIdx = ADC0_Vsense_Corona_VBat;
		cc_print(pHandle, "\nPIN\t\tRAW COUNTS\tMILLIVOLTS\n\r");
	}
	else
	{
		return cc_print(pHandle, "Error: %s is not a valid pin!\n\r", pHandle->argv[1]);
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
		cc_print(pHandle, "%s\t\t%d\t\t%d\n\r", pHandle->argv[1], raw, natural);
		sleep(msDelay);
	}
	return SUCCESS;
}

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
		cc_print(pHandle, "Error: Could not write to BU26507!\n\r");
		return CC_ERROR_GENERIC;
	}

	cc_print(pHandle, "Wrote --> reg(0x%x): 0x%x\n\n\r", reg, data);

	return SUCCESS;
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
	uint32_t address;

	address = (uint32_t)strtoul(pHandle->argv[1], NULL, 16);

	if(wson_erase(address) != SUCCESS)
	{
		cc_print(pHandle, "Error: Could not erase sector!\n\r");
		return CC_ERROR_GENERIC;
	}

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
static corona_err_t cc_process_flr(cc_handle_t *pHandle)
{
	uint32_t address;
	uint32_t numBytes;
	uint8_t *pData;
	int i;


	address  = (uint32_t)strtoul(pHandle->argv[1], NULL, 16);
	numBytes = atoi(pHandle->argv[2]);

	pData = malloc(numBytes);
	if(!pData)
	{
		cc_print(pHandle, "Error: Could not malloc enough memory!\n");
		return CC_ERR_MEM_ALLOC;
	}

	if(wson_read(address, pData, numBytes) != SUCCESS)
	{
		cc_print(pHandle, "Error: Could not read from memory!\n\r");
		free(pData);
		return CC_ERROR_GENERIC;
	}

	for(i = 0; i < numBytes; i++)
	{
		if( ((i%4) == 0) /*&& (i > 0)*/ )
		{
			cc_print(pHandle, "\t");
		}
		if( ((i%20) == 0)/* && (i > 0)*/ )
		{
			cc_print(pHandle, "\n\r");
		}
		cc_print(pHandle, "%x", pData[i]);
	}
	free(pData);
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
		//cc_print(pHandle, "remove me: 0x%x\n\r", pData[i]);
	}

	if(wson_write(address, pData, numBytes, FALSE) != SUCCESS)
	{
		cc_print(pHandle, "Error: Could not write to memory!\n\r");
		free(pData);
		return CC_ERROR_GENERIC;
	}

	free(pData);
	return SUCCESS;
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

	cc_print(pHandle, "Corona Commands\n\r===============\n\r");
	for(i = 0; i < NUM_CC_COMMANDS; i++)
	{
		cc_print(pHandle, g_cc_cmds[i].cmd);
		cc_print(pHandle, "\t\t:");
		cc_print(pHandle, g_cc_cmds[i].sum);
		cc_print(pHandle, "\r\n");
	}
}

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
		*pData = GPIO_READ(GPIOE_PDIR, 1);
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
		*pData = GPIO_READ(GPIOA_PDIR, 13);
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
		*pData = GPIO_READ(GPIOD_PDIR, 0);
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
		*pData = GPIO_READ(GPIOE_PDIR, 2);
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
#if 0
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
#endif
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
	return SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
