/*
 * evt_mgr_block_ptr.c
 * This file is mostly responsible for keeping track of where events are in SPI Flash.
 *
 *  Created on: Apr 5, 2013
 *      Author: Ernie
 *      https://whistle.atlassian.net/wiki/display/COR/Event+Manager+Memory+Management+Design
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "evt_mgr_priv.h"
#include "ext_flash.h"
#include "corona_ext_flash_layout.h"
#include "psptypes.h"
#include "cormem.h"
#include "app_errors.h"
#include "cfg_mgr.h"
#include "crc_util.h"
#include "wassert.h"
#include "wmp_datadump_encode.h"
#include "persist_var.h"
#include <mqx.h>

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define NUM_EVT_BLK_PTRS_IN_BUF      ( EXT_FLASH_PAGE_SIZE / sizeof(evt_blk_ptr_t) )

////////////////////////////////////////////////////////////////////////////////
// Structs
////////////////////////////////////////////////////////////////////////////////

/*
 *   This is a structure we keep in RAM, to hold the event block pointers,
 *   until we are ready to flush.
 */
//#pragma pack(push, 1)
typedef struct evtmgr_block_pointer_buffer_type
{
    uint_16       idx;
    evt_blk_ptr_t ptrs[NUM_EVT_BLK_PTRS_IN_BUF];
    
}evtmgr_blk_ptr_buf_t;
//#pragma pack(pop)

/*
 *   This is the structure we can use for caching a page of event block pointers
 */
//#pragma pack(push, 1)
typedef struct evtmgr_block_pointer_cache_type
{
    uint_8            num_ptrs;
    //uint_8            cache_in_use;  // TODO - Do we need some type of protection to make sure 2 people don't try and use cache?
    evt_blk_ptr_ptr_t blk_ptr;
}evtmgr_blk_ptr_cache_t, *evtmgr_blk_ptr_ptr_cache_t;
//#pragma pack(pop)

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static uint_8  _EVTMGR_evt_blk_is_obsolete(evt_blk_ptr_ptr_t pEvtBlkPtr, uint_32 obsolete_addr);

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
evtmgr_info_t                 g_Info;     
static evtmgr_blk_ptr_buf_t   g_BlkPtrBuf;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Get the event block pointer at the given index with the cache.
 */
evt_blk_ptr_ptr_t _EVTMGR_get_blk_ptr(uint32_t **pCacheHandle, uint_16 idx)
{
    evtmgr_blk_ptr_ptr_cache_t *ppBlkPtrCache = (evtmgr_blk_ptr_ptr_cache_t *)pCacheHandle;
    
    wassert(*ppBlkPtrCache != NULL);
    
    if(idx > (*ppBlkPtrCache)->num_ptrs)
    {
        return NULL;
    }
    
    if( _EVTMGR_addr_is_valid( (*ppBlkPtrCache)->blk_ptr[idx].addr ) )
    {
        if( (*ppBlkPtrCache)->blk_ptr[idx].len > 0 )
        {
            return &((*ppBlkPtrCache)->blk_ptr[idx]);
        }
        else
        {
            /*
             *  COR-523: If we get here, the EVTMGR is probably corrupted somehow, so reset data.
             */
            EVTMGR_reset();
            PROCESS_NINFO( ERR_EVTMGR_ZERO_LENGTH_PAYLOAD, "0 len!" );
            wassert(0);
        }
    }

    return NULL;
}

/*
 *   Get the number of valid pointers currently in the evt blk ptr cache.
 */
