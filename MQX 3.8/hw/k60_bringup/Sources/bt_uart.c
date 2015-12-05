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
#include "bt_uart.h"
//#include "k22_irq.h"
#include "UART0_BT.h"
#include "PORTB_GPIO_LDD.h"
#include "PORTE_GPIO_LDD.h"
#include "pwr_mgr.h"
#include "corona_console.h"
extern cc_handle_t g_cc_handle;


////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
volatile bool g_UART0_DataReceivedFlg = FALSE;
volatile bool g_UART0_DataTransmittedFlg = FALSE;
static LDD_TDeviceData *g_bt_uart_ptr = 0;
static bool g_stress_test_ran = 0;


// ISR vector table is not correct by default.
// Need to overwrite sections 0x33 and 0x34 (I2S0_TX and I2S0_RX) with the UART2 ISR's in vectors.c:
// (tIsrFunc)&UART2_BT_UART_Interrupt,      /* 0x33  0x000000CC   -   ivINT_I2S0_Tx                  unused by PE */
// (tIsrFunc)&UART2_BT_UART_Interrupt,      /* 0x34  0x000000D0   -   ivINT_I2S0_Rx                  unused by PE */
// Also, if we want to interrupt on CTS (for debugging) we need to overwrite section 0x4E (FTM0) with INT_PORTD ISR in vectors.c:
// (tIsrFunc)&Cpu_ivINT_PORTD,          /* 0x4E  0x00000138   8   ivINT_FTM0                     used by PE */

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define HCI_COMMAND_CODE_VENDOR_SPECIFIC_DEBUG_OGF                      0x3F
#define HCI_VS_HCILL_PARAMETERS_OGF             (HCI_COMMAND_CODE_VENDOR_SPECIFIC_DEBUG_OGF)
#define HCI_VS_HCILL_PARAMETERS_OCF             (0x12B)
#define HCI_VS_HCILL_PARAMETERS_SIZE            (5)
#define CMD_SZ (2+1+HCI_VS_HCILL_PARAMETERS_SIZE)

typedef unsigned char  Byte_t;                   /* Generic 8 bit Container.   */
typedef unsigned short Word_t;                   /* Generic 16 bit Container.  */
typedef unsigned long  DWord_t;                  /* Generic 32 bit Container.  */

#define ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD(_x, _y)  \
{                                                                   \
  ((Byte_t *)(_x))[0] = ((Byte_t)(((DWord_t)(_y)) & 0xFF));         \
  ((Byte_t *)(_x))[1] = ((Byte_t)((((DWord_t)(_y)) >> 8) & 0xFF));  \
  ((Byte_t *)(_x))[2] = ((Byte_t)((((DWord_t)(_y)) >> 16) & 0xFF)); \
  ((Byte_t *)(_x))[3] = ((Byte_t)((((DWord_t)(_y)) >> 24) & 0xFF)); \
}

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

/*
 *   Change the baud rate for BT device (woah.)
 */
