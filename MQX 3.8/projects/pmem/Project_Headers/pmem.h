/*
 * pmem.h
 *
 *  Created on: Jul 15, 2013
 *      Author: SpottedLabs
 */

#ifndef PMEM_H_
#define PMEM_H_

/*
 *   Applications must include these sections in their linker files.
 */
#define PMEM_TXT_SEC   ".text.PMEM"

/*
 *   Size allocated to pmem in RAM
 */
#define PMEM_RAM_SZ     (512)

/*
 *   Magic number to say unhandled interrupt occured.
 */
#define EXCEPTION_MAGIC     (0xFACE)

/*
 *   Magic number to tell us whether or not the app has intialized pmem.
 */
#define APP_MAGIC_INIT_VAL      (0xFACEFEED)

/*
 * Note: When changing persistent_data struct please increment the adder in the
 * checksum calculation.
 * 
 *   NOTE:  Total length of the persistent section is 512 bytes, so we cannot exceed that,
 *          unless we change intflash.lcf file to make some more room.
 *          
 ***** WARNING: DO NOT PACK THIS STRUCT. SOME MEMBER VARIABLES NEED TO BE 4-byte aligned *****
 */
typedef struct persistent_data 
{
    uint16_t boot_sequence;
    
    int16_t server_offset_millis;
    int32_t server_offset_seconds;
    
    uint16_t sleep_rel_start_millis;
    uint32_t sleep_rel_start_seconds;
    
    // Unallocated, set to 0
    uint8_t unallocated[6];
    
    uint8_t non_graceful_assert_reason;
    
    uint16_t daily_minutes_active;
    
    uint16_t minact_day;
    
    uint32_t flags;
    
    uint16_t reboot_reason;
    
    uint32_t reboot_thread;
    
    uint32_t proximity_timeout[3]; // sizeof(MQX_TICK_STRUCT)
                                   // cheating here. should be uint8_t[12], but we need to be 4-byte aligned
    
    /*
     *    ARM exception handling.
     *      Since we can't use interrupts and everything pretty much dies when an exception or unhandled interrupt occurs,
     *      we use PMEM to store information about the issue, which we try to store in EVTMGR after the fact on the next reboot.
     */
    uint16_t arm_exception_occured;    // if (this == EXCEPTION_MAGIC), we had an exception (#3) or an un-handled interrupt.
    uint32_t arm_exception_task_id;    // MQX task ID when the exception occured.
    uint32_t arm_exception_task_addr;  // MQX task address in Internal FLASH when the exception occured.
    uint32_t arm_interrupt_vector;     // ARM interrupt vector.  If (this == #3), its an exception/abort.  Otherwise it was an un-handled interrupt at that vector.
    uint32_t arm_exception_PC;         // Value of the Program Counter when the exception happened.  Probably bogus for the unhandled case.
    uint32_t arm_exception_LR;         // Value of the Link Register when the exception happened.  Probably bogus for the unhandled case.
    
    uint32_t on_power_votes;
    
    uint32_t untimely_crumb_mask;   // store any crumbs that happened, that you want to track next boot.
    
    uint32_t pending_action; // Action to be accomplished upon boot.
    
    uint16_t sync_fail_reboot_count; // tracks contiguous sync reboot counts
    
    uint32_t app_initialized_magic;  // will be APP_MAGIC_INIT_VAL if app has initialized.
    
    // eflash Tools for help to debug EXT_FLASH_IN_USE issues.
    uint32_t eflash_in_use_task_id;  // the task ID in use last when EXT FLASH was in use.
    uint32_t eflash_in_use_addr;     // the last address being written or erased when EXT FLASH was in use.
    
    // NOTE:  checksum is considered the last (511th) byte in the persistent data struct.
    //uint8_t checksum;

} persistent_data_t;

/*
 *   NOTE:  Do not change order.  Add new reasons to the BOTTOM of the list.
 */