static uint_32 _EVTMGR_get_num_ptrs_in_cache(evtmgr_blk_ptr_ptr_cache_t pBlkPtrCache)
{
    uint_32 num_ptrs = 0;
    
    wassert(pBlkPtrCache != NULL);
    
    while( _EVTMGR_addr_is_valid( pBlkPtrCache->blk_ptr[num_ptrs].addr ) )
    {   
        /*
         *   Make sure the length field passes basic sanity screenings.
         */
        if( 0 == pBlkPtrCache->blk_ptr[num_ptrs].len )
        {
            return 0;  // invalid cache if any blk ptrs have a zero length.
        }
        else if( MAX_EVT_BLK_LEN < pBlkPtrCache->blk_ptr[num_ptrs].len)
        {
            return 0;  // invalid cache if any blk ptrs have a bogus length field.
        }
        
        num_ptrs++;
        if( num_ptrs == NUM_EVT_BLK_PTRS_IN_BUF )
        {
            break; // can't be any bigger than a single page.
        }
    }
    
    return num_ptrs;
}

/*
 *   Loads a 256-byte page worth of event block pointers in memory.
 */
err_t _EVTMGR_load_evt_blk_ptrs( uint32_t **pCacheHandle, uint_32 addr, uint_16 *pNumPtrs )
{
    evtmgr_blk_ptr_ptr_cache_t *ppBlkPtrCache = (evtmgr_blk_ptr_ptr_cache_t *)pCacheHandle;
    
    *pNumPtrs = 0;
    
    /*
     *   Allocate the memory needed for 1 page of data.
     */
    *ppBlkPtrCache = walloc( sizeof(evtmgr_blk_ptr_cache_t) );
    (*ppBlkPtrCache)->blk_ptr = walloc( EXT_FLASH_PAGE_SIZE );
    
    if( (addr < EXT_FLASH_EVTMGR_POINTERS_ADDR) || (addr >= EXT_FLASH_EVTMGR_POINTERS_LAST_ADDR) )
    {
        /*
         *   bad address --  this shouldn't happen unless things are corrupted.
         */
        EVTMGR_reset();
        persist_set_untimely_crumb( BreadCrumbEvent_EVTMGR_INVALID_ADDRESS );
        wassert(0);
    }
    
    /*
     *   Read the event block pointers into that memory.
     */
    wson_read(addr, (uint8_t *)(*ppBlkPtrCache)->blk_ptr, EXT_FLASH_PAGE_SIZE);
    
    /*
     *   Calculate the actual number of event block pointers here.
     */
    (*ppBlkPtrCache)->num_ptrs = _EVTMGR_get_num_ptrs_in_cache(*ppBlkPtrCache);
    if( 0 == (*ppBlkPtrCache)->num_ptrs )
    {
        wfree( (*ppBlkPtrCache)->blk_ptr );
        wfree( (*ppBlkPtrCache) );
        
        /*
         *   This should not happen, unless we have a corrupt EVTMGR, or we have
         *     JUST booted up fresh for the first time, and immediately started a dump,
         *       before any flushes occured.
         */
        if( _EVTMGR_has_data_to_send() )
        {
            EVTMGR_reset();
            persist_set_untimely_crumb( BreadCrumbEvent_EVTMGR_NO_EVT_BLK_PTRS );
            wassert(0);
        }
        else
        {
        	return ERR_EVTMGR_NO_EVT_BLK_PTRS;
        }
    }
    
    *pNumPtrs = (*ppBlkPtrCache)->num_ptrs;
    
    return ERR_OK;
}

/*
 *   Frees the event block pointers' cache.
 */
err_t _EVTMGR_unload_evt_blk_ptrs( uint32_t **pCacheHandle )
{
    evtmgr_blk_ptr_ptr_cache_t *ppBlkPtrCache = (evtmgr_blk_ptr_ptr_cache_t *)pCacheHandle;
    
    wassert( (*ppBlkPtrCache)->blk_ptr != NULL );
    wfree( (*ppBlkPtrCache)->blk_ptr );
    wfree( *ppBlkPtrCache );
    return ERR_OK;
}

/*
 *   Calculate the CRC of EVTMGR info data structure.
 */
uint_32 _EVTMGR_crc( evtmgr_info_t *pInfo )
{
    return UTIL_crc32_calc( (uint_8 *)pInfo, 
                            (uint_32)( (uint_32)&(pInfo->crc) - (uint_32)pInfo ) );
}

