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
//! \file corona_console.h
//! \brief Corona console header.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
//! \todo add verbosity feature or do we really care?
//! \todo add logging feature
//! \todo add security features
////////////////////////////////////////////////////////////////////////////////

#ifndef CORONA_CONSOLE_H_
#define CORONA_CONSOLE_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "corona_errors.h"
#include "corona_console_man.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define CC_MAX_ARGS		16	// max number of space-delimited arguments you can give to Corona Console.

#define TEST_32KHz_FLAG_BT          0x00000001
#define TEST_32KHz_FLAG_BT_STRESS   0x00000002
#define TEST_32KHz_FLAG_ACC         0x00000004
#define TEST_32KHz_FLAG_WIFI        0x00000008
#define TEST_32KHz_FLAG_FLASH       0x00000010
#define TEST_32KHz_FLAG_MFI         0x00000020

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

typedef enum corona_state
{
    STATE_INITIAL = 0,
    STATE_LED_ON  = 1,
    STATE_LED_OFF = 2
}corona_state_t;

typedef enum corona_console_security
{
	/*
	 * LEVEL 0
	 * Open / non secure - if someone has access to console they can run this command
	 *  purpose of this is to provide basic customer support information and debug information
	 */
	CC_SECURITY_0,
	
	/*
	 * LEVEL 1
	 * Basic password - used to protect basic command console functionality and commands that expose proprietary information
	 */
	CC_SECURITY_1,
	
	/*
	 * LEVEL 2
	 * Critical - used to protect commands that access potentially damaging features/settings, 
	 *  or access regulatory related settings/features (e.g., max Wi-Fi TX power))
	 */
	CC_SECURITY_2

}cc_security_t;

typedef enum corona_console_type
{
	CC_TYPE_UART = 0xAB,
	CC_TYPE_USB  = 0xDE,
	CC_TYPE_JTAG = 0xF9
}console_type_t;

typedef struct corona_console_handle
{
	void *pUart;               // pointer to UART handle for sending data out to UART.
	console_type_t type;
	cc_security_t security;               // security clearance we currently have on this session.
	//cc_verbosity_t verbosity;			  // verbosity requested by user at this time.
	//bool logging; 		              // log data to memory?
	int argc;                             // number of arguments provided by user.
	char *argv[CC_MAX_ARGS];              // string arguments in an array.
	
} cc_handle_t;



////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
corona_err_t cc_init(cc_handle_t *pHandle, void *pDevice, console_type_t type);
corona_err_t cc_print(cc_handle_t *pHandle, const char *format, ...);
corona_err_t cc_process_cmd(cc_handle_t *pHandle, char *pCmd);
unsigned char testbrd(cc_handle_t *pHandle);
void wassert_reboot(void);

#endif /* CORONA_CONSOLE_H_ */

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
