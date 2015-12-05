/*
 * evt_mgr_util.c
 *
 *  Created on: Apr 5, 2013
 *      Author: Ernie
 *      https://whistle.atlassian.net/wiki/display/COR/Event+Manager+Memory+Management+Design
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "wassert.h"
#include "evt_mgr_priv.h"
#include "corona_ext_flash_layout.h"
#include "wmp_datadump_encode.h"
#include "psptypes.h"
#include "ext_flash.h"
#include "pwr_mgr.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define EVTMGR_INFO_TOP_LEVEL_MAGIC1    0x01234567
#define EVTMGR_INFO_TOP_LEVEL_MAGIC2    0xFEDCBA97
#define NUM_PAGE_BITFIELDS              ( sizeof(pg_usd_bf_t) / sizeof(uint_32) )
#define ALL_PAGES_FULL                  ( (NUM_PAGE_BITFIELDS * 32) - 1 )
#define EVTMGR_MIN_MS_UNTIL_REBOOT		5000    // we require at least this much up time to perform critical EVTMGR operations.

////////////////////////////////////////////////////////////////////////////////
// Data Structures
////////////////////////////////////////////////////////////////////////////////

/*
 *   Bitfield for which pages within the EVTMGR info sector is used.
 *   There are 256 pages in 1 sector, so we need 256 bits, or 8 * u32's.
 *   NOTE:  first page of every sector is always just for the header.
 */
typedef struct pages_used_bitfield_type
{
    uint_32 pgs_0;
    uint_32 pgs_1;
    uint_32 pgs_2;
    uint_32 pgs_3;
    uint_32 pgs_4;
    uint_32 pgs_5;
    uint_32 pgs_6;
    uint_32 pgs_7;
}pg_usd_bf_t;

/*
 *   Header for EVTMGR info sector.
 *   This header tells us which page we should write the evtmgr info struct to.
 *   
 *   The algo will try to use each page in the sector, to reduce the number of 
 *     erases we do to the EVTMGR info sector.  Erases take ~200ms in the worst case,
 *     and we are also vulnerable to harsh reboots during this time, which would reset
 *     all the data, so...
 */
typedef struct evtmgr_info_header_type
{
    uint_32     magic1;      // 1st check to make sure EVTMGR info sector is OK.
    uint_32     reserved[2]; // who knows
    pg_usd_bf_t used;        // which pages are used.  NOTE:  do NOT move this!
    uint_32     magic2;      // 2nd check to make sure EVTMGR info sector is OK.

}evtmgr_info_hdr_t;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Do a counting vote for EVTMGR atomic operations.
 *   
 *   This utility will help us manage nested votes within EVTMGR,
 *     so that we only vote for ON/OFF when we really want to.
 */
void _EVTMGR_atomic_vote( evtmgr_atomic_vote_t vote )
{
	static int16_t count = 0;
	
	wassert( count >= 0 );            // make sure no clients of this API are being naughty.
	
	
	wassert( _EVTMGR_is_locked() );   // make sure EVTMGR is actually locked!
	
	switch (vote)
	{
		case EVTMGR_ATOMIC_VOTE_ON:
			if( 0 == count++ )
			{
				/*
				 *   EVTMGR is sensitive rebooting, especially for the ERASE case.
				 *     Because of this, we go a little further to ensure we are not about to reset
				 *        the system when we do critical EVTMGR operations.
				 */
				if( PWRMGR_is_rebooting() )
				{
					/*
					 *   Its OK to be rebooting and still flushing stuff to EVTMGR,
					 *     however we also need to make sure that we aren't too close to a
					 *       a harsh reboot.
					 */
					if( PWRMGR_ms_til_reboot() < EVTMGR_MIN_MS_UNTIL_REBOOT )
					{
						_task_block();
					}
				}
				PWRMGR_VoteForON( PWRMGR_EVTMGR_ATOMIC_VOTE );
				PWRMGR_VoteForAwake( PWRMGR_EVTMGR_ATOMIC_VOTE );
				
		        /*
		         *   Sucks to have to stop task preemption and hog the SPI Flash...   BUT
		         *     We need to make sure the blk ptrs get down to SPI Flash properly,
		         *     without interference from other tasks causing ARM exceptions and/or wassert(0)'s, etc.
		         *     Those types of problems lead to things like this: FW-1071.
		         */
				wson_lock(_task_get_id());
				_task_stop_preemption();
			}
			break;
			
		case EVTMGR_ATOMIC_VOTE_OFF:
			if( --count == 0 )
			{
			    wson_unlock(_task_get_id());
				PWRMGR_VoteForOFF( PWRMGR_EVTMGR_ATOMIC_VOTE );
				_task_start_preemption();
				PWRMGR_VoteForSleep( PWRMGR_EVTMGR_ATOMIC_VOTE );
			}
			break;
			
		default:
			break;
	}
}

