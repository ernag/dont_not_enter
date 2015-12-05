/*
 * ext_flash.c
 * Driver file for the WSON and SPANSION external SPI NOR Flash part on Corona.
 *
 *  Created on: Mar 29, 2013
 *      Author: Ernie
 */


////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "ext_flash.h"
#include "corona_ext_flash_layout.h"
#include "spihelper.h"
#include "wassert.h"
#include "pwr_mgr.h"
#include "main.h"
#include "persist_var.h"
#include "wmutex.h"
#include "datamessage.pb.h"
#include <stdlib.h>
#include <spi.h>
#include <mqx.h>

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define SIZE_TX_BUF    (1 + sizeof(uint32_t))

// How long to wait until concluding something is wrong with the Write/Erase.
#define WIP_WAIT_TIMEOUT_MS         2000
#define WIP_WAIT_MAX_SINGLE_POLL      16  // max amount of time we will wait on a single poll

#define TEST_READBACK_ALL_WRITES    0

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static MQX_FILE_PTR g_spifd = NULL;
static MUTEX_STRUCT g_extf_mutex;
static volatile _task_id g_override_task = MQX_NULL_TASK_ID;  // when this is set, someone wants to bogart the SPI Flash.

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static int  wson_write_en(void);
static int  wip_wait(uint32_t wait_before_polling, uint32_t *pNumPolls);
static int  wson_rdsr1(uint8_t *pStatus);
static int  wson_rdcr1(uint8_t *pCR1);
static int  wson_erase_private(uint32_t address, boolean mutex_protection);
static void copy_addr_to_buf(uint8_t *pDest, uint8_t *pSrc, uint32_t sz);
static int _wson_read_internal(uint32_t address, uint8_t *pData, uint32_t numBytes);

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Is this a valid address for the WSON/SPANSION part?
 */
void wson_addr_ok(uint32_t addr)
{
    wassert ( addr <= EXT_FLASH_REGION_END );
}

/*
 *   This function will not allow anymore acccess to SPI Flash, until a reboot takes place.
 *     It should be called, if possible, before we allow a reboot.
 */
void ext_flash_shutdown( uint32_t num_ms_block )
{
    uint8_t   in_progress = 1;
    uint_64   entered_ms, now_ms;
    uint_32   surpassed;
    
	// don't let the calling task's watchdog expire while we don this critical stuff
    _watchdog_stop();
    
    if( NULL == g_spifd )
    {
        return;
    }
    
    RTC_get_ms(&entered_ms);
    
    while( 1 )
    {   
        if( MQX_OK == _mutex_try_lock(&g_extf_mutex) )
        {
        	in_progress = 0;
            break;
        }
        
        _time_delay(100);
        
        RTC_get_ms(&now_ms);
        
        // make sure time is somewhat sane.
        if( now_ms <= entered_ms )
        {
        	break;  // this is a weird error condition!
        }
        else
        {
        	surpassed = (uint_32)(now_ms - entered_ms);
        }
        
        // if we've blocked long enough, break out, otherwise keep polling.
        if(surpassed >= num_ms_block)
        {
        	break;
        }
        else
        {
        	num_ms_block -= surpassed;
        }
    }
    
    if( in_progress )
    {
        persist_set_untimely_crumb( BreadCrumbEvent_EXT_FLASH_IN_USE_AT_REBOOT );
    }
    
#if 0
    /*
     *   Removing this code, since there is little benefit in closing before reboot,
     *      and some risk associated with trying a close.
     */
    if( g_spifd )
    {
        wassert( MQX_OK == fclose(g_spifd) );
    }
#endif
    
    g_spifd = NULL;
}

/*
 *   Protect the SPI Flash with a mutex, and make sure
 *     the system does not try to reboot while we are using it.
 */
void wson_lock(_task_id task)
{
    if(NULL == g_spifd)
    {
        /*
         *   Do not allow the caller to proceed since they have intentions with SPI Flash.
         *      We cannot let them continue, as they may depend on something that cannot be done
         *      in this boot cycle.
         */
        _task_block();
    }
    
    if( MQX_NULL_TASK_ID != g_override_task )
    {
        if( _task_get_id() == g_override_task )
        {
            // don't allow the override task to block itself.
            return;
        }
    }
    
    /*
     * 			** NOTE **
     * 
     *   Do not vote for ON/OFF in the Flash driver, only awake.
     *     ON/OFF is the responsibility of clients.
     *     
     *     We already have an ON/OFF safety valve in ext_flash_shutdown() to make sure
     *       we don't power off in the middle of transactions, etc.
     */
    wmutex_lock(&g_extf_mutex);
    PWRMGR_VoteForAwake( PWRMGR_EXTFLASH_VOTE );
    g_override_task = task;
}

