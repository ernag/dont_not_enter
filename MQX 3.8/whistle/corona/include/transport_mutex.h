/*
 * transport_mutex.h
 *
 *  Created on: May 30, 2013
 *      Author: Chris
 */

#ifndef TRANSPORT_MUTEX_H_
#define TRANSPORT_MUTEX_H_

void transport_mutex_init(void);
void transport_mutex_blocking_lock(char *file, int line);
boolean transport_mutex_non_blocking_lock(char *file, int line);
void transport_mutex_unlock(char *file, int line);

#endif /* TRANSPORT_MUTEX_H_ */
