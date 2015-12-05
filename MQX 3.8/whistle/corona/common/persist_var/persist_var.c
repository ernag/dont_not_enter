/*
 * persist_var.c
 *
 *  Created on: Jun 5, 2013
 *      Author: SpottedLabs
 */

#include <mqx.h>
#include <mutex.h>
#include "app_errors.h"
#include "wassert.h"
#include "RNG1.h"
#include "persist_var.h"
#include "pmem.h"
#include "hwrand.h"
#include "time_util.h"
#include "wmp_datadump_encode.h"

/*
 *   Define untimely crumb mask here.
 */
#define CRUMB_EVTMGR_RESET                  0x00000001
#define CRUMB_EXT_FLASH_WIP_AT_BOOT         0x00000002
#define CRUMB_EXT_FLASH_IN_USE_AT_REBOOT    0x00000004
#define CRUMB_NON_GRACEFUL_ASSERT           0x00000008
#define CRUMB_EVTMGR_INIT                   0x00000010
#define CRUMB_CFG_DY_DEC_FAILED             0x00000020
#define CRUMB_CFG_DY_RESET                  0x00000040
#define CRUMB_EVTMGR_INVALID_ADDR           0x00000080
#define CRUMB_EVTNGR_NO_EVT_BLK_PTRS        0x00000100
#define CRUMB_EXIT_SHIP_MODE				0x00000200
#define CRUMB_PMEM_INVALID_CRC              0x00000400
#define CRUMB_PMEM_INVALID_MAGIC            0x00000800
#define CRUMB_CFG_DY_DEC_OOB                0x00001000
#define CRUMB_CFG_DY_ENC_OOB                0x00002000
#define CRUMB_CFG_DY_ENC_FAILED             0x00004000
#define CRUMB_INVALID_MASK                  0xDEADBEEF     // This might not be invalid forever (though EXTREMELY unlikely).

extern persist_pending_action_t g_last_pending_action;

uint8_t persist_app_has_initialized(void)
{
    return (APP_MAGIC_INIT_VAL == __SAVED_ACROSS_BOOT.app_initialized_magic)?1:0;
}

/*
 *   Confirms that the app is validating a previously initialized (and therefore valid) pmem.
 */
static void _persist_ok(void)
{
    wassert( persist_app_has_initialized() );
}

/*
 *   Since we don't want to waste a bunch of space storing every crumb,
 *     we store a bitfield that maps to each _special_ crumb.
 */
static uint32_t _persist_get_mask_from_crumb( BreadCrumbEvent crumb )
{
    switch( crumb )
    {
        case BreadCrumbEvent_EVTMGR_RESET:
            return CRUMB_EVTMGR_RESET;
            
        case BreadCrumbEvent_EVTMGR_INIT:
            return CRUMB_EVTMGR_INIT;
            
        case BreadCrumbEvent_EXT_FLASH_WIP_AT_BOOT:
            return CRUMB_EXT_FLASH_WIP_AT_BOOT;
            
        case BreadCrumbEvent_EXT_FLASH_IN_USE_AT_REBOOT:
            return CRUMB_EXT_FLASH_IN_USE_AT_REBOOT;
            
        case BreadCrumbEvent_NON_GRACEFUL_ASSERT:
            return CRUMB_NON_GRACEFUL_ASSERT;
            
        case BreadCrumbEvent_DYNAMIC_CFG_DECODE_FAILED:
            return CRUMB_CFG_DY_DEC_FAILED;
            
        case BreadCrumbEvent_DYNAMIC_CFG_ENCODE_FAILED:
            return CRUMB_CFG_DY_ENC_FAILED;
            
        case BreadCrumbEvent_DYNAMIC_CFG_RESET:
            return CRUMB_CFG_DY_RESET;
            
        case BreadCrumbEvent_EVTMGR_INVALID_ADDRESS:
            return CRUMB_EVTMGR_INVALID_ADDR;
            
        case BreadCrumbEvent_EVTMGR_NO_EVT_BLK_PTRS:
            return CRUMB_EVTNGR_NO_EVT_BLK_PTRS;
            
        case BreadCrumbEvent_EXITING_SHIP_MODE:
        	return CRUMB_EXIT_SHIP_MODE;
        	
        case BreadCrumbEvent_PMEM_RESET_CRC_INVALID:
            return CRUMB_PMEM_INVALID_CRC;
            
        case BreadCrumbEvent_PMEM_RESET_MAGIC_INVALID:
            return CRUMB_PMEM_INVALID_MAGIC;
            
        case BreadCrumbEvent_DYNAMIC_CFG_DEC_OUT_OF_BOUNDS:
            return CRUMB_CFG_DY_DEC_OOB;
            
        case BreadCrumbEvent_DYNAMIC_CFG_ENC_OUT_OF_BOUNDS:
            return CRUMB_CFG_DY_ENC_OOB;
            
            
        default:
            break;
    }
    
    return CRUMB_INVALID_MASK;
}