/*
 *   Check the EVTMGR CRC and return 0 if it fails, non-zero otherwise.
 */
uint_8 _EVTMGR_check_crc( evtmgr_info_t *pInfo )
{
    return ( _EVTMGR_crc(pInfo) == pInfo->crc )? 1:0;
}

void _EVTMGR_init_blk_ptr_buf(void)
{
    g_BlkPtrBuf.idx = 0;
    memset( g_BlkPtrBuf.ptrs, 0xFF, sizeof(g_BlkPtrBuf.ptrs) );
}

/*
 *   Get total amount of data EVTMGR currently has available for dumping.
 */
uint_32 _EVTMGR_total_data_avail(void)
{
    evt_blk_ptr_ptr_t pEvtBlkPtr;
    uint_16 num_evt_blk_ptrs;
    uint_32 *pCache;
    uint_32 retVal;
    err_t err;
    
    err = _EVTMGR_load_evt_blk_ptrs(&pCache, g_Info.evt_blk_rd_addr, &num_evt_blk_ptrs);
    
    if( ERR_EVTMGR_NO_EVT_BLK_PTRS == err )
    {
        return 0;
    }
    
    wassert( ERR_OK == err );
    
    pEvtBlkPtr = _EVTMGR_get_blk_ptr(&pCache, num_evt_blk_ptrs-1);
    corona_print("\tDATA wr %x rd %x", g_Info.write_addr, pEvtBlkPtr->addr);

    if(g_Info.write_addr >= pEvtBlkPtr->addr)
    {
        retVal = g_Info.write_addr - pEvtBlkPtr->addr;
    }
    else
    {
        retVal = ( EXT_FLASH_EVENT_MGR_LAST_ADDR - pEvtBlkPtr->addr ) + 
                 ( g_Info.write_addr - EXT_FLASH_EVENT_MGR_ADDR);
    }
    corona_print("\t(%u mem span)\n", retVal);
    wassert( ERR_OK == _EVTMGR_unload_evt_blk_ptrs(&pCache) );
    return retVal;
}

/*
 *   Determine if there is more data to be consumed.
 */
uint_8 _EVTMGR_has_data_to_send(void)
{
    return (g_Info.evt_blk_wr_addr != g_Info.evt_blk_rd_addr) ? 1 : 0;
}

/*
 *   Gets the latest information from FLASH in case booting up cold.
 *     Initialize it, if it is totally new.
 *
 */
