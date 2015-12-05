/*
 * acc_mgr.h
 *
 *  Created on: Mar 31, 2013
 *      Author: Ben McCrea
 */

#ifndef _ACC_MGR_H_
#define _ACC_MGR_H_

#include "app_errors.h"

void    ACCMGR_init ( void );
void    ACCMGR_Reset(void);
void    ACC_tsk( uint_32 dummy );
void    ACCOFFLD_tsk( uint_32 dummy );
void    ACCMGR_apply_configs( void );
void    ACCMGR_wmp_track_sleep(uint_8 sleep_start);
void    ACCMGR_log_buffer_end_time( void );
err_t   ACCMGR_start_up( void );
err_t   ACCMGR_shut_down( boolean urgent );
boolean ACCMGR_is_ready( void );
boolean ACCMGR_is_snoozing( void );
boolean ACCMGR_is_shutdown( void );
boolean ACCMGR_queue_is_empty( void );
uint_8  lis3dh_comm_ok(void);


#endif /* _ACC_MGR_H_ */
