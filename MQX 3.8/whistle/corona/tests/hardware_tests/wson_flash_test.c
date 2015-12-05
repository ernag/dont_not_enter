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
#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "wson_flash_test.h"
#include "spihelper.h"
#include "ext_flash.h"
#include "corona_ext_flash_layout.h"
#include "wassert.h"
#include <stdlib.h>
#include <spi.h>
#include "cormem.h"

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

typedef struct wson_command
{
    uint8_t command;
    uint8_t num_bytes_write;  // number of bytes to send total (some commands are 1, some commands are 4 or something else).
    uint8_t num_bytes_read;   // how many bytes to we want to read
} wson_cmd_t;

static const wson_cmd_t g_wson_cmds[] = \
{
/*
 *         COMMAND            WR BYTES      RD BYTES
 */
    { WSON_MFG_DEV_ID, 4, 2 },
};
#define NUM_WSON_CMDS    sizeof(g_wson_cmds) / sizeof(wson_cmd_t)

///////////////////////////////////////////////////////////////////////////////
//! \brief Get a WSON command record.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Return address of wson_cmd_t record, corresponding to the given command.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int get_cmd(uint8_t command, const wson_cmd_t *pCmd)
{
    int i;

    for (i = 0; i < NUM_WSON_CMDS; i++)
    {
        if (g_wson_cmds[i].command == command)
        {
            memcpy(pCmd, &(g_wson_cmds[i]), sizeof(wson_cmd_t));
            return 0;
        }
    }
    pCmd = 0;
    return -1;
}

/*
 *   This test makes sure that we are reading/writing/erasing the memory we think we are.
 */
