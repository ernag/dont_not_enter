////////////////////////////////////////////////////////////////////////////////
//! \addtogroup boot2
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
//! \file boot2_mgr.h
//! \brief boot2 manager header.
//! \version version 0.1
//! \date Feb, 2013
//! \author jim@whistle.com
////////////////////////////////////////////////////////////////////////////////
#ifndef BOOT2_MGR_H_
#define BOOT2_MGR_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "types.h"
#include "PE_Types.h"
#include "corona_console.h"
#include "corona_errors.h"
#include "parse_fwu_pkg.h"


////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

#define FLAG_SET           0x55
#define FLAG_CLR           0xFF

#define BT1                0x00000000
#define BT1_HDR            0x00000200

#define B2A                0x00002000
#define B2B                0x00012000

#define B2A_HDR            B2A + 0x200
#define B2B_HDR            B2B + 0x200

#define BOOT2_MAX_SIZE     0x10000

#define APP_LOAD_ADDRS     0x22000
#define APP_ENTRY          APP_LOAD_ADDRS + 4
#define APP_HDR            APP_LOAD_ADDRS + 0x200

// Header enums
typedef enum
{
    HDR_NAME        = 0,
    HDR_VER         = 3,
    HDR_CKSUM       = 6,
    HDR_SUCCESS     = 7,
    HDR_UPDATE      = 8,
    HDR_ATTMPT1     = 9,
    HDR_ATTMPT2     = 10,
    HDR_ATTMPT3     = 11,
    HDR_BAD         = 12,
    HDR_IMG_SAVD    = 13,
    HDR_USED        = 14,
    HDR_VER_TAG     = 16
} hdr_idx_t;


////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////

int b2_send_data (uint8_t* send_buf, uint_16 send_len);
void b2_recv_data (uint8_t* rcv_buf, uint_8 rcv_len);
void b2_recv_init();
void b2_task();

corona_err_t b2_load_app_fw (uint_32 address, uint_32 numBytes);
corona_err_t b2_load_spi (uint32_t start_addrs, uint32_t image_size);
corona_err_t b2_read_pkg (fwu_pkg_t* package, int pkg);



#endif /* BOOT2_MGR_H_ */

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
