/*
 * event_mgr.c
 *
 *  Created on: Mar 29, 2013
 *      Author: Ernie
 *      https://whistle.atlassian.net/wiki/display/COR/Event+Manager+Memory+Management+Design
 */


////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <watchdog.h>
#include "cormem.h"
#include "wassert.h"
#include "wmutex.h"
#include "evt_mgr.h"
#include "evt_mgr_priv.h"
#include "ext_flash.h"
#include "sys_event.h"
#include "corona_ext_flash_layout.h"
#include "corona_console.h"
#include "crc_util.h"
#include "pwr_mgr.h"
#include "main.h"
#include "cfg_mgr.h"
#include "pmem.h"
#include "wmp_datadump_encode.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define INFINITY    0xFFFFFFFF

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

/*
 *   Custom Queue Structure to use for managing the EVTMGR_post() queue.
 */
typedef struct event_manager_post_queue_type
{
    QUEUE_ELEMENT_STRUCT HEADER;    // standard queue element header.
    evt_obj_ptr_t        pObj;      // Generic event object.
    evtmgr_post_cbk_t    pCallback; // callback for notifying caller of completion.
    
}evtmgr_post_qu_elem_t, *evtmgr_post_qu_elem_ptr_t;

typedef struct event_manager_dump_queue_type
{
    QUEUE_ELEMENT_STRUCT HEADER;        // standard queue element header.
    evtmgr_dump_cbk_t    pCallback;     // callback for notifying caller of completion.
    uint32_t             max_blocks;    // max number of event blocks to be dumped (0 = ALL)
    uint8_t              mark_consumed; // whether or not to destroy data after dumping.
    
}evtmgr_dump_qu_elem_t, *evtmgr_dump_qu_elem_ptr_t;

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static void _EVTMGR_post_qu_elem(evtmgr_post_qu_elem_ptr_t pElem);
static void _EVTMGR_dump_qu_elem(evtmgr_dump_qu_elem_ptr_t pElem);
static void _EVTMGR_post_cbk(evtmgr_post_cbk_t pCbk, err_t err, void *pObj);
static void _EVTMGR_post_accel_qu_elem(evtmgr_post_qu_elem_ptr_t pElem);
static void _EVTMGR_post_generic_qu_elem(evtmgr_post_qu_elem_ptr_t pElem);
static void _EVTMGR_read_evt_blk(evt_blk_ptr_ptr_t pElem, uint8_t *pData);

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static MUTEX_STRUCT   g_evtmgr_mutex;
static QUEUE_STRUCT   g_post_qu;
static QUEUE_STRUCT   g_dump_qu;
LWEVENT_STRUCT        g_evtmgr_lwevent;
uint8_t               g_evtmgr_shutting_down = FALSE;
static uint_8         g_evtmgr_is_locked = FALSE;
static uint32_t       g_evtmgr_thread_vote_mask = 0;
static uint8_t        g_evtmgr_is_up = 0;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

static void _EVTMGR_system_shutdown_callback(sys_event_t sys_event)
{
	if (SYS_EVENT_SYS_SHUTDOWN == sys_event && !g_evtmgr_shutting_down)
	{
		g_evtmgr_shutting_down = TRUE;
		
		/*
		 *   Flush any outstanding event block pointers, to assure
		 *     no loss of data due to rebooting.
		 */
        PWRMGR_VoteForON( PWRMGR_EVTMGR_FLUSH_VOTE );
        _lwevent_set(&g_evtmgr_lwevent, EVTMGR_SHUTDOWN_EVENT);
	}
}

/*
 *   Lock the EVTMGR critical resources.
 */
void _EVTMGR_lock(void)
{
    wmutex_lock(&g_evtmgr_mutex);
    g_evtmgr_is_locked = TRUE;
}

/*
 *   Unlock the EVTMGR critical resources.
 */
