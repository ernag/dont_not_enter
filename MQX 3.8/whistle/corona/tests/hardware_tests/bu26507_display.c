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
#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <mqx.h>
#include <bsp.h>
#include <i2c.h>
#include "bu26507_display.h"
#undef corona_print
#define corona_print

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
static volatile display_cmd_t g_display_cmds[] =
{
    //{0x00, 0x01}, // 0) Software Reset
    { 0x7F, 0x00 }, // 1) 7FH 00000000 Select control register
    { 0x21, /*0x0C*/ 0x04}, // 2) 21H 00000000 Select internal OSC for CLK,  AND make SYNC pin = 'H' control LED's on/off.
    { 0x01, 0x08 }, // 3) 01H 00001000 Start OSC
    { 0x11, 0x3F }, // 4) 11H 00111111 Set LED1-6 enable
    { 0x12, 0x3F }, // 5) 12H 00111111 Set LED7-12 enable
    { 0x13, 0x1F }, // 6) 13H 00011111 Set LED13-17 enable
    { 0x20, 0x3F }, // 7) 20H 00111111 Set Max Duty at Slope
    //{0x7F, 0x01}, // 8) 7FH 00000001 Select A-pattern or B-pattern register, Select A-pattern register to write matrix data
    //{0x7F, 0x01}, // 9) 01-77H xxxxxxxx Write A-pattern data
    //{0x7F, 0x00}, // 10) 7FH 00000000 Select control register, Select A-pattern register to output for matrix
    { 0x2D, 0x06 }, // 11) 2DH 00000110 Set SLOPE control enable
    { 0x30, 0x01 }, // 12) 30H 00000001 Start SLOPE sequence
    {0x30, 0x00}  // 13) 30H 00000000 Lights out
};

#define NUM_CMDS    (sizeof(g_display_cmds)/sizeof(display_cmd_t))

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////


/*
 *   NOTE:     This is depcrecated as it does not play nice with Bluetooth.
 *               This needs to use i2c1_dibbs() and i2c1_liberate(), like the real driver does.
 *          OR
 *              It can just use the real driver calls!
 */
MQX_FILE_PTR bu26507_test_init(void)
{
    MQX_FILE_PTR fd;
    uint_32      param;

    /* Open the I2C driver */
    fd = fopen("ii2c1:", NULL);
    if (fd == NULL)
    {
        corona_print("Failed to open the I2C driver!\n");
        return NULL;
    }

    corona_print("Set master mode ... ");
    if (I2C_OK == ioctl(fd, IO_IOCTL_I2C_SET_MASTER_MODE, NULL))
    {
        corona_print("OK\n");
    }
    else
    {
        corona_print("ERROR\n");
        return NULL;
    }

    param = BU26507_0_SLAVE_ADDR;
    corona_print("Set slave address to 0x%02x ... ", param);
    if (I2C_OK == ioctl(fd, IO_IOCTL_I2C_SET_DESTINATION_ADDRESS, &param))
    {
        corona_print("OK\n");
    }
    else
    {
        corona_print("ERROR\n");
        return NULL;
    }

    param = 100000;
    corona_print("Set baud rate ... ");
    if (I2C_OK == ioctl(fd, IO_IOCTL_I2C_SET_BAUD, &param))
    {
        corona_print("%d\n", param);
    }
    else
    {
        corona_print("ERROR\n");
        return NULL;
    }

    return fd;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Display unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Just turn on some LED's using the 0x74 display device on Corona
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int bu26507_display_test(int stayOn)
{
    uint8_t       OutData[2] = { BU_LED_EN_ADDR, 0x3F /*(LED1_MASK | LED3_MASK | LED5_MASK)*/ }; // LED enable register, try illuminating interleaved LED's.
    unsigned char result;
    unsigned int  i;
    uint_32       param;
    MQX_FILE_PTR  fd;
    int err = 0;

    // Initialize the I2C device.
    // NOTE:  Slave Address of the BU26507 is 0x74.  This is shifted to the left since the R/W bit is LSB of
    //        the 8-bit slave address we send.  So slave address is the MS 7 bits.
    //        So PE generates this code for the address: '''DeviceDataPrv->SlaveAddr = 0xE8U;'''

    fd = bu26507_test_init();

    if (fd == NULL)
    {
        corona_print("bu26507_init failed\n");
        return -1;
    }

    for (i = 0; i < NUM_CMDS - stayOn; i++)
    {
        int retries = 100;
        
        OutData[0] = g_display_cmds[i].reg;
        OutData[1] = g_display_cmds[i].data;

        while(retries-- > 0)
        {
            corona_print("  Write ... ");
            if (2 != fwrite(OutData, 1, 2, fd))
            {
                corona_print("ERROR\n");
                continue;
            }
            else
            {
                corona_print("OK\n");
            }

            corona_print("  Flush and check for ack ... ");
            /* Check ack (device exists) */
            if (I2C_OK == ioctl(fd, IO_IOCTL_FLUSH_OUTPUT, &param))
            {
                corona_print("OK\n");

                /* Stop I2C transfer */
                corona_print("  Stop transfer ... ");
                if (I2C_OK == ioctl(fd, IO_IOCTL_I2C_STOP, NULL))
                {
                    corona_print("OK\n");
                }
                else
                {
                    corona_print("ERROR\n");
                    err = -2;
                }

                if (param)
                {
                    corona_print("No Ack\n");
                    err = -1;
                    continue;
                }
                else
                {
                    corona_print("Ack\n");
                    err = 0;
                    break;
                }
            }
            else
            {
                corona_print("ERROR\n");
                err = -3;
                continue;
            }
        }
        
        if(err)
        {
            goto display_done;
        }
        _time_delay(1);
    }

display_done:
    fflush(stdout);
    fclose(fd);
    _time_delay(1);

    return err;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
