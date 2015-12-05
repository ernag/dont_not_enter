/*
 * pwr_mgr.h
 *
 *  Created on: Apr 4, 2013
 *      Author: Ben McCrea
 */

#ifndef _PWR_MGR_H_
#define _PWR_MGR_H_

#include "app_errors.h"
#include "pmem.h"

// Votes masks for RUNM mode requests from other tasks
#define PWRMGR_USB_VOTE             0x00000001
#define PWRMGR_BTNMGR_VOTE          0x00000002
#define PWRMGR_BTMGR_VOTE           0x00000004
#define PWRMGR_WIFIMGR_VOTE         0x00000008
#define PWRMGR_CONSOLE_VOTE         0x00000010
#define PWRMGR_DISMGR_VOTE          0x00000020
#define PWRMGR_ACCMGR_VOTE          0x00000040
#define PWRMGR_EVTMGR_DUMP_VOTE     0x00000080
#define PWRMGR_EVTMGR_POST_VOTE     0x00000100
#define PWRMGR_EVTMGR_UPLOAD_VOTE   0x00000200
#define PWRMGR_PWRMGR_VOTE          0x00000400
#define PWRMGR_SHERLOCK_VOTE        0x00000800
#define PWRMGR_BURNIN_VOTE          0x00001000
#define PWRMGR_ACCOFF_VOTE          0x00002000
#define PWRMGR_PERMGR_VOTE          0x00004000
#define PWRMGR_EVTMGR_FLUSH_VOTE    0x00008000
#define PWRMGR_EXTFLASH_VOTE        0x00010000
#define PWRMGR_SYSEVENT_VOTE        0x00020000
#define PWRMGR_MAIN_TASK_VOTE       0x00040000
#define PWRMGR_CONMGR_VOTE          0x00080000
#define PWRMGR_ACCDMA_VOTE          0x00100000
#define PWRMGR_EVTMGR_ATOMIC_VOTE   0x00200000
#define PWRMGR_RTC_ALARM_VOTE		0x00400000

void    PWRMGR_init ( void );
void    PWRMGR_alpha_runtime_current_improve( void );
void    PWR_tsk( uint_32 dummy );
void    PWRMGR_Ship( boolean urgent );
void    PWRMGR_Reboot( uint_32 millisecs_until_reboot, persist_reboot_reason_t reboot_reason, boolean should_block );
void    PWRMGR_GotoSleep(void);
void    PWRMGR_SetRUNMClockSpeed( uint8_t en );
err_t   PWRMGR_VoteForRUNM( uint_32 my_vote );
err_t   PWRMGR_VoteForVLPR( uint_32 my_vote );
err_t   PWRMGR_VoteForOFF( uint_32 my_vote );
err_t   PWRMGR_VoteForON( uint_32 my_vote );
void    PWRMGR_VoteForAwake( uint_32 my_vote );
void    PWRMGR_VoteForSleep( uint_32 my_vote );
void    PWRMGR_Exit( uint_32 my_vote );
uint_8  PWRMGR_WakingFromShip( void );
uint_8  PWRMGR_WakingFromBoot( void );
uint_8  PWRMGR_process_batt_status( boolean fast_shutdown );
boolean PWRMGR_is_batt_level_safe(void);
void    PWRMGR_assert_radio_shutdowns( void );
void    PWRMGR_radios_OFF( void );
void    PWRMGR_radios_ON( void );
void    PWRMGR_enter_vlls3( void );
void    PWRMGR_enter_vlps( void );
void    PWRMGR_enter_lls( void );
uint_8  PWRMGR_is_battery_low(void);
void    PWRMGR_store_batt_count();
int32_t PWRMGR_get_batt_percent();
int32_t PWRMGR_get_stored_batt_percent( void );
uint32_t PWRMGR_get_stored_batt_count( void );
uint32_t PWRMGR_get_stored_temp_counts( void );
void PWRMGR_get_pwr_votes(uint32_t *awake, uint32_t *on, uint32_t *runm);
uint32_t PWRMGR_ms_til_reboot( void );
void    PWRMGR_watchdog_arm(uint32_t sec);
void    PWRMGR_watchdog_disarm(void);
void    PWRMGR_watchdog_refresh(void);
uint16_t PWRMGR_watchdog_reset_count(void);
void    PWRMGR_watchdog_clear_count(void);
err_t   PWRMGR_Enable26MHz_BT( boolean enable );
boolean PWRMGR_is_rebooting( void );
void    PWRMGR_charging_display(void);
void    PWRMGR_enable_motion_wakeup( uint_8 en );
boolean PWRMGR_chgB_state( void );
void    PWRMGR_process_USB( void );
boolean PWRMGR_usb_is_inserted( void );
void    PWRMGR_start_burnin( void );
void    PWRMGR_reset(boolean wait_for_wifi);
void    PWRMGR_Fail(err_t err, const char *pErr);
void    PWRMGR_breadcrumb(void);
void    PWRMGR_ship_allowed_on_usb_conn( uint_8 ok );

// DEBUG__
void PWRMGR_rtc_alarm_event( void );


#endif /* _PWR_MGR_H_ */