static corona_err_t _bt_set_baud(bt_baud_t baud_rate)
{
	uint8_t cmd[8];
	uint8_t out[16];
	uint32_t i = 0;
	uint32_t msTimeout = 5000;
	LDD_TError Error;
	
	cc_print(&g_cc_handle, "Change BT Baud: [ %u ]\n", baud_rate);
	
	/*
	 *   Example UART bytes sent if the baud rate is 1.25Mbps.
	 *   
	 *   Command = HCI_VS_Update_UART_HCI_Baudrate (0xFF36).
	 *   
	 *      01 36 FF 04 D0 12 13 00
	 *      
	 *      01:  write bit.
	 *      36:  2nd byte of command
	 *      FF:  1st byte of command
	 *      04:  number of bytes in payload.
	 *      
	 *      0xD0121300 is the result of ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD( pDestination, 1,250,000 )
	 */
	cmd[0] = 0x01;
	cmd[1] = 0x36;
	cmd[2] = 0xFF;
	cmd[3] = 0x04;
	
	ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD( &(cmd[4]), (unsigned int)baud_rate )
	
	// First you need to tell the BT chip what the new baud rate is, using the existing (low) baud rate.
	if(!g_bt_uart_ptr)
	{
		g_bt_uart_ptr = UART0_BT_Init(NULL, BT_BAUD_DEFAULT_32KHz_Src);
	}
	
	g_UART0_DataTransmittedFlg = 0;
	g_UART0_DataReceivedFlg = 0;
#if 1
	Error = UART0_BT_ReceiveBlock(g_bt_uart_ptr, out, 8);
	if (Error != ERR_OK) {
		cc_print(&g_cc_handle, "UART0_BT_UART_SendBlock failed %x\n", Error);
		return 1;
	}
#endif
	Error = UART0_BT_SendBlock(g_bt_uart_ptr, cmd, 8);
	if (Error != ERR_OK) {
		cc_print(&g_cc_handle, "UART0_BT_UART_SendBlock failed %x\n", Error);
		return 1;
	}
	
	while (!g_UART0_DataTransmittedFlg) {
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "TIMEOUT waiting for UART send block!\n");
			return CC_ERROR_GENERIC;
		}
		if(msTimeout < 100)
		{
			sleep(1);
		}
	}
	g_UART0_DataTransmittedFlg = 0;

#if 0
	msTimeout = 5000;
	while (!g_UART0_DataReceivedFlg) {
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "TIMEOUT waiting for UART receive block!\n");
			return CC_ERROR_GENERIC;
		}
		
		if(msTimeout < 100)
		{
			sleep(1);
		}
	}
	g_UART0_DataReceivedFlg = 0;
#endif
	
	sleep(1000);  // no data to receive, but give time for K60 to flush out data to BT before stopping UART.
	
	// Next you must reconfigure the UART with this new baud rate.
	UART0_BT_Deinit(g_bt_uart_ptr);
	sleep(500);
	g_bt_uart_ptr = UART0_BT_Init(NULL, baud_rate);  // this is where baud rate is increased.
	
	cc_print(&g_cc_handle, "Checking new BT baud...\n");
	sleep(500);
	
	// Finally, we run testbrd to see if the new baud rate is working.
	return bt_uart_comm_ok(baud_rate);
}

/*
 *   Send pData, read back what we get.
 *   Make sure they  match and return error if not.
 */
