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
//! \file bu26507_display.c
//! \brief ROHM display driver.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
//!
//! \detail This is the ROHM display driver.  It handles talking to the display, managing the PWM.
//! \todo Write a test for DISPLAY_SYNC_PWM.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "bu26507_display.h"
#include "I2C1.h"
//#include "i2c.h"
#include "DISPLAY_SYNC_PWM0.h"
#include "pwr_mgr.h"
#include "corona_console.h"
#include "cfg_mgr.h"
#include "cfg_factory.h"
#include "wson_flash.h"
#include <stdlib.h>
extern cc_handle_t g_cc_handle;


////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
volatile bool g_I2C1_DataReceivedFlg    = FALSE;
volatile bool g_I2C1_DataTransmittedFlg = FALSE;
static LDD_TDeviceData *g_rohm_hdl = NULL;
LDD_TDeviceData *DeviceDataPtr = NULL;
static ledflex_t g_ledflex = LEDFLEX_INVALID;

bu_color_t g_cstate = BU_NUM_COLORS;

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define NUM_CMDS	9
volatile display_cmd_t g_display_cmds[NUM_CMDS] = {
	{0x7F, 0x00}, // 1) 7FH 00000000 Select control register
	{0x21, /*0x0C*/0x04}, // 2) 21H 00000000 Select internal OSC for CLK,  AND make SYNC pin = 'H' control LED's on/off.
	{0x01, 0x08}, // 3) 01H 00001000 Start OSC
	{0x11, 0x3F}, // 4) 11H 00111111 Set LED1-6 enable
	{0x12, 0x3F}, // 5) 12H 00111111 Set LED7-12 enable
	{0x13, 0x1F}, // 6) 13H 00011111 Set LED13-17 enable
	{0x20, 0x04}, // 7) 20H 00111111 duty cycle (brightness setting), 0x3F is MAX.
	{0x2D, 0x06}, // 11) 2DH 00000110 Set SLOPE control enable
	{0x30, 0x01}, // 12) 30H 00000001 Start SLOPE sequence
};

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

static ledflex_t get_ledflex(void)
{
    fcfg_handle_t hdl;
    
    if( LEDFLEX_INVALID == g_ledflex )
    {
        /*
         *   Init the handle, functions to use when talking to the factory config table.
         */
        if( FCFG_OK != FCFG_init( &hdl,
                                  EXT_FLASH_FACTORY_CFG_ADDR,
                                  &wson_read,
                                  &wson_write,
                                  &fcfg_malloc,
                                  &fcfg_free) )
        {
            cc_print(&g_cc_handle, "ERROR: FCFG_init()!\n");
            return CC_ERROR_GENERIC;
        }
        
        /*
         *   Fill up all the factory config values, if they are present.
         */
        CFGMGR_faccfg_get( &hdl, CFG_FACTORY_LPCBAREV, FCFG_INT_SZ, &g_ledflex, "LED Revision"  );
    }
    
    return g_ledflex;
}

corona_err_t bu26507_pwm(uint16_t percentage)
{
	uint16_t ratio = 655 * percentage;

	if(percentage >= 100)
	{
		ratio = 0xFFFF;
	}

	if(!DeviceDataPtr)
	{
		DeviceDataPtr = DISPLAY_SYNC_PWM0_Init(NULL);
	}

	if(DISPLAY_SYNC_PWM0_SetRatio16(DeviceDataPtr, ratio) != ERR_OK)
	{
		cc_print(&g_cc_handle, "PWM0 returned an error!\n");
		return CC_ERR_PWM;
	}
	return SUCCESS;
}