/*
 *   Pass in a single crumb mask that maps to a Bread Crumb.
 *     The corresponding crumb is returned.
 *     Zero is a bogus crumb.
 */
static BreadCrumbEvent _persist_get_crumb_from_mask( uint32_t mask )
{
    if( mask & CRUMB_EVTMGR_RESET )
    {
        return BreadCrumbEvent_EVTMGR_RESET;
    }
    
    if( mask & CRUMB_EVTMGR_INIT )
    {
        return BreadCrumbEvent_EVTMGR_INIT;
    }
    
    if( mask & CRUMB_EXT_FLASH_WIP_AT_BOOT )
    {
        return BreadCrumbEvent_EXT_FLASH_WIP_AT_BOOT;
    }
    
    if( mask & CRUMB_EXT_FLASH_IN_USE_AT_REBOOT )
    {
        return BreadCrumbEvent_EXT_FLASH_IN_USE_AT_REBOOT;
    }
    
    if( mask & CRUMB_NON_GRACEFUL_ASSERT )
    {
        return BreadCrumbEvent_NON_GRACEFUL_ASSERT;
    }
    
    if( mask & CRUMB_CFG_DY_DEC_FAILED )
    {
        return BreadCrumbEvent_DYNAMIC_CFG_DECODE_FAILED;
    }
    
    if( mask & CRUMB_CFG_DY_ENC_FAILED )
    {
        return BreadCrumbEvent_DYNAMIC_CFG_ENCODE_FAILED;
    }
    
    if( mask & CRUMB_CFG_DY_RESET )
    {
        return BreadCrumbEvent_DYNAMIC_CFG_RESET;
    }
    
    if( mask & CRUMB_EVTMGR_INVALID_ADDR )
    {
        return BreadCrumbEvent_EVTMGR_INVALID_ADDRESS;
    }
    
    if( mask & CRUMB_EVTNGR_NO_EVT_BLK_PTRS )
    {
        return BreadCrumbEvent_EVTMGR_NO_EVT_BLK_PTRS;
    }
    
    if( mask & CRUMB_EXIT_SHIP_MODE )
    {
    	return BreadCrumbEvent_EXITING_SHIP_MODE;
    }
    
    if( mask & CRUMB_PMEM_INVALID_CRC )
    {
        return BreadCrumbEvent_PMEM_RESET_CRC_INVALID;
    }
    
    if( mask & CRUMB_PMEM_INVALID_MAGIC )
    {
        return BreadCrumbEvent_PMEM_RESET_MAGIC_INVALID;
    }
    
    if( mask & CRUMB_CFG_DY_DEC_OOB )
    {
        return BreadCrumbEvent_DYNAMIC_CFG_DEC_OUT_OF_BOUNDS;
    }
    
    if( mask & CRUMB_CFG_DY_ENC_OOB )
    {
        return BreadCrumbEvent_DYNAMIC_CFG_ENC_OUT_OF_BOUNDS;
    }

    return 0;
}

/*
 *   If crumb_mask is non-zero, log any crumbs to EVTMGR that are related to it.
 */
