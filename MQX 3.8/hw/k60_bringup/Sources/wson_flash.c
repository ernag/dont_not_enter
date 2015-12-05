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
#include "pwr_mgr.h"
#include <stdlib.h>
#include "corona_console.h"
extern cc_handle_t g_cc_handle;

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define EXT_FLASH_PAGE_SIZE         256
#define EXT_FLASH_64KB_SZ           0x10000 //65536  // First 510 sectors are 64KB large.
#define EXT_FLASH_NUM_64KB_SECTORS  510
#define EXT_FLASH_4KB_SZ            0x1000  //4096   // Last 32 sectors are 4KB large.
#define EXT_FLASH_NUM_4KB_SECTORS   32
#define EXT_FLASH_64KB_REGION       0x0
#define EXT_FLASH_4KB_REGION        EXT_FLASH_64KB_SZ * EXT_FLASH_NUM_64KB_SECTORS // 0x01FE0000
#define EXT_FLASH_REGION_END        EXT_FLASH_4KB_REGION + (EXT_FLASH_4KB_SZ * EXT_FLASH_NUM_4KB_SECTORS)
#define EXT_FLASH_EVENT_MGR_ADDR            0x00140000

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
static LDD_TError wson_read_reg(const uint8_t reg, uint8_t *pVal);
static LDD_TError wson_write_reg(const uint8_t reg, const uint8_t val);
static void copy_addr_to_buf(uint8_t *pDest, uint8_t *pSrc, uint32_t sz);

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Copy 1 byte at a time to destination address.  This was invented to get
 *     around memcpy's ENDIAN-ness issue, resulting in the MSB->LSB not getting
 *     clocked in first, which is what the SPANSION driver requires.
 *   SPANSION wants BIG endian.  K60 is LITTLE endian.
 */
