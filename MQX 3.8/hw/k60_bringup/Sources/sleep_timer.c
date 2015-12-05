////////////////////////////////////////////////////////////////////////////////
//! \addtogroup drivers
//! \ingroup corona
//! @{
//
// Copyright (c) 2013 Whistle Labs
//
// Whistle Labs
// Proprietary and Confidential
//
// This source code and the algorithms implemented therein constitute
// confidential information and may comprise trade secrets of Whistle Labs
// or its associates.
//
//! \file sleep_timer.c
//! \brief Sleep Timer
//! \version version 0.1
//! \date Jan, 2013
//! \author chris@whistle.com
//!
//! \detail Sleep for 5 seconds, just a test.
//! \todo There's got to be a better way to implement sleeping!
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "sleep_timer.h"
//#include "k22_irq.h"
#include "IO_Map.h"
#include "PE_Types.h"
#include "pwr_mgr.h"
#include "TU2.h"

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
static volatile uint32_t tick_count = 0; // 1 tick == 1 ms

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! \brief Tick the tick counter, for sleeping.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function ticks the tick counter when FTM interrupt happens, offering a
//! way to COUNT number of milliseconds or microseconds or whatever FTM is configured,
//! so that we know in hardware how long to sleep.
//!
///////////////////////////////////////////////////////////////////////////////
void sleep_timer_tick(void)
{
	tick_count++;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Sleep for a given number of milliseconds.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function makes use of the FTM and a counter triggered by a periodic
//! interrupt, to determine an accurate amount of time to block and sleep.
//!
//! \todo Implement a less CPU-intensive form of sleep().
///////////////////////////////////////////////////////////////////////////////
void sleep(uint32_t ms)
{
	uint32_t old_count = tick_count;
	LDD_TDeviceData *dev = TU2_Init(NULL);
	while (tick_count < old_count + ms){}
	TU2_Deinit(dev);
}

void quick_sleep(uint32_t arbitrary)
{
	volatile uint32_t dummy = 0xDEADBEEF;
	
	while(arbitrary-- != 0)
	{
		dummy += 0xBEEFBEE5;
	}
}

/*
 *   Handle power management for FTM Timer.
 */
corona_err_t pwr_timer(pwr_state_t state)
{
	
	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:
			/*
			 *   probably not necessary here, since we init/de-init every time we want to sleep().
			 */
			break;
			
		case PWR_STATE_NORMAL:
			break;
			
		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}


////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}