void _EVTMGR_init_info(uint_8 force_reset)
{
    uint8_t  crc_ok              = 1;
    uint8_t  magic_sig_ok        = 1;
    uint8_t  starting_off_fresh  = 0;
    uint32_t old_magic_sig       = 0;
    
    if( !force_reset )
    {
        _EVTMGR_read_info( &g_Info );
        
        if( EVTMGR_MAGIC_SIGNATURE != g_Info.signature )
        {
            magic_sig_ok = 0;
            old_magic_sig = g_Info.signature;
        }
        else
        {
            crc_ok = _EVTMGR_check_crc( &g_Info );
        }
    }
    
    if( ( !magic_sig_ok ) || (force_reset) || (!crc_ok) )
    {
        // Assume this is the first time we've ever tried using this sector (or sector corrupted).
        g_Info.signature       = EVTMGR_MAGIC_SIGNATURE;
        g_Info.write_addr      = EXT_FLASH_EVENT_MGR_ADDR;  
        g_Info.evt_blk_rd_addr = EXT_FLASH_EVTMGR_POINTERS_ADDR;
        g_Info.evt_blk_wr_addr = EXT_FLASH_EVTMGR_POINTERS_ADDR;
        
        starting_off_fresh = 1;
    }
    else
    {
        /*
         *   To handle the case where we crashed or somehow lost power before flushing event block pointer data,
         *      we should go ahead and increment the write address to the next sector boundary.  
         *      This will guarantee we don't start writing into a sector of SPI Flash we 
         *      _think_ is ERASED, when it is in fact not.
         */
        g_Info.write_addr = _EVTMGR_next_sector(g_Info.write_addr);
    }
    
    _EVTMGR_flush_info(&g_Info);
    _EVTMGR_init_blk_ptr_buf();
    
    /*
     *   Mark the moment in time when we (re)-initialized EVTMGR.
     */
    if( starting_off_fresh )
    {
        /*
         *   We want to be doubly sure we get this crumb, so use two different tactics to secure it.
         */
        WMP_post_breadcrumb( BreadCrumbEvent_EVTMGR_INIT );
        persist_set_untimely_crumb(BreadCrumbEvent_EVTMGR_INIT);
        
#if 0   // Useful for testing.
        corona_print("ON VOTES: %x\n", __SAVED_ACROSS_BOOT.on_power_votes);
        _time_delay( 5000 );
        PWRMGR_Fail();
#endif
    }
    
    /*
     *   Record the reason for the (re)-initialization of EVTMGR.
     */
    if( !magic_sig_ok )
    {
        PROCESS_NINFO( ERR_EVTMGR_MAGIC_SIG_MISMATCH, "old %x\tnew %x", old_magic_sig, g_Info.signature );
    }
    else if( !crc_ok )
    {
        PROCESS_NINFO( ERR_EVTMGR_CRC_FAILED, "old %x\tnew %x", old_magic_sig, g_Info.signature );
    }
}

/*
 *   Is this event block ptr valid?
 */
uint_8  _EVTMGR_blk_ptr_is_valid(uint_32 addr)
{
    return ( (addr < EXT_FLASH_EVTMGR_POINTERS_ADDR) || (addr > EXT_FLASH_EVTMGR_POINTERS_LAST_ADDR) )? 0:1;
}

/*
 *   Returns the next place we should write to SPI Flash.
 */
uint_32 _EVTMGR_get_wr_addr(void)
{
    if( ! _EVTMGR_addr_is_valid (g_Info.write_addr) )
    {
        // Zero tolerance policy for a corrupted EVTMGR.
        EVTMGR_reset();
        wassert( ERR_OK == PROCESS_NINFO(ERR_EVTMGR_INVALID_R_BLK, NULL) );
    }
    return g_Info.write_addr;
}

/*
 *   Get the current evt blk rd pointer address.
 */
uint_32 _EVTMGR_get_evt_blk_rd_addr(void)
{
    if ( ! _EVTMGR_blk_ptr_is_valid(g_Info.evt_blk_rd_addr) )
    {
        // Zero tolerance policy for a corrupted EVTMGR.
        EVTMGR_reset();
        wassert( ERR_OK == PROCESS_NINFO(ERR_EVTMGR_INVALID_R_BLK_PTR, NULL) );
    }
    return g_Info.evt_blk_rd_addr;
}

/*
 *   Set the evt blk rd pointer address.
 */
void _EVTMGR_set_evt_blk_rd_addr(uint_32 addr)
{
    if ( _EVTMGR_blk_ptr_is_valid(addr) )
    {
        g_Info.evt_blk_rd_addr = addr;
    }
    else
    {
        // Zero tolerance policy for a corrupted EVTMGR.
        EVTMGR_reset();
        wassert( ERR_OK == PROCESS_NINFO(ERR_EVTMGR_INVALID_W_BLK_PTR, NULL) );
    }
}

/*
 *   Get the current evt blk wr pointer address.
 */
uint_32 _EVTMGR_get_evt_blk_wr_addr(void)
{
    if ( ! _EVTMGR_blk_ptr_is_valid(g_Info.evt_blk_wr_addr) )
    {
        // Zero tolerance policy for a corrupted EVTMGR.
        EVTMGR_reset();
        wassert( ERR_OK == PROCESS_NINFO(ERR_EVTMGR_INVALID_R_BLK_PTR, NULL) );
    }
    return g_Info.evt_blk_wr_addr;
}

