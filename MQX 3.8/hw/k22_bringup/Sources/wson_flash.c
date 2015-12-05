
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
//! \file wson_flash.c
//! \brief SPI Flash Driver
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
//!
//! \detail This is the SPI flash driver, responsible for reading and writing to the WSON SPI Flash.
//! \note K21 ISR is not correct for SPI2 (SPI Flash).  Therefore, we have to manually set it up.
//!       (tIsrFunc)&SPI2_Flash_Interrupt,   // 0x51  0x000000B0   8   ivINT_SPI2

//! \note On the Proto-1 hardware, chip select is not routed to the WSON SPI Flash, so this won't work on P1 hardware.
//! \todo In general, need to add reading the WSON status registers to check things got done and done without errors.
//! \todo Add arbitrary register read/writes console support.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "wson_flash.h"
#include "PE_Types.h"
#include "SPI2_Flash.h"
#include "k22_irq.h"
#include "pwr_mgr.h"
#include <stdlib.h>

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
volatile uint8 g_SPI2_Flash_Wait_for_Send = TRUE;
volatile uint8 g_SPI2_Flash_Wait_for_Receive = TRUE;
static 	LDD_TDeviceData *masterDevData = NULL;

static const wson_cmd_t g_wson_cmds[] = \
{
/*
 *         COMMAND            WR BYTES      RD BYTES
 */
	{     WSON_MFG_DEV_ID,       4,            2       },
};
#define NUM_WSON_CMDS	sizeof(g_wson_cmds) / sizeof(wson_cmd_t)

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static LDD_TError wson_write_en(void);
static LDD_TError wip_wait(void);
static LDD_TError wson_rdsr1(uint8_t *pStatus);
static uint32_t   get_sector_addr(uint32_t address, uint32_t sectorSz, uint32_t regionStart);