void _EVTMGR_unlock(void)
{
    // NOTE: purposefully do not wassert on an error (so no using wmutex_unlock).
    // TODO: untimely crumb when there is an issue unlocking.
    g_evtmgr_is_locked = FALSE;
    _mutex_unlock(&g_evtmgr_mutex);
}

uint_8 _EVTMGR_is_locked(void)
{
    return g_evtmgr_is_locked;
}

/*
 *   Just reset the event block pointers so they point to each other (empty data storage).
 */
void EVTMGR_reset(void)
{
    // NOTE: We intentionally do not wait on a mutex here.  
    //       The reason is because when we do a reset, it is generally a fatal error we don't want to block.
    
    _mutex_try_lock(&g_evtmgr_mutex);  // we don't want to block, but it'd be nice to lock it down before resetting.
    
    corona_print("\n\t***%s***\n\n", __func__);
    _EVTMGR_init_info(1);
    
    /*
     *   If we reset EVTMGR, we want to BE SURE we know about it, so set it here in addition to below.
     */
    persist_set_untimely_crumb(BreadCrumbEvent_EVTMGR_RESET);

    _EVTMGR_unlock();
    
    WMP_post_breadcrumb( BreadCrumbEvent_EVTMGR_RESET );
}

/*
 *   Post an event to SPI Flash, or to a RAM buffer, to be flushed to SPI Flash later.
 */
err_t EVTMGR_post(evt_obj_ptr_t pObj, evtmgr_post_cbk_t pCallback)
{
    evtmgr_post_qu_elem_ptr_t pQu;
    
    if( 0 == g_evtmgr_is_up )
    {
        pCallback( ERR_OK, pObj );
        goto DONE;
    }
    
    /*
     *   COR-519 - don't start logging data until after a server checkin.
     */
    if( !g_st_cfg.evt_st.ignore_checkin_status && !g_dy_cfg.sysinfo_dy.deviceHasCheckedIn )
    {
        corona_print("\t* no post b/f checkin\n");
        pCallback( ERR_OK, pObj );
        goto DONE;
    }
    
    if( g_Info.signature != EVTMGR_MAGIC_SIGNATURE )
    {
        // haven't initialized EVTMGR yet.
        pCallback( ERR_OK, pObj );
        goto DONE;
    }
    
    /*
     *   Make sure the client is passing in a payload that is greater than zero.
     */
    wassert( pObj->payload_len > 0 );
    
    /*
     *   Make sure the client is passing in a payload below the maximum bounds.
     */
    wassert( pObj->payload_len <= MAX_EVT_BLK_LEN );
    
    
    pQu = walloc( sizeof(evtmgr_post_qu_elem_t) );
    
    pQu->pObj = pObj;
    pQu->pCallback = pCallback;
    
    PWRMGR_VoteForON(PWRMGR_EVTMGR_POST_VOTE);  // here guarantees task runs
    wassert(_queue_enqueue(&g_post_qu, (QUEUE_ELEMENT_STRUCT_PTR) pQu));
  
    _lwevent_set(&g_evtmgr_lwevent, EVTMGR_POST_ENQUEUED_EVENT);

DONE:
    return ERR_OK;
}

/*
 *   Dump event blocks to the desired target.
 */
err_t EVTMGR_dump(uint32_t max_blocks, evtmgr_dump_cbk_t pCallback, uint8_t mark_consumed)
{
    evtmgr_dump_qu_elem_ptr_t pQu;
    
    wassert( g_Info.signature == EVTMGR_MAGIC_SIGNATURE );
    
    pQu = walloc( sizeof(evtmgr_dump_qu_elem_t) );
    
    pQu->mark_consumed = mark_consumed;
    pQu->max_blocks = max_blocks;
    pQu->pCallback = pCallback;
    
    PWRMGR_VoteForON(PWRMGR_EVTMGR_DUMP_VOTE);  // here guarantees task runs
    wassert( 0 != _queue_enqueue(&g_dump_qu, (QUEUE_ELEMENT_STRUCT_PTR) pQu) );
    _lwevent_set(&g_evtmgr_lwevent, EVTMGR_DUMP_ENQUEUED_EVENT);
    return ERR_OK;
}

