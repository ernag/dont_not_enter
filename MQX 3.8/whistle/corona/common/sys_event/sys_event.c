/*
 * sys_event.c
 *
 *  Created on: Mar 21, 2013
 *      Author: Chris
 */

#include "sys_event.h"
#include "wassert.h"
#include "pwr_mgr.h"
#include <watchdog.h>

#define MAX_NUM_CALLBACKS    10

struct sys_event_callback_s
{
    sys_event_t event; // event this callback cares about
    sys_event_cbk_t cb; // callback
};

/*
 *    Holds all the callback addresses.
 */
static LWEVENT_STRUCT the_events;
static struct sys_event_callback_s g_sys_event_callbacks[MAX_NUM_CALLBACKS] =
{ 0 };

static void sys_event_broadcast(sys_event_t event);

void SYSEVT_tsk(uint_32 dummy)
{
    _mqx_uint signaled_events, an_event;
    uint8_t i;
    
    while (1)
    {
        _watchdog_stop();
        
        PWRMGR_VoteForOFF(PWRMGR_SYSEVENT_VOTE);
        PWRMGR_VoteForSleep(PWRMGR_SYSEVENT_VOTE);

        /* wait forever for any event(s) */
        _lwevent_wait_ticks(&the_events, 0xffffffff, FALSE, 0);
        
        PWRMGR_VoteForON(PWRMGR_SYSEVENT_VOTE);  // This ON vote should be redundant, but do it jic
        PWRMGR_VoteForAwake(PWRMGR_SYSEVENT_VOTE);
        
        _watchdog_start(60*1000); // who knows what threads will do with these events. give plenty of time.
        
        signaled_events = _lwevent_get_signalled();
        
        for (i = 0; i < MQX_INT_SIZE_IN_BITS; ++i)
        {
            an_event = signaled_events & (1 << i);
            if (an_event)
            {
                sys_event_broadcast(an_event);
            }
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Initialize sys_event system
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function initializes the sys ISR and clears interrupt flags.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
void sys_event_init(void)
{
    _lwevent_create(&the_events, LWEVENT_AUTO_CLEAR);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Broadcast a sys event to all the folks who've registered callbacks.
//!
//! \fntype Non-Reentrant Function
//!
//! \return None
//!
///////////////////////////////////////////////////////////////////////////////
static void sys_event_broadcast(sys_event_t event)
{
    int i;
    
    for (i = 0; i < MAX_NUM_CALLBACKS; i++)
    {
        if ((g_sys_event_callbacks[i].cb != NULL)
                && (g_sys_event_callbacks[i].event & event))
        {
            g_sys_event_callbacks[i].cb(event);
        }
    }
}

/*
 *   Callback registration API
 */
err_t sys_event_register_cbk(sys_event_cbk_t pCallback, sys_event_t event)
{
    int i;
    
    for (i = 0; i < MAX_NUM_CALLBACKS; i++)
    {
        if (g_sys_event_callbacks[i].cb == NULL)
        {
            g_sys_event_callbacks[i].cb = pCallback;
            g_sys_event_callbacks[i].event = event;
            return ERR_OK;
        }
    }
    
    wassert(0);
    return ERR_GENERIC;
}

#if 0
/*
 *   Callback UN-registration API
 *   NOTE:  commenting out since nobody actually uses this.
 */
err_t sys_event_unregister_cbk(sys_event_cbk_t pCallback)
{
    int i;
    
    for (i = 0; i < MAX_NUM_CALLBACKS; i++)
    {
        if (g_sys_event_callbacks[i].cb == pCallback)
        {
            g_sys_event_callbacks[i].cb = NULL;
            return ERR_OK;
        }
    }

    return ERR_GENERIC;
}
#endif

void sys_event_post(sys_event_t event)
{
    PWRMGR_VoteForON(PWRMGR_SYSEVENT_VOTE);  // here guarantees task runs
    _lwevent_set(&the_events, event);
}