void wson_flash_sector_test(void)
{
    uint8_t *pData, *pCheck;
    uint32_t i;
    uint32_t addr = EXT_FLASH_RSVD_4KB_ADDR;
    uint32_t num_pages;
    
    _time_delay( 2 * 1000 );
    corona_print("sector test\n");
    
    pData = walloc( EXT_FLASH_PAGE_SIZE );
    pCheck = walloc( EXT_FLASH_PAGE_SIZE );
    
    for(i=0; i < EXT_FLASH_PAGE_SIZE; i++)
    {
        pData[i] = i;
    }
    
    
    /*
     *    4KB sector test.
     */
    
    // First erase it, and write to it.
    corona_print("test 4KB sector %x\n");
    wson_erase(addr);
    for(num_pages = 0; num_pages < (EXT_FLASH_4KB_SZ/EXT_FLASH_PAGE_SIZE); num_pages++)
    {
        wson_write(addr, pData, EXT_FLASH_PAGE_SIZE, 0);
    }
    
    // Read it all back to make sure it matches.
    for(num_pages = 0; num_pages < (EXT_FLASH_4KB_SZ/EXT_FLASH_PAGE_SIZE); num_pages++)
    {
        wson_read(addr, pCheck, EXT_FLASH_PAGE_SIZE);
        for(i=0; i < EXT_FLASH_PAGE_SIZE; i++)
        {
            if(pData[i] != pCheck[i])
            {
                corona_print("does not match %x, pData=%x, pCheck=%x\n", addr, pData, pCheck);
                fflush(stdout);
                while(1){}
            }
        }
    }
    
    // Now erase the sector and make sure it is all zeroes.
    wson_erase(addr);
    for(num_pages = 0; num_pages < (EXT_FLASH_4KB_SZ/EXT_FLASH_PAGE_SIZE); num_pages++)
    {
        wson_read(addr, pCheck, EXT_FLASH_PAGE_SIZE);
        for(i=0; i < EXT_FLASH_PAGE_SIZE; i++)
        {
            if(0xFF != pCheck[i])
            {
                corona_print("addr %x, not erased! pCheck=%x\n", addr, pCheck);
                fflush(stdout);
                while(1){}
            }
        }
    }
    
    /*
     *    64KB sector test.
     */
    
    corona_print("test 64KB sector %x\n");
    addr = EXT_FLASH_RSVD_64KB_ADDR;
    wson_erase(addr);
    for(num_pages = 0; num_pages < (EXT_FLASH_64KB_SZ/EXT_FLASH_PAGE_SIZE); num_pages++)
    {
        wson_write(addr, pData, EXT_FLASH_PAGE_SIZE, 0);
    }
    
    // Read it all back to make sure it matches.
    for(num_pages = 0; num_pages < (EXT_FLASH_64KB_SZ/EXT_FLASH_PAGE_SIZE); num_pages++)
    {
        wson_read(addr, pCheck, EXT_FLASH_PAGE_SIZE);
        for(i=0; i < EXT_FLASH_PAGE_SIZE; i++)
        {
            if(pData[i] != pCheck[i])
            {
                corona_print("does not match %x, pData=%x, pCheck=%x\n", addr, pData, pCheck);
                fflush(stdout);
                while(1){}
            }
        }
    }
    
    // Now erase the sector and make sure it is all zeroes.
    wson_erase(addr);
    for(num_pages = 0; num_pages < (EXT_FLASH_64KB_SZ/EXT_FLASH_PAGE_SIZE); num_pages++)
    {
        wson_read(addr, pCheck, EXT_FLASH_PAGE_SIZE);
        for(i=0; i < EXT_FLASH_PAGE_SIZE; i++)
        {
            if(0xFF != pCheck[i])
            {
                corona_print("addr %x, not erased! pCheck=%x\n", addr, pCheck);
                fflush(stdout);
                while(1){}
            }
        }
    }
    
    corona_print("sector test PASS\n");
    
    _lwmem_free(pCheck);
    _lwmem_free(pData);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief SPI Flash unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Run a read/write unit test on the WSON SPI Flash.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int wson_flash_test(void)
{
    MQX_FILE_PTR spifd = NULL;
    uint8_t tx[16];
    uint8_t rx[16];

    corona_print("wson_flash_test\n");

    ext_flash_init( &spifd );

    memset(rx, 0, 16); // init receive buffer with zeroes so we don't get confused on what we are seeing here.
    memset(tx, 0, 16);

    for ( ; ; )
    {
        wson_cmd_t cmd;

        if (get_cmd(WSON_MFG_DEV_ID, &cmd) != 0)
        {
            corona_print("ERROR: get_cmd() not valid...?\n");
            return -1;
        }

        tx[0] = cmd.command; // \NOTE need 1 byte, followed by 3 bytes of 0.  Expect 0x 01 18


        if (cmd.num_bytes_write != spiWrite(tx, cmd.num_bytes_write, TRUE, FALSE, spifd))
        {
            return -2;
        }

        if (cmd.num_bytes_read != spiRead(rx, cmd.num_bytes_read, spifd))
        {
            return -3;
        }

        if (0 != spiManualDeassertCS(spifd))
        {
            corona_print("spiManualDeassertCS failed\n");
            return -4;
        }

        corona_print("Data Read Back: 0x%x 0x%x\n",
               rx[0], rx[1]);

        fflush(stdout);

        _time_delay(1);
    }
    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief SPI Flash unit test (2)
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Run a read/write unit test on the WSON SPI Flash.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int wson_flash_torture(void)
{
#define NUM_64KB_SECTORS_TO_TEST        10   // must be less than 510.
#define NUM_PAGES_TO_TEST_AT_A_TIME     4     // pick a number that equally divides into 16.
    
    int      i;
    uint8_t  *rx;
    uint8_t  *pData;
    uint32_t address      = 0;
    uint32_t numBytesTest = ( EXT_FLASH_PAGE_SIZE*NUM_PAGES_TO_TEST_AT_A_TIME - 33 ); // make it stress a little bit, not a page boundary.
    uint32_t failCount    = 0;
    int      err          = 0;

    corona_print("SPI Flash Torture Test\n");

    ext_flash_init( NULL );

    pData = _lwmem_alloc(numBytesTest);
    rx    = _lwmem_alloc(numBytesTest);

    if ((!pData) || (!rx))
    {
        corona_print("Memory allocation Error in Torture Test!!\n");
        while (1) {}
    }

#define LOOP_FOREVER_FLASH
#ifdef LOOP_FOREVER_FLASH
    while(1)
#endif
    {
        for (address = EXT_FLASH_RSVD_64KB_ADDR; 
             address < (EXT_FLASH_64KB_REGION+(NUM_64KB_SECTORS_TO_TEST*EXT_FLASH_64KB_SZ)); 
             address += EXT_FLASH_PAGE_SIZE*NUM_PAGES_TO_TEST_AT_A_TIME) // need to multiply *2 since we are bleeding into multiple sectors each time.
        {
            boolean erase = FALSE;
            
            if(address > EXT_FLASH_REGION_END)
            {
                corona_print("ADDRESS OVERFLOW !!!!!!\n"); fflush(stdout);
                break;
            }
            
            if( (address + numBytesTest) >= EXT_FLASH_REGION_END )
            {
                corona_print("Changing numBytes test so we don't overflow the memory...\n");
                numBytesTest = EXT_FLASH_REGION_END - address;
            }
            
            srand(RTC_TSR);   // keep seeding the random number generator with RTC seconds counts.
            
            //corona_print("Testing Address: 0x%x\n", address);  // remove this for faster performance...
            for (i = 0; i < numBytesTest; i++)
            {
                pData[i] = (uint8_t)( rand() % 256 ); // fill up this buffer with a completely obscure pattern, to be written to SPI FLASH.
                rx[i]    = 0xA5;                      // reset the check-against buffer with a stupid pattern.
            }
    
            /*
             *   ERASE only a sector boundary.
             */
            if(address >= EXT_FLASH_64KB_REGION)
            {
                if( (address % EXT_FLASH_64KB_SZ) == 0)
                {
                    corona_print("ERASE-ing 64KB SECTOR: (0x%x), testing SECTOR's 256 PAGEs.\n", address);
                    erase = 1;
                }
            }
            else if( (address % EXT_FLASH_4KB_SZ) == 0)
            {
                corona_print("ERASE-ing 4KB SECTOR: (0x%x), testing SECTOR's 16 PAGEs.\n", address);
                erase = 1;
            }
            
            fflush(stdout);
            
            err = wson_write(address, pData, numBytesTest, erase);
            erase = 0;
            
            if (err != 0)
            {
                corona_print("Wson write() returned an error (0x%x)!\n", err); fflush(stdout);
                while (1) {}
            }
            
            _time_delay(10);
    
            err = wson_read(address, rx, numBytesTest);
            if (err != 0)
            {
                corona_print("Wson read() returned an error (0x%x)!\n", err); fflush(stdout);
                while (1) {}
            }
    
            for (i = 0; i < numBytesTest; i++)
            {
                if (rx[i] != pData[i])
                {
                    corona_print("Error! WSON Flash Test failed at index %d\nAddress: 0x%x, rx[i] = 0x%x, pData[i] = 0x%x\n", i, address, rx[i], pData[i]);
                    fflush(stdout);
                    failCount++;
                }
            }
            
            _time_delay(10);
        }
    }

    if (pData)
    {
        wfree(pData);
    }

    if (rx)
    {
        wfree(rx);
    }

    if (failCount == 0)
    {
        corona_print("WSON Unit test passed.\n");
        fflush(stdout);

        return 0;
    }
    else
    {
        corona_print("WSON Unit Test had %d failures!\n", failCount);
        fflush(stdout);

        return -2;
    }
}
#endif

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