/*
 *   Just check the data being uploaded for corrupted data, like "all FF's".
 */
void __TEST_check_for_corruption(uint8_t *pData, uint32_t addr, uint16_t len)
{
#if 0
    uint16_t i;
    uint16_t num_times_ff = 0;
    
    for(i=0; i<len; i++)
    {
        if( 0xFF == pData[i] )
        {
            num_times_ff++;
        }
    }
    
    /*
     *   Raise an error if over half of your data is all FF's.
     */
    if(num_times_ff > (len/2))
    {
        PROCESS_NINFO( ERR_EVTMGR_POTENTIAL_DATA_CORRUPT, "addr:%x,len:%i", addr, len );
    }
#endif
}

/*
 *    Dump an event taken from the queue.
 */
static void _EVTMGR_dump_qu_elem(evtmgr_dump_qu_elem_ptr_t pElem)
{
    uint32_t evt_blk_wr_addr;
    uint32_t *pWipEvtBlkRdAddr;
    uint32_t wip_evt_blk_rd_addr_temp;
    uint32_t max_blocks  = INFINITY;
    uint8_t last_block = FALSE;
    err_t err;

    _watchdog_start(60*1000);
    _EVTMGR_lock();

    if (g_st_cfg.evt_st.debug_hold_evt_rd_ptr)
    {
    	wip_evt_blk_rd_addr_temp = _EVTMGR_get_evt_blk_rd_addr();
    	pWipEvtBlkRdAddr = &wip_evt_blk_rd_addr_temp;
    }
    else
    {
    	pWipEvtBlkRdAddr = &g_Info.wip_evt_blk_rd_addr;
    }
    
    evt_blk_wr_addr = _EVTMGR_get_evt_blk_wr_addr();
    *pWipEvtBlkRdAddr = _EVTMGR_get_evt_blk_rd_addr();

    if(*pWipEvtBlkRdAddr == evt_blk_wr_addr)
    {
        err = ERR_EVTMGR_NO_DATA_TO_DUMP;
        goto end_failure_dump;
    }
    
    if(pElem->max_blocks > 0)
    {
        max_blocks = pElem->max_blocks;
#if 0
        if(pElem->mark_consumed)
        {
            err = ERR_EVTMGR_MARK_CONSUMED_INVALID;
            goto end_failure_dump;
        }
#endif
    }
    
    while(1)
    {
        uint16_t num_evt_blk_ptrs;
        uint16_t idx = 0;
        uint32_t *pHandle = NULL;
        
        if(*pWipEvtBlkRdAddr == evt_blk_wr_addr)
        {
            /*
             *   The read pointer has wrapped all the way back around to the write pointer.
             */
            goto end_successful_dump;
        }
        
        /*
         *   Load the evtmgr cache.
         */
        err = _EVTMGR_load_evt_blk_ptrs( &pHandle, *pWipEvtBlkRdAddr, &num_evt_blk_ptrs );
        if(err == ERR_EVTMGR_NO_EVT_BLK_PTRS)
        {
            /*
             *   This means that read address did not in fact have any data to dump.
             */
            goto end_successful_dump;
        }
        
        if(err != ERR_OK)
        {
            goto end_failure_dump;
        }
        
        /*
         *   Iterate over the block pointers in the cache, dumping the contents as we go along.
         */
        while(idx < num_evt_blk_ptrs)
        {
            evt_blk_ptr_ptr_t pEvtBlkPtr;
            uint8_t *pData;
            
            // get next evt blk ptr at index.
            pEvtBlkPtr = _EVTMGR_get_blk_ptr(&pHandle, idx++);
            wassert(pEvtBlkPtr != NULL);
            
            // malloc enough memory to hold the data we are trying to dump.
            pData = walloc( pEvtBlkPtr->len );
            
            // corona_print("EM: reading from addr: %d\n", pEvtBlkPtr->addr); fflush(stdout);
            // get the data in your buffer.
            _EVTMGR_read_evt_blk(pEvtBlkPtr, pData);

            /*
             *   Here is the controversial per-block print out in the upload path.
             */
#if 0
            corona_print("Blk ptr %d/%d (%d)\n", idx, num_evt_blk_ptrs, pEvtBlkPtr->len);
            __TEST_check_for_corruption(pData, pEvtBlkPtr->addr, pEvtBlkPtr->len);
#endif
            // call the user's callback.
            err = pElem->pCallback(ERR_OK, pData, pEvtBlkPtr->len, FALSE);
            
            // free the memory being used for data.
            wfree(pData);
            
            if(ERR_OK != err)
            {
                _EVTMGR_unload_evt_blk_ptrs(&pHandle);
                PROCESS_NINFO(err, "dump");
                goto end_failure_dump;
            }
            
            // make sure we are not providing the user with more event blocks than they want.
            if(--max_blocks == 0)
            {
                
                /*
                 *   The user-requested maximum number of event blocks has been reached.
                 */
                if (idx < num_evt_blk_ptrs)
                {
                    /*
                     * The user requested maximum number of event blocks is less than what
                     * is in the block pointer. Ideally we'd have a way to flush back what
                     * was left.
                     */
                    //TODO: Add method to post back to flash the remaining blk_ptrs.
                    *pWipEvtBlkRdAddr = _EVTMGR_update_evt_blk_ptr_addr(*pWipEvtBlkRdAddr);
                }
                else
                {
                    /*
                     * The user requested number of event blocks is the same as what
                     * was in the block pointer so need to move the rd block pointer forward.
                     */
                    *pWipEvtBlkRdAddr = _EVTMGR_update_evt_blk_ptr_addr(*pWipEvtBlkRdAddr);
                }  

                err = _EVTMGR_unload_evt_blk_ptrs(&pHandle);
                if(err != ERR_OK)
                {
                    goto end_failure_dump;
                }
                goto end_successful_dump;
            }

            // This was here moons ago, to help with WIFI stability, but now we buffer and do other dances with HACK_DELAY_BETWEEN_CONSEND_MS.
            //_time_delay(200);
        }
        
        err = _EVTMGR_unload_evt_blk_ptrs(&pHandle);
        if(err != ERR_OK)
        {
            goto end_failure_dump;
        }
        
        *pWipEvtBlkRdAddr = _EVTMGR_update_evt_blk_ptr_addr(*pWipEvtBlkRdAddr);
    }
    
end_successful_dump:
    err = ERR_OK;
    
end_failure_dump:
    _EVTMGR_unlock();
    wfree(pElem);
    pElem->pCallback(err, NULL, 0, TRUE);
    return;
}

