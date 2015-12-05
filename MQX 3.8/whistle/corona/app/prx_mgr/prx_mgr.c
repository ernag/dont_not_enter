/*
 * prx_mgr.c
 *
 *  Created on: Oct 8, 2013
 *      Author: Chris
 */

#include <mqx.h>
#include <bsp.h>
#include <mutex.h>
#include "cormem.h"
#include "wassert.h"
#include "prx_mgr.h"
#include "pwr_mgr.h"
#include "evt_mgr.h"
#include "cfg_mgr.h"
#include "time_util.h"
#include "bt_mgr.h"

#define PRX_EVENT_POSTED_DONE       0x00000001

#define BT_PROX_ENCODE_BUF_SZ       64

static LWEVENT_STRUCT g_prxmgr_lwevent;

// This callback executes when EVTMGR is done posting, so it doesn't block.
// Callers of EVTMGR_post() must be careful not to take away the resources/pointers that post() uses,
//   until this callback returns.
static void _PRXMGR_event_post_callback(err_t err, evt_obj_ptr_t pPrxObj)
{
    wassert(ERR_OK == err);
    _lwevent_set(&g_prxmgr_lwevent, PRX_EVENT_POSTED_DONE);
}

// Perform a prox scan.
// transport mutex must be locked before calling this function.
// Do not modify this function to lock the transport mutex internally.
void PRXMGR_scan(void)
{
    uint64_t *pMacAddrs = NULL;
    uint8_t numPairedDevices;
    uint8_t i;
	err_t err;
	evt_obj_t prx_obj;
	uint64_t absTime;
    uint64_t relTime;
		   
	prx_obj.pPayload = NULL;

    pMacAddrs = walloc( MAX_BT_REMOTES * sizeof(uint64_t) );
    
    numPairedDevices = 0;
	                
	if (PWRMGR_is_rebooting())
	{
		return;
	}
	
    BTMGR_request(); // Grab a reference to the stack
                
	err = BTMGR_check_proximity(&numPairedDevices, MAX_BT_REMOTES, pMacAddrs);
				
	BTMGR_release();
	
    if( (ERR_OK != err) || (0 == numPairedDevices) )
	{
	    goto DONE;
	}
    
    /*
     *   Get the current time.
     */
    UTIL_time_abs_rel(&relTime, &absTime);

    /*
     *   Alloc a buffer for encoding our WMP message(s).
     */
	prx_obj.pPayload = walloc( BT_PROX_ENCODE_BUF_SZ );

	/*
	 *   For all the devices we found, encode a prox WMP msg, and post the event to EVTMGR.
	 */
	for(i = 0; i < numPairedDevices; i++)
	{

	    /*
	     *   Encode the BT Prox info to our buffer.
	     */
        WMP_encode_DATADUMP_BtProx( absTime, 
                                    relTime,
                                    pMacAddrs[i],
                                    prx_obj.pPayload,
                                    &prx_obj.payload_len,
                                    BT_PROX_ENCODE_BUF_SZ );
        
        if( 0 == prx_obj.payload_len )
        {
            /*
             *   Don't post an empty event.  Instead, raise an error, and call the callback
             *     directly, rather than waiting on EVTMGR to call it.
             */
            PROCESS_NINFO( ERR_PRX_ZERO_LEN_PAYLOAD, "0 len prx" );
            _PRXMGR_event_post_callback( ERR_PRX_ZERO_LEN_PAYLOAD, &prx_obj );
        }
        else
        {
            /*
             *   Post it to the event manager, so we can upload it to the server later.
             */
            EVTMGR_post(&prx_obj, _PRXMGR_event_post_callback);
        }
        
        /*
         *   Block until the event is posted.
         */
        _lwevent_wait_ticks(&g_prxmgr_lwevent, PRX_EVENT_POSTED_DONE, FALSE, 0);
	}
	
DONE:
    /*
     *   Free memory resources if needed.
     */
    if( NULL != pMacAddrs )
    {
        wfree(pMacAddrs);
        pMacAddrs = NULL;
    }
    
    if( NULL != prx_obj.pPayload )
    {
        wfree(prx_obj.pPayload);
        prx_obj.pPayload = NULL;
    }
}

void PRXMGR_init(void)
{
    _lwevent_create(&g_prxmgr_lwevent, LWEVENT_AUTO_CLEAR);
}

