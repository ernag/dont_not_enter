/*
 * wmutex.c
 * 
 * Standard Whistle API for creating, locking, and unlocking mutexes.
 *
 *  Created on: Oct 28, 2013
 *      Author: Ernie
 */

#include <mqx.h>
#include "wassert.h"
#include "wmutex.h"
#include "app_errors.h"

/*
 *   Initialize mutex, and check errors / assert() if necessary.
 */
void wmutex_init( MUTEX_STRUCT_PTR ptr )
{
    MUTEX_ATTR_STRUCT mutexattr;
    _mqx_uint ret;
    err_t err;
    
    ret = _mutatr_init(&mutexattr);
    if( MQX_OK != ret )
    {
        err = ERR_MUTEX_ATTR;
        goto wmutex_init_done;
    }
    mutexattr.SCHED_PROTOCOL = MUTEX_PRIO_INHERIT;
    mutexattr.WAIT_PROTOCOL = MUTEX_PRIORITY_QUEUEING;
    
    ret = _mutex_init(ptr, &mutexattr);
    if( MQX_OK != ret )
    {
        err = ERR_MUTEX_INIT;
        goto wmutex_init_done;
    }
    else
    {
        return;
    }
    
wmutex_init_done:
    if( MQX_OK != ret )
    {
        uint_32 u32_LR = 0;  // holds the link register for more debugging help.
        
        __asm
        {
            mov u32_LR, LR
        }
        
        PROCESS_NINFO( err, "%u tsk %x LR %x ptr %x", ret, _task_get_id(), u32_LR, ptr );
        wassert(0);
    }
}

/*
 *   Lock the mutex, log error if needed.
 */
void wmutex_lock( MUTEX_STRUCT_PTR ptr )
{
    _mqx_uint ret;
    
    ret = _mutex_lock(ptr);
    if( MQX_OK != ret )
    {
        uint_32 u32_LR = 0;  // holds the link register for more debugging help.
        
        __asm
        {
            mov u32_LR, LR
        }
        
        PROCESS_NINFO( ERR_MUTEX_LOCK, "%u tsk %x LR %x ptr %x", ret, _task_get_id(), u32_LR, ptr );
        goto wmutex_lock_done;
    }
    else
    {
        return;
    }

wmutex_lock_done:
    if( MQX_OK != ret )
    {
        wassert(0);
    }
}

/*
 *   Unlock the mutex, log error if needed.
 */
void wmutex_unlock( MUTEX_STRUCT_PTR ptr )
{
    _mqx_uint ret;
    
    ret = _mutex_unlock(ptr);
    if( MQX_OK != ret )
    {
        uint_32 u32_LR = 0;  // holds the link register for more debugging help.
        
        __asm
        {
            mov u32_LR, LR
        }
        
        PROCESS_NINFO( ERR_MUTEX_UNLOCK, "%u tsk %x LR %x ptr %x", ret, _task_get_id(), u32_LR, ptr );
        goto wmutex_unlock_done;
    }
    else
    {
        return;
    }

wmutex_unlock_done:
    if( MQX_OK != ret )
    {
        wassert(0);
    }
}