static void _EVTMGR_read_evt_blk(evt_blk_ptr_ptr_t pEvtBlkPtr, uint8_t *pData)
{
    uint32_t end_address = pEvtBlkPtr->addr + pEvtBlkPtr->len - 1;
    
    // Make sure we wrap our read if the data extends beyond the spi flash region we are using
    if (end_address > EXT_FLASH_EVENT_MGR_LAST_ADDR)
    {
        uint32_t bytes_left_over = end_address - EXT_FLASH_EVENT_MGR_LAST_ADDR;
        uint32_t first_chunk_len = pEvtBlkPtr->len - bytes_left_over;
        
        wson_read(pEvtBlkPtr->addr, pData, first_chunk_len);
        wson_read(EXT_FLASH_EVENT_MGR_ADDR, pData + first_chunk_len, bytes_left_over);
    } 
    else
    {
        wson_read(pEvtBlkPtr->addr, pData, pEvtBlkPtr->len);
    }
}

/*
 *   Call an event's post callback.
 */
static void _EVTMGR_post_cbk(evtmgr_post_cbk_t pCbk, err_t err, void *pObj)
{
    if(pCbk)
    {
        pCbk(err, pObj);
    }
}

/*
 *    Post an event taken from the queue.
 */
static void _EVTMGR_post_qu_elem(evtmgr_post_qu_elem_ptr_t pElem)
{   
    evt_obj_ptr_t pGenObj;
    evt_blk_ptr_t blk_ptr;
    
    /*
     *   There is no temporary RAM buffer for queueing up GE events,
     *      so just immediately flush to SPI Flash.
     */
    
    pGenObj = pElem->pObj;
    if(pGenObj->payload_len == 0)
    {
        pElem->pCallback(ERR_EVTMGR_ZERO_LENGTH_PAYLOAD, pElem->pObj);
        return;
    }
    
    blk_ptr.len  = pGenObj->payload_len;
    blk_ptr.crc  = UTIL_crc32_calc(pGenObj->pPayload, pGenObj->payload_len);
    
    _EVTMGR_post_cbk(pElem->pCallback, _EVTMGR_wr_evt_blk(&blk_ptr, pGenObj->pPayload), pElem->pObj);
    wfree(pElem);
}

