/*
 * hardware_tests.c
 *
 *  Created on: Mar 12, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "wson_flash_test.h"
#include "adc_sense.h"
#include "apple_mfi.h"
#include "accel_test.h"
#include "bu26507_display.h"
#include "gpio.h"
#include "bt_uart.h"
#include "ar4100p.h"
#include "crc_test.h"
#include "wassert.h"
#include "pwr_mgr.h"
#include "main.h"

/*
 *   Toggle the 26MHz BT to see if 32KHz goes bonkers.
 */
static void silego_test(void)
{
    LWGPIO_STRUCT bt_pwdn_b;
    
    PWRMGR_VoteForRUNM( PWRMGR_BURNIN_VOTE );
    
    whistle_task_destroy(ACC_TASK_NAME);
    whistle_task_destroy(ACCOFF_TASK_NAME);
    whistle_task_destroy(FWU_TASK_NAME);
    whistle_task_destroy(PER_TASK_NAME);
    whistle_task_destroy(UPLOAD_TASK_NAME);
    whistle_task_destroy(DUMP_TASK_NAME);
    whistle_task_destroy(BTNMGR_TASK_NAME);
    whistle_task_destroy(BT_TASK_NAME);
    
    corona_print("SILEGO TEST\n");
    
    wassert( lwgpio_init(&bt_pwdn_b, BSP_GPIO_BT_PWDN_B_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE) );
    lwgpio_set_functionality(&bt_pwdn_b, BSP_GPIO_BT_PWDN_B_MUX_GPIO);

    while(1)
    {
        printf("v"); fflush(stdout);
        lwgpio_set_value(&bt_pwdn_b, LWGPIO_VALUE_LOW);
        _time_delay(500);
        
        // low to high transition is not problematic.
        // high to low transition is problematic, so give that one more time.
        
        printf("^"); fflush(stdout);
        lwgpio_set_value(&bt_pwdn_b, LWGPIO_VALUE_HIGH);
        _time_delay(1500);
    }
}

void hardware_tests(void)
{
//    crc_mutex_test();
//    SHA256_unit_test();
//    silego_test();
//    crc_test();
//    wson_flash_test();
//    wson_flash_torture();
//    adc_sense_test();
//    apple_mfi_test();
//    accel_spidma_test();
//    bu26507_display_test();
//    gpio_test();
//    bt_uart_test();
//    ar4100p_test();
#if 0
    while(1)
    {
        PWRMGR_VoteForAwake( PWRMGR_BURNIN_VOTE );
        wson_flash_sector_test();
    }
#endif
    
}
#endif
