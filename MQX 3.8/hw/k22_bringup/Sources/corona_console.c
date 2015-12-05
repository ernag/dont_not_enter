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
#include "corona_console.h"
#include "corona_isr_util.h"
#include "bu26507_display.h"
#include "pwr_mgr.h"
#include "ADC0_Vsense.h"
#include "wson_flash.h"
#include "PORTA_GPIO.h"
#include "PORTB_GPIO_LDD.h"
#include "PORTC_GPIO_LDD.h"
#include "PORTD_GPIO_LDD.h"
#include "PORTE_GPIO_LDD.h"
#include <stdarg.h>
#include <stdlib.h>

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

typedef enum corona_console_io_direction
{
	/*
	 *  READ TYPE
	 *  Can only be used with ior
	 */
	CC_IO_READ,
	
	/*
	 *  READ/WRITE TYPE
	 *  Can be used for read or writes
	 */
	CC_IO_WRITE
} cc_io_dir_t;

typedef corona_err_t (*cc_io_callback_t)(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir);

typedef struct corona_console_io_entry
{
	cc_io_type_t     type;
	cc_io_cmd_t      cmd;
	cc_io_callback_t callback;
}cc_io_entry_t;

////////////////////////////////////////////////////////////////////////////////
// External Variables
////////////////////////////////////////////////////////////////////////////////
extern volatile bool g_UART5_DataTransmittedFlg;   // needed for letting us know when hardware has TX'd data.

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static const uint32_t g_table_test[10] = {0xFFFFFFFF, 0, 0xDEADBEEF, 0xDEAD, 0xBEEF, 0x9999, 0x1234fdec, 8888, 9876, 999999};

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
static corona_err_t cc_process_io(cc_handle_t *pHandle, cc_io_dir_t direction);
/*
 *    Command Line Callbacks
 */
static corona_err_t cc_process_adc(cc_handle_t *pHandle);
static corona_err_t cc_process_bfr(cc_handle_t *pHandle);
static corona_err_t cc_process_bfw(cc_handle_t *pHandle);
//static corona_err_t cc_process_disr(cc_handle_t *pHandle);
static corona_err_t cc_process_dis(cc_handle_t *pHandle);
static corona_err_t cc_process_fle(cc_handle_t *pHandle);
static corona_err_t cc_process_flr(cc_handle_t *pHandle);
static corona_err_t cc_process_flw(cc_handle_t *pHandle);
static corona_err_t cc_process_help(cc_handle_t *pHandle);
static corona_err_t cc_process_ior(cc_handle_t *pHandle);
static corona_err_t cc_process_iow(cc_handle_t *pHandle);
static corona_err_t cc_process_intc(cc_handle_t *pHandle);
static corona_err_t cc_process_intr(cc_handle_t *pHandle);
static corona_err_t cc_process_key(cc_handle_t *pHandle);
static corona_err_t cc_process_man(cc_handle_t *pHandle);
static corona_err_t cc_process_memw(cc_handle_t *pHandle);
static corona_err_t cc_process_memr(cc_handle_t *pHandle);
static corona_err_t cc_process_pwr(cc_handle_t *pHandle);
static corona_err_t cc_process_test_console(cc_handle_t *pHandle);


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
	  {"adc",  "sample UID or VBatt voltage",          4,  CC_MAN_ADC,  cc_process_adc},
	  {"bfr",  "bit field read",                       4,  CC_MAN_BFR,  cc_process_bfr},
	  {"bfw",  "bit field write",                      5,  CC_MAN_BFW,  cc_process_bfw},