static void copy_addr_to_buf(uint8_t *pDest, uint8_t *pSrc, uint32_t sz)
{
    uint32_t i = 0;

    while(sz)
    {
        pDest[i++] = pSrc[--sz];
    }
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Determine if external NOR flash communication is OK or not.
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t wson_comm_ok(void)
{
	LDD_TError err;
	corona_err_t cerr;
	uint8 tx[8];
	uint8 rx[8];
	wson_cmd_t cmd;
	uint32_t msTimeout = 1000;

	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
	}

	memset(rx, 0, 8); // init receive buffer with zeroes so we don't get confused on what we are seeing here.
	memset(tx, 0, 8);

	if(get_cmd(WSON_MFG_DEV_ID, &cmd) != SUCCESS)
	{
		cc_print(&g_cc_handle, "ERROR: get_cmd() not valid...?\n");
		return -1;
	}

	// prepare to read - this doesn't actually do anything to the hardware. just prepares recv buffer
	err = SPI2_Flash_ReceiveBlock(masterDevData, rx, cmd.num_bytes_write + cmd.num_bytes_read);

	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI2_Flash_ReceiveBlock error %x\n", err);
		return err;
	}

	tx[0] = cmd.command;   // \NOTE need 1 byte, followed by 3 bytes of 0.  Expect 0x 01 18
	err = SPI2_Flash_SendBlock(masterDevData, tx, cmd.num_bytes_write + cmd.num_bytes_read);

	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
		return err;
	}

	while(g_SPI2_Flash_Wait_for_Send)
	{
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "Timeout waiting for SPI FLASH send!!!\n");
			return CC_ERROR_GENERIC;
		}
		sleep(1);
	}
	msTimeout = 1000;
	while(g_SPI2_Flash_Wait_for_Receive)
	{
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "Timeout waiting for SPI FLASH receive!!!\n");
			return CC_ERROR_GENERIC;
		}
		sleep(1);
	}

	g_SPI2_Flash_Wait_for_Send = TRUE;
	g_SPI2_Flash_Wait_for_Receive = TRUE;

	cc_print(&g_cc_handle, "\rData Read Back: 0x%x %x\n",
			rx[4], rx[5]); // Valid read bytes are between (cmd.num_bytes_write+1) and (cmd.num_bytes_write + cmd.num_bytes_read).

	if(rx[4] == 0x01)
	{
		cc_print(&g_cc_handle, "\rManufacturer = 0x1 = SPANSION\n");
		if( (rx[5] != 0x17) && (rx[5] != 0x18) )
		{
			cc_print(&g_cc_handle, "\rERROR: Did not recognized Spansion Device ID!\n");
			return CC_ERROR_GENERIC;
		}
		return SUCCESS;
	}
	else if(rx[4] == 0xEF)
	{
		cc_print(&g_cc_handle, "\rManufacturer = 0x1 = WSON\n");
		if( (rx[5] != 0x17) && (rx[5] != 0x18) )
		{
			cc_print(&g_cc_handle, "\rERROR: Did not recognized Winbond Device ID!\n");
			return CC_ERROR_GENERIC;
		}
		return SUCCESS;
	}
	else
	{
		cc_print(&g_cc_handle, "\rERROR: Did not recognize Manufacturer ID\n");
		return CC_ERROR_GENERIC;
	}

	return SUCCESS;
}

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
	}

	do {
		LDD_TError err = wson_rdsr1(&status);
		if(err != ERR_OK)
		{
			cc_print(&g_cc_handle, "wson_rdsr1 SPI read error ! %d\n", err);
			return err;
		}
		if(status & WSON_SR1_P_ERR_MASK)
		{
			cc_print(&g_cc_handle, "****  SPI Flash Programming Error occured!\n");
			return 0xBEEF;
		}
		if(status & WSON_SR1_E_ERR_MASK)
		{
			cc_print(&g_cc_handle, "****  SPI Flash Erase Error occurred!\n");
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
//!
///////////////////////////////////////////////////////////////////////////////
static LDD_TError wson_rdsr1(uint8_t *pStatus)
{
	return wson_read_reg(WSON_RDSR1, pStatus);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Write a register on the Spansion Flash.
//!
//! \fntype Non-Reentrant Function
//!
//! \return LDD_TError error code
//!
///////////////////////////////////////////////////////////////////////////////
static LDD_TError wson_write_reg(const uint8_t reg, const uint8_t val)
{
	return ERR_OK;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Read a register on the Spansion Flash.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Return value of status register.
//!
//! \return LDD_TError error code
//!
///////////////////////////////////////////////////////////////////////////////
static LDD_TError wson_read_reg(const uint8_t reg, uint8_t *pVal)
{
	LDD_TError err;
	uint8_t pBuf[2];

	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
	}

	err = SPI2_Flash_ReceiveBlock(masterDevData, pBuf, 2);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI2_Flash_ReceiveBlock error %x\n", err);
		return err;
	}

	pBuf[0] = reg;

	err = SPI2_Flash_SendBlock(masterDevData, pBuf, 2);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
		return err;
	}
	while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
	while(g_SPI2_Flash_Wait_for_Receive){/* sleep */}
	g_SPI2_Flash_Wait_for_Send = TRUE;
	g_SPI2_Flash_Wait_for_Receive = TRUE;
	*pVal = pBuf[1];
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
	
    if (address >= EXT_FLASH_64KB_REGION)
    {
        tx[0] = WSON_4SE;
        regionStartAddr = EXT_FLASH_64KB_REGION;
        sectorSize      = EXT_FLASH_64KB_SZ;
    }
    else
    {
        tx[0] = WSON_4P4E;
        regionStartAddr = EXT_FLASH_4KB_REGION;
        sectorSize      = EXT_FLASH_4KB_SZ;
    }
	
	if(!masterDevData)
	{
		return CC_ERR_WSON_OUTBOUNDS;
	}

	address = get_sector_addr(address, sectorSize, regionStartAddr);
	copy_addr_to_buf(&(tx[1]), (uint8_t *)&address, sizeof(address));
	wson_write_en();

	sleep(1);

	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
	}

	err = SPI2_Flash_SendBlock(masterDevData, tx, 5);
	if (err != ERR_OK)
	{
		cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
		return err;
	}
	while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
	g_SPI2_Flash_Wait_for_Send = TRUE;
	sleep(5);

	err2 = wip_wait();
	if(err2 != ERR_OK)
	{
		cc_print(&g_cc_handle, "wip_wait() Error! ! 0x%x\n", err2);
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
	}

	cmd = WSON_WREN;
	err = SPI2_Flash_SendBlock(masterDevData, &cmd, 1);
	if (err != ERR_OK)
	{
		cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
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
int wson_write(uint32_t address,
               uint8_t *pData,
               uint32_t numBytes,
               uint32_t eraseSector)
{
    LDD_TError    err, err2;
    corona_err_t  cerr = SUCCESS;
    uint8_t*      pSrcData = (uint8_t*) pData;
    uint32_t      spi_address = address;
    uint32_t      spi_bytes;
    uint32_t      src_bytes = numBytes;
    uint8_t       dBuf[5 + WSON_PAGE_SIZE];  // command + address + max data
    uint8_t*      pBuf = 0;
    uint32_t      wrt_count = 0;

    if(!masterDevData)
    {
        masterDevData = SPI2_Flash_Init(NULL);
    }

    if(eraseSector)
    {
        cerr = wson_erase(address);
        if(cerr != SUCCESS)
        {
            return cerr;
        }

        err2 = wip_wait();
        if(err2 != ERR_OK)
        {
            cc_print(&g_cc_handle, "wip_wait() Error! ! 0x%x\n", err2);
            cerr = CC_ERR_WSON_GENERIC;
            return cerr;
        }
    }

    do {

        pBuf = dBuf;
        g_SPI2_Flash_Wait_for_Send = TRUE;
        wson_write_en(); // write enabling is required before programming or erasing...
        sleep(1);        // We probably need this sleep!
        // Adjust the number of bytes to write (256 max at a time)
        if (src_bytes > WSON_PAGE_SIZE)
        {
            spi_bytes = WSON_PAGE_SIZE;
            src_bytes -= WSON_PAGE_SIZE;
        }
        else
        {
            spi_bytes = src_bytes;
        }
        // Set the write command, the SPI address, number of bytes to write
        pBuf[0] = WSON_4PP;
        copy_addr_to_buf(&(pBuf[1]), (uint8_t *)&spi_address, sizeof(uint32_t));
        // Copy the data to be written
        memcpy(&(pBuf[5]), pSrcData, spi_bytes);

        err = SPI2_Flash_SendBlock (masterDevData, pBuf, spi_bytes+5);
        if (err != ERR_OK)
        {
            cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
            cerr = CC_ERR_SPI_TX_ERR;
            return cerr;
        }
        while(g_SPI2_Flash_Wait_for_Send){/* sleep */}

        sleep(5);  // We probably need this sleep!
        err2 = wip_wait();
        if(err2 != ERR_OK)
        {
            cc_print(&g_cc_handle, "wip_wait() Error! ! 0x%x\n", err2);
            cerr = CC_ERR_WSON_GENERIC;
            return cerr;
        }
        pSrcData    += spi_bytes;
        spi_address += spi_bytes;
        wrt_count   += spi_bytes;

  } while (wrt_count < numBytes);

    g_SPI2_Flash_Wait_for_Send = TRUE; // in case someone else doesn't initialize upfront
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
// Can only read 256 bytes from at a time, contrary to the spec on the spi flash chip
// todo: Look at the driver details
//
int wson_read(uint32_t address, uint8_t *pData, uint32_t numBytes)
{
    LDD_TError err;
    corona_err_t cerr = SUCCESS;
    uint8_t*      pDstData = (uint8_t*) pData;
    uint32_t      spi_address = address;
    uint32_t      spi_bytes;
    uint32_t      rd_bytes = numBytes;
    uint8_t       dBuf[5 + WSON_PAGE_SIZE];  // command + address + max read data
    uint8_t*      pBuf = 0;
    uint32_t      rd_count = 0;

    if(!masterDevData)
    {
    masterDevData = SPI2_Flash_Init(NULL);
    }

    // Read up to 256 bytes at a time.
    // TODO - We should be able to read any number of bytes without intermediate reads.
    do
    {
        pBuf = dBuf;
        g_SPI2_Flash_Wait_for_Send = TRUE;
        g_SPI2_Flash_Wait_for_Receive = TRUE;

        if (rd_bytes > WSON_PAGE_SIZE)
        {
            spi_bytes = WSON_PAGE_SIZE;
            rd_bytes -= WSON_PAGE_SIZE;
        }
        else
        {
            spi_bytes = rd_bytes;
        }

        err = SPI2_Flash_ReceiveBlock(masterDevData, dBuf, 5+spi_bytes);
        if (err != ERR_OK)
        {
            cc_print(&g_cc_handle, "SPI2_Flash_ReceiveBlock error %x\n", err);
            return CC_ERR_SPI_RX_ERR;
        }

        // Set the read command and address
        pBuf[0] = WSON_4READ;
        copy_addr_to_buf(&(pBuf[1]), (uint8_t *)&spi_address, sizeof(uint32_t));

        err = SPI2_Flash_SendBlock(masterDevData, pBuf, 5+spi_bytes);
        if (err != ERR_OK)
        {
            cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
            return CC_ERR_SPI_TX_ERR;
        }

        while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
        while(g_SPI2_Flash_Wait_for_Receive){/* sleep */}

        memcpy(pDstData, pBuf+5, spi_bytes);
        pDstData    += spi_bytes;
        spi_address += spi_bytes;
        rd_count    += spi_bytes;

    } while (rd_count < numBytes);

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

	cc_print(&g_cc_handle, "wson_flash_test\n");

	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
	}

	memset(rx, 0, 16); // init receive buffer with zeroes so we don't get confused on what we are seeing here.
	memset(tx, 0, 16);

	for(;;)
	{
		wson_cmd_t cmd;

		if(get_cmd(WSON_MFG_DEV_ID, &cmd) != SUCCESS)
		{
			cc_print(&g_cc_handle, "ERROR: get_cmd() not valid...?\n");
			return -1;
		}

		// prepare to read - this doesn't actually do anything to the hardware. just prepares recv buffer
		err = SPI2_Flash_ReceiveBlock(masterDevData, rx, cmd.num_bytes_write + cmd.num_bytes_read);

		if (err != ERR_OK) {
			cc_print(&g_cc_handle, "SPI2_Flash_ReceiveBlock error %x\n", err);
		}

		tx[0] = cmd.command;   // \NOTE need 1 byte, followed by 3 bytes of 0.  Expect 0x 01 18
		err = SPI2_Flash_SendBlock(masterDevData, tx, cmd.num_bytes_write + cmd.num_bytes_read);

		if (err != ERR_OK) {
			cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
		}

		while(g_SPI2_Flash_Wait_for_Send){/* sleep */}
		while(g_SPI2_Flash_Wait_for_Receive){/* sleep */}


		g_SPI2_Flash_Wait_for_Send = TRUE;
		g_SPI2_Flash_Wait_for_Receive = TRUE;

		cc_print(&g_cc_handle, "Data Read Back: 0x%x %x %x %x %x %x\n",
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
	uint8    *pData;
	uint32_t address = 0;
	uint32_t numBytesTest = EXT_FLASH_PAGE_SIZE; // PAGE SIZE
	uint32_t failCount = 0;
	corona_err_t err;
	uint8 erase = 0;

	cc_print(&g_cc_handle, "SPI Flash Torture Test\n");

	if(!masterDevData)
	{
		masterDevData = SPI2_Flash_Init(NULL);
	}

	pData = malloc(numBytesTest);
	rx = malloc(numBytesTest);

	if( (!pData) || (!rx) )
	{
		cc_print(&g_cc_handle, "Memory allocation Error in Torture Test!!\n"); //fflush(stdout);
		while(1){}
	}

	for(address = (EXT_FLASH_4KB_REGION - EXT_FLASH_64KB_SZ*3)/*EXT_FLASH_EVENT_MGR_ADDR*/; (address + EXT_FLASH_PAGE_SIZE) < WSON_REGION_END; address+= EXT_FLASH_PAGE_SIZE)
	{
		//cc_print(&g_cc_handle, "Testing Address: 0x%x\n", address);  // remove this for faster performance...
		for(i = 0; i < numBytesTest; i++)
		{
			pData[i] = (i * RTC_TSR) % 256; // random number based on RTC ticks.
			rx[i] = 0xA5;   	            // reset the check-against buffer with a stupid pattern.
		}

        /*
         *   ERASE only a sector boundary.
         */
        if(address < EXT_FLASH_4KB_REGION)
        {
            if( (address % EXT_FLASH_64KB_SZ) == 0)
            {
                cc_print(&g_cc_handle, "ERASE-ing 64KB SECTOR: (0x%x), testing SECTOR's 256 PAGEs.\n", address);
                erase = 1;
            }
        }
        else if( (address % EXT_FLASH_4KB_SZ) == 0)
        {
            cc_print(&g_cc_handle, "ERASE-ing 4KB SECTOR: (0x%x), testing SECTOR's 16 PAGEs.\n", address);
            erase = 1;
        }

		err = wson_write(address, (uint8_t *)pData, numBytesTest, erase);
		erase = 0;

		if(err != SUCCESS)
		{
			cc_print(&g_cc_handle, "Wson write() returned an error (0x%x)!\n", err);
			while(1){}
		}

		err = wson_read(address, rx, numBytesTest);
		if(err != SUCCESS)
		{
			cc_print(&g_cc_handle, "Wson read() returned an error (0x%x)!\n", err);
			while(1){}
		}

		for(i = 0; i < numBytesTest; i++)
		{
			if(rx[i] != pData[i])
			{
				cc_print(&g_cc_handle, "Error! WSON Flash Test failed at index %d\nAddress: 0x%x, rx[i] = 0x%x, pData[i] = 0x%x\n", i, address, rx[i], pData[i]);
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
		cc_print(&g_cc_handle, "WSON Unit test passed.\n");
		return SUCCESS;
	}
	else
	{
		cc_print(&g_cc_handle, "WSON Unit Test had %d failures!\n", failCount);
		return CC_ERROR_GENERIC;
	}
}

/*
 *   Handle power management for the WSON SPI Flash.
 *    \note There is no power management necessary.
 *          De-asserting chip select is the best you can do.
 */
corona_err_t pwr_wson(pwr_state_t state)
{
#if 0
	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:
		case PWR_STATE_NORMAL:

			break;

		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
#endif
	return SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
