/*
 * ext_flash.h
 *
 *  Created on: Mar 29, 2013
 *      Author: Ernie
 */

#ifndef EXT_FLASH_H_
#define EXT_FLASH_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include "app_errors.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

/*
 *      WSON/Spansion SPI Commands
 */
#define WSON_MFG_DEV_ID        0x90 // read Mfg ID and Dev ID, // \NOTE need 1 byte, followed by 3 bytes of 0.  Expect 0x0118
#define WSON_4READ             0x13 // read from a 4-byte address.
#define WSON_4PP               0x12 // program a 4-byte address with data.
#define WSON_WREN              0x06 // enable writing.
#define WSON_WRDI              0x04 // disable writing.
#define WSON_4P4E              0x21 // erase a 4KB sector
#define WSON_4SE               0xDC // erase a 64KB sector
#define WSON_CLSR              0x30 // clear status register
#define WSON_RDCR              0x35 // read configuration register 1
#define WSON_RDSR1             0x05 // Read status register 1
#define WSON_SR1_SRWD_MASK     0x80
#define WSON_SR1_P_ERR_MASK    0x40
#define WSON_SR1_E_ERR_MASK    0x20
#define WSON_SR1_BP_MASK       0x1C
#define WSON_SR1_WEL_MASK      0x02
#define WSON_SR1_WIP_MASK      0x01

#define WSON_RDSR2             0x07 // Read status register 2
#define WSON_RDCR              0x35 // Read configuration register
#define WSON_BRRD              0x16 // Read Bank Register

/*
 *      The Write Registers (WRR) command allows new values to be written to both the Status Register-1 and
 *      Configuration Register. Before the Write Registers (WRR) command can be accepted by the device, a Write
 *      Enable (WREN) command must be received. After the Write Enable (WREN) command has been decoded
 *      successfully, the device will set the Write Enable Latch (WEL) in the Status Register to enable any write
 *      operations.
 */
#define WSON_WRR               0x01 // allow writing to Status Register-1 and Confg Register


////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
void ext_flash_init(MQX_FILE_PTR *spifd);
void ext_flash_shutdown(uint32_t num_ms_block);
void wson_lock(_task_id task);
void wson_unlock(_task_id task);
uint8_t wson_comm_ok(void);
void wson_addr_ok(uint32_t addr);
uint32_t get_sector_addr(uint32_t address, uint32_t sectorSz, uint32_t regionStart);
int wson_read(uint32_t address, uint8_t *pData, uint32_t numBytes);
int wson_erase(uint32_t address);
int wson_write(uint32_t      address,
               uint8_t       *pData,
               uint32_t      numBytes,
               boolean       eraseSector);

#endif /* EXT_FLASH_H_ */
////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}



