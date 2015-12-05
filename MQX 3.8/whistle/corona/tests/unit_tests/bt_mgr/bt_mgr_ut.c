/*
 * bt_mgr_ut.c
 *
 *  Created on: Apr 19, 2013
 *      Author: Chris
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <mqx.h>
#include <bsp.h>
#include "wassert.h"
#include <app_errors.h>
#include "bt_mgr.h"
#include "main.h"
#include "pwr_mgr.h"

char *hello = "hello world";
volatile int chris = 1;

static void _bt_init_ut(void)
{
	int stack_id;
	int iteration = 0;
	
	PWRMGR_VoteForRUNM( PWRMGR_BTMGR_VOTE );
	
    whistle_task_destroy(ACC_TASK_NAME);
    whistle_task_destroy(ACCOFF_TASK_NAME);
    whistle_task_destroy(FWU_TASK_NAME);
    whistle_task_destroy(PER_TASK_NAME);
    whistle_task_destroy(UPLOAD_TASK_NAME);
    whistle_task_destroy(DUMP_TASK_NAME);
    whistle_task_destroy(BTNMGR_TASK_NAME);
    whistle_task_destroy(BT_TASK_NAME);
	
	while(1)
	{
		stack_id = OpenStack();
		corona_print("\n\n\t***\t [ %u ] \t***\n\nStack ID: %i\n", iteration, stack_id);

		if(stack_id <= 0)
		{
			corona_print("\n\n\t############## OpenStack() Failed!\n\n");
			fflush(stdout);
			while(1){}
		}
		iteration++;

		corona_print("CloseStack()\n");
		CloseStack();

#if 0
		if(iteration == 15)
		{
			PWRMGR_Reboot(0, PV_REBOOT_REASON_REBOOT_HACK, 1);
		}
#endif
	}
}

void bt_mgr_ut(void)
{
    int_32                received;
    unsigned char         response[8];
    err_t                 err;
    uint_32 length;
    int_32 sent;
    int handle;
    BD_ADDR_t ANY_ADDR;
	LmMobileStatus lmMobileStatus;
	
	_bt_init_ut();
	return;
    
    ASSIGN_BD_ADDR(ANY_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00); // NULL means any
    
    length = (uint_32)strlen(hello);
    
    while (ERR_OK != BTMGR_connect(ANY_ADDR, 5000, &lmMobileStatus))
    {
    	_time_delay(1000); // keep trying
    }
            
	wassert(ERR_OK == BTMGR_open(&handle));

    while (1)
    {
    	err = BTMGR_receive(handle, (void *)response, sizeof(response), 0, &received, FALSE);
    	if (received > 0)
    	{
    		int i;
    		
    		corona_print("Received %d bytes:\n", received);
    		corona_print("First 20 bytes...\n");
    		for (i = 0; i < received && i < 20; ++i)
    		{
    			corona_print("%02X ", response[i]);
    		}
    		
			corona_print("\n");

			if (0 == strcmp(response, "go"))
    			break;
    		
    		BTMGR_zero_copy_free();
    	}
    	else
    	{
    		if (ERR_BT_NOT_CONNECTED == err)
    		{
    			_time_delay(1000);
    		}
    		else
    		{
    			corona_print("BTMGR_received failed with code 0x%x\n", err);
    			return;
    		}
    	}
    }
    
    while(chris)
    {
    	wassert(ERR_OK == BTMGR_send(handle, (uint_8 *)hello, length, &sent) && sent == length);
    	_time_delay(1000);
    }
    
	wassert(ERR_OK == BTMGR_close(handle));
	wassert(ERR_OK == BTMGR_disconnect());
}
#endif
