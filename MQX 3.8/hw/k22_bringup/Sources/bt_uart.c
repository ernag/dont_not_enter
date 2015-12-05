/*
 * bt_uart.c
 *
 *  Created on: Jan 15, 2013
 *      Author: Chris
 */


#include "bt_uart.h"
#include "k22_irq.h"
#include "UART2_BT_UART.h"
#include "pwr_mgr.h"

volatile bool g_UART2_DataReceivedFlg = FALSE;
volatile bool g_UART2_DataTransmittedFlg = FALSE;


// \TODO - Test to make sure RTS/CTS works!

// ISR vector table is not correct by default.
// Need to overwrite sections 0x33 and 0x34 (I2S0_TX and I2S0_RX) with the UART2 ISR's in vectors.c:
// (tIsrFunc)&UART2_BT_UART_Interrupt,      /* 0x33  0x000000CC   -   ivINT_I2S0_Tx                  unused by PE */
// (tIsrFunc)&UART2_BT_UART_Interrupt,      /* 0x34  0x000000D0   -   ivINT_I2S0_Rx                  unused by PE */
// Also, if we want to interrupt on CTS (for debugging) we need to overwrite section 0x4E (FTM0) with INT_PORTD ISR in vectors.c:
// (tIsrFunc)&Cpu_ivINT_PORTD,          /* 0x4E  0x00000138   8   ivINT_FTM0                     used by PE */


#define HCI_COMMAND_CODE_VENDOR_SPECIFIC_DEBUG_OGF                      0x3F
#define HCI_VS_HCILL_PARAMETERS_OGF             (HCI_COMMAND_CODE_VENDOR_SPECIFIC_DEBUG_OGF)
#define HCI_VS_HCILL_PARAMETERS_OCF             (0x12B)
#define HCI_VS_HCILL_PARAMETERS_SIZE            (5)
#define CMD_SZ (2+1+HCI_VS_HCILL_PARAMETERS_SIZE)

k2x_error_t bt_uart_test(void)
{
	char InpData[10];
	LDD_TError Error;
	LDD_TDeviceData *MySerialPtr;
	unsigned char cmd[CMD_SZ];

	printf("BT UART Test\n");
	
	MySerialPtr = UART2_BT_UART_Init(NULL);                                     // Initialization of BT UART.

	// Set up the NVIC so we get interrupted properly.
	k22_enable_irq(IRQ_UART2_STATUS, VECTOR_UART2_STATUS, UART2_BT_UART_Interrupt);
	k22_enable_irq(IRQ_UART2_ERR, VECTOR_UART2_ERR, UART2_BT_UART_Interrupt);
	
	BT_PWDN_B_SetFieldValue(NULL, PIN4_1, 1);

	cmd[0] = HCI_VS_HCILL_PARAMETERS_OCF & 0xff;
	cmd[1] = ((HCI_VS_HCILL_PARAMETERS_OCF >> 8) & 0x03) | (HCI_VS_HCILL_PARAMETERS_OGF << 2);
	cmd[2] = HCI_VS_HCILL_PARAMETERS_SIZE;
	cmd[3] = 0;
	cmd[4] = 1;
	cmd[5] = 0;
	cmd[6] = 1;
	cmd[7] = 0;
	cmd[8] = 1;

	Error = UART2_BT_UART_SendBlock(MySerialPtr, cmd, CMD_SZ);
	if (Error != ERR_OK) {
		printf("UART2_BT_UART_SendBlock failed %x", Error);
	}

	Error = UART2_BT_UART_ReceiveBlock(MySerialPtr, InpData, 1U);             // Start reception of one character 
	if (Error != ERR_OK) {
		printf("UART2_BT_UART_ReceiveBlock failed %x", Error);
	}

	while (!g_UART2_DataReceivedFlg) 
	{                                      // Wait until 'e' character is received 
		//sleep()
	}

	g_UART2_DataReceivedFlg = FALSE;

	printf("Received %x\n", InpData[0]);

	return SUCCESS;
}

/*
 *   Handle power management for Bluetooth.
 */
corona_err_t pwr_bt(pwr_state_t state)
{
	
	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:
			// 1.  Assert BT_PWDN_B pin.
			BT_PWDN_B_SetFieldValue(NULL, PIN4_1, 0);
			
			// 2.  Remove 26MHz clock from Silego.
			break;
			
		case PWR_STATE_NORMAL:
			// 1.  Enable the 26MHz clock from Silego.
			
			// 2.  De-assert BT_PWDN_B pin.
			BT_PWDN_B_SetFieldValue(NULL, PIN4_1, 1);
			break;
			
		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}
