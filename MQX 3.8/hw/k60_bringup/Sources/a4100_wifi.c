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
//! \file a4100_wifi.c
//! \brief Driver for A4100 WIFI chip.
//! \version version 0.1
//! \date Jan, 2013
//! \author chris@whistle.com
//!
//! \detail This is the A4100 WIFI driver.  It sends some commands and reads back results for bringup purposes.
//! \todo How will CS assertion happen when DMA comes into play?
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "a4100_wifi.h"
#include "SPI0_WIFI.h"
#include "PORTE_GPIO_LDD.h"
#include "pwr_mgr.h"
#include "corona_console.h"
extern cc_handle_t g_cc_handle;


////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
volatile uint8 g_SPI0_Wifi_Wait_for_Send = 1;
volatile uint8 g_SPI0_Wifi_Wait_for_Receive = 1;
static 	LDD_TDeviceData *masterDevData = NULL;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Handle power management for A4100 WIFI chip.
 */
corona_err_t pwr_wifi(pwr_state_t state)
{

	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:

			if(masterDevData)
			{
				//UART3_WIFI_Disable(masterDevData); // this was causing a crash for some reason.  :-(
			}

			// Drive the TXD signal high.
			PORTC_PCR17 = PORT_PCR_MUX(0x01);
			GPIOC_PDDR |= 0x20000;    // pin 17 is a GPIO output now
			GPIOC_PDOR &= ~(0x20000); // pin 17 is WIFI_UART_TXD

			PORTE_GPIO_LDD_SetFieldValue(NULL, WIFI_PD_B, 0);
			break;

		case PWR_STATE_NORMAL:
			break;

		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Execute the A4100 WIFI communication test.
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t a4100_wifi_comm_ok(void)
{
	LDD_TError err;
	uint16 tx[2];
	uint16 rx[2];
	uint32_t msTimeout = 1000;

	if(!masterDevData)
	{
		masterDevData = SPI0_WIFI_Init(NULL);
	}

	PORTE_GPIO_LDD_SetFieldValue(NULL, WIFI_PD_B, 0);
	sleep(100);
	PORTE_GPIO_LDD_SetFieldValue(NULL, WIFI_PD_B, 1);
	sleep(200);

	//tx = A4100_REG_READ_MASK | A4100_SPI_CFG;
	tx[0] = 0xc500; // actual command to read the SPI_CONF register
	tx[1] = 0;      // Need these 'idle' transmit bits to keep CS asserted during read of same number of bits
	rx[0] = 0;		// This 16-bit character will be garbage since it is clocked in while we clock out tx[0]
	rx[1] = 0;		// This will be the real received character, clocked in while we clock out tx[1] 'idle'

	err = SPI0_WIFI_ReceiveBlock(masterDevData, rx, 2);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI0_WIFI_ReceiveBlock error %x\n", err);
		return err;
	}

	err = SPI0_WIFI_SendBlock(masterDevData, &tx[0], 1);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI0_WIFI_SendBlock error %x\n", err);
		return err;
	}

	while(g_SPI0_Wifi_Wait_for_Send)
	{
		if(msTimeout-- == 0)
		{
		    cc_print(&g_cc_handle, "Timeout waiting for WIFI send!\n");
			return CC_ERROR_GENERIC;
		}
	}

	err = SPI0_WIFI_SendBlock(masterDevData, &tx[1], 1);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI0_WIFI_SendBlock error %x\n", err);
		return err;
	}

	msTimeout = 1000;
	while(g_SPI0_Wifi_Wait_for_Send)
	{
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "Timeout waiting for WIFI send!\n");
			return CC_ERROR_GENERIC;
		}
	}

	msTimeout = 1000;
	while(g_SPI0_Wifi_Wait_for_Receive)
	{
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "Timeout waiting for WIFI send!\n");
			return CC_ERROR_GENERIC;
		}
	}

	g_SPI0_Wifi_Wait_for_Send = 1;
	g_SPI0_Wifi_Wait_for_Receive = 1;

	cc_print(&g_cc_handle, "Data Read Back = don't care = 0x%x, care = 0x%x\n", rx[0], rx[1]);
	if(rx[1] != 0x10)
	{
		cc_print(&g_cc_handle, "ERROR: Expected 0x10!\n");
		return CC_ERROR_GENERIC;
	}

	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Execute the A4100 WIFI unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function runs an infinite loop, printing feedback from the A4100 chip.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t a4100_wifi_test(void)
{

	LDD_TError err;
	uint16 tx[2];
	uint16 rx[2];
	int i;

	cc_print(&g_cc_handle, "a4100_wifi_test\n");

	if(!masterDevData)
	{
		masterDevData = SPI0_WIFI_Init(NULL);
	}

	PORTE_GPIO_LDD_SetFieldValue(NULL, WIFI_PD_B, 0);
	sleep(100);
	PORTE_GPIO_LDD_SetFieldValue(NULL, WIFI_PD_B, 1);
	sleep(200);

	//tx = A4100_REG_READ_MASK | A4100_SPI_CFG;
	tx[0] = 0xc500; // actual command to read the SPI_CONF register
	tx[1] = 0;      // Need these 'idle' transmit bits to keep CS asserted during read of same number of bits
	rx[0] = 0;		// This 16-bit character will be garbage since it is clocked in while we clock out tx[0]
	rx[1] = 0;		// This will be the real received character, clocked in while we clock out tx[1] 'idle'

	for(;;)
	{
		// We need to send 2 16-bit characters. The first is to send the actual 16-bit character command (tx[0]).
		// The second is a dummy send of 'idle' (tx[1] = 0) just to kick off the 'receive' phase of the AR4100P 'SPI internal read' operation.
		// MISO is sampled during both character transmits, but since we need to tx the first character to the AR4100P to tell it
		// what we want to 'read', MISO during the first character tx is useless to our side (MCU).
		// We only do it because loading up the TX FIFO with control
		// commands (upper half of 32 bit writes to PUSHR), which is what happens during every tx (the ones where we have real data and
		// the ones that contain idle) is the only way to assert/reassert CS, which we need to get the MCU to start clocking in MISO.
		//
		// So, during the first character transmit, tx[0] is valid, but we ignore rx[0]. During the second character transmit,
		// tx[1] is 'idle' (0's), and we care about what goes into rx[1].
		//
		// The reason we need to separate transmits into two separate SPI0_WIFI_SendBlock()'s of 1 character each
		// instead of one SPI0_WIFI_SendBlock() of 2 characters is
		// because we need CS to deassert/reassert between each 16-bit character per AR4100P spec. The implementation for SPI0_WIFI_SendBlock() is to keep
		// CS asserted during the entire block transfer, not deassert/reassert per character within the block.
		//
		// Finally, we call SPI0_WIFI_ReceiveBlock() before calling SPI0_WIFI_TransmitBlock() so the receive logic can be prepared
		// to receive on MISO before receive data can possibly arrive.


		// prepare to read - this doesn't actually do anything to the hardware. just prepares recv buffer
		cc_print(&g_cc_handle, "SPI0_WIFI_ReceiveBlock(2)...\n");
		err = SPI0_WIFI_ReceiveBlock(masterDevData, rx, 2);

		if (err != ERR_OK) {
			cc_print(&g_cc_handle, "SPI0_WIFI_ReceiveBlock error %x\n", err);
		}

		cc_print(&g_cc_handle, "Send 0x%x\n", tx[0]);
		err = SPI0_WIFI_SendBlock(masterDevData, &tx[0], 1);

		if (err != ERR_OK) {
			cc_print(&g_cc_handle, "SPI0_WIFI_SendBlock error %x\n", err);
		}

		while(g_SPI0_Wifi_Wait_for_Send){/* sleep */}

		// Guaranteed to have CS deasserted here. TBD: How will this happen during MQX with DMA???

		cc_print(&g_cc_handle, "Send 0x%x\n", tx[1]);
		err = SPI0_WIFI_SendBlock(masterDevData, &tx[1], 1);

		if (err != ERR_OK) {
			cc_print(&g_cc_handle, "SPI0_WIFI_SendBlock error %x\n", err);
		}

		while(g_SPI0_Wifi_Wait_for_Send){/* sleep */}

		// Should have our 2 characters already waiting, but wait just in case.

		while(g_SPI0_Wifi_Wait_for_Receive){/* sleep */}

		g_SPI0_Wifi_Wait_for_Send = 1;
		g_SPI0_Wifi_Wait_for_Receive = 1;
		//cc_print(&g_cc_handle, "SPI1_Accel_ReceiveBlock Err: 0x%x\n", err);

		cc_print(&g_cc_handle, "Data Read Back = don't care = 0x%x, care = 0x%x\n", rx[0], rx[1]);

		sleep(2000);
	}

	return SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
