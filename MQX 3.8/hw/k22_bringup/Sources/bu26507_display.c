/*
 * bu26507_display.c
 *
 *  Created on: Jan 15, 2013
 *      Author: Ernie Aguilar
 */

#include "bu26507_display.h"
#include "I2C_Display.h"
#include "i2c.h"
#include "pwr_mgr.h"

volatile bool g_I2C1_DataReceivedFlg    = FALSE;
volatile bool g_I2C1_DataTransmittedFlg = FALSE;
static LDD_TDeviceData *MyI2CPtr = NULL;

#define NUM_CMDS	9
volatile display_cmd_t g_display_cmds[NUM_CMDS] = {
	//{0x00, 0x01}, // 0) Software Reset
	{0x7F, 0x00}, // 1) 7FH 00000000 Select control register
	{0x21, /*0x0C*/0x00}, // 2) 21H 00000000 Select internal OSC for CLK,  AND make SYNC pin = 'H' control LED's on/off.
	{0x01, 0x08}, // 3) 01H 00001000 Start OSC
	{0x11, 0x3F}, // 4) 11H 00111111 Set LED1-6 enable
	{0x12, 0x3F}, // 5) 12H 00111111 Set LED7-12 enable
	{0x13, 0x1F}, // 6) 13H 00011111 Set LED13-17 enable
	{0x20, 0x3F}, // 7) 20H 00111111 Set Max Duty at Slope
	//{0x7F, 0x01}, // 8) 7FH 00000001 Select A-pattern or B-pattern register, Select A-pattern register to write matrix data
	//{0x7F, 0x01}, // 9) 01-77H xxxxxxxx Write A-pattern data
	//{0x7F, 0x00}, // 10) 7FH 00000000 Select control register, Select A-pattern register to output for matrix
	{0x2D, 0x06}, // 11) 2DH 00000110 Set SLOPE control enable
	{0x30, 0x01}, // 12) 30H 00000001 Start SLOPE sequence
	//{0x30, 0x00}  // 13) 30H 00000000 Lights out
};

corona_err_t bu26507_init(void)
{
	MyI2CPtr = I2C_Display_Init(NULL);
	if(!MyI2CPtr)
	{
		PE_DEBUGHALT();
		return CC_I2C_UNINITIALIZED;
	}
	return SUCCESS;
}

