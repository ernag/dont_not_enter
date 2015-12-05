/*
 * per_mgr.h
 *
 *  Created on: Oct 8, 2013
 *      Author: Chris
 */

#ifndef PER_MGR_H_
#define PER_MGR_H_


void PERMGR_set_timeout(uint_32 seconds);
void PERMGR_init(void);
void PER_tsk(uint_32 dummy);
void PERMGR_stop_timer(void);


#endif /* PER_MGR_H_ */
