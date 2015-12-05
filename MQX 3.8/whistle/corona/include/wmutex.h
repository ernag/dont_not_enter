/*
 * wmutex.h
 *
 *  Created on: Oct 28, 2013
 *      Author: Ernie
 */

#ifndef WMUTEX_H_
#define WMUTEX_H_

#include <mutex.h>

void wmutex_init( MUTEX_STRUCT_PTR ptr );
void wmutex_lock( MUTEX_STRUCT_PTR ptr );
void wmutex_unlock( MUTEX_STRUCT_PTR ptr );

#endif /* WMUTEX_H_ */
