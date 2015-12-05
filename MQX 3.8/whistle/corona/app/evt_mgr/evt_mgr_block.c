/*
 * evt_mgr_block.c
 *
 *  Created on: Apr 5, 2013
 *      Author: Ernie
 *      https://whistle.atlassian.net/wiki/display/COR/Event+Manager+Memory+Management+Design
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "evt_mgr_priv.h"
#include "corona_ext_flash_layout.h"
#include "ext_flash.h"
#include "wassert.h"

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Returns 1 if address is in valid event block range, 0 otherwise.
 */
uint_8  _EVTMGR_addr_is_valid(uint_32 addr)
{
    if( (addr >= EXT_FLASH_EVENT_MGR_ADDR ) && (addr <= EXT_FLASH_EVENT_MGR_LAST_ADDR) )
    {
        return 1;
    }
    return 0;
}

/*
 *   Flushes an event block to SPI Flash and updates the relevant pointers.
 *     NOTE:  Since this function is writing/erasing EVTMGR sectors,
 *            mutual exclusion protection is mandatory via a lock.
 */
err_t _EVTMGR_wr_evt_blk(evt_blk_ptr_ptr_t pBlkPtr, uint_8 *pPayload)
{
    uint_16 bytes2write = pBlkPtr->len;
    
    if(bytes2write == 0)
    {
        return ERR_OK;
    }
    
    corona_print("EM wr %x (%u)\n", _EVTMGR_get_wr_addr(), bytes2write);

    _watchdog_start(65*1000);
    _EVTMGR_lock();
    _EVTMGR_atomic_vote( EVTMGR_ATOMIC_VOTE_ON );
    
    pBlkPtr->addr = _EVTMGR_get_wr_addr();
    
    /*
     *    Write the data to SPI Flash.
     */
    do
    {
        uint_16 num_bytes = (bytes2write < EXT_FLASH_PAGE_SIZE) ? bytes2write : EXT_FLASH_PAGE_SIZE;
        
        _EVTMGR_write_page( _EVTMGR_get_wr_addr(), pPayload, num_bytes );

        bytes2write -= num_bytes;
        pPayload += num_bytes;
        
        _EVTMGR_inc_wr_addr();
        
    }while(bytes2write);
    
    /*
     *   Add the block pointer corresponding to the data we've written.
     */
    _EVTMGR_add_blk_ptr(pBlkPtr);
    _EVTMGR_atomic_vote( EVTMGR_ATOMIC_VOTE_OFF );
    _EVTMGR_unlock();
    return ERR_OK;
}