static corona_err_t _bt_reflect(uint8_t *pData, uint32_t len)
{
	uint8_t out[256];
	uint32_t i = 0;
	uint32_t msTimeout = 5000;
	LDD_TError Error;
	
	memset(out, 0, 256);
	g_UART0_DataTransmittedFlg = 0;
	g_UART0_DataReceivedFlg = 0;

	Error = UART0_BT_ReceiveBlock(g_bt_uart_ptr, out, len);
	if (Error != ERR_OK) {
		cc_print(&g_cc_handle, "UART0_BT_UART_SendBlock failed %x\n", Error);
		return 1;
	}

	Error = UART0_BT_SendBlock(g_bt_uart_ptr, pData, len);
	if (Error != ERR_OK) {
		cc_print(&g_cc_handle, "UART0_BT_UART_SendBlock failed %x\n", Error);
		return 1;
	}
	
	while (!g_UART0_DataTransmittedFlg) {
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "TIMEOUT waiting for UART send block!\n");
			return CC_ERROR_GENERIC;
		}
		if(msTimeout < 100)
		{
			sleep(1);
		}
	}
	g_UART0_DataTransmittedFlg = 0;
	msTimeout = 5000;

	while (!g_UART0_DataReceivedFlg) {
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "TIMEOUT waiting for UART receive block!\n");
			return CC_ERROR_GENERIC;
		}
		
		if(msTimeout < 100)
		{
			sleep(1);
		}
	}
	g_UART0_DataReceivedFlg = 0;

	while(i < len) 
	{
		if(out[i] != pData[i])
		{
			cc_print(&g_cc_handle, "ERR: idx %u\tout %x\tin %x\n", i, out[i], pData[i]);
			return 1;
		}
		i++;
	}
	
	return 0;
	
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Bluetooth UART stress test.
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t bt_stress(void)
{
	LDD_TError Error;
	uint32_t msTimeout = 100, loop = 0;
	unsigned char cmd[16], InpData[10];
	
	if( !g_stress_test_ran )
	{
		/*
		 *   We can only support testbrd if we haven't enabled UART loopback,
		 *     as UART loopback requires a reset of the BT chip, therefore we just skip
		 *     to the part where we retest the UART loopback below, which will work.
		 */
		
		// First, up the baud rate to ~3Mbps
		if( _bt_set_baud(BT_BAUD_HIGH) )
		{
			cc_print(&g_cc_handle, "ERR: BT FAIL set baud COM\n");
			return 1;
		}
		cc_print(&g_cc_handle, "BT Baud OK\n");
		
		cc_print(&g_cc_handle, "Running testbrd for BT 10 times.\n");
		while(loop < 10)
		{
			cc_print(&g_cc_handle, "%u\n", loop);
			
			if(!g_bt_uart_ptr)
			{
				g_bt_uart_ptr = UART0_BT_Init(NULL, BT_BAUD_HIGH);
			}
			
			if( bt_uart_comm_ok(BT_BAUD_HIGH) )
			{
				cc_print(&g_cc_handle, "ERR: BT FAIL testbrd COMM\n");
				return 1;
			}
			loop++;
		}
		
		loop = 0;

		if(!g_bt_uart_ptr)
		{
			g_bt_uart_ptr = UART0_BT_Init(NULL, BT_BAUD_HIGH);
		}
		
		// Send the UART_Loopback_enable command (0xFD4B).
		g_stress_test_ran = 1;
		cmd[0] = 0x01;  // command bit.
		cmd[1] = 0x4B;  // last char of command.
		cmd[2] = 0xFD;  // first char of command.
		cmd[3] = 0x00;
	
		Error = UART0_BT_ReceiveBlock(g_bt_uart_ptr, InpData, 7);
		if (Error != ERR_OK) {
			cc_print(&g_cc_handle, "UART0_BT_UART_SendBlock failed %x\n", Error);
		}
	
		Error = UART0_BT_SendBlock(g_bt_uart_ptr, cmd, 4);
		if (Error != ERR_OK) {
			cc_print(&g_cc_handle, "UART0_BT_UART_SendBlock failed %x\n", Error);
		}
		while (!g_UART0_DataTransmittedFlg)
		{
			if(msTimeout-- == 0)
			{
				cc_print(&g_cc_handle, "TIMEOUT waiting for UART send block!\n");
				return CC_ERROR_GENERIC;
			}
			sleep(1);
		}
	
		msTimeout = 100;
	
		while (!g_UART0_DataReceivedFlg)
		{
			if(msTimeout-- == 0)
			{
				cc_print(&g_cc_handle, "TIMEOUT waiting for UART receive block!\n");
				return CC_ERROR_GENERIC;
			}
			sleep(1);
		}
		g_UART0_DataTransmittedFlg = 0;
		g_UART0_DataReceivedFlg = 0;
	
		cc_print(&g_cc_handle, "Received Data: 0x%x %x %x %x %x %x %x\n",
				InpData[0],InpData[1],InpData[2],InpData[3],
				InpData[4],InpData[5],InpData[6]);
	
		// Expected: 0x4 e 4 1 4B FD 0  (after the Enable UART loopback command.
		if( (InpData[0] != 0x4) || 
			(InpData[1] != 0xe) || 
			(InpData[2] != 0x4) || 
			(InpData[3] != 0x1) || 
			(InpData[4] != 0x4b)||
			(InpData[5] != 0xfd) )
		{
			cc_print(&g_cc_handle, "ERROR! Expected 0x4 e 4 1 4b fd 0.\n");
			return CC_ERROR_GENERIC;
		}
	
	}
	
	if(!g_bt_uart_ptr)
	{
		g_bt_uart_ptr = UART0_BT_Init(NULL, BT_BAUD_HIGH);
	}
	
	/*
	 *   At this point, the UART is just a reflector.  Everything we send, we will receive.  :-)
	 */
	#define how_big	  256		// how many samples to send each time.
	#define __num_loops (256)
	while( loop < __num_loops )
	{
		uint8_t fake[how_big];
		int i;
		
		for(i = 0; i < how_big; i++)
		{
			fake[i] = (loop + i) % 0xFF;
		}
		
		if( _bt_reflect(fake, how_big) )
		{
			cc_print(&g_cc_handle, "\n\rFAIL: loop %u\n\r", loop);
			return 1;
		}
		
#if 0
		cc_print(&g_cc_handle, "LOOPBACK[%u bytes]:\t[ # %u ]\n\r", how_big, loop);
#else
		if( 0 == (loop%1000) )
		{
			cc_print(&g_cc_handle, "LOOPBACK [ %u bytes ]\n\r", how_big*loop, loop);
		}
#endif
		loop++;
	}

	cc_print(&g_cc_handle, "\n\n\r***\tUART LOOPBACK PASS! [ %u ] total bytes\t***\n\r\n\r", how_big*__num_loops);

	UART0_BT_Deinit(g_bt_uart_ptr);
    g_bt_uart_ptr = NULL;
	
	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Bluetooth UART communication test.
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t bt_uart_comm_ok(bt_baud_t baud)
{
    LDD_TError Error;
    uint32_t msTimeout = 500;
    uint32_t retries = 10;
    corona_err_t cerr = SUCCESS;
    unsigned char cmd[16], InpData[16];
    
#if 0
    if( g_stress_test_ran )
    {
    	cc_print(&g_cc_handle, "\r\n\r\n\t***\tUnit must be reset after btstress before running BT testbrd !\t***\n\r\n\r");
    	return 1;
    }
#endif
    
    if(!g_bt_uart_ptr)
    {
#if 0
        // The 32KHz clock should be turned on early on now.
        //PORTE_GPIO_LDD_SetFieldValue(NULL, BT_32KHz_EN, 1);
        //sleep(100);
        PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 0);
        sleep(50);
        PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 1);
        sleep(500);
#endif
        g_bt_uart_ptr = UART0_BT_Init(NULL, baud);
    }

    cmd[0] = 0x01;
    cmd[1] = 0x03;
    cmd[2] = 0x0C;
    cmd[3] = 0x00;

    do
    {
        Error = UART0_BT_ReceiveBlock(g_bt_uart_ptr, InpData, 7);
        if (Error != ERR_OK) 
        {
            sleep(50);
        }
    }while( (ERR_BUSY == Error) && (retries-- > 0) );
    
    if(Error != ERR_OK)
    {
        cerr = CC_ERROR_GENERIC;
        goto done_bttest;
    }
    
    retries = 10;

    do
    {
        Error = UART0_BT_SendBlock(g_bt_uart_ptr, cmd, 4);
        if (Error != ERR_OK) 
        {
            sleep(50);
        }
    }while( (ERR_BUSY == Error) && (retries-- > 0) );

    if(Error != ERR_OK)
    {
        cerr = CC_ERROR_GENERIC;
        goto done_bttest;
    }
    
    while (!g_UART0_DataTransmittedFlg)
    {
        if(msTimeout-- == 0)
        {
            cc_print(&g_cc_handle, "\rTIMEOUT waiting for UART send block!\n\r");
            cerr = CC_ERROR_GENERIC;
            goto done_bttest;
        }
        sleep(1);
    }
    g_UART0_DataTransmittedFlg = FALSE;

    msTimeout = 500;

    while (!g_UART0_DataReceivedFlg)
    {
        if(msTimeout-- == 0)
        {
            cc_print(&g_cc_handle, "\rTIMEOUT waiting for UART receive block!\n\r");
            cerr = CC_ERROR_GENERIC;
            goto done_bttest;
        }
        sleep(1);
    }
    g_UART0_DataReceivedFlg = FALSE;