/*
 *   Periodic task for posting events when they arrive in the post queue.
 */
void EVTPST_tsk(uint_32 dummy)
{
    while(1)
    {
        evtmgr_post_qu_elem_ptr_t pQuElem;
        _mqx_uint mask;
        
        /*
         *   Wait for something to be posted...
         */
wait_for_elems_to_post:
        
        _watchdog_stop();

        PWRMGR_VoteForOFF(PWRMGR_EVTMGR_POST_VOTE);
        PWRMGR_VoteForSleep(PWRMGR_EVTMGR_POST_VOTE);
        
        _lwevent_wait_ticks(    &g_evtmgr_lwevent, 
                                EVTMGR_POST_ENQUEUED_EVENT | 
                                EVTMGR_FLUSH_REQUEST_EVENT |
                                EVTMGR_SHUTDOWN_EVENT, 
                                FALSE, 
                                0);
        
        PWRMGR_VoteForAwake(PWRMGR_EVTMGR_POST_VOTE);
        PWRMGR_VoteForON(PWRMGR_EVTMGR_POST_VOTE);  // This ON vote should be redundant, but do it jic
        
        mask = _lwevent_get_signalled();
        
        /*
         * Allow Flush request even if we're shutting down.
         */
        if (mask & EVTMGR_FLUSH_REQUEST_EVENT)
        {
            /*
             *   If we are shutting down, leave it to the EVTMGR_SHUTDOWN_EVENT to clean up blk ptrs.
             */
            if( !g_evtmgr_shutting_down )
            {
                _watchdog_start(60*1000);

                _EVTMGR_lock();
                _EVTMGR_flush_evt_blk_ptrs( 0 );
                _EVTMGR_unlock();
                PWRMGR_VoteForOFF( PWRMGR_EVTMGR_FLUSH_VOTE );
            }

            goto wait_for_elems_to_post;
        }
        
        if (mask & EVTMGR_SHUTDOWN_EVENT)
        {
            /*
             *   If we received the shutdown signal, let's give a little bit of time for people to post
             *      any events, so we can capture them and flush them.
             */
            _watchdog_start(60*1000);
        	CFGMGR_postponed_flush();
            _time_delay( 100 );
        }
        
        /*
         *   If we get here, it should mean that we have elements in the queue to process, so les dunk it.
         */
        while(1)
        {
            _watchdog_start(60*1000);

            /*
             *   Check to see if there are elements in the queue.
             */
            pQuElem = NULL;
            
            if( _queue_get_size(&g_post_qu) > 0 )
            {
                pQuElem = (evtmgr_post_qu_elem_ptr_t) _queue_dequeue(&g_post_qu);
            }
            
            if(NULL == pQuElem)
            {
                // Queue is now empty, so we're done posting elements.
                break;
            }

            /*
             *    Post the event.
             */
            _EVTMGR_post_qu_elem(pQuElem);
        }
        
        /*
         *  If we are shutting down, flush blk ptrs. vote for off, and end the task.
         */
        if (mask & EVTMGR_SHUTDOWN_EVENT)
        {
            _EVTMGR_lock();
            _EVTMGR_flush_evt_blk_ptrs( 0 );
            _EVTMGR_unlock();
            
            goto evtmgr_end_post_task;
        }
    }

evtmgr_end_post_task:
    _watchdog_stop();    	
    PWRMGR_VoteExit(PWRMGR_EVTMGR_POST_VOTE|PWRMGR_EVTMGR_FLUSH_VOTE);
    _EVTMGR_lock();
    _task_block(); // Don't exit or our MQX resources may be deleted
}

