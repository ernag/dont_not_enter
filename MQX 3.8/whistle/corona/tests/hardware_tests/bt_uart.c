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
//! \file bt_uart.c
//! \brief BT UART driver.
//! \version version 0.1
//! \date Jan, 2013
//! \author chris@whistle.com
//!
//! \detail This is the accel driver.  It handles talking to the accel and handling accel events.
//! \todo BT_PWDN_SET inside bt_uart.c needs to be fixed.
//! \todo DataTx and DataRx flag in related interrupts aren't there.  Need to add for all.
//! \todo Test to make sure RTS/CTS works!
//! \todo Try a MUCH faster baud rate for Bluetooth, like 921600.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "bt_uart.h"

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#define HCI_COMMAND_CODE_VENDOR_SPECIFIC_DEBUG_OGF    0x3F
#define HCI_VS_HCILL_PARAMETERS_OGF                   (HCI_COMMAND_CODE_VENDOR_SPECIFIC_DEBUG_OGF)
#define HCI_VS_HCILL_PARAMETERS_OCF                   (0x12B)
#define HCI_VS_HCILL_PARAMETERS_SIZE                  (5)
#define CMD_SZ                                        (2 + 1 + HCI_VS_HCILL_PARAMETERS_SIZE)

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! \brief Bluetooth UART unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Execute a unit test for bluetooth UART.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int bt_uart_test(void)
{
    unsigned char cmd[16], InpData[10];
    LWGPIO_STRUCT clk32K, btpwrdn;
    unsigned char result;
    MQX_FILE_PTR  fd;
    uint_32       param;

    corona_print("BT UART Test\n");

    /* 32 KHz clock test */
    if (!lwgpio_init(&clk32K, BSP_GPIO_BT_32KHz_EN_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_BT_32KHz_EN_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&clk32K, BSP_GPIO_BT_32KHz_EN_MUX_GPIO);

    lwgpio_set_value(&clk32K, LWGPIO_VALUE_HIGH);

    _time_delay(100);

    /*  BT_PWRDN_B */
    if (!lwgpio_init(&btpwrdn, BSP_GPIO_BT_PWDN_B_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_BT_PWDN_B_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&btpwrdn, BSP_GPIO_BT_PWDN_B_MUX_GPIO);

    lwgpio_set_value(&btpwrdn, LWGPIO_VALUE_HIGH);
    _time_delay(100);
    lwgpio_set_value(&btpwrdn, LWGPIO_VALUE_LOW);
    _time_delay(100);
    lwgpio_set_value(&btpwrdn, LWGPIO_VALUE_HIGH);
    _time_delay(100);

    fd = (pointer)fopen("ittya:", (const char *)IO_SERIAL_HW_FLOW_CONTROL);
    if (fd == NULL)
    {
        corona_print("Cannot open the ittya:\n");
        _task_block();
    }

#if 0
    // sample code to do it manually instead of during fopen
    param = IO_SERIAL_HW_FLOW_CONTROL;
    if (0 != ioctl(fd, IO_IOCTL_SERIAL_SET_FLAGS, &param))
    {
        corona_print("IO_SERIAL_HW_FLOW_CONTROL error");
        _task_block();
    }

    param = 115200;
    if (0 != ioctl(fd, IO_IOCTL_SERIAL_SET_BAUD, &param))
    {
        corona_print("IO_IOCTL_SERIAL_SET_BAUD error");
        _task_block();
    }

    param = 8;
    if (0 != ioctl(fd, IO_IOCTL_SERIAL_SET_DATA_BITS, &param))
    {
        corona_print("IO_IOCTL_SERIAL_SET_DATA_BITS error");
        _task_block();
    }

    param = IO_SERIAL_PARITY_NONE;
    if (0 != ioctl(fd, IO_IOCTL_SERIAL_SET_PARITY, &param))
    {
        corona_print("IO_IOCTL_SERIAL_SET_PARITY error");
        _task_block();
    }
#endif

    for ( ; ; )
    {
        /*
         *   On Tue, Feb 19, 2013 at 4:36 PM, Tim Thomas <tthomas@stonestreetone.com> wrote:
         * 01 03 0C 00 Reset command
         * 04 0F 04 01 03 0C 00 Response
         *
         * The above ended up being one of the big "fixes" to make BT comm "work".
         */
        cmd[0] = 0x01;
        cmd[1] = 0x03;
        cmd[2] = 0x0C;
        cmd[3] = 0x00;

        if (4 != fwrite(cmd, 1, 4, fd))
        {
            corona_print("fwrite failed\n");
        }

        fflush(fd);

        result = 0;
        do
        {
            result += fread(InpData + result, 1, 7 - result, fd);
        } while (result < 7);

        corona_print("Received Data: 0x%x %x %x %x %x %x %x\n",
               InpData[0], InpData[1], InpData[2], InpData[3],
               InpData[4], InpData[5], InpData[6]);

        fflush(stdout);
    }

    return 0;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
