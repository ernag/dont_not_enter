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
//! \file dbg_uart.c
//! \brief UART5 driver for serial COM on Corona.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
//!
//! \detail This is the UART5 driver which TX's and RX's data from FTDI chip on Corona.
//! \todo Test to make sure RTS/CTS works!
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "dbg_uart.h"
//#include "k22_irq.h"
#include "UART5_DBG.h"
#include "corona_console.h"
#include "pwr_mgr.h"
extern cc_handle_t g_cc_handle;



////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define RX_BUF_SIZE	128

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
volatile bool g_UART5_DataReceivedFlg = FALSE;      // Used to let software know when hardware has RX'd.
volatile bool g_UART5_DataTransmittedFlg = FALSE;   // Used to let software know when hardware has TX'd.
static 	LDD_TDeviceData *MySerialPtr = NULL;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! \brief UART5 COM test
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Init UART5 for serial COM and launch a polling console application.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t dbg_uart_test(void)
{
	//char OutData[] = "UART TEST Passed\n";
	char InpData[10];
	char rxBuffer[RX_BUF_SIZE];
	const char *pIntro = "\n\r\n*** Corona Console (ChEOS)***\n\r\n]";
	char * pRx;
	uint16_t rxIdx = 0;
	LDD_TError Error;
	cc_handle_t ccHandle;

	cc_print(&g_cc_handle, "Debug UART Test\n");

	// Set up the NVIC so we get interrupted properly for the console.
	//k22_enable_irq(IRQ_UART5_STATUS, VECTOR_UART5_STATUS, UART5_DBG_Interrupt);
	//k22_enable_irq(IRQ_UART5_ERR, VECTOR_UART5_ERR, UART5_DBG_Interrupt);

	MySerialPtr = UART5_DBG_Init(NULL);  // Initialization of DBG UART.
	Error = UART5_DBG_SendBlock(MySerialPtr, (void *)pIntro, strlen(pIntro));
	while (!g_UART5_DataTransmittedFlg){}
	g_UART5_DataTransmittedFlg = FALSE;

	cc_init(&ccHandle, MySerialPtr, CC_TYPE_UART);

	for(;;)  // loop forever
	{
		for(;;) // loop until RX buffer is full or a newline is received.
		{
			Error = UART5_DBG_ReceiveBlock(MySerialPtr, InpData, 1); // Start reception of one character
			while (!g_UART5_DataReceivedFlg) {}

			rxBuffer[rxIdx] = InpData[0]; // buffer input until a \n is received.

			Error = UART5_DBG_SendBlock(MySerialPtr, InpData, 1); // give the user feedback of what they just entered.
			while (!g_UART5_DataTransmittedFlg){}

			g_UART5_DataReceivedFlg = FALSE;
			g_UART5_DataTransmittedFlg = FALSE;

			if( (rxBuffer[rxIdx] == '\n') || (rxBuffer[rxIdx] == '\r') ) // newline was received.  Process the input
			{
				rxBuffer[rxIdx+1] = 0; // null terminate the str
				cc_process_cmd(&ccHandle, rxBuffer); // send off the command + args to be processed.
				break;
			}
			else if( (++rxIdx + 1) >= RX_BUF_SIZE ) // make sure buffer doesn't over-run.
			{
				const char *pMsg = "\n\rRX Buffer Over-run.  Start command over!\n\r";
				Error = UART5_DBG_SendBlock(MySerialPtr, (void *)pMsg, strlen(pMsg));
				while (!g_UART5_DataTransmittedFlg){}
				g_UART5_DataTransmittedFlg = FALSE;
				break; // don't let the buffer over-run.
			}
		}

		rxIdx = 0;
		Error = UART5_DBG_SendBlock(MySerialPtr, "\n\r]", 3); // prompt
		while (!g_UART5_DataTransmittedFlg){}
		g_UART5_DataTransmittedFlg = FALSE;
	}
	return SUCCESS;
}

/*
 *   Handle power management for the Debug UART.
 */
corona_err_t pwr_uart(pwr_state_t state)
{

	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:
			if(MySerialPtr)
			{
				UART5_DBG_Disable(MySerialPtr);
			}
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

