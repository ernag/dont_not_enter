/*
 * pmem.c
 *
 *  Created on: Jul 15, 2013
 *      Author: SpottedLabs
 */

#include <cstdint>
#include <stdbool.h>
#include "pmem.h"

#define TRUE 1
#define FALSE 0

/*
 *   Set a persistent RAM variable.
 *   Return 0 for success, nonzero otherwise.
 */
int pmem_var_set(persist_var_t var, const void *pValue)
{
    switch(var)
    {
        case PV_DAILY_MINUTES_ACTIVE:
            memcpy( &__SAVED_ACROSS_BOOT.daily_minutes_active, pValue, sizeof(__SAVED_ACROSS_BOOT.daily_minutes_active) );
            break;
            
        case PV_MIN_ACTIVE_DAY:
            memcpy( &__SAVED_ACROSS_BOOT.minact_day, pValue, sizeof(__SAVED_ACROSS_BOOT.minact_day) );
            break;
        
        case PV_SLEEP_REL_START_MILLIS:
            memcpy( &__SAVED_ACROSS_BOOT.sleep_rel_start_millis, pValue, sizeof(__SAVED_ACROSS_BOOT.sleep_rel_start_millis) );
            break;
       
        case PV_SLEEP_REL_START_SECONDS:
            memcpy( &__SAVED_ACROSS_BOOT.sleep_rel_start_seconds, pValue, sizeof(__SAVED_ACROSS_BOOT.sleep_rel_start_seconds) );
            break;
            
        case PV_REBOOT_REASON:
            __SAVED_ACROSS_BOOT.reboot_reason = (uint16_t)(*((persist_reboot_reason_t *)pValue));

        	switch (__SAVED_ACROSS_BOOT.reboot_reason)
        	{
				case PV_REBOOT_REASON_BUTTON_MANUAL_SYNC_PENDING:
				case PV_REBOOT_REASON_FWU_COMPLETED:
					__SAVED_ACROSS_BOOT.pending_action = (uint32_t) PV_PENDING_MANUAL_SYNC;
					break;
				/*
				 * When reason = ShipMode setting pending_action to NO_ACTION because shipmode logic
				 * still driven by reboot_reason & flags (didn't want to mess with that)
				 */
				case PV_REBOOT_REASON_SHIP_MODE:
					__SAVED_ACROSS_BOOT.pending_action = (uint32_t) PV_PENDING_NO_ACTION;
					break;
				case PV_REBOOT_REASON_BUTTON_BT_PAIR_PENDING:
					__SAVED_ACROSS_BOOT.pending_action = (uint32_t) PV_PENDING_BT_PAIR;
					break;
				case PV_REBOOT_REASON_BT_FAIL:
				case PV_REBOOT_REASON_WIFI_FAIL:
					__SAVED_ACROSS_BOOT.pending_action = (uint32_t) PV_PENDING_SYNC_RETRY;
					break;
        	}
            break;
            
        case PV_REBOOT_THREAD:
            __SAVED_ACROSS_BOOT.reboot_thread = *((uint32_t *)pValue);
            break;
            
        case PV_PROXIMITY_TIMEOUT:
            memcpy( __SAVED_ACROSS_BOOT.proximity_timeout, pValue, sizeof(__SAVED_ACROSS_BOOT.proximity_timeout) );
            break;
            
        case PV_ON_PWR_VOTES:
            memcpy( &(__SAVED_ACROSS_BOOT.on_power_votes), pValue, sizeof(__SAVED_ACROSS_BOOT.on_power_votes) );
            break;
            
        case PV_UNTIMELY_CRUMB_MASK:
            memcpy( &(__SAVED_ACROSS_BOOT.untimely_crumb_mask), pValue, sizeof(__SAVED_ACROSS_BOOT.untimely_crumb_mask) );
            break;
           
        case PV_PENDING_ACTION:
            __SAVED_ACROSS_BOOT.pending_action = (uint32_t)(*( (persist_pending_action_t*) pValue ));
            break;
        
        case PV_SYNC_FAIL_REBOOT_COUNT:
        	__SAVED_ACROSS_BOOT.sync_fail_reboot_count = *( (uint16_t *) pValue );
        	break;
            
#if 0
            // memcpy() doesn't work when we are in the context of an unhandled interrupt, so we do it manually in the app.
        case PV_UNHANDLED_INT_OCCURED:
            memcpy( __SAVED_ACROSS_BOOT.unhandled_interrupt_occured, pValue, sizeof(__SAVED_ACROSS_BOOT.unhandled_interrupt_occured) );
            break;
            
        case PV_UNHANDLED_INT_TASK_ID:
            memcpy( __SAVED_ACROSS_BOOT.unhandled_int_task_id, pValue, sizeof(__SAVED_ACROSS_BOOT.unhandled_int_task_id) );
            break;
            
        case PV_UNHANDLED_INT_VECTOR:
            memcpy( __SAVED_ACROSS_BOOT.unhandled_int_vector, pValue, sizeof(__SAVED_ACROSS_BOOT.unhandled_int_vector) );
            break;
            
        case PV_UNHANDLED_INT_PC:
            memcpy( __SAVED_ACROSS_BOOT.unhandled_int_pc, pValue, sizeof(__SAVED_ACROSS_BOOT.unhandled_int_pc) );
            break;
            
        case PV_UNHANDLED_INT_LR:
            memcpy( __SAVED_ACROSS_BOOT.unhandled_int_lr, pValue, sizeof(__SAVED_ACROSS_BOOT.unhandled_int_lr) );
            break;
#endif
        default:
            return -1;
    }
    
    pmem_update_checksum();
    return 0;
}

