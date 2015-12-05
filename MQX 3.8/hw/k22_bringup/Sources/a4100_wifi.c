/*
 * a4100_wifi.c
 *
 *  Created on: Jan 14, 2013
 *      Author: Ernie Aguilar
 */


#include "a4100_wifi.h"
#include "SPI0_WIFI.h"
#include "pwr_mgr.h"

volatile uint8 g_SPI0_Wifi_Wait_for_Send = 1;
volatile uint8 g_SPI0_Wifi_Wait_for_Receive = 1;

/*
 *   Handle power management for A4100 WIFI chip.
 */
corona_err_t pwr_wifi(pwr_state_t state)
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

k2x_error_t a4100_wifi_test(void)
{
	LDD_TDeviceData *masterDevData;
	LDD_TError err;
	uint16 tx[2];
	uint16 rx[2];

	printf("a4100_wifi_test\n");
	
	masterDevData = SPI0_WIFI_Init(NULL);
	
	WIFI_PD_B_SetFieldValue(masterDevData, PIN24_1, 0);
	sleep(1);
	WIFI_PD_B_SetFieldValue(masterDevData, PIN24_1, 1);
	sleep(10);

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
		printf("SPI0_WIFI_ReceiveBlock(2)...\n");
		err = SPI0_WIFI_ReceiveBlock(masterDevData, rx, 2);
		
		if (err != ERR_OK) {
			printf("SPI0_WIFI_ReceiveBlock error %x", err);
		}
		
		printf("Send 0x%x\n", tx[0]);
		err = SPI0_WIFI_SendBlock(masterDevData, &tx[0], 1);
		
		if (err != ERR_OK) {
			printf("SPI0_WIFI_SendBlock error %x", err);
		}
		
		while(g_SPI0_Wifi_Wait_for_Send){/* sleep */}
		
		// Guaranteed to have CS deasserted here. TBD: How will this happen during MQX with DMA???
		
		printf("Send 0x%x\n", tx[1]);
		err = SPI0_WIFI_SendBlock(masterDevData, &tx[1], 1);
		
		if (err != ERR_OK) {
			printf("SPI0_WIFI_SendBlock error %x", err);
		}
		
		while(g_SPI0_Wifi_Wait_for_Send){/* sleep */}

		// Should have our 2 characters already waiting, but wait just in case.
		
		while(g_SPI0_Wifi_Wait_for_Receive){/* sleep */}
	
		g_SPI0_Wifi_Wait_for_Send = 1;
		g_SPI0_Wifi_Wait_for_Receive = 1;
		//printf("SPI1_Accel_ReceiveBlock Err: 0x%x\n", err);
		
		printf("Data Read Back = don't care = 0x%x, care = 0x%x\n", rx[0], rx[1]);

		sleep(2000);
	}
	
	return SUCCESS;
}

