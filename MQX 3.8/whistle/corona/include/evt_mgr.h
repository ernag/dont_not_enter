/*
 * evt_mgr.h
 *
 *  Created on: Mar 29, 2013
 *      Author: Ernie
 */

#ifndef EVT_MGR_H_
#define EVT_MGR_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include <bsp.h>
#include "app_errors.h"

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Enums
////////////////////////////////////////////////////////////////////////////////

/*
 *   Define events here.
 */
typedef enum event_type
{
    
    EVENT_UNKNOWN            = 0,
    EVENT_ACCEL              = 1,
    EVENT_ACCEL_SLEEP        = 2,
    EVENT_BUTTON             = 3,
    EVENT_USER_PAIRED        = 4,
    EVENT_ACCEL_DMA_OVERRUN  = 5,
    EVENT_ACCEL_DMA_UNDERRUN = 6,
    EVENT_BT_PROX            = 7,
    
    EVENT_GENERIC            = 99
    
}event_t;

////////////////////////////////////////////////////////////////////////////////
// Event Object Definitions
////////////////////////////////////////////////////////////////////////////////

/*
 *    Generic Event Object
 */
typedef struct event_object_type
{
    uint_16 payload_len;
    uint_8  *pPayload;
    
} evt_obj_t, *evt_obj_ptr_t;

////////////////////////////////////////////////////////////////////////////////
// Callback Definitions
////////////////////////////////////////////////////////////////////////////////

/*
 *   Callback Interface event manager posters use.
 *   err - EVTMGR will call the callback with an error code.
 *   pObj - This is the address of the object originally passed to EVTMGR.
 *          This can be used to snychronize tasks when they need to call post() multiple times.
 */
typedef void (*evtmgr_post_cbk_t)(err_t err, evt_obj_ptr_t pObj);

/*
 *   Callback Interface event manager dumpers use.
 *   This callback will keep getting called by EVTMGR until there is no more data.
 *   
 *   err        - error code from EVTMGR.
 *   pData      - pointer to the data EVTMGR is dumping to the callback.
 *   len        - size of this block the EVTMGR is dumping.
 *   last_block - 0 if there will be more data coming, 1 otherwise.
 */
typedef err_t (*evtmgr_dump_cbk_t)(err_t err, uint8_t *pData, uint32_t len, uint8_t last_block);

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////

/*
 *   Initialization Routine for the EVTMGR.
 *   This must be called prior to using the EVTMGR.
 */
void EVTMGR_init(void);

/*
 *   Call this API to post your event, with its payload.
 *   Provide a callback for post() to call upon completion with error code.
 *   The callback is used so that post() doesn't block.
 *   NOTE:  EVTMGR_post() assumes that the resources/memory/pointers you send to it
 *           are still valid, up until the point that pCallback is called.
 */
err_t EVTMGR_post(evt_obj_ptr_t pObj, evtmgr_post_cbk_t pCallback);

/*
 *   Call this API to dump event block data to a target.
 *   
 *   max_blocks - maximum number of event blocks to dump (0 means ALL EVENTS).
 *   pCallback  - callback that EVTMGR will call to dump data. 
 *   mark_consumed - 0 if data should persist after the dump, non-zero to destroy data after-dump.
 *                   mark_consumed is ignored if max_blocks is non-zero.
 *   
 */
err_t EVTMGR_dump(uint32_t max_blocks, evtmgr_dump_cbk_t pCallback, uint8_t mark_consumed);

/*
 *   Call this API to reset all event data to zero.
 */
void EVTMGR_reset(void);

/*
 *   Call this API to immediately update the seconds between event data uploads.
 */
void EVTMGR_upload_dutycycle(uint_32 seconds_between_uploads);

/*
 * Call this API to request an immediate flush
 * 
 */
void EVTMGR_request_flush( void );

/*
 *   Event Manager Tasks to be started and used after EVTMGR_init().
 */
void EVTPST_tsk(uint_32 dummy);
void EVTDMP_tsk(uint_32 dummy);
void EVTUPLD_tsk(uint_32 dummy);

#endif /* EVT_MGR_H_ */