/*
 *   Unlock the SPI Flash b/c we are done using it.
 */
void wson_unlock(_task_id task)
{
    if( MQX_NULL_TASK_ID != g_override_task )
    {
        if( MQX_NULL_TASK_ID == task )
        {
            // don't allow the override task to UN-explicitly unblock.
            return;
        }
    }
    
	PWRMGR_VoteForSleep( PWRMGR_EXTFLASH_VOTE );
	g_override_task = MQX_NULL_TASK_ID;  // we're unblocked, so override task gets reset unconditionally.
    wmutex_unlock(&g_extf_mutex);
}

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

/*
 *   Return non-zero if SPI Flash comm is OK.
 */
uint8_t wson_comm_ok(void)
{
    uint8_t pTXBuf[1];
    uint8_t pRXBuf[4];

    pTXBuf[0] = WSON_MFG_DEV_ID;

    if (1 != spiWrite(pTXBuf, 1, TRUE, FALSE, g_spifd))
    {
        return 0;
    }

    if (4 != spiRead(pRXBuf, 4, g_spifd))
    {
        return 0;
    }

    if (0 != spiManualDeassertCS(g_spifd))
    {
        return 0;
    }
    
    corona_print("ID:%x: ", pRXBuf[3]);
    
    if(pRXBuf[3] == 0x01)
    {
        corona_print("SPAN\n");
    }
    else if(pRXBuf[3] == 0xEF)
    {
        corona_print("WSON\n");
    }
    else
    {
        return 0;
    }
    
    return 1;
}

/*
 *   Initialize the Spansion/WSON 32MB SPI Flash driver.
 */