void _persist_log_untimely_crumb( uint32_t crumb_mask )
{
    uint32_t bit = 1;
    
    if( 0 == crumb_mask )
    {
        return;
    }
    
    corona_print("\tUntimely Crumbs %x\n", crumb_mask);
    
    while(1)
    {
        if( crumb_mask & bit )
        {
            WMP_post_breadcrumb( _persist_get_crumb_from_mask(bit) );
            
            /*
             *    We have some extra information for the non-graceful wassert(0) crumb.
             */
            if( crumb_mask & CRUMB_NON_GRACEFUL_ASSERT )
            {
                PROCESS_NINFO( ERR_NON_GRACEFUL_ASSERT_VERBOSE,
                             "rsn %u LR %x tsk %x ln %u",
                             __SAVED_ACROSS_BOOT.non_graceful_assert_reason,
                             __SAVED_ACROSS_BOOT.arm_exception_LR,
                             __SAVED_ACROSS_BOOT.arm_exception_task_id,
                             __SAVED_ACROSS_BOOT.arm_exception_task_addr );
                
                // Reset state of the related variables.
                wint_disable();
                __SAVED_ACROSS_BOOT.non_graceful_assert_reason = 0;
                __SAVED_ACROSS_BOOT.arm_exception_LR = 0;
                __SAVED_ACROSS_BOOT.arm_exception_task_id = 0;
                __SAVED_ACROSS_BOOT.arm_exception_task_addr = 0;
                pmem_update_checksum();
                wint_enable();
            }
            
            /*
             *   We have some extra information for the EXT Flash in use crumb.
             */
            if( crumb_mask & CRUMB_EXT_FLASH_IN_USE_AT_REBOOT )
            {
                PROCESS_NINFO( ERR_EFLASH_IN_USE_AT_REBOOT, 
                             "addr %x tsk %x",
                             __SAVED_ACROSS_BOOT.eflash_in_use_addr,
                             __SAVED_ACROSS_BOOT.eflash_in_use_task_id );
            }
        }
        
        if( 0x80000000 == bit )
        {
            /*
             *   We've tried all the bits, time to clear the crumb mask for next time.
             */
            bit = 0;
            _corona_pmem_var_set( PV_UNTIMELY_CRUMB_MASK, &bit );
            return;
        }
        
        bit = bit << 1;
    }
}

/*
 *   Some crumbs happen when we aren't at liberty to store them to SPI Flash.
 *   This function will store them in pmem, then we can dig them out later.
 *     Examples include:
 *      - when you are almost shutting down/rebooting.
 *      - when EVTMGR is totally corrupted and you are rebooting.
 */
void persist_set_untimely_crumb( BreadCrumbEvent crumb )
{
    uint32_t mask = _persist_get_mask_from_crumb(crumb);
    
    if( CRUMB_INVALID_MASK != mask )
    {
        mask |= __SAVED_ACROSS_BOOT.untimely_crumb_mask;
        _corona_pmem_var_set(PV_UNTIMELY_CRUMB_MASK, &mask);
    }
}

void _corona_pmem_var_set(persist_var_t var, const void *pValue)
{
    _persist_ok();
    
    wint_disable();
    wassert( 0 == pmem_var_set(var, pValue) );
    wint_enable();
}

int _corona_pmem_flag_set(persist_flag_t flag, const int bValue)
{
    int ret;
    
    _persist_ok();
    
    wint_disable();
    ret = pmem_flag_set(flag, bValue);
    wint_enable();
    
    return ret;
}

void _corona_pmem_update_checksum(void)
{
    _persist_ok();
    
    wint_disable();
    pmem_update_checksum();
    wint_enable();
}

unsigned char _corona_pmem_calc_checksum(void)
{
    unsigned char checksum;
    
    wint_disable();
    checksum = pmem_calc_checksum();
    wint_enable();
    
    return checksum;
}

void _corona_pmem_clear(void)
{
    wint_disable();
    pmem_clear();
    wint_enable();
}

void PERSIST_init(void)
{
    wassert( sizeof(__SAVED_ACROSS_BOOT) <= (512-1) );  // must fit into 511 bytes, 1 byte reserved for CRC.
}

