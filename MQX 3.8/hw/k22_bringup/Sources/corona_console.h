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
#include "PE_Types.h"
//#include "corona_errors.h"
#include "k2x_errors.h"
#include "corona_console_man.h"
#include "UART5_DBG.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define CC_MAX_ARGS		16	// max number of space-delimited arguments you can give to Corona Console.

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

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


typedef struct corona_console_handle
{
	LDD_TDeviceData *pUart;               // pointer to UART handle for sending data out to UART.
	cc_security_t security;               // security clearance we currently have on this session.
	//cc_verbosity_t verbosity;			  // verbosity requested by user at this time.
	//bool logging; 		              // log data to memory?
	int argc;                             // number of arguments provided by user.
	char *argv[CC_MAX_ARGS];              // string arguments in an array.
	
} cc_handle_t;



////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
corona_err_t cc_init(cc_handle_t *pHandle, LDD_TDeviceData *pUart);
corona_err_t cc_print(cc_handle_t *pHandle, const char *format, ...);
corona_err_t cc_process_cmd(cc_handle_t *pHandle, char *pCmd);


#endif /* CORONA_CONSOLE_H_ */

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