////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! \brief Get a WSON command record.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Return address of wson_cmd_t record, corresponding to the given command.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t get_cmd(uint8_t command, const wson_cmd_t *pCmd)
{
	int i;
	
	for(i = 0; i < NUM_WSON_CMDS; i++)
	{
		if(g_wson_cmds[i].command == command)
		{
			memcpy(pCmd, &(g_wson_cmds[i]), sizeof(wson_cmd_t));
			return SUCCESS;
		}
	}
	pCmd = 0;
	return CC_ERR_INVALID_WSON_CMD;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Wait for WIP (Write in progress) to complete, and check for errors.
//!
//! \fntype Non-Reentrant Function
//!
//! \return LDD_TError error code
//! \todo  Add a timeout, so we don't loop forever waiting when we have errors.
//!
///////////////////////////////////////////////////////////////////////////////
static LDD_TError wip_wait(void)
{
	LDD_TError err;
	uint8_t status;
	
	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
		k22_enable_irq(IRQ_SPI2, VECTOR_SPI2, SPI2_Flash_Interrupt);
	}
	
	//sleep(1000);  // give some time for the write or erase command to even have been processed.  \todo check this is necessary!
	
	do {
		LDD_TError err = wson_rdsr1(&status);
		if(err != ERR_OK)
		{
			printf("wson_rdsr1 SPI read error ! %d\n", err);
			return err;
		}
		if(status & WSON_SR1_P_ERR_MASK)
		{
			printf("****  SPI Flash Programming Error occured!\n");
			return 0xBEEF;
		}
		if(status & WSON_SR1_E_ERR_MASK)
		{
			printf("****  SPI Flash Erase Error occurred!\n");
			return 0xDEAD;
		}
	} while(status & WSON_SR1_WIP_MASK); // keep trying until no more write in progress.
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Read Status Register 1 on Spansion Flash.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Return value of status register.
//!
//! \return LDD_TError error code
//! \todo  Change this into a generic "read register" command...
//!
///////////////////////////////////////////////////////////////////////////////
static LDD_TError wson_rdsr1(uint8_t *pStatus)
{
	LDD_TError err;
	uint8_t pBuf[2];
	
	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
		k22_enable_irq(IRQ_SPI2, VECTOR_SPI2, SPI2_Flash_Interrupt);
	}
	
	err = SPI2_Flash_ReceiveBlock(masterDevData, pBuf, 2);
	if (err != ERR_OK) {
		printf("SPI2_Flash_ReceiveBlock error %x\n", err);
		return err;
	}

	pBuf[0] = WSON_RDSR1;
	
	err = SPI2_Flash_SendBlock(masterDevData, pBuf, 2);
	if (err != ERR_OK) {
		printf("SPI2_Flash_SendBlock error %x\n", err);
		return err;
	}
	while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
	while(g_SPI2_Flash_Wait_for_Receive){/* sleep */}
	g_SPI2_Flash_Wait_for_Send = TRUE;
	g_SPI2_Flash_Wait_for_Receive = TRUE;
	*pStatus = pBuf[1];
	return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Returns the sector address for the given arbitrary address.
//!
//! \fntype Non-Reentrant Function
//! \return sector address.
//!
///////////////////////////////////////////////////////////////////////////////
static uint32_t get_sector_addr(uint32_t address, uint32_t sectorSz, uint32_t regionStart)
{
	uint32_t remainder = (address - regionStart) % sectorSz;
	return address - remainder;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Erase a sector on the WSON Flash
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Erases sector that the address exists in.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t wson_erase(uint32_t address)
{
	uint8_t cmd;
	uint8_t status[128];
	uint8_t tx[5];
	uint32_t regionStartAddr;
	uint32_t sectorSize;
	LDD_TError err, err2;
	
	int i;  // temp variable.
	
	if(address < WSON_REGION1)
	{
		tx[0] = WSON_4SE;  // 64KB sectors are at the beginning by default.
		regionStartAddr = WSON_REGION0;
		sectorSize = WSON_SECTOR0_SZ;
	}
	else if(address < WSON_REGION_END)
	{
		tx[0] = WSON_4P4E; // 4KB sectors are at the bottom by default.
		regionStartAddr = WSON_REGION1;
		sectorSize = WSON_SECTOR1_SZ;
	}
	else
	{
		return CC_ERR_WSON_OUTBOUNDS;
	}
	
	address = get_sector_addr(address, sectorSize, regionStartAddr);
	memcpy(&(tx[1]), &address, sizeof(address));
	wson_write_en();
	
	sleep(100); // \TODO this should be checking a status register...
	
	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
		k22_enable_irq(IRQ_SPI2, VECTOR_SPI2, SPI2_Flash_Interrupt);
	}
	
	err = SPI2_Flash_SendBlock(masterDevData, tx, 5);
	if (err != ERR_OK) {
		printf("SPI2_Flash_SendBlock error %x\n", err);
		return err;
	}
	while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
	g_SPI2_Flash_Wait_for_Send = TRUE;
	
	err2 = wip_wait();
	if(err2 != ERR_OK)
	{
		printf("wip_wait() Error! ! %d\n", err2);
		return err2;
	}

	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Enable writing via WREN.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Sends the "write enable" command to the SPI Flash.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
static LDD_TError wson_write_en(void)
{
	LDD_TError err;
	uint8_t cmd;

	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
		k22_enable_irq(IRQ_SPI2, VECTOR_SPI2, SPI2_Flash_Interrupt);
	}
	
	cmd = WSON_WREN;
	err = SPI2_Flash_SendBlock(masterDevData, &cmd, 1);
	if (err != ERR_OK) {
		printf("SPI2_Flash_SendBlock error %x\n", err);
		return err;
	}
	while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
	g_SPI2_Flash_Wait_for_Send = TRUE;
	return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Write data to WSON Flash
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Writes given data to SPI Flash at given address.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t wson_write(uint32_t address, 
						const uint8_t *pData, 
						uint32_t numBytes,
						bool eraseSector)
{
	LDD_TError err, err2;
	corona_err_t cerr = SUCCESS;
	uint8_t *pBuf = 0;
	uint32_t totalBytes = numBytes + 5; // command + address + number of bytes
	
	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
		k22_enable_irq(IRQ_SPI2, VECTOR_SPI2, SPI2_Flash_Interrupt);
	}
	
	if(eraseSector)
	{
		cerr = wson_erase(address);
		if(cerr != SUCCESS)
		{
			goto wson_write_err;
		}
		
		err2 = wip_wait();
		if(err2 != ERR_OK)
		{
			printf("wip_wait() Error! ! %d\n", err2);
			cerr = CC_ERR_WSON_GENERIC;
			goto wson_write_err;
		}
	}
	
	pBuf = malloc(totalBytes);
	if(!pBuf)
	{
		cerr = CC_ERR_MEM_ALLOC;
		goto wson_write_err;
	}
	
	wson_write_en(); // write enabling is required before programming or erasing...
	sleep(100); // \TODO this should be checking a status register...

	pBuf[0] = WSON_4PP;
	memcpy(&(pBuf[1]), &address, sizeof(uint32_t));
	memcpy(&(pBuf[5]), pData, numBytes); // \todo Shouldn't need to memcpy huge buffers in the final product, just make use of existing buffer somehow!
	
	err = SPI2_Flash_SendBlock(masterDevData, pBuf, totalBytes);
	if (err != ERR_OK) {
		printf("SPI2_Flash_SendBlock error %x\n", err);
		cerr = CC_ERR_SPI_TX_ERR;
		goto wson_write_err;
	}
	while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
	g_SPI2_Flash_Wait_for_Send = TRUE;
	memcpy(pData, pBuf+5, numBytes);   // \todo - In the future, I'm sure this can be optimized a lot, without the memcpy...

	err2 = wip_wait();
	if(err2 != ERR_OK)
	{
		printf("wip_wait() Error! ! %d\n", err2);
		cerr = CC_ERR_WSON_GENERIC;
		goto wson_write_err;
	}
	
wson_write_err:
	if(pBuf)
	{
		free(pBuf);
	}
	return cerr;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Read data to WSON Flash
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Reads data from SPI Flash and puts it in the buffer provided.
//!
//! \return corona_err_t error code
//! \todo Waits need timeouts.
//! \todo 
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t wson_read(uint32_t address, uint8_t *pData, uint32_t numBytes)
{
	LDD_TError err;
	corona_err_t cerr = SUCCESS;
	uint8_t *pBuf = 0;
	uint32_t totalBytes = numBytes +5; // command + address + number of bytes
	
	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
		k22_enable_irq(IRQ_SPI2, VECTOR_SPI2, SPI2_Flash_Interrupt);
	}
	
	pBuf = malloc(totalBytes);
	if(!pBuf)
	{
		cerr = CC_ERR_MEM_ALLOC;
		goto wson_read_err;
	}
	
	err = SPI2_Flash_ReceiveBlock(masterDevData, pBuf, totalBytes);
	if (err != ERR_OK) {
		printf("SPI2_Flash_ReceiveBlock error %x\n", err);
		cerr = CC_ERR_SPI_RX_ERR;
		goto wson_read_err;
	}

	pBuf[0] = WSON_4READ;
	memcpy(&(pBuf[1]), &address, sizeof(uint32_t));
	
	err = SPI2_Flash_SendBlock(masterDevData, pBuf, totalBytes);
	if (err != ERR_OK) {
		printf("SPI2_Flash_SendBlock error %x\n", err);
		cerr = CC_ERR_SPI_TX_ERR;
		goto wson_read_err;
	}
	while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
	while(g_SPI2_Flash_Wait_for_Receive){/* sleep */}
	
	memcpy(pData, pBuf+5, numBytes);   // \todo - In the future, I'm sure this can be optimized a lot, without the memcpy...
	
wson_read_err:
	if(pBuf)
	{
		free(pBuf);
	}
	g_SPI2_Flash_Wait_for_Send = TRUE;
	g_SPI2_Flash_Wait_for_Receive = TRUE;
	return cerr;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief SPI Flash unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Run a read/write unit test on the WSON SPI Flash.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t wson_flash_test(void)
{
	LDD_TError err;
	corona_err_t cerr;
	uint8 tx[16];
	uint8 rx[16];

	printf("wson_flash_test\n");
	
	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
	}
	k22_enable_irq(IRQ_SPI2, VECTOR_SPI2, SPI2_Flash_Interrupt);
	
	memset(rx, 0, 16); // init receive buffer with zeroes so we don't get confused on what we are seeing here.
	memset(tx, 0, 16);
	
	for(;;)
	{
		wson_cmd_t cmd;
		
		if(get_cmd(WSON_MFG_DEV_ID, &cmd) != SUCCESS)
		{
			printf("ERROR: get_cmd() not valid...?\n");
			return -1;
		}
		
		// prepare to read - this doesn't actually do anything to the hardware. just prepares recv buffer
		err = SPI2_Flash_ReceiveBlock(masterDevData, rx, cmd.num_bytes_write + cmd.num_bytes_read);
		
		if (err != ERR_OK) {
			printf("SPI2_Flash_ReceiveBlock error %x\n", err);
		}
		
		tx[0] = cmd.command;   // \NOTE need 1 byte, followed by 3 bytes of 0.  Expect 0x 01 18
		err = SPI2_Flash_SendBlock(masterDevData, tx, cmd.num_bytes_write + cmd.num_bytes_read);
		
		if (err != ERR_OK) {
			printf("SPI2_Flash_SendBlock error %x\n", err);
		}
		
		while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
		while(g_SPI2_Flash_Wait_for_Receive){/* sleep */}

		g_SPI2_Flash_Wait_for_Send = TRUE;
		g_SPI2_Flash_Wait_for_Receive = TRUE;
		
		printf("Data Read Back: 0x%x %x %x %x %x %x\n", 
				rx[0], rx[1], rx[2], rx[3], rx[4], rx[5]); // Valid read bytes are between (cmd.num_bytes_write+1) and (cmd.num_bytes_write + cmd.num_bytes_read).

		sleep(1);
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief SPI Flash unit test (2)
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Run a read/write unit test on the WSON SPI Flash.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t wson_flash_torture(void)
{
	int i;
	uint8 *rx;
	uint8 *pData;
	uint32_t address = 0;
	uint32_t numBytesTest = 256;
	uint32_t failCount = 0;
	corona_err_t err;

	printf("wson_flash_test (2)\n");
	k22_enable_irq(IRQ_SPI2, VECTOR_SPI2, SPI2_Flash_Interrupt);
	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
	}
	
	pData = malloc(numBytesTest);
	rx = malloc(numBytesTest);
	
	if( (!pData) || (!rx) )
	{
		printf("Memory allocation Error!\n");
		while(1){}
	}
	
	for(address = 0; (address + numBytesTest) < WSON_REGION_END; address+= numBytesTest)
	{
		for(i = 0; i < numBytesTest; i++)
		{
			pData[i] = address + i; // fill up this buffer with a completely obscure pattern, to be written to SPI FLASH.
			rx[i] = 0xA5;   					 // reset the check-against buffer with a stupid pattern.
		}
		
		err = wson_write(address, pData, numBytesTest, TRUE);
		if(err != SUCCESS)
		{
			printf("Wson write() returned an error (%d)!\n", err);
			while(1){}
		}
		
		//sleep(1000); // \TODO - DO I NEED TO CHECK SOME STATUS REGISTER HERE TO MAKE SURE THE WRITE COMPLETED ?!
		
		err = wson_read(address, rx, numBytesTest);
		if(err != SUCCESS)
		{
			printf("Wson read() returned an error (%d)!\n", err);
			while(1){}
		}
		
		for(i = 0; i < numBytesTest; i++)
		{
			if(rx[i] != pData[i])
			{
				printf("Error! WSON Flash Test failed at index %d\nAddress: 0x%x, rx[i] = 0x%x, pData[i] = 0x%x\n", i, address, rx[i], pData[i]);
				failCount++;
				//while(1){}
			}
		}
	}
	
	if(pData)
	{
		free(pData);
	}
	
	if(rx)
	{
		free(rx);
	}
	
	if(failCount == 0)
	{
		printf("WSON Unit test passed.\n");
		return SUCCESS;
	}
	else
	{
		printf("WSON Unit Test had %d failures!\n", failCount);
		return CC_ERROR_GENERIC;
	}	
}

/*
 *   Handle power management for the WSON SPI Flash.
 */
corona_err_t pwr_wson(pwr_state_t state)
{
	
	switch(state)
	{
		case PWR_STATE_SHIP:
			break;
			
		case PWR_STATE_STANDBY:
			break;
			
		case PWR_STATE_NORMAL:
			break;
			
		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