void persist_var_init()
{
    uint32_t randnum;
    
    //corona_print("\t*\tP <SEED>\n");  // happens before console init... so it's pointless.
    
    wint_disable();
    pmem_clear();
    hwrand_uint32(&randnum);
    
    /*
     *   NOTE:  There is no reason to initialize variables to ZERO here,
     *          since we called pmem_clear() above.
     */
    __SAVED_ACROSS_BOOT.boot_sequence = (uint16_t) (0xFFFF & (randnum >> 16) );
    __SAVED_ACROSS_BOOT.reboot_reason = (uint_16)PV_REBOOT_REASON_UNKNOWN;
    __SAVED_ACROSS_BOOT.pending_action = (uint_32) PV_PENDING_NO_ACTION;
    __SAVED_ACROSS_BOOT.app_initialized_magic = APP_MAGIC_INIT_VAL;

    UTIL_time_unset((whistle_time_t *)__SAVED_ACROSS_BOOT.proximity_timeout);
    
    _persist_ok();
    pmem_update_checksum();
    wint_enable();
}

void persist_print_array()
{
    uint32_t i;
    uint8_t *ptr;
    
    _persist_ok();
    ptr = (uint8_t*) &__SAVED_ACROSS_BOOT;
    corona_print("PRST (%d bytes)\n", sizeof(__SAVED_ACROSS_BOOT));
    
    for (i = 0; i < sizeof(__SAVED_ACROSS_BOOT); i++)
    {
        corona_print("0x%.2X  ", ptr[i]);
    }
    
    corona_print("\n");
}

void print_persist_data()
{
    _persist_ok();
    corona_print("PRST (%d bytes)\n", sizeof(__SAVED_ACROSS_BOOT));
    
    if ( pmem_calc_checksum() == pmem_get_saved_checksum() )
    {
        corona_print(" BootSeq %u\n", __SAVED_ACROSS_BOOT.boot_sequence);
        corona_print(" Offset %u.%d\n", __SAVED_ACROSS_BOOT.server_offset_seconds,__SAVED_ACROSS_BOOT.server_offset_millis);
#if (CALCULATE_MINUTES_ACTIVE)
        corona_print(" MINACT %u\n", __SAVED_ACROSS_BOOT.daily_minutes_active);
        corona_print(" MINACT DAY: %u\n", __SAVED_ACROSS_BOOT.minact_day);
#endif
        corona_print(" Flags %x\n", __SAVED_ACROSS_BOOT.flags);
        corona_print(" RebootR %u\n", __SAVED_ACROSS_BOOT.reboot_reason);
        corona_print(" RebootT %u\n", __SAVED_ACROSS_BOOT.reboot_thread);
        corona_print(" On Votes %x\n", __SAVED_ACROSS_BOOT.on_power_votes);
        corona_print(" exp mgc %x\n", __SAVED_ACROSS_BOOT.arm_exception_occured);
        corona_print(" exp vect %u\n", __SAVED_ACROSS_BOOT.arm_interrupt_vector);
        corona_print(" exp tsk %x addr %x\n", __SAVED_ACROSS_BOOT.arm_exception_task_id, __SAVED_ACROSS_BOOT.arm_exception_task_addr);
        corona_print(" eflash tsk %x addr %x\n", __SAVED_ACROSS_BOOT.eflash_in_use_task_id, __SAVED_ACROSS_BOOT.eflash_in_use_addr);
        if (g_last_pending_action != 0)
        {
			corona_print("\tPendingA %u\n", g_last_pending_action);
        }
        if (__SAVED_ACROSS_BOOT.sync_fail_reboot_count != 0)
        {
			corona_print("\tWiFi Rtry %d\n", __SAVED_ACROSS_BOOT.sync_fail_reboot_count);
        }
        
        /****** HACK WARNING!! We're not supposed to know how UTIL_timers are implemented here *****/
        {
            whistle_time_t now;
            // BEN: RTC time is used for proximity manager, so we want RTC time here, too.
            //UTIL_time_get(&now);
            RTC_get_time_ms_ticks( &now );
            corona_print(" Prox TO %d (raw %u %u %u)\n", UTIL_time_diff_seconds((whistle_time_t *)__SAVED_ACROSS_BOOT.proximity_timeout, &now),
            		__SAVED_ACROSS_BOOT.proximity_timeout[0], __SAVED_ACROSS_BOOT.proximity_timeout[1], __SAVED_ACROSS_BOOT.proximity_timeout[2]);
        }
        
        corona_print(" CRC 0x%0.2X (%d)\n", pmem_get_saved_checksum(), pmem_is_data_valid());
    }
    else
    {
        WPRINT_ERR("Cksm");
    }
}