/*
 *   Return TRUE if the passed in event block pointer, points to an event block that has been impacted
 *     by the (ERASE'd) obsolete addr sector.
 */
static uint_8 _EVTMGR_evt_blk_is_obsolete(evt_blk_ptr_ptr_t pEvtBlkPtr, uint_32 obsolete_addr)
{
    uint_32 evt_blk_sector;
    
    /*
     *   The obsolete addr passed in must be a sector a boundary, since it was erased.
     */
    wassert( obsolete_addr == get_sector_addr( obsolete_addr, EXT_FLASH_64KB_SZ, EXT_FLASH_64KB_REGION) );
    
    evt_blk_sector = get_sector_addr( pEvtBlkPtr->addr, EXT_FLASH_64KB_SZ, EXT_FLASH_64KB_REGION );
    
    /*
     *   If the sectors match each other, then this is a piece of cake.
     */
    if( evt_blk_sector == obsolete_addr )
    {
        return 1;
    }
    
    /*
     *   Check to make sure that the event block doesn't _BLEED INTO_ the impacted region.
     */
    if( (pEvtBlkPtr->addr + pEvtBlkPtr->len) >= (evt_blk_sector + EXT_FLASH_64KB_SZ) )
    {
        
        /*
         *   If we are here, that means the event block's payload crosses a sector boundary.
         *   Let's see if the sector it crosses into was impacted.
         */
        evt_blk_sector += EXT_FLASH_64KB_SZ;
        
        /*
         *   Handle wrap around to the top case.
         */
        if( evt_blk_sector > EXT_FLASH_EVENT_MGR_LAST_ADDR )
        {
            evt_blk_sector = EXT_FLASH_EVENT_MGR_ADDR;
        }
        
        if( evt_blk_sector == obsolete_addr )
        {
            /*
             *   The event block's payload crosses a boundary until an obsolete sector.
             *   Therefore, the event block pointer is obsolete, and should be removed.
             */
            return 1;
        }
    }
    
    return 0;
}

/*
 *   Update the event block pointer_pointers that point to this now obsolete place in memory (because we ERASE'd it).
 */
