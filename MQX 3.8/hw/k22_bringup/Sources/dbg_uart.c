/*
 * dbg_uart.c
 *
 *  Created on: Jan 14, 2013
 *      Author: Ernie Aguilar
 */


#include "dbg_uart.h"
#include "k22_irq.h"
#include "UART5_DBG.h"
#include "corona_console.h"
#include "pwr_mgr.h"

#define RX_BUF_SIZE	128

volatile bool g_UART5_DataReceivedFlg = FALSE;
volatile bool g_UART5_DataTransmittedFlg = FALSE;

// \TODO - Test to make sure RTS/CTS works!

// ISR vector table is not correct by default.
// Need to overwrite sections 0x54 and 0x55 (PIT0 and PIT1), with the UART5 ISR's in vectors.c
//(tIsrFunc)&UART5_DBG_Interrupt,         /* 0x54  0x00000150   -   ivINT_PIT0                     unused by PE */
//(tIsrFunc)&&UART5_DBG_Interrupt,         /* 0x55  0x00000154   -   ivINT_PIT1                     unused by PE */

k2x_error_t dbg_uart_test(void)
{
	//char OutData[] = "UART TEST Passed\n";
	char InpData[10];
	char rxBuffer[RX_BUF_SIZE];
	const char *pIntro = "\n\r\n*** Corona Console ***\n\r\n]";
	char * pRx;
	uint16_t rxIdx = 0;
	LDD_TError Error;
	LDD_TDeviceData *MySerialPtr;
	cc_handle_t ccHandle;
	
	printf("Debug UART Test\n");

	// Set up the NVIC so we get interrupted properly for the console.
	k22_enable_irq(IRQ_UART5_STATUS, VECTOR_UART5_STATUS, UART5_DBG_Interrupt);
	k22_enable_irq(IRQ_UART5_ERR, VECTOR_UART5_ERR, UART5_DBG_Interrupt);

	MySerialPtr = UART5_DBG_Init(NULL);  // Initialization of DBG UART.
	Error = UART5_DBG_SendBlock(MySerialPtr, (void *)pIntro, strlen(pIntro));
	while (!g_UART5_DataTransmittedFlg){}
	g_UART5_DataTransmittedFlg = FALSE;
	
	cc_init(&ccHandle, MySerialPtr);
	
	for(;;)  // loop forever
	{	
		for(;;) // loop until RX buffer is full or a newline is received.
		{
			Error = UART5_DBG_ReceiveBlock(MySerialPtr, InpData, 1); // Start reception of one character 
			while (!g_UART5_DataReceivedFlg) {sleep(1);}
			
			rxBuffer[rxIdx] = InpData[0]; // buffer input until a \n is received.
			
			Error = UART5_DBG_SendBlock(MySerialPtr, InpData, 1); // give the user feedback of what they just entered.
			while (!g_UART5_DataTransmittedFlg){sleep(1);}
	
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
		while (!g_UART5_DataTransmittedFlg){sleep(1);}
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
