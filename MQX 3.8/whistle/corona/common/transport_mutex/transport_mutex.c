/*
 * transport_mutex.c
 *
 *  Created on: May 30, 2013
 *      Author: Chris
 */

#include <mqx.h>
#include <bsp.h>
#include "wmutex.h"
#include "app_errors.h"
#include "transport_mutex.h"
#include "wassert.h"

static MUTEX_STRUCT g_transport_mutex;

void transport_mutex_blocking_lock(char *file, int line)
{
	corona_print("tran blk %s %d\n", file, line);
    wmutex_lock(&g_transport_mutex);
}

/* lock if unlocked and return TRUE. return FALSE if already locked */
boolean transport_mutex_non_blocking_lock(char *file, int line)
{
	corona_print("tran noblk %s %d\n", file, line);

	switch( _mutex_try_lock(&g_transport_mutex) )
	{
        case MQX_OK:
            return TRUE;
    
        case MQX_EDEADLK:
        case MQX_EBUSY:
            break;
    
        default:
            wassert(0);
	}
	return FALSE;
}

void transport_mutex_unlock(char *file, int line)
{
	corona_print("tran unlk %s %d\n", file, line);
    wmutex_unlock(&g_transport_mutex);
}

void transport_mutex_init(void)
{
    wmutex_init(&g_transport_mutex);
}
