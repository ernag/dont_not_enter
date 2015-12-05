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
//! \file apple_mfi.c
//! \brief Control the MFI obstacle.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
//!
//! \detail Driver for controlling the idiotic MFI Apple Bluetooth chip and running a unit test.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "apple_mfi.h"
#include "I2C1.h"  // \todo - Change name of I2C module.  "I2C_Display" doesn't make sense since I2C is shared with others.
#include "pwr_mgr.h"
#include "corona_console.h"
extern cc_handle_t g_cc_handle;


////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
extern volatile bool g_I2C1_DataReceivedFlg;
extern volatile bool g_I2C1_DataTransmittedFlg;
static 	LDD_TDeviceData *MyI2CPtr = NULL;

////////////////////////////////////////////////////////////////////////////////
// Prototypes
////////////////////////////////////////////////////////////////////////////////
static inline void mfi_reset_enable(void);
static inline void mfi_reset_disable(void);
static inline uint8 mfi_reset_state(void);
static void mfi_warm_reset(void);

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

corona_err_t mfi_comm_ok(void)
{
	uint8_t OutData[2];
	uint8_t InpData[16];
	uint8_t mfi_slave_addr;
	uint32_t msTimeout = 1000;
	LDD_TError Error;
	LDD_I2C_TErrorMask ErrorMask;
	unsigned char result;
	unsigned long retries = 100;

	g_I2C1_DataReceivedFlg    = FALSE;      // TODO - This is a pretty extreme hack.  We need a better way for arbitrating I2C, since this is shared with the display too.
	g_I2C1_DataTransmittedFlg = FALSE;
	if(!MyI2CPtr)
	{
		MyI2CPtr = I2C1_Init(NULL);
	}

	Error = I2C1_SelectSlaveDevice(MyI2CPtr, LDD_I2C_ADDRTYPE_7BITS, AAPL_MFI_ADDR_RST1);
	if(Error != ERR_OK)
	{
		cc_print(&g_cc_handle, "Could not select slave device!\n");
		return CC_I2C_UNINITIALIZED;
	}


	OutData[0] = MFI_PROCTL_MAJOR_VERSION; //MAJOR_VRSN=0x2  //MFI_DEV_VERSION;  // just see if the chip spits back 0x05, which is dev version.

	while (!g_I2C1_DataTransmittedFlg)  // TODO - These transmit flags need a TIMEOUT.
	{
		do
		{
			Error = I2C1_MasterSendBlock(MyI2CPtr, OutData, 1U, LDD_I2C_SEND_STOP); /* Send OutData (4 bytes) on the I2C bus and generates a stop condition to end transmission */
			retries--;
		}while( (Error == ERR_BUSY) && (retries != 0) );

		if ( (Error != ERR_OK) || (retries == 0) )
		{
			cc_print(&g_cc_handle, "I2C MFI Error!\n");
			return CC_ERROR_GENERIC;
		}

		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "I2C MFI Error!\n");
			return CC_ERROR_GENERIC;
		}
		sleep(1);
	}                                   /* Wait until OutData are transmitted */
	g_I2C1_DataTransmittedFlg = FALSE;
	Error = I2C1_GetError(MyI2CPtr, &ErrorMask);
	if(Error || ErrorMask)
	{
		//cc_print(&g_cc_handle, "I2C1 TX ERROR:  Error (0x%x), ErrorMask(0x%x)\n", Error, ErrorMask); // \todo Why do we keep getting NACK error for MFI, when it is working?
		//return Error;
	    //return -1;
	}

	while (!g_I2C1_DataReceivedFlg)
	{
	    retries = 100;
		do
		{
			Error = I2C1_MasterReceiveBlock(MyI2CPtr, InpData, 1U, LDD_I2C_SEND_STOP); /* Receive InpData (16 bytes) from the I2C bus and generates a stop condition to end transmission */
			retries--;
		} while( (Error == ERR_BUSY) && (retries != 0) );

		if ((Error != ERR_OK) || (retries == 0) )
		{
			cc_print(&g_cc_handle, "I2C MFI Error!\n");
			return CC_ERROR_GENERIC;
		}

		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "I2C MFI Error!\n");
			return CC_ERROR_GENERIC;
		}
		sleep(1);

	}                                      /* Wait until InpData are received */
	g_I2C1_DataReceivedFlg = FALSE;
	Error = I2C1_GetError(MyI2CPtr, &ErrorMask);
	if(Error || ErrorMask)
	{
		//cc_print(&g_cc_handle, "I2C1 RX ERROR:  Error (0x%x), ErrorMask(0x%x)\n", Error, ErrorMask); // \todo Why do we keep getting NACK error for MFI, when it is working?
		//return Error;
	}

	cc_print(&g_cc_handle, "Received Data: 0x%x\n", InpData[0]);
	if(InpData[0] != 0x2)
	{
		cc_print(&g_cc_handle, "ERROR! Expected 0x2 from MFI!\n");
		return CC_ERROR_GENERIC;
	}

	return SUCCESS;
}

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

