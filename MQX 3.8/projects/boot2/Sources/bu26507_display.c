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
#include "I2C2.h"
//#include "i2c.h"
#include "corona_console.h"
#include "cfg_factory.h"
#include "wson_flash.h"
#include "corona_ext_flash_layout.h"
#include "sleep_timer.h"
#include <stdlib.h>

extern cc_handle_t g_cc_handle;
extern unsigned long fcfg_free(void *pAddr);
extern void *fcfg_malloc(unsigned long size);

#ifndef bool
#define bool unsigned char
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef NULL
#define NULL 0
#endif


////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
volatile bool g_I2C1_DataReceivedFlg    = FALSE;
volatile bool g_I2C1_DataTransmittedFlg = FALSE;
static LDD_TDeviceData *g_rohm_hdl = NULL;
LDD_TDeviceData *DeviceDataPtr = NULL;
static fcfg_led_t g_ledflex = FCFG_LED_INVALID;

bu_color_t g_cstate = BU_NUM_COLORS;

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define NUM_CMDS	9
volatile display_cmd_t g_display_cmds[NUM_CMDS] = {
	{0x7F, 0x00}, // 1) 7FH 00000000 Select control register
	{0x21, /*0x0C*/0x04}, // 2) 21H 00000000 Select internal OSC for CLK,  AND make SYNC pin = 'H' control LED's on/off.
	{0x01, 0x08}, // 3) 01H 00001000 Start OSC
	// 0x11 is what is used for color.
	{0x11, 0x00}, // 4) 11H 00111111 Set LED1-6 enable
	{0x12, 0x3F}, // 5) 12H 00111111 Set LED7-12 enable
	{0x13, 0x1F}, // 6) 13H 00011111 Set LED13-17 enable
	{0x20, 0x3F}, // 7) 20H 00111111 duty cycle (brightness setting), 0x3F is MAX.
	{0x2D, 0x06}, // 11) 2DH 00000110 Set SLOPE control enable
	{0x30, 0x01}, // 12) 30H 00000001 Start SLOPE sequence
};

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

static fcfg_led_t get_ledflex(void)
{
    fcfg_handle_t hdl;
    
    if( FCFG_LED_INVALID == g_ledflex )
    {
        g_ledflex = 0;
        
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
            //cc_print(&g_cc_handle, "ERROR: FCFG_init()!\n");
            return FCFG_LED_INVALID;
        }
        
#if 0
        /*
         *   Fill up all the factory config values, if they are present.
         */
        if( FCFG_OK != FCFG_get(&hdl, CFG_FACTORY_LPCBAREV, &g_ledflex) )
        {
            return LEDFLEX_INVALID;
        }
#else
        g_ledflex = FCFG_led(&hdl);
#endif
    }
    
    return g_ledflex;
}

#if 0
corona_err_t bu26507_pwm(uint16_t percentage)
{
	uint16_t ratio = 655 * percentage;

	if(percentage >= 100)
	{
		ratio = 0xFFFF;
	}

#if 0
	if(!DeviceDataPtr)
	{
		DeviceDataPtr = DISPLAY_SYNC_PWM0_Init(NULL);
	}
#endif
	
	if(DISPLAY_SYNC_PWM0_SetRatio16(DeviceDataPtr, ratio) != ERR_OK)
	{
		cc_print(&g_cc_handle, "PWM0 returned an error!\n");
		return CC_ERR_PWM;
	}
	return SUCCESS;
}
#endif

corona_err_t bu26507_init(void)
{
	if(!g_rohm_hdl)
	{
		g_rohm_hdl = I2C2_Init(NULL);      // by default it is just going to init with device 0x74.
	}
	if(!g_rohm_hdl)
	{
		return CC_I2C_UNINITIALIZED;
	}
	return SUCCESS;
}

/*
 *   Just flickers the lights a specific color for a brief moment,
 *     then leaves it on the color you chose.
 */