/*
 *   Update the "pages used" bitfield in the header.
 */
static void _EVTMGR_update_pgs_used(uint_32 *pUsed, uint_32 num_pgs_used)
{
    
    uint_32 count = 0;
    
    memset(pUsed, 0xFFFFFFFF, sizeof(pg_usd_bf_t));
    pUsed[0] = 0xFFFFFFFE; // header is used always.
    
    /*
     *   First clear whole pages if number of pages to clear > 32.
     */
    while( count < (num_pgs_used / 32) )
    {
        pUsed[count] = 0;
        count++;
    }
    
    /*
     *   Now we should be left with the modulus, which we can use on the final page.
     */
    pUsed += count;
    count = 0;
    
    while( count < (num_pgs_used % 32) )
    {
        *pUsed &= ~(0x00000001 << count);
        count++;
    }
}

/*
 *   Grabs the index corresponding to which page we are on based on the bitfield.
 */
static uint_32 _EVTMGR_get_last_used_page_idx(uint_32 *pBitfield)
{
    uint_32 unused_idx = 1;
    uint_32 pg_bf_count = 0;
    
    while(pg_bf_count < NUM_PAGE_BITFIELDS)
    {
        uint_32 bit = 0;
        
        if(0 == pg_bf_count)
        {
            bit = 1; // skip the header.
        }
        
        while(bit < 32)
        {
            if( *pBitfield & (0x00000001 << bit) )  // unused = 1, used = 0
            {
                return unused_idx;
            }
            bit++;
            unused_idx++;
        }
        
        pBitfield++;
        pg_bf_count++;
    }
    
    return ALL_PAGES_FULL;
}

/*
 *   Calculates the CRC and writes the info the address passed in.
 */
static void _EVTMGR_write_info( uint_32 addr, evtmgr_info_t *pInfo )
{
    pInfo->crc = _EVTMGR_crc( pInfo );
    wson_write(addr, (uint_8 *)pInfo, sizeof(evtmgr_info_t), 0);
}

/*
 *   The EVTMGR info sector has wrapped, or is initializing for the first time.
 */
static void _EVTMGR_info_wrap( evtmgr_info_t *pInfo, evtmgr_info_hdr_t *pHdr )
{
	uint_32 idx;
	
    memset(pHdr, 0xFFFFFFFF, sizeof(evtmgr_info_hdr_t));
    pHdr->magic1 = EVTMGR_INFO_TOP_LEVEL_MAGIC1;
    pHdr->magic2 = EVTMGR_INFO_TOP_LEVEL_MAGIC2;
    pHdr->used.pgs_0 = 0xFFFFFFFC; // the header itself is used.
    
    idx = _EVTMGR_get_last_used_page_idx((uint_32 *)&(pHdr->used));
    
    /*
     *   NOTE:  We have to guarantee to the largest extent possible, that we do not reboot in between
     *           erasing FLASH, writing the header, and writing the new info page.
     */
    
    _EVTMGR_atomic_vote( EVTMGR_ATOMIC_VOTE_ON );
    
    /*
     *   Write the header and erase the sector.
     */
    wson_write(EXT_FLASH_EVTMGR_INFO_ADDR, (uint_8 *)pHdr, sizeof(evtmgr_info_hdr_t), 1);
    
    /*
     *   Write out the info to the first page past the header.
     */
    _EVTMGR_write_info( EXT_FLASH_EVTMGR_INFO_ADDR + (idx * EXT_FLASH_PAGE_SIZE), pInfo );
    
    WMP_post_breadcrumb( BreadCrumbEvent_EVTMGR_WRAP_INFO );
    
    _EVTMGR_atomic_vote( EVTMGR_ATOMIC_VOTE_OFF );
}

/*
 *   Return 0 if the address's page is all FF's for at least size sizeof(evtmgr_info_t).
 *     This is a safety check to make sure we're OK writing to this page freshly.
 */
