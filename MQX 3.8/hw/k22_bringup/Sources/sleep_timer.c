/*
 * periodic_timer.c
 *
 *  Created on: Jan 14, 2013
 *      Author: Chris
 */

#include "sleep_timer.h"
#include "k22_irq.h"
#include "IO_Map.h"
#include "PE_Types.h"
#include "pwr_mgr.h"

static volatile int tick_count = 0; // 1 tick == 1 ms

// We have to explicitly init the timer ISR stuff, since PE for M20KN512VMC10 
//   doesn't place the ISR vector table in the right place.
//   (tIsrFunc)&TU1_Interrupt, /* 0x3A  0x000000E8   -   ivINT_CAN1_Wake_Up      
k2x_error_t sleep_timer_init(void)
{
	PE_ISR(TU1_Interrupt);
	
	printf("Initializing periodic timer (FTM0) IRQ\n");
	k22_enable_irq(IRQ_FTM0, VECTOR_FTM0, TU1_Interrupt); 
	return SUCCESS;
}

void sleep_timer_tick(void)
{
	tick_count++;
}

void sleep(int ms)
{
	int old_count = tick_count;

	while (tick_count < old_count + ms)
		;
}

/*
 *   Handle power management for FTM Timer.
 */
corona_err_t pwr_timer(pwr_state_t state)
{
	
	switch(state)
	{
		case PWR_STATE_SHIP:
			break;
			
		case PWR_STATE_STANDBY:
			break;
			
		case PWR_STATE_NORMAL:
			break;
			
		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}