typedef enum persist_reboot_reason_type
{
    
	PV_REBOOT_REASON_UNKNOWN,                           // 0
	PV_REBOOT_REASON_WHISTLE_ASSERT,
	PV_REBOOT_REASON_WIFI_FAIL,
	PV_REBOOT_REASON_BT_FAIL,
	PV_REBOOT_REASON_REBOOT_HACK,
	PV_REBOOT_REASON_USER_REQUESTED,                    // 5
	PV_REBOOT_REASON_WATCHDOG,
	PV_REBOOT_REASON_FWU_COMPLETED,
	PV_REBOOT_REASON_APP_ERR,
	PV_REBOOT_REASON_UNPLUG_BURNIN,
	PV_REBOOT_REASON_BUTTON_MANUAL_SYNC_PENDING,        // 10       
	PV_REBOOT_REASON_BUTTON_BT_PAIR_PENDING,
	PV_REBOOT_REASON_BUTTON_HACK_BT_RECONNECT_PENDING,
	PV_REBOOT_REASON_UNHANDLED_INTERRUPT_EXCEPTION,
	PV_REBOOT_REASON_SHIP_MODE,                         // 14
	PV_REBOOT_ACCEL_FAIL,
	
	// add new events above this line, and increment number
	PV_REBOOT_REASON_FIRST_INVALID_VAL
}persist_reboot_reason_t;

typedef enum persist_variable_type
{
    PV_DAILY_MINUTES_ACTIVE,
    PV_MIN_ACTIVE_DAY,
    PV_SLEEP_REL_START_MILLIS,
    PV_SLEEP_REL_START_SECONDS,
    PV_REBOOT_REASON,
    PV_REBOOT_THREAD,
    PV_PROXIMITY_TIMEOUT,
    PV_UNHANDLED_INT_OCCURED,
    PV_UNHANDLED_INT_TASK_ID,
    PV_UNHANDLED_INT_VECTOR,
    PV_UNHANDLED_INT_PC,
    PV_UNHANDLED_INT_LR,
    PV_ON_PWR_VOTES,
    PV_UNTIMELY_CRUMB_MASK,
    PV_PENDING_ACTION,
    PV_SYNC_FAIL_REBOOT_COUNT
}persist_var_t;

typedef enum persist_pending_action_type
{
	PV_PENDING_NO_ACTION,
	PV_PENDING_MANUAL_SYNC,
	PV_PENDING_SHIP_MODE, //Not currently implemented
	PV_PENDING_BT_PAIR,
	PV_PENDING_SYNC_RETRY
} persist_pending_action_t;

typedef enum persist_flag_type
{
    PV_FLAG_POWER_ON_RESET,  // power on vs. reboot
    PV_FLAG_GO_TO_SHIP_MODE, // Boot2 messaging the app to go to ship mode.
    PV_FLAG_BURNIN_TESTBRD_IN_PROGRESS,
    PV_FLAG_IS_SYNC_MANUAL, // is the current sync manual or auto?
    PV_FLAG_ACCEL_WAS_SLEEPING, // was the device sleeping?
    PV_FLAG_BURNIN_PWR_DRAIN_SUCCESS,
    PV_FLAG_BURNIN_PWR_DRAIN_RUNNING,
    PV_FLAG_BURNIN_PWR_DRAIN_FAILURE,
    PV_FLAG_FWU, // boot2 firmware update occurred
    PV_FLAG_PREFER_WIFI, // prefer wifi instead of bt
    PV_FLAG_2ND_SYNC_PENDING // still need to sync 10 min after initial LM setup
}persist_flag_t;

extern persistent_data_t __SAVED_ACROSS_BOOT;

int  pmem_var_set(persist_var_t var, const void *pValue) __attribute__ ((section (PMEM_TXT_SEC)));
int  pmem_flag_set(persist_flag_t flag, const int bValue) __attribute__ ((section (PMEM_TXT_SEC)));
int  pmem_flag_get(persist_flag_t flag) __attribute__ ((section (PMEM_TXT_SEC)));
void pmem_update_checksum(void) __attribute__ ((section (PMEM_TXT_SEC)));
void pmem_clear(void) __attribute__ ((section (PMEM_TXT_SEC)));
int  pmem_is_data_valid(void) __attribute__ ((section (PMEM_TXT_SEC)));
unsigned char pmem_calc_checksum(void) __attribute__ ((section (PMEM_TXT_SEC)));
unsigned char pmem_get_saved_checksum(void) __attribute__ ((section (PMEM_TXT_SEC)));

#endif /* PMEM_H_ */