// Just turn on some LED's using the 0x74 display device on Corona
corona_err_t bu26507_display_test(void)
{
	uint8_t OutData[2] = {BU_LED_EN_ADDR, 0x3F/*(LED1_MASK | LED3_MASK | LED5_MASK)*/}; // LED enable register, try illuminating interleaved LED's.
	uint8_t InpData[16];
	LDD_TError Error;
	unsigned char result;
	unsigned char led_masker = (BU_LED1_MASK | BU_LED3_MASK | BU_LED5_MASK);
	unsigned int i;
	
	printf("BU26507 Display Unit Test - Illuminate some LEDs\n");
	
#define USE_PE_EXAMPLE
#ifndef USE_PE_EXAMPLE
	
	//Initialize I2C
	init_I2C();
	
	// Get initial reading of LED_EN_ADDR register.
	result = I2CReadRegister(LED_EN_ADDR);
	printf("Original LED_EN_ADDR reading: 0x%x\n", result);
	printf("Attempting to change LED_EN_ADDR register to: 0x%x\n", led_masker);
	I2CWriteRegister(LED_EN_ADDR,  led_masker); // just light up some of the LED's.
	sleep(1);
	
	while(1)
	{
		result = I2CReadRegister(LED_EN_ADDR);
		//I2CWriteRegister(LED_EN_ADDR,  led_masker);
		//printf("LED_EN_ADDR: 0x%x\n", result);
		sleep(50);
	}
	

#else
	// Initialize the I2C device.
	// NOTE:  Slave Address of the BU26507 is 0x74.  This is shifted to the left since the R/W bit is LSB of
	//        the 8-bit slave address we send.  So slave address is the MS 7 bits.
	//        So PE generates this code for the address: '''DeviceDataPrv->SlaveAddr = 0xE8U;'''
	MyI2CPtr = I2C_Display_Init(NULL);      // by default it is just going to init with device 0x74.
	
	/* Configure I2C BUS device(e.g. RTC) - Write Operation */
#if 0
	do
	{
		Error = I2C_Display_MasterSendBlock(MyI2CPtr, OutData, 2U, LDD_I2C_SEND_STOP); /* Send OutData (4 bytes) on the I2C bus and generates a stop condition to end transmission */
	}while(Error == ERR_BUSY);
	
	if (Error != ERR_OK)
	{
		PE_DEBUGHALT();
	}
	
	while (!g_I2C1_DataTransmittedFlg) {}                                   /* Wait until OutData are transmitted */
	g_I2C1_DataTransmittedFlg = FALSE;
	sleep(2);
	printf("Done sending data to BU26507, Looping Forever\n");
#endif
	
	for(i = 0; i < NUM_CMDS; i++)
	{
		OutData[0] = g_display_cmds[i].reg;
		OutData[1] = g_display_cmds[i].data;
		
		do
		{
			Error = I2C_Display_MasterSendBlock(MyI2CPtr, OutData, 2U, LDD_I2C_SEND_STOP); /* Send OutData (2 bytes) on the I2C bus and generates a stop condition to end transmission */
		}while(Error == ERR_BUSY);
		
		if (Error != ERR_OK)
		{
			PE_DEBUGHALT();
		}
		
		while (!g_I2C1_DataTransmittedFlg) {}                                   /* Wait until OutData are transmitted */
		g_I2C1_DataTransmittedFlg = FALSE;
	}

#if 0
	for(;;)
	{
		/* Read configuration of I2C BUS device(e.g. RTC) - Read Operation */  
		//OutData[0] = 0x00U;                                             /* Initialization of OutData buffer */   
		//Error = I2C_Display_MasterSendBlock(MyI2CPtr, OutData, 1U, LDD_I2C_NO_SEND_STOP); /* Send OutData (1 byte) on the I2C bus stop condition and will not generate the end of the transmission */
		//while (!g_I2C1_DataTransmittedFlg) {                                   /* Wait until OutData are transmitted */
		//}
		//g_I2C1_DataTransmittedFlg = FALSE;
		
		do
		{
			Error = I2C_Display_MasterSendBlock(MyI2CPtr, OutData, 1U, LDD_I2C_NO_SEND_STOP/*LDD_I2C_SEND_STOP*/); /* Send OutData (4 bytes) on the I2C bus and generates a stop condition to end transmission */
		}while(Error == ERR_BUSY);
		
		if (Error != ERR_OK)
		{
			PE_DEBUGHALT();
		}
		
		while (!g_I2C1_DataTransmittedFlg) {}                                   /* Wait until OutData are transmitted */
		g_I2C1_DataTransmittedFlg = FALSE;
		
		do
		{
			Error = I2C_Display_MasterReceiveBlock(MyI2CPtr, InpData, 1U, LDD_I2C_SEND_STOP); /* Receive InpData (16 bytes) from the I2C bus and generates a stop condition to end transmission */  
			printf("Error: 0x%x\n", Error);
		} while(Error == ERR_BUSY);
		
		if (Error != ERR_OK)
		{
			PE_DEBUGHALT();
		}

		while (!g_I2C1_DataReceivedFlg) {}                                      /* Wait until InpData are received */
		g_I2C1_DataReceivedFlg = FALSE;
		
		printf("Received Data: 0x%x\n", InpData[0]);
		sleep(20);
	}
#endif
#endif
	
	return SUCCESS;
}

// Write a register with data.
corona_err_t bu26507_write(uint8_t reg, uint8_t data)
{
	LDD_TError Error;
	uint8_t OutData[2];
	
	if(!MyI2CPtr)
	{
		printf("BU26507 has not been initialized yet!!!!!!!\n");
		PE_DEBUGHALT();
		return CC_I2C_UNINITIALIZED;
	}
	
	OutData[0] = reg;
	OutData[1] = data;
	
	do
	{
		Error = I2C_Display_MasterSendBlock(MyI2CPtr, OutData, 2U, LDD_I2C_SEND_STOP); /* Send OutData (2 bytes) on the I2C bus and generates a stop condition to end transmission */
	}while(Error == ERR_BUSY);
	
	if (Error != ERR_OK)
	{
		PE_DEBUGHALT();
	}
	
	while (!g_I2C1_DataTransmittedFlg) {}  /* Wait until OutData are transmitted */
	g_I2C1_DataTransmittedFlg = FALSE;
	return SUCCESS;
}

/*
 *   Handle power management for BU26507 ROHM chip.
 */
corona_err_t pwr_bu26507(pwr_state_t state)
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

#if 0
/*
 *     READING IS NOT SUPPORTED WITH THE DISPLAY DEVICE !
 */
// Read data from a register.
corona_err_t bu26507_read(uint8_t reg, uint8_t *pData)
{
	LDD_TError Error;
	
	while (!g_I2C1_DataTransmittedFlg)  // TODO - These transmit flags need a TIMEOUT.
	{
		do
		{
			Error = I2C_Display_MasterSendBlock(MyI2CPtr, &reg, 1U, LDD_I2C_SEND_STOP); /* Send OutData (1 bytes) on the I2C bus and generates a stop condition to end transmission */
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
			Error = I2C_Display_MasterReceiveBlock(MyI2CPtr, pData, 1U, LDD_I2C_SEND_STOP); /* Receive InpData (1 bytes) from the I2C bus and generates a stop condition to end transmission */  
		} while(Error == ERR_BUSY);

		if (Error != ERR_OK)
		{
			PE_DEBUGHALT();
		}
	}                                      /* Wait until InpData are received */
	g_I2C1_DataReceivedFlg = FALSE;
	return SUCCESS;
}
#endif