/*
 *   Periodic task for dumping events when they arrive in the dump queue.
 */
void EVTDMP_tsk(uint_32 dummy)
{
    while(1)
    {
        evtmgr_dump_qu_elem_ptr_t pQuElem;
        _mqx_uint mask;
        
        /*
         *   Wait for a dump request.
         */
wait_for_elems_to_dump:
        PWRMGR_VoteForOFF(PWRMGR_EVTMGR_DUMP_VOTE);

        if ( g_evtmgr_shutting_down )
        {
            goto DONE;
        }
        
        _watchdog_stop();
        
        _lwevent_wait_ticks(&g_evtmgr_lwevent, EVTMGR_DUMP_ENQUEUED_EVENT, FALSE, 0);

        PWRMGR_VoteForON(PWRMGR_EVTMGR_DUMP_VOTE); // This ON vote should be redundant, but do it jic

        if ( g_evtmgr_shutting_down )
        {
            goto DONE;
        }
        
        mask = _lwevent_get_signalled();
        wassert( (mask & EVTMGR_DUMP_ENQUEUED_EVENT) != 0 );
        
        while(1)
        {
            wassert(_watchdog_start(100*1000));

            /*
             *   Check to see if there are elements in the queue.
             */
            pQuElem = NULL;

            if( _queue_get_size(&g_dump_qu) > 0 )
            {
                pQuElem = (evtmgr_dump_qu_elem_ptr_t) _queue_dequeue(&g_dump_qu);
            }

            if(pQuElem == NULL)
            {
                // Queue is now empty, go back to waiting on enqueue event.
                goto wait_for_elems_to_dump;
            }

			/*
			 *    Dump the event block data.
			 */
			_EVTMGR_dump_qu_elem(pQuElem);

        }
    }
    
DONE:
    _watchdog_stop();
    PWRMGR_VoteExit(PWRMGR_EVTMGR_DUMP_VOTE);
    _task_block(); // Don't exit or our MQX resources may be deleted
}

void EVTMGR_request_flush(void)
{
    if ( !_EVTMGR_is_flushed() && !g_evtmgr_shutting_down )
    {
    	// Raise a Flush Request event 
        PWRMGR_VoteForON( PWRMGR_EVTMGR_FLUSH_VOTE ); // here guarantees task runs

    	_lwevent_set(&g_evtmgr_lwevent, EVTMGR_FLUSH_REQUEST_EVENT);
    }
}

void EVTMGR_init(void)
{
    /*
     *   Mutex protection against write pointers blasting a current read.
     */
    wmutex_init(&g_evtmgr_mutex);
    
    /*
     *   Initialize EVTMGR's lwevent.
     */
    _lwevent_create( &g_evtmgr_lwevent, LWEVENT_AUTO_CLEAR);
    
    /*
     *   Initialize the queues needed for posting and dumping events w/o blocking.
     */
    _queue_init(&g_post_qu, 0);
    _queue_init(&g_dump_qu, 0);
    
    _EVTMGR_lock();
    g_evtmgr_is_up = 1;
    
    /* Register for the shutdown event */
    sys_event_register_cbk(_EVTMGR_system_shutdown_callback, SYS_EVENT_SYS_SHUTDOWN);
    
    /*
     *   Initialize the event block pointers and system info.
     */
    _EVTMGR_init_info(0);
    _EVTMGR_unlock();
}