#if 0
    cc_print(&g_cc_handle, "Received Data: 0x%x %x %x %x %x %x %x\n\r",
                            InpData[0],InpData[1],InpData[2],InpData[3],
                            InpData[4],InpData[5],InpData[6]);
#endif
    
    // Expected: 0x4 e 4 1 3 c 0
    if( (InpData[0] != 0x4) || (InpData[1] != 0xe) || (InpData[2] != 0x4) || (InpData[3] != 0x1) || (InpData[4] != 0x3) )
    {
        cc_print(&g_cc_handle, "\rERROR! Expected 0x4 e 4 1 3 c 0.\n\r");
        cerr = CC_ERROR_GENERIC;
        goto done_bttest;
    }
    
done_bttest:

    UART0_BT_Deinit(g_bt_uart_ptr);
    g_bt_uart_ptr = NULL;
    return cerr;

    return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Bluetooth UART communication test.
//!
//! \fntype Non-Reentrant Function
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t bt_getmac(char *pBD_ADDR)
{
	LDD_TError Error;
	uint32_t msTimeout = 3000;
	uint32_t retries = 10;
	corona_err_t cerr = SUCCESS;
	unsigned char cmd[32], InpData[32];
	
	if( SUCCESS != bt_uart_comm_ok(BT_BAUD_DEFAULT) )
	{
	    cc_print(&g_cc_handle, "\ERR UART comm bad\n\r");
	    return CC_ERROR_GENERIC;
	}
	
    if(!g_bt_uart_ptr)
    {
#if 0
        //PORTE_GPIO_LDD_SetFieldValue(NULL, BT_32KHz_EN, 1);
        //sleep(30);
        PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 0);
        sleep(300);
        PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 1);
        sleep(1000);