static void _flicker(bu_color_t color)
{
    uint8_t i;
    
    for(i=0;i<5;i++)
    {
        bu26507_color(BU_COLOR_OFF);
        sleep(50);
        bu26507_color(color);
        sleep(50);
    }
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
corona_err_t bu26507_display_pattern(bu_pattern_t pattern)
{
	uint8_t OutData[2] = {BU_LED_EN_ADDR, 0x3F/*(LED1_MASK | LED3_MASK | LED5_MASK)*/}; // LED enable register, try illuminating interleaved LED's.
	uint8_t InpData[16];
	LDD_TError Error;
	LDD_I2C_TErrorMask ErrorMask;
	unsigned char result;
	uint32_t retries = 10;
	unsigned int i;

	//cc_print(&g_cc_handle, "BU26507 Display Unit Test - Illuminate some LEDs\n");

	// Initialize the I2C device.
	// NOTE:  Slave Address of the BU26507 is 0x74.  This is shifted to the left since the R/W bit is LSB of
	//        the 8-bit slave address we send.  So slave address is the MS 7 bits.
	//        So PE generates this code for the address: '''DeviceDataPrv->SlaveAddr = 0xE8U;'''
	if( SUCCESS != bu26507_init() )
	{
	    return 1;
	}

	Error = I2C2_SelectSlaveDevice(g_rohm_hdl, LDD_I2C_ADDRTYPE_7BITS, BU26507_0_SLAVE_ADDR);
	if(Error != ERR_OK)
	{
		//cc_print(&g_cc_handle, "Could not select slave device!\n");
		return CC_I2C_UNINITIALIZED;
	}

	for(i = 0; i < NUM_CMDS; i++)
	{
		OutData[0] = g_display_cmds[i].reg;
		OutData[1] = g_display_cmds[i].data;

		do
		{
			Error = I2C2_MasterSendBlock(g_rohm_hdl, OutData, 2U, LDD_I2C_SEND_STOP); /* Send OutData (2 bytes) on the I2C bus and generates a stop condition to end transmission */
			if(ERR_OK != Error)
			{
			    sleep(1);
			    retries--;
			}
		}while( (0 != retries) && (Error == ERR_BUSY) );

		if ( (Error != ERR_OK) || (0 == retries) )
		{
			return 1;
		}

		retries = 10;
		while ( (!g_I2C1_DataTransmittedFlg) && (retries != 0) ) 
		{
		    sleep(1);
		    retries--;
		}                                   /* Wait until OutData are transmitted */
		
		if(0 == retries)
		{
		    return 1;
		}
		
		g_I2C1_DataTransmittedFlg = FALSE;

		Error = I2C2_GetError(g_rohm_hdl, &ErrorMask);
		if(Error || ErrorMask)
		{
			//cc_print(&g_cc_handle, "I2C1 TX ERROR:  Error (0x%x), ErrorMask(0x%x)\n", Error, ErrorMask);
			return Error;
		}
	}
	
	switch(pattern)
	{
	    case BU_PATTERN_SOLID_PURPLE:
	        bu26507_color(BU_COLOR_PURPLE);
	        break;
	        
	    case BU_PATTERN_OFF:
	        bu26507_color(BU_COLOR_OFF);
	        break;
	        
	    case BU_PATTERN_BOOTLDR_MENU:
	        _flicker(BU_COLOR_PURPLE);
	        break;
            
        case BU_PATTERN_BOOTLDR_JUMP_TO_APP:
            _flicker(BU_COLOR_WHITE);
            bu26507_color(BU_COLOR_OFF);
            break;
            
        case BU_PATTERN_BOOTLDR_STAY_IN_BOOT:
            _flicker(BU_COLOR_GREEN);
            bu26507_color(BU_COLOR_PURPLE);
            break;
            
        case BU_PATTERN_BOOTLDR_RESET_FACTORY_DEFAULTS:
            _flicker(BU_COLOR_RED);
            sleep(500);
            _flicker(BU_COLOR_RED);
            bu26507_color(BU_COLOR_GREEN);
            sleep(2000);
            break;
            
        case BU_PATTERN_BOOTLDR_INITIAL_PRESS:
            bu26507_color(BU_COLOR_YELLOW);
            break;
            
        case BU_PATTERN_BOOTLDR_WAIT_FOR_CLICK:
            _flicker(BU_COLOR_RED);
            bu26507_color(BU_COLOR_YELLOW);
            break;
            
        case BU_PATTERN_ABORT:
        default:
            _flicker(BU_COLOR_RED);
            break;
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
    fcfg_led_t flex = get_ledflex();
    
    if( FCFG_LED_INVALID == flex )
    {
        
    }
    else if( FCFG_LED_ROHM == flex )
    {
        return 0x09;
    }
    else if( FCFG_LED_EVERBRIGHT == flex )
    {
        return 0x12;
    }
    
    return _RED();
}

static uint8_t _GREEN(void)
{
    fcfg_led_t flex = get_ledflex();
    
    if( FCFG_LED_INVALID == flex )
    {
        
    }
    else if( FCFG_LED_ROHM == flex )
    {
        return 0x12;
    }
    else if( FCFG_LED_EVERBRIGHT == flex )
    {
        return 0x09;
    }
    
    return _RED();
}

static uint8_t _YELLOW(void)
{
    return ( _RED() | _GREEN() );
}

static uint8_t _PURPLE(void)
{
    return ( _RED() | _BLUE() );
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
            
        case BU_COLOR_PURPLE:
            data = _PURPLE();
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
	uint32_t retries = 10;

	if(!g_rohm_hdl)
	{
		bu26507_init();
	}

	Error = I2C2_SelectSlaveDevice(g_rohm_hdl, LDD_I2C_ADDRTYPE_7BITS, BU26507_0_SLAVE_ADDR);
	if(Error != ERR_OK)
	{
		//cc_print(&g_cc_handle, "Could not select slave device!\n");
		return CC_I2C_UNINITIALIZED;
	}

	OutData[0] = reg;
	OutData[1] = data;

	do
	{
		Error = I2C2_MasterSendBlock(g_rohm_hdl, OutData, 2U, LDD_I2C_SEND_STOP); /* Send OutData (2 bytes) on the I2C bus and generates a stop condition to end transmission */
		if(ERR_OK != Error)
		{
		    sleep(1);
		    retries--;
		}
	}while( (0 != retries) && (Error == ERR_BUSY) );

	if ( (Error != ERR_OK) || (0 == retries) )
	{
		return 1;
	}

    retries = 10;
    while ( (!g_I2C1_DataTransmittedFlg) && (retries != 0) ) 
    {
        sleep(1);
        retries--;
    }                                   /* Wait until OutData are transmitted but don't block 4ever. */
    
    if(0 == retries)
    {
        return 1;
    }
    
	g_I2C1_DataTransmittedFlg = FALSE;

	Error = I2C2_GetError(g_rohm_hdl, &ErrorMask);
	if(Error || ErrorMask)
	{
		//cc_print(&g_cc_handle, "I2C1 RX ERROR:  Error (0x%x), ErrorMask(0x%x)\n", Error, ErrorMask);
		return Error;
	}

	return SUCCESS;
}

#if 0
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

#if 0
			if(g_rohm_hdl)
			{
				I2C2_Disable(g_rohm_hdl);
			}
#endif

			break;

		case PWR_STATE_NORMAL:
			break;

		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}

#endif

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
