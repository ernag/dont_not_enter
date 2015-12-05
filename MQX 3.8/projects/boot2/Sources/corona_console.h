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
#include "PE_LDD.h"
#include "corona_errors.h"
#include "corona_console_man.h"
#include "sherlock.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define CC_MAX_ARGS		16	// max number of space-delimited arguments you can give to Corona Console.

typedef enum console_mode
{
  CC_COMMAND,
  CC_DATA,
  CC_LOAD_INT,    // Load image to internal flash (boot2 or app)
  CC_LOAD_SPI,    // Load image to SPI flash
  CC_LOAD_PKG     // Load package to SPI flash
} cc_mode_t;

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

#if 0
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
#endif

typedef enum corona_console_type
{
	CC_TYPE_UART = 0xAB,
	CC_TYPE_USB = 0xDE,
	CC_TYPE_JTAG = 0xF9
}console_type_t;

typedef struct corona_console_handle
{
	void *pUart;               // pointer to UART handle for sending data out to UART.
	console_type_t type;
	int argc;                             // number of arguments provided by user.
	char *argv[CC_MAX_ARGS];              // string arguments in an array.
	shlk_handle_t shlk_hdl;

} cc_handle_t;



////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
corona_err_t cc_init(cc_handle_t *pHandle, void *pDevice, console_type_t type);
corona_err_t cc_print(cc_handle_t *pHandle, const char *format, ...);
corona_err_t cc_process_cmd(cc_handle_t *pHandle, char *pCmd);
void shlk_printf(const char *pStr, uint32_t len);
int factoryreset(cc_handle_t *pHandle);

unsigned long fcfg_free(void *pAddr);
void *fcfg_malloc(unsigned long size);

#endif /* CORONA_CONSOLE_H_ */

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
