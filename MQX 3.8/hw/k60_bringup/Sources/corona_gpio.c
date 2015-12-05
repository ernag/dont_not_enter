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
//! \file corona_gpio.c
//! \brief Corona GPIO utilities
//! \version version 0.1
//! \date Feb, 2013
//! \author ernie@whistle.com
//!
//! \detail This file is for misc GPIO stuff on Corona.
//! \todo For Proto2, need to test the PE GPIO "Driver" files.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "corona_gpio.h"
#include "sleep_timer.h"
#include "pwr_mgr.h"
#include "PORTB_GPIO_LDD.h"
#include "PORTC_GPIO_LDD.h"
#include "PORTD_GPIO_LDD.h"
#include "PORTE_GPIO_LDD.h"
#include "corona_console.h"
extern cc_handle_t g_cc_handle;


///////////////////////////////////////////////////////////////////////////////
//! \brief Provide a mask given bit range.
//!
//! \fntype Reentrant Function
//!
//! \detail Give a mask corresponding to msb:lsb.
//!
//! \return 32-bit mask corresponding to msb:lsb.
//!
///////////////////////////////////////////////////////////////////////////////
uint32_t mask(uint32_t msb, uint32_t lsb)
{
	uint32_t ret = 0;
	uint32_t numOnes = msb - lsb + 1;
	if(msb < lsb)
	{
		return 0;
	}

	while(numOnes-- > 0)
	{
		ret = 1 | (ret << 1);
	}

	return (ret << lsb);
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Run some GPIO tests.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function is a catch-all for testing GPIO LDD drivers.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t corona_gpio_test(void)
{
	rf_sw_vers_t rf_sw_vers = RF_SW_VERS_0x01;
	uint32_t mcu_26mhz_en = 1;
	uint32_t i;


/*  ************  GPIO's THAT REQUIRE TESTING ***************

 PORT B
	#define BT_PWDN_B ((LDD_GPIO_TBitField)0)
	#define MFI_RST_B ((LDD_GPIO_TBitField)1)
	#define RF_SW_VERSION ((LDD_GPIO_TBitField)2)
	#define DATA_SPI_WP_B ((LDD_GPIO_TBitField)3)
	#define DATA_SPI_HOLD_B ((LDD_GPIO_TBitField)4)

PORT C
	#define MCU_26MHz_EN ((LDD_GPIO_TBitField)0)

PORT D
	#define LED_DBG ((LDD_GPIO_TBitField)0)

PORT E
	#define WIFI_PD_B ((LDD_GPIO_TBitField)0)
	#define BT_32KHz_EN ((LDD_GPIO_TBitField)1)

*********************************************************************
*/
#if 0
	cc_print(&g_cc_handle, "Testing setting the RF_SW_VERS to 0x%x\n", rf_sw_vers);
	PORTB_GPIO_LDD_SetFieldValue(NULL, RF_SW_VERSION, rf_sw_vers);

	cc_print(&g_cc_handle, "Testing setting the MCU_26MHz_EN to 0x%x\n", mcu_26mhz_en);
	PORTC_GPIO_LDD_SetFieldValue(NULL, MCU_26MHz_EN, mcu_26mhz_en);

	cc_print(&g_cc_handle, "Testing all the different LED combinations...\n");
	for(i = 0; i < NUM_LED_DBG_COMBOS; i++)
	{
		PORTD_GPIO_LDD_SetFieldValue(NULL, LED_DBG, i);
		sleep(250 * 1000);
	}

	cc_print(&g_cc_handle, "Testing 32K_B_ENABLE can be set to 0...\nBETTER SET IT BACK TO 1 to use BT!");
	PORTE_GPIO_LDD_SetFieldValue(NULL, BT_32KHz_EN, 0);
#endif
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
		case PWR_STATE_STANDBY:
			PORTD_GPIO_LDD_SetFieldValue(NULL, LED_DBG, 0xF);   // all debug LED's off.
			break;

		case PWR_STATE_NORMAL:
			//PORTD_GPIO_LDD_SetFieldValue(NULL, LED_DBG, 0xA); // interweaving pattern
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
 *   Handle power management for Silego clock chip related stuff.
 */
corona_err_t pwr_silego(pwr_state_t state)
{

	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:
		case PWR_STATE_NORMAL:
#if 0
			PORTC_GPIO_LDD_SetFieldValue(NULL, MCU_26MHz_EN, 0); // we never want to use the 26MHz clock.
#endif
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