static uint_8 _EVTMGR_pg_erased( uint_32 address )
{
    evtmgr_info_t dummy1, dummy2;
    
    memset(&dummy1, 0xFFFFFFFF, sizeof(dummy1));
    wson_read(address, (uint_8 *)&dummy2, sizeof(dummy2));
    
    return ( 0 == memcmp(&dummy1, &dummy2, sizeof(dummy1)) )?1:0;
}

/*
 *   Flushes the latest Event Manager information to the SPI FLASH.
 */
void _EVTMGR_flush_info( evtmgr_info_t *pInfo )
{
    evtmgr_info_hdr_t hdr;
    uint_32 page_idx;
    uint_32 address;

    _EVTMGR_atomic_vote( EVTMGR_ATOMIC_VOTE_ON );
    
#if 0
    /*
     *   Do NOT push this.  This is just for stress testing purposes!
     */
    _EVTMGR_info_wrap(pInfo, &hdr);
    goto done_flush_info;
    
#endif
    
    /*
     *   Read the top level header.
     */
    wson_read(EXT_FLASH_EVTMGR_INFO_ADDR, (uint8_t *)&hdr, sizeof(hdr));
    
    /*
     *   Is the top level header valid.
     */
    if( (EVTMGR_INFO_TOP_LEVEL_MAGIC1 != hdr.magic1) || (EVTMGR_INFO_TOP_LEVEL_MAGIC2 != hdr.magic2) )
    {
        /*
         *   This is either the first time we've ever used this sector, or it became corrupted.
         *     Therefore, we want to erase the sector and write this info to the first page.
         */
        _EVTMGR_info_wrap(pInfo, &hdr);
        goto done_flush_info;
    }
    else
    {
        /*
         *   The top level header is valid, so figure out where the next page to write to is.
         *     We want to come here more often, since it is more optimal/safe than the above.
         */
        page_idx = _EVTMGR_get_last_used_page_idx((uint_32 *)&(hdr.used));
        
        /*
         *   As another safety check, we want to make sure the page we are going to write to
         *     is fully erased.
         */
        do
        {
            page_idx += 1;
            
            if( page_idx >= ALL_PAGES_FULL )
            {
                /*
                 *   If we've used all the pages in our sector, go ahead and start from scratch at the top.
                 */
                _EVTMGR_info_wrap(pInfo, &hdr);
                goto done_flush_info;
            }
            
            address = EXT_FLASH_EVTMGR_INFO_ADDR + (page_idx * EXT_FLASH_PAGE_SIZE);
            
            _EVTMGR_update_pgs_used((uint_32 *)&(hdr.used), page_idx);
            
        }while( 0 == _EVTMGR_pg_erased(address) );
        
        /*
         *   Write the updated header.
         */
        wson_write(EXT_FLASH_EVTMGR_INFO_ADDR, (uint_8 *)&hdr, sizeof(evtmgr_info_hdr_t), 0);
        
        /*
         *   Sanity test the target address here.
         */
        wassert( (address - EXT_FLASH_EVTMGR_INFO_ADDR) < EXT_FLASH_64KB_SZ );
        
        /*
         *   Write the info to the appropriate address.
         */
        _EVTMGR_write_info( address, pInfo );
    }
    
done_flush_info:
	_EVTMGR_atomic_vote( EVTMGR_ATOMIC_VOTE_OFF );
}

/*
 *   Read EVTMGR info for the caller.
 */
void _EVTMGR_read_info( evtmgr_info_t *pInfo )
{
    evtmgr_info_hdr_t hdr;
    uint_32 page_idx;
    
    /*
     *   Read the top level header.
     */
    wson_read(EXT_FLASH_EVTMGR_INFO_ADDR, (uint8_t *)&hdr, sizeof(hdr));
    
    if( (EVTMGR_INFO_TOP_LEVEL_MAGIC1 != hdr.magic1) || (EVTMGR_INFO_TOP_LEVEL_MAGIC2 != hdr.magic2) )
    {
        /*
         *   This is either the first time we've ever used this sector, or it became corrupted.
         *     Therefore, we want to return a bogus value to the caller for their magic number.
         */
        pInfo->signature = 0xFACEFACE;
    }
    else
    {
        page_idx = _EVTMGR_get_last_used_page_idx((uint_32 *)&(hdr.used));
        wson_read(EXT_FLASH_EVTMGR_INFO_ADDR + (page_idx * EXT_FLASH_PAGE_SIZE), (uint_8 *)pInfo, sizeof(evtmgr_info_t));
    }
}

