////////////////////////////////////////////////////////////////////////////////
//! \addtogroup drivers
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
//! \file wson_flash.h
//! \brief SPI Flash driver header.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
////////////////////////////////////////////////////////////////////////////////

#ifndef WSON_FLASH_H_
#define WSON_FLASH_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "PE_Types.h"
#include "k2x_errors.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
typedef struct wson_command
{
	uint8_t command;
	uint8_t num_bytes_write;  // number of bytes to send total (some commands are 1, some commands are 4 or something else).
	uint8_t num_bytes_read;   // how many bytes to we want to read
}wson_cmd_t;

#define WSON_PAGE_SIZE	256

/*
 *   The location of the 32x4KB subsectors defaults to the BOTTOM of memory.
 *   This is set by Configuration Register 1 (CR1) bit2 TBPARM.
 */
#define WSON_SECTOR0_SZ		65536  // First 510 sectors are 64KB large.
#define WSON_NUM_SECTOR0	510
#define WSON_SECTOR1_SZ		4096   // Last 32 sectors are 4KB large.
#define WSON_NUM_SECTOR1	32
#define WSON_REGION0		0x0
#define WSON_REGION1		WSON_SECTOR0_SZ * WSON_NUM_SECTOR0 // 0x01FE0000
#define WSON_REGION_END		WSON_REGION1 + (WSON_SECTOR1_SZ * WSON_NUM_SECTOR1)

/*
 *   	WSON/Spansion SPI Commands
 */
#define WSON_MFG_DEV_ID		0x90	// read Mfg ID and Dev ID, // \NOTE need 1 byte, followed by 3 bytes of 0.  Expect 0x0118
#define WSON_4READ			0x13    // read from a 4-byte address.
#define WSON_4PP			0x12    // program a 4-byte address with data.
#define WSON_4SE			0xDC	// erase a sector.
#define WSON_WREN			0x06	// enable writing.
#define WSON_WRDI			0x04	// disable writing.
#define WSON_4P4E			0x21    // erase a 4KB sector
#define WSON_4SE			0xDC	// erase a 64KB sector
#define WSON_CLSR			0x30	// clear status register
#define WSON_RDSR1			0x05	// Read status register 1
	#define WSON_SR1_SRWD_MASK		0x80
	#define WSON_SR1_P_ERR_MASK		0x40
	#define WSON_SR1_E_ERR_MASK		0x20
	#define WSON_SR1_BP_MASK		0x1C
	#define WSON_SR1_WEL_MASK		0x02
	#define WSON_SR1_WIP_MASK		0x01

#define WSON_RDSR2			0x07	// Read status register 2
#define WSON_RDCR			0x35	// Read configuration register
#define WSON_BRRD			0x16	// Read Bank Register

/*
		The Write Registers (WRR) command allows new values to be written to both the Status Register-1 and
		Configuration Register. Before the Write Registers (WRR) command can be accepted by the device, a Write
		Enable (WREN) command must be received. After the Write Enable (WREN) command has been decoded
		successfully, the device will set the Write Enable Latch (WEL) in the Status Register to enable any write
		operations.
 */
#define WSON_WRR			0x01	// allow writing to Status Register-1 and Confg Register


////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
corona_err_t wson_flash_test(void);
corona_err_t wson_flash_torture(void);
corona_err_t get_cmd(uint8_t command, const wson_cmd_t *pCmd);
corona_err_t wson_read(uint32_t address, uint8_t *pData, uint32_t numBytes);
corona_err_t wson_erase(uint32_t address);
corona_err_t wson_write(uint32_t address, 
						const uint8_t *pData, 
						uint32_t numBytes,
						bool eraseSector);



#endif /* WSON_FLASH_H_ */
////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
