/*
 * pwr_mgr.c
 *
 *  Created on: Feb 14, 2013
 *      Author: Ernie Aguilar
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "pwr_mgr.h"

////////////////////////////////////////////////////////////////////////////////
// External Functions
////////////////////////////////////////////////////////////////////////////////
extern corona_err_t pwr_adc(pwr_state_t state);
extern corona_err_t pwr_mfi(pwr_state_t state);
extern corona_err_t pwr_bt(pwr_state_t state);
extern corona_err_t pwr_wifi(pwr_state_t state);
extern corona_err_t pwr_bu26507(pwr_state_t state);
extern corona_err_t pwr_led(pwr_state_t state);    // k60, this goes in corona_gpio
extern corona_err_t pwr_uart(pwr_state_t state);
extern corona_err_t pwr_wson(pwr_state_t state);
extern corona_err_t pwr_usb(pwr_state_t state);    // k60, this goes in USB
extern corona_err_t pwr_accel(pwr_state_t state);
extern corona_err_t pwr_gpio(pwr_state_t state);   // k60, this goes in corona_gpio
extern corona_err_t pwr_timer(pwr_state_t state);
extern corona_err_t pwr_silego(pwr_state_t state); // k60, this goes in corona_gpio

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static corona_err_t pwr_mcu(pwr_state_t state);

////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////
pwr_mgr_cbk_t g_power_callbacks[] = \
{
	pwr_wifi,
	pwr_adc,
	pwr_mfi,
	pwr_bt,
	pwr_wifi,
	pwr_bu26507,
	pwr_led,
	pwr_uart,
	pwr_wson,
	pwr_usb,
	pwr_accel,
	pwr_silego,
	pwr_timer,
	pwr_gpio,
	pwr_mcu
};

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define NUM_CALLBACKS	sizeof(g_power_callbacks) / sizeof(pwr_mgr_cbk_t)

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Handle power management for MCU.
 */
corona_err_t pwr_mcu(pwr_state_t state)
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


/*
 *   Handle power management for Debug LED's.
 */
corona_err_t pwr_led(pwr_state_t state)
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


/*
 *   Handle power management for GPIO-related items.
 */
corona_err_t pwr_gpio(pwr_state_t state)
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

/*
 *   Handle power management for USB.
 */
corona_err_t pwr_usb(pwr_state_t state)
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

/*
 *   Handle power management for Silego clock chip related stuff.
 */
corona_err_t pwr_silego(pwr_state_t state)
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

/*
 *   Process all the power management callbacks.
 */
corona_err_t pwr_mgr_state(pwr_state_t state)
{
	int i;
	
	for(i = 0; i < NUM_CALLBACKS; i++)
	{
		corona_err_t err;
		err = g_power_callbacks[i](state);
		if (err != SUCCESS)
		{
			printf("\n\nPower Callback (%d) raised error (%d)!!!\n\n", i, err);
			return err;
		}
	}
}