#endif
        g_bt_uart_ptr = UART0_BT_Init(NULL, BT_BAUD_DEFAULT);
    }
    else
    {
        sleep(50); // sleep a bit as not to collide with the above "comm_ok()" stuff.
    }

    /*
     * 
     *   Command Sequence for BT MAC Address Retrieval
     *   
     *   HCI Write Enter Length 4:
     *       01 09 10 00
     *       HCI Write Exit ret_val 0
     *
     *   HCI Read: Count 13
     *       04 0E 0A 01 09 10 00 E2 3C 31 29 6A BC
     *
     */
	cmd[0] = 0x01;
	cmd[1] = 0x09;
	cmd[2] = 0x10;
	cmd[3] = 0x00;
	
    do
    {
        Error = UART0_BT_ReceiveBlock(g_bt_uart_ptr, InpData, 13);
        if (Error != ERR_OK) 
        {
            sleep(50);
        }
    }while( (ERR_BUSY == Error) && (retries-- > 0) );

    if(Error != ERR_OK)
    {
        cerr = CC_ERROR_GENERIC;
        goto done_btmac;
    }

    retries = 10;
    
    do
    {
        Error = UART0_BT_SendBlock(g_bt_uart_ptr, cmd, 4);
        if (Error != ERR_OK) 
        {
            sleep(50);
        }
    }while( (ERR_BUSY == Error) && (retries-- > 0) );

    if(Error != ERR_OK)
    {
        cerr = CC_ERROR_GENERIC;
        goto done_btmac;
    }
	
	while ( FALSE == g_UART0_DataTransmittedFlg )
	{
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "\rTIMEOUT waiting for UART send block!\n\r");
	        cerr = CC_ERROR_GENERIC;
	        goto done_btmac;
		}
		sleep(1);
	}
    g_UART0_DataTransmittedFlg = FALSE;

	msTimeout = 3000;

	while ( FALSE == g_UART0_DataReceivedFlg )
	{
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "\rTIMEOUT waiting for UART receive block!\n\r");
	        cerr = CC_ERROR_GENERIC;
	        goto done_btmac;
		}
		sleep(1);
	}
	g_UART0_DataReceivedFlg = FALSE;
    
    /*
     *   Copy BT MAC Addr into the caller's stupid buffer.
     */
    sprintf(pBD_ADDR, "%02x:%02x:%02x:%02x:%02x:%02x\0",
            InpData[12], InpData[11], InpData[10], InpData[9], InpData[8], InpData[7]);
    