/*
 *   Return TRUE if the address is right on a sector boundary.
 */
uint_8 _EVTMGR_is_sector_boundary(uint_32 addr)
{
    /*
     *   Be sure that this address is indeed in the 64KB region.
     */
    wassert(addr >= EXT_FLASH_64KB_REGION);
    
    if( (addr % EXT_FLASH_64KB_SZ) == 0 )
    {
        return 1;
    }
    return 0;
}

/*
 *   Returns the sector boundary the address is on, plus 64KB.
 */
uint32_t _EVTMGR_next_sector(uint32_t address)
{
    uint32_t sector_addr = get_sector_addr(address, EXT_FLASH_64KB_SZ, EXT_FLASH_64KB_REGION);
    
    if( (sector_addr + EXT_FLASH_64KB_SZ) >= EXT_FLASH_EVENT_MGR_LAST_ADDR )
    {
        return EXT_FLASH_EVENT_MGR_ADDR;  // we've reached the end of event data region.  wrap around
    }
    else
    {
        return sector_addr + EXT_FLASH_64KB_SZ;
    }
}

/*
 *   Returns the sector boundary address for block pointers.
 */
uint32_t _EVTMGR_next_evt_blk_sector(uint32_t address)
{
    uint32_t sector_addr = get_sector_addr(address, EXT_FLASH_64KB_SZ, EXT_FLASH_64KB_REGION);
    
    if( (sector_addr + EXT_FLASH_64KB_SZ) >= EXT_FLASH_EVTMGR_POINTERS_LAST_ADDR )
    {
        return EXT_FLASH_EVTMGR_POINTERS_ADDR;
    }
    else
    {
        return sector_addr + EXT_FLASH_64KB_SZ;
    }
}

/*
 *   WRITE a single 256-byte (or less) page to SPI FLash.
 *   WRITE's must be on a PAGE boundary.
 */
void _EVTMGR_write_page(uint_32 addr, uint_8 *pPage, uint_16 len)
{
    _EVTMGR_atomic_vote( EVTMGR_ATOMIC_VOTE_ON );
    
    _EVTMGR_prep_mem( addr );
    
#if 0
    __TEST_check_for_corruption(pPage, addr, len);
#endif
    
    wson_write( addr, pPage, len, 0 );
    
    _EVTMGR_atomic_vote( EVTMGR_ATOMIC_VOTE_OFF );
}

/*
 *   Prepare SPI Flash memory.
 *      ERASE if we are on a sector boundary.  
 *      WRITE's don't care if they pass up a READ.
 *      
 *   NOTE:  This function MUST be called within the context of EVTMGR_ATOMIC_VOTE.
 */
void _EVTMGR_prep_mem( uint_32 addr )
{
    /*
     *   If we are on a sector boundary, go ahead and ERASE to prepare for the WRITE.
     */
    if( _EVTMGR_is_sector_boundary( addr ) )
    {
        // Since the write isn't going to cause any data loss, then erase this sector.
        wson_erase( addr );
        _EVTMGR_update_rd_blk_ptr( addr );
    }
}

/*
 *   Returns the next page address in EVTMGR's circular buffer.
 */
uint_32 _EVTMGR_update_page_addr(uint_32 addr)
{
    if( (addr + EXT_FLASH_PAGE_SIZE) >= EXT_FLASH_EVENT_MGR_LAST_ADDR)
    {
        /*
         *    This is the "circular" nature of writing/reading  EVTMGR data to SPI FLASH,
         *      loop back around to (James Earl Jones voice) "The Beginning".
         */
        addr = EXT_FLASH_EVENT_MGR_ADDR;
    }
    else
    {
        addr += EXT_FLASH_PAGE_SIZE;
    }
    
    return addr;
}

/*
 *   Returns the next page address in EVTMGR evt blk ptr's circular buffer.
 */
uint_32 _EVTMGR_update_evt_blk_ptr_addr(uint_32 addr)
{
    if( (addr + EXT_FLASH_PAGE_SIZE) >= EXT_FLASH_EVTMGR_POINTERS_LAST_ADDR)
    {
        /*
         *    This is the "circular" nature of writing/reading  EVTMGR data to SPI FLASH,
         *      loop back around to (James Earl Jones voice) "The Beginning".
         */
        addr = EXT_FLASH_EVTMGR_POINTERS_ADDR;
    }
    else
    {
        addr += EXT_FLASH_PAGE_SIZE;
    }
    
    return addr;
}