corona_err_t bu26507_init(void)
{
	if(!g_rohm_hdl)
	{
		g_rohm_hdl = I2C1_Init(NULL);      // by default it is just going to init with device 0x74.
	}
	if(!g_rohm_hdl)
	{
		return CC_I2C_UNINITIALIZED;
	}
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Display unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Just turn on some LED's using the 0x74 display device on Corona
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t bu26507_display_test(void)
{
	uint8_t OutData[2] = {BU_LED_EN_ADDR, 0x3F/*(LED1_MASK | LED3_MASK | LED5_MASK)*/}; // LED enable register, try illuminating interleaved LED's.
	uint8_t InpData[16];
	LDD_TError Error;
	LDD_I2C_TErrorMask ErrorMask;
	unsigned char result;
	unsigned char led_masker = (BU_LED1_MASK | BU_LED3_MASK | BU_LED5_MASK);
	unsigned int i;
	unsigned long retries = 15;

	//cc_print(&g_cc_handle, "BU26507 Display Unit Test - Illuminate some LEDs\n");

	// Initialize the I2C device.
	// NOTE:  Slave Address of the BU26507 is 0x74.  This is shifted to the left since the R/W bit is LSB of
	//        the 8-bit slave address we send.  So slave address is the MS 7 bits.
	//        So PE generates this code for the address: '''DeviceDataPrv->SlaveAddr = 0xE8U;'''
	bu26507_init();

	Error = I2C1_SelectSlaveDevice(g_rohm_hdl, LDD_I2C_ADDRTYPE_7BITS, BU26507_0_SLAVE_ADDR);
	if(Error != ERR_OK)
	{
		cc_print(&g_cc_handle, "Could not select slave device!\n");
		return CC_I2C_UNINITIALIZED;
	}

	for(i = 0; i < NUM_CMDS; i++)
	{
		OutData[0] = g_display_cmds[i].reg;
		OutData[1] = g_display_cmds[i].data;

		do
		{
			Error = I2C1_MasterSendBlock(g_rohm_hdl, OutData, 2U, LDD_I2C_SEND_STOP); /* Send OutData (2 bytes) on the I2C bus and generates a stop condition to end transmission */
		    retries--;
		    sleep(1);
		}while((Error == ERR_BUSY) && (retries != 0) );

		if ( (Error != ERR_OK) || (retries == 0) )
		{
			return -1;
		}

		retries = 15;
		while ( (!g_I2C1_DataTransmittedFlg) && (retries != 0) )
		{
		    sleep(10);
		    retries -= 1;
		}                                   /* Wait until OutData are transmitted */
		g_I2C1_DataTransmittedFlg = FALSE;
		
		if(retries == 0)
		{
		    return -1;
		}

		Error = I2C1_GetError(g_rohm_hdl, &ErrorMask);
		if(Error || ErrorMask)
		{
			cc_print(&g_cc_handle, "I2C1 TX ERROR:  Error (0x%x), ErrorMask(0x%x)\n", Error, ErrorMask);
			return Error;
		}
	}

	return SUCCESS;
}

static uint8_t _RED(void)
{
    return 0x24;
}

static uint8_t _WHITE(void)
{
    return 0x3f;
}

static uint8_t _BLUE(void)
{
    if( LEDFLEX_EVERBRIGHT <= get_ledflex() )
    {
        return 0x12;
    }
    else
    {
        return 0x09;
    }
}

static uint8_t _GREEN(void)
{
    if( LEDFLEX_EVERBRIGHT <= get_ledflex() )
    {
        return 0x09;
    }
    else
    {
        return 0x12;
    }
}

static uint8_t _YELLOW(void)
{
    return ( _RED() | _GREEN() );
}

corona_err_t bu26507_color(bu_color_t c)
{
    uint8_t data;
    
    switch(c)
    {
        case BU_COLOR_RED:
            data = _RED();
            break;
            
        case BU_COLOR_WHITE:
            data = _WHITE();
            break;
            
        case BU_COLOR_GREEN:
            data = _GREEN();
            break;
            
        case BU_COLOR_BLUE:
            data = _BLUE();
            break;
            
        case BU_COLOR_YELLOW:
            data = _YELLOW();
            break;
            
        case BU_COLOR_OFF:
            data = 0x0;
            break;
            
        default:
            return CC_ERROR_GENERIC;
    }
    
    return bu26507_write(0x11, data);
}

// Write a register with data.
corona_err_t bu26507_write(uint8_t reg, uint8_t data)
{
	LDD_TError Error;
	LDD_I2C_TErrorMask ErrorMask;
	uint8_t OutData[2];
	unsigned long retries = 15;

	if(!g_rohm_hdl)
	{
		bu26507_init();
	}

	Error = I2C1_SelectSlaveDevice(g_rohm_hdl, LDD_I2C_ADDRTYPE_7BITS, BU26507_0_SLAVE_ADDR);
	if(Error != ERR_OK)
	{
		cc_print(&g_cc_handle, "Could not select slave device!\n");
		return CC_I2C_UNINITIALIZED;
	}

	OutData[0] = reg;
	OutData[1] = data;

	do
	{
		Error = I2C1_MasterSendBlock(g_rohm_hdl, OutData, 2U, LDD_I2C_SEND_STOP); /* Send OutData (2 bytes) on the I2C bus and generates a stop condition to end transmission */
		retries--;
	}while( (Error == ERR_BUSY) && (retries != 0) );

	if ( (Error != ERR_OK) || (retries == 0) )
	{
		return -1;
	}

	retries = 15;
	while ( (!g_I2C1_DataTransmittedFlg) && (retries != 0) )
	{
	    sleep(10);
	    retries--;
	}  /* Wait until OutData are transmitted */
	g_I2C1_DataTransmittedFlg = FALSE;

	Error = I2C1_GetError(g_rohm_hdl, &ErrorMask);
	if(Error || ErrorMask)
	{
		cc_print(&g_cc_handle, "I2C1 RX ERROR:  Error (0x%x), ErrorMask(0x%x)\n", Error, ErrorMask);
		return Error;
	}

	return SUCCESS;
}

/*
 *   Handle power management for BU26507 ROHM chip.
 */
corona_err_t pwr_bu26507(pwr_state_t state)
{
	uint8_t reg_idx;

	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:
			// 0.  Set PWM to turn LED's off.
			bu26507_pwm(100);

			// 1.  Disable the PWM module.
			DISPLAY_SYNC_PWM0_Disable(DeviceDataPtr);

			// 2.  Turn off OSCEN.
			bu26507_write(0x7f, 0); // select control register
			bu26507_write(BU_OSC_CTRL, 0);

			// 3.  Disable all of the LED's.
			bu26507_write(0x11, 0);
			bu26507_write(0x12, 0);
			bu26507_write(0x13, 0);
			bu26507_write(0x17, 0);

			// 4.  PWM at minimum duty cycle.
			bu26507_write(0x20, 0);

			// 5.  SYNC/clocks to off/initial.
			bu26507_write(0x21, 0);

			// 6.  Slope operation and PWM off.
			bu26507_write(0x2d, 0);

			// 7.  Scroll operation off.
			bu26507_write(0x2f, 0);

			// 8.  Turn off matrix lighting.
			bu26507_write(0x30, 0);

			// 9.  Now, cycle through all of the A and B pattern registers
			bu26507_write(0x7f, 0x1); // select A pattern register

			for(reg_idx = 0; reg_idx <= 0x77; reg_idx++)   // loop through all the A pattern registers, and make sure we are driving 0.00mA
			{
				bu26507_write(reg_idx, 0x0);
			}

			bu26507_write(0x7f, 0x7); // select B pattern register

			for(reg_idx = 0; reg_idx <= 0x77; reg_idx++)   // loop through all the B pattern registers, and make sure we are driving 0.00mA
			{
				bu26507_write(reg_idx, 0x0);
			}

			if(g_rohm_hdl)
			{
				I2C1_Disable(g_rohm_hdl);
			}


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

	if(!g_rohm_hdl)
	{
		bu26507_init();
	}

	Error = I2C1_SelectSlaveDevice(g_rohm_hdl, LDD_I2C_ADDRTYPE_7BITS, BU26507_0_SLAVE_ADDR);
	if(Error != ERR_OK)
	{
		cc_print(&g_cc_handle, "Could not select slave device!\n");
		return CC_I2C_UNINITIALIZED;
	}

	while (!g_I2C1_DataTransmittedFlg)  // TODO - These transmit flags need a TIMEOUT.
	{
		do
		{
			Error = I2C_Display_MasterSendBlock(g_rohm_hdl, &reg, 1U, LDD_I2C_SEND_STOP); /* Send OutData (1 bytes) on the I2C bus and generates a stop condition to end transmission */
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
			Error = I2C_Display_MasterReceiveBlock(g_rohm_hdl, pData, 1U, LDD_I2C_SEND_STOP); /* Receive InpData (1 bytes) from the I2C bus and generates a stop condition to end transmission */
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


////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