//	  {"disr", "display read, ROHM register",          2,  CC_MAN_DISR, cc_process_disr}, // ROHM chip is un-readable.
	  {"dis", "display write, ROHM register",          3,  CC_MAN_DIS,  cc_process_dis},
	  {"fle", "erase sector corresponding to address",            2,  CC_MAN_FLE,  cc_process_fle},
	  {"flr", "read from SPI Flash memory", 3,  CC_MAN_FLR,  cc_process_flr},
	  {"flw", "write to SPI Flash memory",             3,  CC_MAN_FLW,  cc_process_flw},
	  {"intc", "Clear an IRQ count",                   2,  CC_MAN_INTC, cc_process_intc},
	  {"intr", "Read an IRQ count",                    2,  CC_MAN_INTR, cc_process_intr},
	  {"ior",  "read GPIO(s)",                         2,  CC_MAN_IOR,  cc_process_ior},
	  {"iow",  "write GPIO(s)",                        3,  CC_MAN_IOW,  cc_process_iow},
	  {"key",  "change security",                      2,  CC_MAN_KEY,  cc_process_key},
	  {"man",  "present manual for command of choice", 2,  CC_MAN_MAN,  cc_process_man},
	  {"memr", "read from memory",                     3,  CC_MAN_MEMR, cc_process_memr},
	  {"memw", "write to memory",                      3,  CC_MAN_MEMW, cc_process_memw},
	  {"pwr",  "change power state",                   2,  CC_MAN_PWR,  cc_process_pwr},
	  {"test", "test console argv",                    1,  CC_MAN_TEST, cc_process_test_console},
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
#define PAD_RIGHT 1
#define PAD_ZERO 2
#define PRINT_BUF_LEN 12   // should be enough for 32 bit int
#define GPIO_READ(PORT, PIN)	( PORT & mask(PIN, PIN) ) >> PIN;

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
corona_err_t cc_init(cc_handle_t *pHandle, LDD_TDeviceData *pUart)
{
	pHandle->security = CC_SECURITY_0; // default is strictest security level until proven more lenient.
	pHandle->pUart = pUart; // UART pointer we can use to write serial data to the K60 UART.
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
    
    va_start( args, format );
    print( pHandle, 0, format, args );

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
			if( *format == 'd' ) {
				pc += printi (pHandle, out, va_arg( args, int ), 10, 1, width, pad, 'a');
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
				pc += printi (pHandle, out, va_arg( args, int ), 10, 0, width, pad, 'a');
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
		
		Error = UART5_DBG_SendBlock(pHandle->pUart, (void *)&c, 1);
		while (!g_UART5_DataTransmittedFlg){}
		g_UART5_DataTransmittedFlg = FALSE;
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
	int i;
	
	if(!strcmp(pHandle->argv[1], CC_SECURITY_1_PASSWORD))
	{
		cc_print(pHandle, "Security 1 Granted.\n\r");
		pHandle->security = CC_SECURITY_1;
	}
	else if(!strcmp(pHandle->argv[1], CC_SECURITY_0_RESET))
	{
		cc_print(pHandle, "Reset to Security 0.\n\r");
		pHandle->security = CC_SECURITY_0;
	}
	else
	{
		cc_print(pHandle, "Password not recognized.\n\r");
	}
	
	//! \todo Add SECURITY_2 logic
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
	if(!strcmp(pHandle->argv[1], "standby"))
	{
		err = pwr_mgr_state(PWR_STATE_STANDBY);
	}
	else if(!strcmp(pHandle->argv[1], "ship"))
	{
		err = pwr_mgr_state(PWR_STATE_SHIP);
	}
	else if(!strcmp(pHandle->argv[1], "normal"))
	{
		err = pwr_mgr_state(PWR_STATE_NORMAL);
	}
	else
	{
		cc_print(pHandle, "Error: %s is not a valid power state.\n\r", pHandle->argv[1]);
	}
	return err;
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
					g_cc_io_entries[i].callback(pHandle, &data, direction);
					cc_print(pHandle, "%s: 0x%x\n\r", g_cc_io_entries[i].cmd, data);
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
		sleep(msDelay); // todo - check that this delay is actually correct on K60...
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
		cc_print(pHandle, "%x", pData[i]);
		if( ((i%5) == 0) && (i > 0) )
		{
			cc_print(pHandle, "\t");
		}
		if( ((i%20) == 0) && (i > 0) )
		{
			cc_print(pHandle, "\n\r");
		}
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
	
	if(wson_write(address, pData, numBytes, TRUE/*FALSE*/) != SUCCESS)
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
		*pData = GPIO_READ(GPIOA_PDIR, 29);
		return SUCCESS;
	}
	return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}

static corona_err_t cc_io_bt_pwdn(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = GPIO_READ(GPIOB_PDIR, 0);
	}
	return SUCCESS;
}

static corona_err_t cc_io_mfi_rst(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_mfi_rst()\n\r");
	return SUCCESS;
}

static corona_err_t cc_io_rf_sw1(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_rf_sw1()\n\r");
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief GPIO routine for reading/writing RF_SW_V1 signal.
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static corona_err_t cc_io_rf_sw_vers(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_rf_sw_vers()\n\r");
	return SUCCESS;
}

static corona_err_t cc_io_data_wp(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_data_wp()\n\r");
	return SUCCESS;
}

static corona_err_t cc_io_data_hold(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_data_hold()\n\r");
	return SUCCESS;
}

static corona_err_t cc_io_acc_int1(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_acc_int1()\n\r");
	return SUCCESS;
}

static corona_err_t cc_io_acc_int2(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_acc_int2()\n\r");
	return SUCCESS;
}

static corona_err_t cc_io_chg(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = GPIO_READ(GPIOC_PDIR, 14);
		return SUCCESS;
	}
	return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}

static corona_err_t cc_io_pgood(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	if(ioDir == CC_IO_READ)
	{
		*pData = GPIO_READ(GPIOC_PDIR, 15);
		return SUCCESS;
	}
	return CC_UNWRITABLE_GPIO; // read is only type of IO supported.
}

static corona_err_t cc_io_26mhz(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_26mhz()\n\r");
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

static corona_err_t cc_io_wifi_pd(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_wifi_pd()\n\r");
	return SUCCESS;
}

static corona_err_t cc_io_wifi_int(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_wifi_int()\n\r");
	return SUCCESS;
}

static corona_err_t cc_io_32khz(cc_handle_t *pHandle, uint32_t *pData, cc_io_dir_t ioDir)
{
	cc_print(pHandle, "cc_io_32khz()\n\r");
	return SUCCESS;
}


////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