err_t _EVTMGR_update_rd_blk_ptr(uint_32 obsolete_addr)
{   
    uint_32 evt_blk_rd_addr = _EVTMGR_get_evt_blk_rd_addr();
    
    if( (obsolete_addr < EXT_FLASH_EVENT_MGR_ADDR) || (obsolete_addr > EXT_FLASH_EVENT_MGR_LAST_ADDR) )
    {
        return ERR_OK; // Only need to update read block event pointer pointers if we are in the DATA region.
    }
    
    /*
     * If the pointers are equal, then we have consumed all the data we could,
     *   or there is no data to consume.
     */
    if(evt_blk_rd_addr == _EVTMGR_get_evt_blk_wr_addr())
    {
        return ERR_OK;
    }
    
    /*
     *    Iterate over the event blocks that the rd_block pointers point to, until one of the criteria
     *      for stopping the iteration is met.
     */
    while(1)
    {
        //uint8_t  whole_evt_blk_ptr_page_has_non_obsolete_pointers;
        uint16_t num_evt_blk_ptrs;
        uint16_t idx = 0;
        uint32_t *pHandle = NULL;
        err_t err;

        err = _EVTMGR_load_evt_blk_ptrs( &pHandle, evt_blk_rd_addr, &num_evt_blk_ptrs );
        if(err == ERR_EVTMGR_NO_EVT_BLK_PTRS)
        {
            /*
             *   Apparently no event block pointers to speak of in this PAGE.
             *     Increment to the next SECTOR (as long as you don't lap wr_addr), as perhaps this is due
             *     to a WRITE erasing a sector that a READ was on.
             *     
             *   In theory, we should never get here, since we have the mutex protection back now, as of 9/6/2013-ish.
             */
            PROCESS_NINFO( ERR_EVTMGR_NO_EVT_BLK_PTRS, "wr:%x,rd:%x", _EVTMGR_get_evt_blk_wr_addr(), evt_blk_rd_addr );
#if 0
            // EA: no longer need this b/c we reset EVTMGR in load_evt_blk_ptrs() for this bad case.
            evt_blk_rd_addr = _EVTMGR_next_evt_blk_sector(evt_blk_rd_addr);
#endif
            goto end_updating_rd_blk_ptrs;
        }
        
        wassert(err == ERR_OK);
        wassert( num_evt_blk_ptrs > 0 );
        //whole_evt_blk_ptr_page_has_non_obsolete_pointers = 1;
        
        /*
         *   Iterate over the block pointers in the cache, checking to see if they point to 
         *     data that was impacted by a sector erase.
         */
        while(idx < num_evt_blk_ptrs)
        {
            evt_blk_ptr_ptr_t pEvtBlkPtr;
            uint8_t *pData;
            
            // get next evt blk ptr at index.
            pEvtBlkPtr = _EVTMGR_get_blk_ptr(&pHandle, idx++);
            wassert( pEvtBlkPtr != NULL );      
            
            if( _EVTMGR_evt_blk_is_obsolete(pEvtBlkPtr, obsolete_addr) )
            {
                /*
                 *   Keep iterating until we have an event block that is not obsolete, so we can point to that.
                 */
#if 0
                whole_evt_blk_ptr_page_has_non_obsolete_pointers = 0;
                break;
#else
                continue;
#endif
            }
            else
            {
                /*
                 *   We are already pointing to an event block that has not yet been consumed or deleted.
                 *     Therefore, we can keep the event block pointer we have.  Cleanup and return here.
                 */
#if 1
                wassert( _EVTMGR_unload_evt_blk_ptrs(&pHandle) == ERR_OK );
                goto end_updating_rd_blk_ptrs;        
#endif
            }
        }
        
#if 0
        /*
         *   In order to conclude that we've reached a point where we can safely say we've reached the point
         *      where we are now pointing to non-obsolete places in memory, we require that the whole page of blk pointers
         *      be pointing to valid data.
         */
        if( whole_evt_blk_ptr_page_has_non_obsolete_pointers )
        {
            wassert( _EVTMGR_unload_evt_blk_ptrs(&pHandle) == ERR_OK );
            goto end_updating_rd_blk_ptrs;
        }
#endif
        wassert( _EVTMGR_unload_evt_blk_ptrs(&pHandle) == ERR_OK );
        
        evt_blk_rd_addr = _EVTMGR_update_evt_blk_ptr_addr(evt_blk_rd_addr);
        
        if( evt_blk_rd_addr == _EVTMGR_get_evt_blk_wr_addr() )
        {
            /*
             *   We've wrapped around all the way to write address, so go ahead and return.
             */
            goto end_updating_rd_blk_ptrs;
        }
    }
    
end_updating_rd_blk_ptrs:

    /*
     *   Flush the info, if the pointer was changed.
     */
    if(evt_blk_rd_addr != _EVTMGR_get_evt_blk_rd_addr())
    {
        _EVTMGR_set_evt_blk_rd_addr(evt_blk_rd_addr);
        _EVTMGR_flush_info(&g_Info);
    }
    
    return ERR_OK;
}

/*
 *   Flush the evt blk pointer RAM buffer to SPI Flash, on a page boundary.
 *     num_bytes:  number of bytes pointed to by these pointers (send 0 if unknown).
 */