done_btmac:

    UART0_BT_Deinit(g_bt_uart_ptr);
    g_bt_uart_ptr = NULL;
	return cerr;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Bluetooth UART unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Execute a unit test for bluetooth UART.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t bt_uart_test(void)
{
	LDD_TError Error;
	unsigned char cmd[16], InpData[10];
	corona_err_t cerr = SUCCESS;

	cc_print(&g_cc_handle, "BT UART Test\n");
	if(!g_bt_uart_ptr)
	{
#if 0
        PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 0);
        sleep(300);
        PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 1);
        sleep(1000);
#endif
        g_bt_uart_ptr = UART0_BT_Init(NULL, BT_BAUD_DEFAULT);
	}

	for(;;)
	{
/*
 *   On Tue, Feb 19, 2013 at 4:36 PM, Tim Thomas <tthomas@stonestreetone.com> wrote:
			 01 03 0C 00 Reset command
			 04 0F 04 01 03 0C 00 Response

			 The above ended up being one of the big "fixes" to make BT comm "work".
 */
		cmd[0] = 0x01;
		cmd[1] = 0x03;
		cmd[2] = 0x0C;
		cmd[3] = 0x00;

		Error = UART0_BT_ReceiveBlock(g_bt_uart_ptr, InpData, 7);
		if (Error != ERR_OK) {
			cc_print(&g_cc_handle, "UART0_BT_UART_SendBlock failed %x\n", Error);
			goto done_uart;
		}

		Error = UART0_BT_SendBlock(g_bt_uart_ptr, cmd, 4);
		if (Error != ERR_OK) {
			cc_print(&g_cc_handle, "UART0_BT_UART_SendBlock failed %x\n", Error);
			goto done_uart;
		}
		while ( FALSE == g_UART0_DataTransmittedFlg ) {}
		g_UART0_DataTransmittedFlg = TRUE;

		while ( FALSE == g_UART0_DataReceivedFlg ) {}
		g_UART0_DataReceivedFlg = TRUE;

		cc_print(&g_cc_handle, "Received Data: 0x%x %x %x %x %x %x %x\n",
				InpData[0],InpData[1],InpData[2],InpData[3],
				InpData[4],InpData[5],InpData[6]);
	}

done_uart:

    UART0_BT_Deinit(g_bt_uart_ptr);
    g_bt_uart_ptr = NULL;
    return cerr;
}

/*
 *   Handle power management for Bluetooth.
 */
corona_err_t pwr_bt(pwr_state_t state)
{
	if(!g_bt_uart_ptr)
	{
#if 0
        PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 0);
        sleep(300);
        PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 1);
        sleep(1000);
#endif
        g_bt_uart_ptr = UART0_BT_Init(NULL, BT_BAUD_DEFAULT);
	}

	switch(state)
	{
		case PWR_STATE_SHIP:
		case PWR_STATE_STANDBY:
			// 1.  Disable UART0.
			UART0_BT_Disable(g_bt_uart_ptr);

			// 2.  Make sure RTS and TX are driven high.
			PORTD_PCR4 = PORT_PCR_MUX(0x01);
			GPIOD_PDDR |= 0x10; // pin 4 is an output now
			GPIOD_PDOR |= 0x10; // pin 4 is BT_RTS

			PORTD_PCR7 = PORT_PCR_MUX(0x01);
			GPIOD_PDDR |= 0x80; // pin 7 is an output now
			GPIOD_PDOR |= 0x80; // pin 7 is BT_TXD

			// 3.  Assert BT_PWDN_B pin.
			PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 0);

			// 4.  Remove 32KHz clock from Silego.
			PORTE_GPIO_LDD_SetFieldValue(NULL, BT_32KHz_EN, 0);
			break;

		case PWR_STATE_NORMAL:
			// 1.  Enable UART0.
			UART0_BT_Enable(g_bt_uart_ptr);

			// 2.  Enable the 32KHz clock from Silego.
			PORTE_GPIO_LDD_SetFieldValue(NULL, BT_32KHz_EN, 1);

			// 3.  De-assert BT_PWDN_B pin.
			PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 1);
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
