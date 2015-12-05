/*
 * apple_mfi.c
 * Driver for controlling the idiotic MFI Apple Bluetooth chip and running a unit test.
 *
 *  Created on: Jan 16, 2013
 *      Author: Ernie Aguilar
 */

#include "apple_mfi.h"
#include "I2C_Display.h"  // \todo - Change name of I2C module.  "I2C_Display" doesn't make sense since I2C is shared with others.
#include "i2c.h"
#include "pwr_mgr.h"

extern volatile bool g_I2C1_DataReceivedFlg;
extern volatile bool g_I2C1_DataTransmittedFlg;

static inline void mfi_reset_enable(void);
static inline void mfi_reset_disable(void);
static inline uint8 mfi_reset_state(void);
static void mfi_warm_reset(void);

// Perform a warm reset of the MFI.
// NOTE - This has not been tested to make sure it works.  Need to test it.
static void mfi_warm_reset(void)
{
	// Toggle MFI_RST_B for 10us < t < 1ms.
	mfi_reset_enable();
	sleep(10);
	mfi_reset_disable();
	sleep(1);
	mfi_reset_enable();
	sleep(10);
}

// Return the MFI_RST_B state
static inline uint8 mfi_reset_state(void)
{
	return (GPIOD_PDOR & 0x0020) ? 0x01:0x00;
}

// Set MFI_RST_B to a logic '1'
static inline void mfi_reset_enable(void)
{
	// MFI_RST_B is on PTD5
	GPIOD_PDOR |= 0x0020;
}

// Clear MFI_RST_B to a logic '0'
static inline void mfi_reset_disable(void)
{
	// MFI_RST_B is on PTD5
	GPIOD_PDOR &= ~0x0020;
}

k2x_error_t apple_mfi_test(void)
{
	uint8_t OutData[2];
	uint8_t InpData[16];
	uint8_t mfi_slave_addr;
	LDD_TError Error;
	LDD_TDeviceData *MyI2CPtr;
	unsigned char result;
	
	printf("Apple MFI Test\n");

	g_I2C1_DataReceivedFlg    = FALSE;      // TODO - This is a pretty extreme hack.  We need a better way for arbitrating I2C, since this is shared with the display too.
	g_I2C1_DataTransmittedFlg = FALSE;
	MyI2CPtr = I2C_Display_Init(NULL);      // by default it is just going to init with device 0x74, so need to change for Apple MFI part.
	
	// Select slave device for Apple MFI Device.
	mfi_slave_addr = AAPL_MFI_ADDR_RST1;
	Error = I2C_Display_SelectSlaveDevice(MyI2CPtr, LDD_I2C_ADDRTYPE_7BITS, mfi_slave_addr);
	
	/* Configure I2C BUS device(e.g. RTC) - Write Operation */
	for(;;)
	{
		OutData[0] = MFI_PROCTL_MAJOR_VERSION; //MAJOR_VRSN=0x2  //MFI_DEV_VERSION;  // just see if the chip spits back 0x05, which is dev version.
		
		while (!g_I2C1_DataTransmittedFlg)  // TODO - These transmit flags need a TIMEOUT.
		{
			do
			{
				Error = I2C_Display_MasterSendBlock(MyI2CPtr, OutData, 1U, LDD_I2C_SEND_STOP); /* Send OutData (4 bytes) on the I2C bus and generates a stop condition to end transmission */
			}while(Error == ERR_BUSY);
			
			if (Error != ERR_OK)
			{
				PE_DEBUGHALT();
			}
		}                                   /* Wait until OutData are transmitted */
		g_I2C1_DataTransmittedFlg = FALSE;

		while (!g_I2C1_DataReceivedFlg)
		{
			do
			{
				Error = I2C_Display_MasterReceiveBlock(MyI2CPtr, InpData, 1U, LDD_I2C_SEND_STOP); /* Receive InpData (16 bytes) from the I2C bus and generates a stop condition to end transmission */  
				//printf("Error: 0x%x\n", Error);
			} while(Error == ERR_BUSY);
	
			if (Error != ERR_OK)
			{
				PE_DEBUGHALT();
			}
		}                                      /* Wait until InpData are received */
		g_I2C1_DataReceivedFlg = FALSE;
		
		printf("Received Data: 0x%x\n", InpData[0]);
	}
	
	return SUCCESS;
}

/*
 *   Handle power management for MFI.
 */
corona_err_t pwr_mfi(pwr_state_t state)
{
	/*
	 *   NOTE:  The MFI chip goes into low power sleep mode automatically, and can't be forced to it externally.
	 */
	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:
		case PWR_STATE_NORMAL:
		default:
			break;
	}
	return SUCCESS;
}
