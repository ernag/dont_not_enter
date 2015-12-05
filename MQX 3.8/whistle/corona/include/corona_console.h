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
//! \todo add security features
////////////////////////////////////////////////////////////////////////////////

#ifndef CORONA_CONSOLE_H_
#define CORONA_CONSOLE_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "app_errors.h"
#include "corona_console_man.h"
#include "sherlock.h"

////////////////////////////////////////////////////////////////////////////////
// Public Externs
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define CC_MAX_ARGS         16   // max number of space-delimited arguments you can give to Corona Console.
#define BT_RELAY_SUPPORT    0
/*
 *   Console LW Events
 */
#define EVENT_CONSOLE_BTRELAY_DATA_AVAILABLE    0x1
#define EVENT_CONSOLE_SHERLOCK_NODE_AVAILABLE   0x2
#define EVENT_CONSOLE_SHERLOCK_FLUSH            0x4

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
    CC_SECURITY_0 = 0,

    /*
     * LEVEL 1
     * Basic password - used to protect basic command console functionality and commands that expose proprietary information
     */
    CC_SECURITY_1 = 1,

    /*
     * LEVEL 2
     * Critical - used to protect commands that access potentially damaging features/settings,
     *  or access regulatory related settings/features (e.g., max Wi-Fi TX power))
     */
    CC_SECURITY_2 = 2
} cc_security_t;

typedef enum corona_console_type
{
//    CC_TYPE_UART = 0xAB,
    CC_TYPE_USB  = 0xDE,
    CC_TYPE_JTAG = 0xF9 // Used for UART and JTAG output
} console_type_t;

typedef struct corona_console_handle
{
    //void *pUart;               // pointer to UART handle for sending data out to UART.
    uint_8 type;
    //uint_8 security;              // security clearance we currently have on this session.
    //cc_verbosity_t verbosity;			  // verbosity requested by user at this time.
    int            argc;                  // number of arguments provided by user.
    char           *argv[CC_MAX_ARGS];    // string arguments in an array.
    shlk_handle_t  shlk_hdl;
} cc_handle_t;

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
err_t cc_init(cc_handle_t *pHandle, void *pDevice, console_type_t type);
err_t corona_print(const char *format, ...); // public version for printing to the USB console.
err_t cc_process_cmd(cc_handle_t *pHandle, char *pCmd);
err_t testbrd(void);
void CNSL_tsk(uint_32 dummy);
void SHLK_tsk(uint_32 dummy);
void CONSOLE_init(void);
void shlk_printf(const char *pStr, uint_32 len);
void shlk_flush( void );
void bytes_to_string(const void *pData, const unsigned int num_bytes, char *pOutput);

////////////////////////////////////////////////////////////////////////////////
// Macros
////////////////////////////////////////////////////////////////////////////////

// To log wake up messages depending on the log_wakeups static config 
#define LOG_WKUP(...) if (g_st_cfg.cc_st.log_wakeups) corona_print(__VA_ARGS__)

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
#endif /* CORONA_CONSOLE_H_ */
//! @}
