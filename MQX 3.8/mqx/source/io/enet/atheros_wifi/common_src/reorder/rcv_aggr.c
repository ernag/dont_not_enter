//------------------------------------------------------------------------------
// Copyright (c) 2011 Qualcomm Atheros, Inc.
// All Rights Reserved.
// Qualcomm Atheros Confidential and Proprietary.
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is
// hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================
#include <a_config.h>
#include <a_types.h>
#include <a_osapi.h>
#include <aggr_recv_api.h>
#include "wmi.h"


#if WLAN_CONFIG_11N_AGGR_SUPPORT


#define NUM_OF_TIDS         8
#define AGGR_SZ_DEFAULT     8

#define IEEE80211_MAX_SEQ_NO        0xFFF


typedef struct {
    A_UINT32		aggr_enabled_tid_mask;            /* config value of aggregation size */    
    A_UINT16        seq_last[NUM_OF_TIDS];    
}AGGR_INFO;

static AGGR_INFO aggr_context;

A_VOID
aggr_deinit(A_VOID* cntxt)
{
	A_UNUSED_ARGUMENT(cntxt);
}

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : aggr_init
*  Returned Value : 
*  Comments       :
*  		Initializes aggregation data structures.
*
*END*-----------------------------------------------------------------*/ 
A_VOID*
aggr_init(A_VOID)
{
   /* some implementations malloc a context but this version simply uses
    * a global context */
    
    aggr_reset_state((A_VOID*)&aggr_context);
    
    return &aggr_context;
}


/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : aggr_recv_addba_req_evt
*  Returned Value : 
*  Comments       :
*  		Firmware has indicated that aggregation is enabled 
*		for this TID.
*
*END*-----------------------------------------------------------------*/ 
A_VOID
aggr_recv_addba_req_evt(A_VOID *cntxt, A_UINT8 tid, A_UINT16 seq_no, A_UINT8 win_sz)
{
    AGGR_INFO *p_aggr = (AGGR_INFO *)cntxt;
    A_ASSERT(p_aggr);
    A_ASSERT(tid < NUM_OF_TIDS);   
    A_UNUSED_ARGUMENT(win_sz);
    p_aggr->aggr_enabled_tid_mask |= (1<<tid);
    p_aggr->seq_last[tid] = seq_no;
}



/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : aggr_recv_delba_req_evt
*  Returned Value : 
*  Comments       :
*  		Firmware has indicated that aggregation is no longer enabled 
*		for this TID.
*
*END*-----------------------------------------------------------------*/ 
A_VOID
aggr_recv_delba_req_evt(A_VOID *cntxt, A_UINT8 tid)
{
    AGGR_INFO *p_aggr = (AGGR_INFO *)cntxt;
    A_ASSERT(p_aggr);

    // As per the specification, TID value must not exceed 16 (10.3.27.5.2 Semantics of the service primitive
    // TID Integer 0–15 Specifies the TID of the MSDUs)
    // Olca Use the tid valu as MUX (connid, TID), and TID alone, due to that there is a possibility of hitting assert
    // hence removing it
    // A_ASSERT(tid <= 0x10);   
    
    p_aggr->aggr_enabled_tid_mask &= ~(1<<tid);
}



/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : aggr_reset_state
*  Returned Value : 
*  Comments       :
*  		Called when it is deemed necessary to clear the aggregate
*  		hold Q state.  Examples include when a Connect event or 
*  		disconnect event is received.
*
*END*-----------------------------------------------------------------*/ 
A_VOID
aggr_reset_state(A_VOID *cntxt)
{
    
    AGGR_INFO *p_aggr = (AGGR_INFO *)cntxt;

    A_ASSERT(p_aggr);

    p_aggr->aggr_enabled_tid_mask = 0;
}




/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : aggr_process_recv_frm
*  Returned Value : TRUE - forward frame it is in-order
*					FALSE - drop frame as its out-of-order
*  Comments       :
*        Processes a received frame to determine whether it should be 
*		 dropped.  Aggregation allows for frames to be sent out-of-order
*		 which some systems/tcp stacks fail to handle.  One option is 
*		 to queue packets in the driver and re-order them so that the
*		 driver always delivers packets to the next layer in-order. 
*		 That however requires significant buffer space and so this
* 		 implementation instead will drop any out-of-order packets.
*		 it does this by returning TRUE for in-order packets and 
*		 FALSE for out-of-order packets.  If aggregation
*		 is not enabled then there is no need to monitor the tid
*		 and the return value will always be TRUE.
*
*END*-----------------------------------------------------------------*/
A_BOOL
aggr_process_recv_frm(A_VOID *cntxt, A_UINT8 tid, A_UINT16 seq_no, A_BOOL is_amsdu, A_VOID **osbuf)
{
    AGGR_INFO *p_aggr = (AGGR_INFO *)cntxt;    
    A_BOOL result = FALSE;
    A_ASSERT(p_aggr);
    A_ASSERT(tid < NUM_OF_TIDS);   
	A_UNUSED_ARGUMENT(osbuf);
	A_UNUSED_ARGUMENT(is_amsdu);
    
    do{
    	if(!p_aggr->aggr_enabled_tid_mask & (1<<tid)) {       
        	result = TRUE;
        	break;
    	}
	    
	    if(p_aggr->seq_last[tid] > seq_no){
	    	if(p_aggr->seq_last[tid] - seq_no <= (IEEE80211_MAX_SEQ_NO>>1)){
	    		/* drop this out-of-order packet */
	    		break;
	    	}
	    }else{
	    	if(seq_no - p_aggr->seq_last[tid] > (IEEE80211_MAX_SEQ_NO>>1)){
	    		/* drop this out-of-order packet */
	    		break;
	    	}
	    }
	    /* adopt the new seq_no and allow the packet */
	    result = TRUE;
	    p_aggr->seq_last[tid] = seq_no;
	}while(0);
	
	
	return result;
}       
#endif