void ext_flash_init(MQX_FILE_PTR *spifd)
{
#if 0
    uint8_t  cr1;
#endif
    uint32_t num_polls;
    
    /*
     *   Make sure the SPI Flash Memory Layout is valid.
     *     RESERVED sections at the end, so they should equal last address.
     */
    
    wassert( EXT_FLASH_REGION_END == (EXT_FLASH_BUF_SHLKPATCH_ADDR + (EXT_FLASH_BUF_SHLKPATCH_NUM_SECTORS * EXT_FLASH_64KB_SZ) - 1) );
    wassert( EXT_FLASH_64KB_REGION == (EXT_FLASH_RSVD_4KB_ADDR + (EXT_FLASH_RSVD_4KB_NUM_SECTORS * EXT_FLASH_4KB_SZ)) );
    
    if( NULL != g_spifd )
    {
        goto ext_flash_init_done;
    }
    
    /*
     *   NOTE: The Spansion SPI Flash chip is capable of running at 50MHz.
     */
    g_spifd = spiInit(SPANSION_RUNM_BAUD_RATE, SPI_CLK_POL_PHA_MODE3, SPI_DEVICE_BIG_ENDIAN, SPI_DEVICE_MASTER_MODE);
    if( 0 == g_spifd )
    {
        goto EFLASH_HW_FAIL;
    }
    
#if 0
    /*
     *   Check Configuration Register 1.
     *   If things are not configured as we want, go ahead and write them out.
     */
    wassert(!wson_rdcr1(&cr1));
#endif
    
    wmutex_init(&g_extf_mutex);
    
ext_flash_init_done:
    if(NULL != spifd)
    {
        *spifd = g_spifd; // caller wants access to the handle.
    }
    
    /*
     *   Make sure the SPI Flash hardware is good.
     */
    if( 0 == wson_comm_ok() )
    {
        goto EFLASH_HW_FAIL;
    }
    
    /*
     *   Make sure the SPI Flash hardware is not busy with some other outstanding task.
     */
    if( 0 != wip_wait( 0, &num_polls ) )
    {
        goto EFLASH_HW_FAIL;
    }
    
    if( num_polls > 0 )
    {
        persist_set_untimely_crumb( BreadCrumbEvent_EXT_FLASH_WIP_AT_BOOT );
    }
    
    return;
    
EFLASH_HW_FAIL:
    PWRMGR_Fail(ERR_EFLASH_HW_BROKEN, NULL);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Change the Spansion/WSON 32MB SPI Flash Baud Rate.
//!
//! \fntype Non-Reentrant Function
//!
//! \return err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
err_t ext_flash_set_baud( uint32_t baud )
{
    if (NULL == g_spifd)
    {
        // Flash isn't initialized yet, so just exit without doing anything
        goto FLASH_SET_BAUD_DONE;
    }
    if ( SPI_OK != spiChangeBaud( g_spifd, baud ) )
    {
        return ERR_EFLASH_REINIT;
    }
FLASH_SET_BAUD_DONE:
    return ERR_OK;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Wait for WIP (Write in progress) to complete, and check for errors.
//!
//! \fntype Non-Reentrant Function
//!
//! \return int error code
//! \todo  Add a timeout, so we don't loop forever waiting when we have errors.
//!
///////////////////////////////////////////////////////////////////////////////
static int wip_wait(uint32_t wait_before_polling, uint32_t *pNumPolls)
{
    uint8_t  status;
    int32_t  timeout = WIP_WAIT_TIMEOUT_MS;
    int32_t  num_ms_between_poll_exp = 1;  // use an exponential algo to poll.
    
//#define NUM_POLLS_DBG_INSTR
#ifdef NUM_POLLS_DBG_INSTR
    uint32_t        total_poll_time = 0;
    static uint16_t wip_wait_times[512] = {0};
    static uint16_t count = 0;
    static uint32_t wrap = 0;
#endif
    
    if( wait_before_polling > 0 )
    {
        _time_delay( wait_before_polling ); // don't hog the system by polling, wait for write or erase to finish.
    }
    
    if( pNumPolls )
    {
        *pNumPolls = 0;
    }

    // now start polling, since we're more in the ballpark.
    while( timeout > 0 )
    {
        int err = wson_rdsr1(&status);
        
        if (err != 0)
        {
            PROCESS_NINFO(ERR_EFLASH_READ, NULL);
            return -1;
        }
        if (status & WSON_SR1_P_ERR_MASK)
        {
            PROCESS_NINFO(ERR_EFLASH_WRITE, NULL);
            return -2;
        }
        if (status & WSON_SR1_E_ERR_MASK)
        {
            PROCESS_NINFO(ERR_EFLASH_ERASE, NULL);
            return -3;
        }
        
        if(0 == (status & WSON_SR1_WIP_MASK))
        {
            goto WIP_DONE;
        }

#ifdef NUM_POLLS_DBG_INSTR
        total_poll_time += num_ms_between_poll_exp;
#endif
        _time_delay( num_ms_between_poll_exp );  // when we get here, we are officially polling status.
        timeout -= num_ms_between_poll_exp;
        
        /*
         *   Exponentially increase polling delay, as there is quite a bit of variance in how long we need to poll.
         */
        if( 1 == num_ms_between_poll_exp )
        {
            num_ms_between_poll_exp = 2;
        }
        else if( WIP_WAIT_MAX_SINGLE_POLL <= num_ms_between_poll_exp )
        {
            num_ms_between_poll_exp = WIP_WAIT_MAX_SINGLE_POLL;
        }
        else
        {
            num_ms_between_poll_exp *= num_ms_between_poll_exp;
        }
        
        if( pNumPolls )
        {
            *pNumPolls += 1;
        }
    }

    /*
     *   We timed out before we could read back a good WIP status.
     *     Therefore, we are toast.
     */
    wassert( ERR_OK == PROCESS_NINFO(ERR_EFLASH_TIMEOUT, "wipTO") );

WIP_DONE:

#ifdef NUM_POLLS_DBG_INSTR
    /*
     *    NOTE: The corona_print() below causes flash accesses to spread out and makes it
     *              more difficult to reproduce the bad FIFO issue
     */
    //corona_print("wip_polls=%ld\n",num_polls);

    wip_wait_times[count] = total_poll_time;
    count++;
    if(count >= 512)
    {
        count = 0;
        wrap++;
    }
#endif
    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Read Status Register 1 on Spansion Flash.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Return value of status register.
//!
//! \return int error code
//! \todo  Change this into a generic "read register" command...
//!
///////////////////////////////////////////////////////////////////////////////
static int wson_rdsr1(uint8_t *pStatus)
{
    uint8_t pTXBuf[1];
    uint8_t pRXBuf[1];

    pTXBuf[0] = WSON_RDSR1;

    if (1 != spiWrite(pTXBuf, 1, TRUE, FALSE, g_spifd))
    {
        return -1;
    }

    if (1 != spiRead(pRXBuf, 1, g_spifd))
    {
        return -2;
    }

    if (0 != spiManualDeassertCS(g_spifd))
    {
        return -3;
    }

    *pStatus = pRXBuf[0];
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Read Configuration Register 1 on Spansion Flash.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Return value of status register.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
static int wson_rdcr1(uint8_t *pCR1)
{
    uint8_t pTXBuf[1];
    uint8_t pRXBuf[1];

    pTXBuf[0] = WSON_RDCR;

    if (1 != spiWrite(pTXBuf, 1, TRUE, FALSE, g_spifd))
    {
        return -1;
    }

    if (1 != spiRead(pRXBuf, 1, g_spifd))
    {
        return -2;
    }

    if (0 != spiManualDeassertCS(g_spifd))
    {
        return -3;
    }

    *pCR1 = pRXBuf[0];
    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Returns the sector address for the given arbitrary address.
//!
//! \fntype Non-Reentrant Function
//! \return sector address.
//!
///////////////////////////////////////////////////////////////////////////////
uint32_t get_sector_addr(uint32_t address, uint32_t sectorSz, uint32_t regionStart)
{
    uint32_t remainder = (address - regionStart) % sectorSz;

    return address - remainder;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Erase a sector on the WSON Flash, mutex protect the resource if needed.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Erases sector that the address exists in.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
static int wson_erase_private(uint32_t address, boolean mutex_protection)
{
    uint8_t  cmd;
    uint8_t  tx[SIZE_TX_BUF];
    uint32_t regionStartAddr;
    uint32_t sectorSize;
    uint32_t wip_wait_ms;
    int      err = ERR_OK;
    
    wson_addr_ok(address);
    
    if(mutex_protection)
    {
        wson_lock(MQX_NULL_TASK_ID);
        
        if( persist_app_has_initialized() )
        {
            __SAVED_ACROSS_BOOT.eflash_in_use_addr = address;
            __SAVED_ACROSS_BOOT.eflash_in_use_task_id = _task_get_id();
            PMEM_UPDATE_CKSUM();
        }
    }

#if 0
    if( EXT_FLASH_EVENT_MGR_ADDR == address )
    {
        corona_print("DATA wrap around!\n");
    }
    else if( EXT_FLASH_EVTMGR_POINTERS_ADDR == address )
    {
        corona_print("BLK PTR wrap around!\n");
    }
#endif
    
    corona_print("\tERASE %x\n", address);
    
    wassert(address <= EXT_FLASH_REGION_END);
    
    if (address >= EXT_FLASH_64KB_REGION)
    {
        tx[0] = WSON_4SE;
        regionStartAddr = EXT_FLASH_64KB_REGION;
        sectorSize      = EXT_FLASH_64KB_SZ;
        wip_wait_ms     = WAIT_AFTER_64KB_ERASE_MS;
    }
    else
    {
        tx[0] = WSON_4P4E;
        regionStartAddr = EXT_FLASH_4KB_REGION;
        sectorSize      = EXT_FLASH_4KB_SZ;
        wip_wait_ms     = WAIT_AFTER_4KB_ERASE_MS;
    }

    address = get_sector_addr(address, sectorSize, regionStartAddr);
    copy_addr_to_buf(&(tx[1]), (uint8_t *)&address, sizeof(address));
    if( wson_write_en() )
    {
        err = -4;
        goto wson_erase_done;
    }

    if ( SIZE_TX_BUF != spiWrite(tx, SIZE_TX_BUF, TRUE, TRUE, g_spifd))
    {
        PROCESS_NINFO(ERR_EFLASH_SPI, NULL);
        err = -2;
        goto wson_erase_done;
    }

    err = wip_wait( 0, NULL );
    if (err != 0)
    {
        err = -3;
        goto wson_erase_done;
    }

wson_erase_done:

    if( 0 != err )
    {
        wassert( ERR_OK == PROCESS_NINFO( ERR_EFLASH_ERASE, "tsk %x %i", _task_get_id(), err ) );
    }

    if(mutex_protection)
    {
        wson_unlock(MQX_NULL_TASK_ID);
    }
    
    return err;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Erase a sector on the WSON Flash
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Erases sector that the address exists in.
//!
//! \return int error code
//!
//!*   NOTE:  There is no need to wassert() on the result.  
//!*          That is done under the hood here to reduce need
//!*              to wassert() all over the place in the code, which is expensive.
//!
///////////////////////////////////////////////////////////////////////////////
int wson_erase(uint32_t address)
{
    return wson_erase_private(address, TRUE);
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Enable writing via WREN.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Sends the "write enable" command to the SPI Flash.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
static int wson_write_en(void)
{
    uint8_t cmd;

    cmd = WSON_WREN;

    if (1 != spiWrite(&cmd, 1, TRUE, TRUE, g_spifd))
    {
        PROCESS_NINFO(ERR_EFLASH_SPI, NULL);
        return -1;
    }

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Write data to WSON Flash
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Writes given data to SPI Flash at given address.
//!
//! \return int error code
//!*   NOTE:  There is no need to wassert() on the result.  
//!*          That is done under the hood here to reduce need
//!*              to wassert() all over the place in the code, which is expensive.
//!
///////////////////////////////////////////////////////////////////////////////
int wson_write(uint32_t      address,
               uint8_t       *pData,
               uint32_t      numBytes,
               boolean       eraseSector)
{
    uint8_t   pBuf[SIZE_TX_BUF];  // command + 32-bit address.
    uint32_t  src_bytes = numBytes;
    uint32_t  spi_address = address;
    uint32_t  spi_bytes;
    uint32_t  wrt_count = 0;
    int       err = ERR_OK ;
    
#if TEST_READBACK_ALL_WRITES
    uint8_t  *pTestBufBefore, *pTestBufAfter;
#endif
    
    // DEBUG__  display time it takes for wson_write to execute
    //    TIME_STRUCT wr_start, wr_end, diff_time;
    //    _time_get(&wr_start);
    
    wson_addr_ok(address);

    wson_lock(MQX_NULL_TASK_ID);
    
    if( persist_app_has_initialized() )
    {
        __SAVED_ACROSS_BOOT.eflash_in_use_addr = address;
        __SAVED_ACROSS_BOOT.eflash_in_use_task_id = _task_get_id();
        PMEM_UPDATE_CKSUM();
    }
    
#if TEST_READBACK_ALL_WRITES
    pTestBufBefore = _lwmem_alloc(numBytes);
    wassert( pTestBufBefore );
    memcpy( pTestBufBefore, pData, numBytes );
    
    pTestBufAfter = _lwmem_alloc(numBytes);
    wassert( pTestBufAfter );
#endif

    if (eraseSector)
    {
        err = wson_erase_private(address, FALSE);
        if (err != 0)
        {
            goto wson_write_err;
        }
    }
    
    do
    {
        if( wson_write_en() )
        {
            err = -10;
            goto wson_write_err;
        }
        
        // Adjust the number of bytes to write (1 page at a time)
        if (src_bytes > EXT_FLASH_PAGE_SIZE) 
        {
            spi_bytes = EXT_FLASH_PAGE_SIZE;
            src_bytes -= EXT_FLASH_PAGE_SIZE;
        }
        else 
        {
            spi_bytes = src_bytes;
        }
        
        // Set the write command, the SPI address.
        pBuf[0] = WSON_4PP;
        copy_addr_to_buf(&(pBuf[1]), (uint8_t *)&spi_address, sizeof(uint32_t));
        
        /*
         *   To avoid memcpy'ing to a common buffer, first send the command/address,
         *      then use the client's buffer for sending the actual data after.
         */
        if(SIZE_TX_BUF != spiWrite(pBuf, SIZE_TX_BUF, FALSE, FALSE, g_spifd))
        {
            PROCESS_NINFO(ERR_EFLASH_SPI, NULL);
            err = -12;
            goto wson_write_err;
        }
        
        // Send the user's data to SPI Flash.
        if(spi_bytes != spiWrite(pData, spi_bytes, TRUE, TRUE, g_spifd))
        {
            PROCESS_NINFO(ERR_EFLASH_SPI, NULL);
            err = -14;
            goto wson_write_err;
        }
        
        err = wip_wait( WAIT_AFTER_PAGE_PROGRAM_MS, NULL );
        if (err != 0)
        {
            PROCESS_NINFO(ERR_EFLASH_SPI, NULL);
            goto wson_write_err;
        }
        
        pData       += spi_bytes;
        spi_address += spi_bytes;
        wrt_count   += spi_bytes;
        
    } while (wrt_count < numBytes);

wson_write_err:

    if( 0 != err )
    {
        wassert( ERR_OK == PROCESS_NINFO( ERR_EFLASH_WRITE, "tsk %x %i", _task_get_id(), err ) );
    }
    
#if TEST_READBACK_ALL_WRITES
    
    /*
     *   Make sure what we just wrote will read back as the exact value that was written.
     */
    wassert( 0 == _wson_read_internal(address, pTestBufAfter, numBytes) );
    
    /*
     *   Ignore Sherlock since it uses some crusty bitfield stuff that breaks this logic.
     */
    if( _task_get_id() != _task_get_id_from_name( SHLK_TASK_NAME ) )
    {
        if( 0 != memcmp(pTestBufBefore, pTestBufAfter, numBytes) )
        {
            PROCESS_NINFO(ERR_EFLASH_RD_WR_MISMATCH, "addr:0x%x\tlen:0x%x\ttsk:0x%x", address, numBytes, _task_get_id());
            wassert(0);
        }
    }
    
    wfree( pTestBufBefore );
    wfree( pTestBufAfter );
    
#endif
    
    wson_unlock(MQX_NULL_TASK_ID);
    
// DEBUG
//_time_get (&wr_end);
//_time_diff(&wr_start, &wr_end, &diff_time);
//corona_print("wson_write:%ldms\n", diff_time.MILLISECONDS);
    
    return err;
}

/*
 *   Public API uses mutex.  Internal one does not.
 *   
 *   NOTE:  There is no need to wassert() on the result.  
 *          That is done under the hood here to reduce need
 *              to wassert() all over the place in the code, which is expensive.
 */
int wson_read(uint32_t address, uint8_t *pData, uint32_t numBytes)
{
    int ret;
    
    wson_lock(MQX_NULL_TASK_ID);
    ret = _wson_read_internal(address, pData, numBytes);
    wson_unlock(MQX_NULL_TASK_ID);
    
    return ret;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Read data to WSON Flash
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Reads data from SPI Flash and puts it in the buffer provided.
//!
//! \return int error code
//! \todo Waits need timeouts.
//!
///////////////////////////////////////////////////////////////////////////////
static int _wson_read_internal(uint32_t address, uint8_t *pData, uint32_t numBytes)
{
    int           err = ERR_OK;
    uint32_t      spi_address = address;
    uint32_t      spi_bytes;
    uint32_t      rd_bytes = numBytes;
    uint8_t       pTXBuf[SIZE_TX_BUF];
    uint32_t      rd_count = 0;
    
    wson_addr_ok(address);
    
    // Read up to a PAGE at a time, but no more.
    do {
        
        if (rd_bytes > EXT_FLASH_PAGE_SIZE) 
        {
          spi_bytes = EXT_FLASH_PAGE_SIZE;
          rd_bytes -= EXT_FLASH_PAGE_SIZE;
        }
        else 
        {
          spi_bytes = rd_bytes;
        }
        
        // Set the read command and address
        pTXBuf[0] = WSON_4READ;
        copy_addr_to_buf(&(pTXBuf[1]), (uint8_t *)&spi_address, sizeof(uint32_t));
        
        if (SIZE_TX_BUF != spiWrite(pTXBuf, SIZE_TX_BUF, TRUE, FALSE, g_spifd))
        {
            err = -1;
            goto wson_read_err;
        }
    
        if (spi_bytes != spiRead(pData, spi_bytes, g_spifd))
        {
            err = -2;
            goto wson_read_err;
        }

        if (ERR_OK != spiManualDeassertCS(g_spifd))
        {
            err = -3;
            goto wson_read_err;
        }
        
        pData       += spi_bytes;
        spi_address += spi_bytes;
        rd_count    += spi_bytes;
        
    } while (rd_count < numBytes);
    
wson_read_err:

    if( 0 != err )
    {
        wassert( ERR_OK == PROCESS_NINFO( ERR_EFLASH_READ, "tsk %x %i", _task_get_id(), err ) );
    }
    return err;
}


////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