/*
 *   Set a persistent RAM flag.
 *   Return 0 for success, nonzero otherwise.
 */
int pmem_flag_set(persist_flag_t flag, const int bValue)
{
//    wassert(sizeof(__SAVED_ACROSS_BOOT.flags) == sizeof(uint32_t));
    
    if (bValue)
    {
        __SAVED_ACROSS_BOOT.flags |= (uint32_t)(1 << flag);
    }
    else
    {
        __SAVED_ACROSS_BOOT.flags &= ~((uint32_t)(1 << flag));
    }       
    
    pmem_update_checksum();
    return 0;
}

int pmem_flag_get(persist_flag_t flag)
{
//    wassert(sizeof(__SAVED_ACROSS_BOOT.flags) == sizeof(uint32_t));
    
    return ( 1 == ((__SAVED_ACROSS_BOOT.flags >> flag) & 1) );
}

/*
 *   The saved checksum is the last byte of the pmem RAM section.
 */
unsigned char pmem_get_saved_checksum(void)
{
    return ( (uint8_t *)&__SAVED_ACROSS_BOOT )[PMEM_RAM_SZ - 1];
}

/*
 *   NOTE:  Changing the checksum mechanism means modifying boot2 to match it.
 *          In summary, don't mess with it !
 *          
 *          Checksum is all the bytes sum'd up over all 512 bytes, except the very last byte,
 *            which is where the checksum itself is stored.
 */
unsigned char pmem_calc_checksum(void)
{
    uint32_t i = 0;
    uint8_t *ptr = (uint8_t *)&__SAVED_ACROSS_BOOT;
    uint8_t sum = 0;
        
    for (i = 0; i < (PMEM_RAM_SZ - 1); i++)  // iterate over the whole struct, except the checksum itself.
    {
//        wassert( &ptr[i] != &__SAVED_ACROSS_BOOT.checksum );  // checksum can't be included.
        sum += ptr[i];
    }
    
    sum += 2; // Need to do something so that an initial set of all 0s doesn't pass checksum;
              // Incrementing this number everytime we change the structure of the __SAVED_ACROSS_BOOT
              //   struct allows us to make sure this is forced to change if FW is updated but a hard
              //   reset is not pushed.
    return sum;
}

/*
 *   pmem is considered valid if the last byte is a valid checksum of the whole 512 chunk.
 */
int pmem_is_data_valid(void)
{
    uint8_t *ptr = (uint8_t *)&__SAVED_ACROSS_BOOT;
    
    return ( pmem_calc_checksum() == pmem_get_saved_checksum() ) ? TRUE:FALSE;
}

void pmem_clear(void)
{
    memset(&__SAVED_ACROSS_BOOT, 0, PMEM_RAM_SZ);  // Do not set the CKSM here!
}

void pmem_update_checksum(void)
{
    ( (uint8_t *)&__SAVED_ACROSS_BOOT )[PMEM_RAM_SZ - 1] = pmem_calc_checksum();
}