///////////////////////////////////////////////////////////////////////////////
//! \brief Simple MFI Unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function runs infinite loop, asking MFI chip for register info for unit testing.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t apple_mfi_test(void)
{
	uint8_t OutData[2];
	uint8_t InpData[16];
	uint8_t mfi_slave_addr;
	LDD_TError Error;
	LDD_I2C_TErrorMask ErrorMask;
	unsigned char result;

	cc_print(&g_cc_handle, "Apple MFI Test\n");

	g_I2C1_DataReceivedFlg    = FALSE;      // TODO - This is a pretty extreme hack.  We need a better way for arbitrating I2C, since this is shared with the display too.
	g_I2C1_DataTransmittedFlg = FALSE;
	if(!MyI2CPtr)
	{
		MyI2CPtr = I2C1_Init(NULL);
	}

	Error = I2C1_SelectSlaveDevice(MyI2CPtr, LDD_I2C_ADDRTYPE_7BITS, AAPL_MFI_ADDR_RST1);
	if(Error != ERR_OK)
	{
		cc_print(&g_cc_handle, "Could not select slave device!\n");
		return CC_I2C_UNINITIALIZED;
	}

	/* Configure I2C BUS device(e.g. RTC) - Write Operation */
	for(;;)
	{
		OutData[0] = MFI_PROCTL_MAJOR_VERSION; //MAJOR_VRSN=0x2  //MFI_DEV_VERSION;  // just see if the chip spits back 0x05, which is dev version.

		while (!g_I2C1_DataTransmittedFlg)  // TODO - These transmit flags need a TIMEOUT.
		{
			do
			{
				Error = I2C1_MasterSendBlock(MyI2CPtr, OutData, 1U, LDD_I2C_SEND_STOP); /* Send OutData (4 bytes) on the I2C bus and generates a stop condition to end transmission */
			}while(Error == ERR_BUSY);

			if (Error != ERR_OK)
			{
				PE_DEBUGHALT();
			}
		}                                   /* Wait until OutData are transmitted */
		g_I2C1_DataTransmittedFlg = FALSE;
		Error = I2C1_GetError(MyI2CPtr, &ErrorMask);
		if(Error || ErrorMask)
		{
			//cc_print(&g_cc_handle, "I2C1 TX ERROR:  Error (0x%x), ErrorMask(0x%x)\n", Error, ErrorMask); // \todo Why do we keep getting NACK error for MFI, when it is working?
			//return Error;
		}

		while (!g_I2C1_DataReceivedFlg)
		{
			do
			{
				Error = I2C1_MasterReceiveBlock(MyI2CPtr, InpData, 1U, LDD_I2C_SEND_STOP); /* Receive InpData (16 bytes) from the I2C bus and generates a stop condition to end transmission */
				//cc_print(&g_cc_handle, "Error: 0x%x\n", Error);
			} while(Error == ERR_BUSY);

			if (Error != ERR_OK)
			{
				PE_DEBUGHALT();
			}
		}                                      /* Wait until InpData are received */
		g_I2C1_DataReceivedFlg = FALSE;
		Error = I2C1_GetError(MyI2CPtr, &ErrorMask);
		if(Error || ErrorMask)
		{
			cc_print(&g_cc_handle, "I2C1 RX ERROR:  Error (0x%x), ErrorMask(0x%x)\n", Error, ErrorMask); // \todo Why do we keep getting NACK error for MFI, when it is working?
			//return Error;
		}

		cc_print(&g_cc_handle, "Received Data: 0x%x\n", InpData[0]);
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

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
