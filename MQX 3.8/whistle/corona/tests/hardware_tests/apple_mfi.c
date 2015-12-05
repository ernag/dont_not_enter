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
#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <mqx.h>
#include <bsp.h>
#include <i2c.h>
#include "apple_mfi.h"

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Prototypes
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////
//! \brief Simple MFI Unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function runs infinite loop, asking MFI chip for register info for unit testing.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int apple_mfi_test(void)
{
    uint_8        OutData[2];
    uint_8        InpData[16];
    unsigned char result;
    uint_32       n;
    MQX_FILE_PTR  fd;
    LWGPIO_STRUCT mfi_rst_b;
    uint_32       param;

    corona_print("Apple MFI Test\n");

#if 0
    /* MFI_RST_B GPIO needs to be initialized high */
    if (!lwgpio_init(&mfi_rst_b, BSP_GPIO_MFI_RST_B_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_MFI_RST_B_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* switch pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&mfi_rst_b, BSP_GPIO_MFI_RST_B_MUX_GPIO);

    lwgpio_set_value(&mfi_rst_b, LWGPIO_VALUE_HIGH);

    _time_delay(100);
#endif

    /* Open the I2C driver */
    fd = fopen("ii2c1:", NULL);
    if (fd == NULL)
    {
        corona_print("Failed to open the I2C driver!\n");
        _task_block();
    }

    corona_print("Set master mode ... ");
    if (I2C_OK == ioctl(fd, IO_IOCTL_I2C_SET_MASTER_MODE, NULL))
    {
        corona_print("OK\n");
    }
    else
    {
        corona_print("ERROR\n");
    }

    param = AAPL_MFI_ADDR_RST1;
    corona_print("Set slave address to 0x%02x ... ", param);
    if (I2C_OK == ioctl(fd, IO_IOCTL_I2C_SET_DESTINATION_ADDRESS, &param))
    {
        corona_print("OK\n");
    }
    else
    {
        corona_print("ERROR\n");
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
    }

    /* Configure I2C BUS device(e.g. RTC) - Write Operation */
    for ( ; ; )
    {
        OutData[0] = MFI_PROCTL_MAJOR_VERSION; //MAJOR_VRSN=0x2  //MFI_DEV_VERSION;  // just see if the chip spits back 0x05, which is dev version.

        for ( ; ; )
        {
            corona_print("  Write ... ");
            if (1 != fwrite(OutData, 1, 1, fd))
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
                }

                if (param)
                {
                    corona_print("No Ack\n");
                    continue;
                }
                else
                {
                    corona_print("Ack\n");
                    break;
                }
            }
            else
            {
                corona_print("ERROR\n");
                continue;
            }
        }

#if 0
        corona_print("  Wait for I2C_STATE_READY ... ");
        do
        {
            if (I2C_OK == ioctl(fd, IO_IOCTL_I2C_GET_STATE, &param))
            {
                corona_print("OK\n");
            }
            else
            {
                corona_print("ERROR\n");
            }
            corona_print("STATE = %x\n", param);
        } while (param != I2C_STATE_READY);
#endif


        /* Set read request */
        n     = 1; // number of bytes to read
        param = n;
        corona_print("  Set number of bytes requested to %d ... ", param);
        if (I2C_OK == ioctl(fd, IO_IOCTL_I2C_SET_RX_REQUEST, &param))
        {
            corona_print("OK\n");
        }
        else
        {
            corona_print("ERROR\n");
        }

        corona_print("  Read %d bytes ... ", n);
        result = 0;
        do
        {
            result += fread(InpData + result, 1, n - result, fd);
        } while (result < n);
        corona_print("OK\n");

        corona_print("  ioctl flush ... ");
        if (I2C_OK == ioctl(fd, IO_IOCTL_FLUSH_OUTPUT, &param))
        {
            corona_print("OK\n");
        }
        else
        {
            corona_print("ERROR\n");
        }

        /* Stop I2C transfer */
        corona_print("  Stop transfer ... ");
        if (I2C_OK == ioctl(fd, IO_IOCTL_I2C_STOP, NULL))
        {
            corona_print("OK\n");
        }
        else
        {
            corona_print("ERROR\n");
        }

        corona_print("Received Data: 0x%x\n", InpData[0]);

        _time_delay(1000);
    }

    return 0;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