void _EVTMGR_flush_evt_blk_ptrs(uint_32 num_bytes)
{
    if ( 0 == g_BlkPtrBuf.idx )
    {
        return;
    }
    
    _EVTMGR_atomic_vote(EVTMGR_ATOMIC_VOTE_ON);
    
    _EVTMGR_write_page (g_Info.evt_blk_wr_addr,
                       (uint_8 *)g_BlkPtrBuf.ptrs,
                        EXT_FLASH_PAGE_SIZE);
    
    corona_print("\tEM Flush %d blk ptrs\n", g_BlkPtrBuf.idx);
    
    if( 0 != num_bytes)
    {
        corona_print("\t(%d bytes)\n", num_bytes);
    }
    
    g_Info.evt_blk_wr_addr = _EVTMGR_update_evt_blk_ptr_addr(g_Info.evt_blk_wr_addr);
    
    /*
     *   If the evt blk wr address is on a sector boundary, the next write will erase all data in
     *     that sector.  Therefore, we need to update the read address if it happens to be in that
     *     same sector.
     */
    if( _EVTMGR_is_sector_boundary( g_Info.evt_blk_wr_addr ) )
    {
        if(g_Info.evt_blk_wr_addr == get_sector_addr(g_Info.evt_blk_rd_addr, EXT_FLASH_64KB_SZ, EXT_FLASH_64KB_REGION))
        {
            g_Info.evt_blk_rd_addr = _EVTMGR_next_evt_blk_sector( g_Info.evt_blk_rd_addr );
        }
    }
    else if(g_Info.evt_blk_wr_addr == g_Info.evt_blk_rd_addr)
    {
        /*
         *   Handle the case where the blk pointer write pointer just lapped the read pointer,
         *     but not on a sector boundary,
         *     so the read blk pointer will need to increment to next page as well.
         */
        g_Info.evt_blk_rd_addr = _EVTMGR_update_evt_blk_ptr_addr(g_Info.evt_blk_rd_addr);
    }
    
    _EVTMGR_flush_info(&g_Info);
    
done_flushing_evt_blk_ptrs:

    _EVTMGR_init_blk_ptr_buf();
    _EVTMGR_atomic_vote(EVTMGR_ATOMIC_VOTE_OFF);
}

/*
 *   Increment the page of the current write address.
 */
void _EVTMGR_inc_wr_addr(void)
{
    g_Info.write_addr = _EVTMGR_update_page_addr( _EVTMGR_get_wr_addr() );
}

/*
 *   Adds a new event block pointer to the RAM buffer.  
 *   Flush to SPI Flash if a threshold/condition is met.
 */
void _EVTMGR_add_blk_ptr(evt_blk_ptr_ptr_t pBlkPtr)
{
    static uint32_t event_blk_payload_len = 0;  // keeps track of total payload length this evt blk ptr buffer points to prior to flushing to SPI Flash.
    
    if( (pBlkPtr->len + event_blk_payload_len) >= g_st_cfg.evt_st.max_upload_transfer_len )
    {
        /*
         *   Add this event block pointer to a fresh new buffer, since adding it here,
         *   would exceed the max payload we can send in an HTTP packet.
         */
        
        _EVTMGR_flush_evt_blk_ptrs( event_blk_payload_len );
        event_blk_payload_len = 0;
        
        memcpy(&(g_BlkPtrBuf.ptrs[g_BlkPtrBuf.idx]), pBlkPtr, sizeof(evt_blk_ptr_t));
        g_BlkPtrBuf.idx++;
        event_blk_payload_len += pBlkPtr->len;
    }
    else
    {
        memcpy(&(g_BlkPtrBuf.ptrs[g_BlkPtrBuf.idx]), pBlkPtr, sizeof(evt_blk_ptr_t));
        g_BlkPtrBuf.idx++;
        event_blk_payload_len += pBlkPtr->len;
        
        if(NUM_EVT_BLK_PTRS_IN_BUF == g_BlkPtrBuf.idx)
        {
            /*
             *   This is the last event block pointer that will fit into this page, so go ahead and flush.
             */
            _EVTMGR_flush_evt_blk_ptrs( event_blk_payload_len );
            event_blk_payload_len = 0;
        }
    }
}

uint_8 _EVTMGR_is_flushed( void )
{
    return (g_BlkPtrBuf.idx == 0 ? 1 : 0);
}


