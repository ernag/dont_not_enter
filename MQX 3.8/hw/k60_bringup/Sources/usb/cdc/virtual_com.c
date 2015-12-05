/******************************************************************************
 *
 * Freescale Semiconductor Inc.
 * (c) Copyright 2004-2010 Freescale Semiconductor, Inc.
 * ALL RIGHTS RESERVED.
 *
 **************************************************************************//*!
 *
 * @file virtual_com.c
 *
 * @author
 *
 * @version
 *
 * @date May-28-2009
 *
 * @brief  The file emulates a USB PORT as Loopback Serial Port.
 *****************************************************************************/

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "hidef.h"          /* for EnableInterrupts macro */
#include "derivative.h"     /* include peripheral declarations */
#include "types.h"          /* Contains User Defined Data Types */
#include "usb_cdc.h"        /* USB CDC Class Header File */
#include "virtual_com.h"    /* Virtual COM Application Header File */
#include "corona_console.h"
#include "bu26507_display.h"
#include <stdio.h>

#if (defined _MCF51MM256_H) || (defined _MCF51JE256_H)
#include "exceptions.h"
#endif

/* skip the inclusion in dependency state */
#ifndef __NO_SETJMP
	#include <stdio.h>
#endif
#include <stdlib.h>
#include <string.h>

/*****************************************************************************
 * Constant and Macro's - None
 *****************************************************************************/
#ifndef SIM_SOPT1CFG
    #define SIM_SOPT1CFG_URWE_MASK                   0x1000000u
    #define SIM_SOPT1CFG_UVSWE_MASK                  0x2000000u
    #define SIM_SOPT1CFG_USSWE_MASK                  0x4000000u
#endif

/*****************************************************************************
 * Global Functions Prototypes
 *****************************************************************************/
void TestApp_Init(void);
void usb_reset(void);

/****************************************************************************
 * Global Variables
 ****************************************************************************/
#if HIGH_SPEED_DEVICE
uint_32 g_cdcBuffer[DIC_BULK_OUT_ENDP_PACKET_SIZE>>1];
#endif

cc_handle_t g_cc_handle;
uint32_t usb_send_err = 0;

/*****************************************************************************
 * Local Types - None
 *****************************************************************************/

/*****************************************************************************
 * Local Functions Prototypes
 *****************************************************************************/
static void USB_App_Callback(uint_8 controller_ID,
                        uint_8 event_type, void* val);
static void USB_Notify_Callback(uint_8 controller_ID,
                        uint_8 event_type, void* val);
static void Virtual_Com_App(void);
/*****************************************************************************
 * Local Variables
 *****************************************************************************/
#ifdef _MC9S08JS16_H
#pragma DATA_SEG APP_DATA
#endif
/* Virtual COM Application start Init Flag */
static volatile boolean start_app = FALSE;
/* Virtual COM Application Carrier Activate Flag */
//static volatile boolean start_transactions = FALSE;
static volatile boolean start_transactions = TRUE;
/* Receive Buffer */
static uint_8 g_curr_recv_buf[DATA_BUFF_SIZE];
/* Send Buffer */
static uint_8 g_curr_send_buf[DATA_BUFF_SIZE];

#define CONSOLE_BUF_SZ	128
static uint_8 g_console_buf[CONSOLE_BUF_SZ];

/* Receive Data Size */
static uint_8 g_recv_size;
/* Send Data Size */
static uint_8 g_send_size;
/*****************************************************************************
 * Local Functions
 *****************************************************************************/


void usb_reset(void)
{
    uint32_t *const pSIM_SOPT1CFG = (uint32_t *)0x40047004; // address of SIM_SOPT1CFG, since not defined in IO_Map.h
   
    /*
     *   Generate a hard reset of the USB module.
     */
    SIM_SCGC4 |= SIM_SCGC4_USBOTG_MASK;  
    
    USB0_USBTRC0 = 0x80U;   
    while( USB0_USBTRC0 & 0x80 );
    
    /* Detach CFv1 core to USB bus*/
    USB0_USBTRC0 &= ~0x40;
    
    /* Clear USB interrupts*/
    USB0_ISTAT = 0xbf;
    
    /* Disable USB RESET Interrupt */
    USB0_INTEN &= ~USB_INTEN_USBRSTEN_MASK;
    
    /* Disable USB module */
    USB0_CTL &= ~USB_CTL_USBENSOFEN_MASK;
        
    USB0_OTGCTL &= ~(USB_OTGCTL_DPHIGH_MASK | USB_OTGCTL_OTGEN_MASK);
    
    /*
     *   Turn off USB regulator.
     */
    *pSIM_SOPT1CFG |= SIM_SOPT1CFG_URWE_MASK | SIM_SOPT1CFG_UVSWE_MASK | SIM_SOPT1CFG_USSWE_MASK;
    SIM_SOPT1 &= ~(SIM_SOPT1_USBREGEN_MASK | 0x40000000u | 0x20000000u);

    /*
     *   Turn off clock gate of USB module.
     */
    SIM_SCGC4 &= ~SIM_SCGC4_USBOTG_MASK;   
#if 0
    wassert_reboot();
#endif
}

 /******************************************************************************
 *
 *   @name        TestApp_Init
 *
 *   @brief       This function is the entry for the Virtual COM Loopback app
 *
 *   @param       None
 *
 *   @return      None
 *****************************************************************************
 * This function starts the Virtual COM Loopback application
 *****************************************************************************/

void TestApp_Init(void)
{
    uint_8   error;

    g_recv_size = 0;
    g_send_size= 0;
    DisableInterrupts;
    #if (defined _MCF51MM256_H) || (defined _MCF51JE256_H)
     usb_int_dis();
    #endif
    /* Initialize the USB interface */
    error = USB_Class_CDC_Init(CONTROLLER_ID,USB_App_Callback,
                                NULL,USB_Notify_Callback, TRUE);
    if(error != USB_OK)
    {
        wassert_reboot();
    }
    EnableInterrupts;
	#if (defined _MCF51MM256_H) || (defined _MCF51JE256_H)
     usb_int_en();
    #endif
    memset(g_console_buf, 0, CONSOLE_BUF_SZ);
}


///////////////////////////////////////////////////////////////////////////////
// Sends data to the USB Host
///////////////////////////////////////////////////////////////////////////////
int virtual_com_send (uint8_t* send_data, uint8_t data_len)
{
  int status;
  uint32_t usb_retries = 10;

  do
  {
    status = USB_Class_CDC_Interface_DIC_Send_Data(CONTROLLER_ID, send_data, data_len);
    if (status == 0xC1)
    {
        wassert_reboot();
    }
  } while ((status == 0xC1) && (usb_retries-- != 0)); // 0xC1 is USBERR_DEVICE_BUSY

  return status;
}

/******************************************************************************
 *
 *   @name        TestApp_Task
 *
 *   @brief       Application task function. It is called from the main loop
 *
 *   @param       None
 *
 *   @return      None
 *
 *****************************************************************************
 * Application task function. It is called from the main loop
 *****************************************************************************/
void TestApp_Task(void)
{
#if (defined _MC9S08JE128_H) || (defined _MC9S08JM16_H) || (defined _MC9S08JM60_H) || (defined _MC9S08JS16_H) || (defined _MC9S08MM128_H)
	if(USB_PROCESS_PENDING())
	{
		USB_Engine();
	}
#endif
        /* call the periodic task function */
        USB_Class_CDC_Periodic_Task();

       /*check whether enumeration is complete or not */
        if((start_app==TRUE) && (start_transactions==TRUE))
        {
            Virtual_Com_App();
        }
}

/******************************************************************************
 *
 *    @name       Virtual_Com_App
 *
 *    @brief      Implements Loopback COM Port
 *
 *    @param      None
 *
 *    @return     None
 *
 *****************************************************************************
 * Receives data from USB Host and transmits back to the Host
 *****************************************************************************/
static void Virtual_Com_App(void)
{
    static uint_8 status = 0;
    static uint_8 count = 0;
    int x;
    uint_8 *prompt = "\n\rT] ";
	uint32_t usb_retries = 5000;
	
	if( g_cstate != BU_NUM_COLORS )
	{
	    bu26507_color(g_cstate);
	    g_cstate = BU_NUM_COLORS;   // set it back to the bogus state, so we don't keep setting colors...
	}

    /* Loopback Application Code */
    if(g_recv_size)
    {
    	/* Copy Received Buffer to Send Buffer */
    	for (x = 0; x < g_recv_size; x++)
    	{
    		g_curr_send_buf[x] = g_curr_recv_buf[x];

			if(count < CONSOLE_BUF_SZ)
			{
				g_console_buf[count] = g_curr_recv_buf[x];
			}
	    	else
	    	{
	    		// \todo Make sure no overflow occuring on the command line for this case.
	    		g_console_buf[CONSOLE_BUF_SZ - 1] = '\n';
	    		count = CONSOLE_BUF_SZ - 1;
	    	}

			if( (g_console_buf[count] == '\n') || (g_console_buf[count] == '\r') ) // newline was received.  Process the input
			{
				g_console_buf[count] = 0; // null terminate the str
				cc_process_cmd(&g_cc_handle, &(g_console_buf[0])); // send off the command + args to be processed.
				count = 0;
				
				status = virtual_com_send (prompt, strlen(prompt));
#if 0
				do
				{
					status = USB_Class_CDC_Interface_DIC_Send_Data(CONTROLLER_ID, prompt, strlen(prompt));
					sleep(20);
				} while( (status == 0xC1) && (usb_retries-- != 0) ); // 0xC1 is USBERR_DEVICE_BUSY
#endif
		        if(status != USB_OK)
		        {
		            wassert_reboot();
		        }
		        continue;
			}
		    count++;
    	}

    	g_send_size = g_recv_size;
    	g_recv_size = 0;
    }
    if(g_send_size)
    {
        /* Send Data to USB Host*/
        uint_8 size = g_send_size;
        g_send_size = 0;
        usb_retries = 1;

        do
        {
        	if( (g_curr_send_buf[0] != '\n') && (g_curr_send_buf[0] != '\r') ) // don't let the app print newlines, we want to manage that explicitly.
        	{
        	    status = virtual_com_send (g_curr_send_buf, size);
#if 0
        		status = USB_Class_CDC_Interface_DIC_Send_Data(CONTROLLER_ID, g_curr_send_buf, size);
#endif
        	} 
        	else 
        	{ 
        	    status = USB_OK; 
        	}
        }while( (status == 0xC1) && (usb_retries-- != 0) ); // 0xC1 is USBERR_DEVICE_BUSY
        if(status != USB_OK)
        {
            wassert_reboot();
        }
    }

    return;
}

/******************************************************************************
 *
 *    @name        USB_App_Callback
 *
 *    @brief       This function handles Class callback
 *
 *    @param       controller_ID    : Controller ID
 *    @param       event_type       : Value of the event
 *    @param       val              : gives the configuration value
 *
 *    @return      None
 *
 *****************************************************************************
 * This function is called from the class layer whenever reset occurs or enum
 * is complete. After the enum is complete this function sets a variable so
 * that the application can start.
 * This function also receives DATA Send and RECEIVED Events
 *****************************************************************************/

static void USB_App_Callback (
    uint_8 controller_ID,   /* [IN] Controller ID */
    uint_8 event_type,      /* [IN] value of the event */
    void* val               /* [IN] gives the configuration value */
)
{
    UNUSED (controller_ID)
    UNUSED (val)
    if(event_type == USB_APP_BUS_RESET)
    {
        start_app=FALSE;
    }
    else if(event_type == USB_APP_ENUM_COMPLETE)
    {
#if HIGH_SPEED_DEVICE
    	// prepare for the next receive event
    	USB_Class_CDC_Interface_DIC_Recv_Data(&controller_ID,
    	                                      (uint_8_ptr)g_cdcBuffer,
    	                                      DIC_BULK_OUT_ENDP_PACKET_SIZE);
#endif
        start_app=TRUE;
    }
    else if((event_type == USB_APP_DATA_RECEIVED)&&
            (start_transactions == TRUE))
    {
        /* Copy Received Data buffer to Application Buffer */
        USB_PACKET_SIZE BytesToBeCopied;
        APP_DATA_STRUCT* dp_rcv = (APP_DATA_STRUCT*)val;
        uint_8 index;
        BytesToBeCopied = (USB_PACKET_SIZE)((dp_rcv->data_size > DATA_BUFF_SIZE) ?
                                      DATA_BUFF_SIZE:dp_rcv->data_size);
        for(index = 0; index<BytesToBeCopied ; index++)
        {
            g_curr_recv_buf[index]= dp_rcv->data_ptr[index];
        }
        g_recv_size = index;
    }
    else if((event_type == USB_APP_SEND_COMPLETE) && (start_transactions == TRUE))
    {
        /* Previous Send is complete. Queue next receive */
#if HIGH_SPEED_DEVICE
    	(void)USB_Class_CDC_Interface_DIC_Recv_Data(CONTROLLER_ID, g_cdcBuffer, DIC_BULK_OUT_ENDP_PACKET_SIZE);
#else
        (void)USB_Class_CDC_Interface_DIC_Recv_Data(CONTROLLER_ID, NULL, 0);
#endif
    }

    return;
}

/******************************************************************************
 *
 *    @name        USB_Notify_Callback
 *
 *    @brief       This function handles PSTN Sub Class callbacks
 *
 *    @param       controller_ID    : Controller ID
 *    @param       event_type       : PSTN Event Type
 *    @param       val              : gives the configuration value
 *
 *    @return      None
 *
 *****************************************************************************
 * This function handles USB_APP_CDC_CARRIER_ACTIVATED and
 * USB_APP_CDC_CARRIER_DEACTIVATED PSTN Events
 *****************************************************************************/

static void USB_Notify_Callback (
    uint_8 controller_ID,   /* [IN] Controller ID */
    uint_8 event_type,      /* [IN] PSTN Event Type */
    void* val               /* [IN] gives the configuration value */
)
{
    UNUSED (controller_ID)
    UNUSED (val)
    if(start_app == TRUE)
    {
        if(event_type == USB_APP_CDC_CARRIER_ACTIVATED)
        {
            start_transactions = TRUE;
        }
        else if(event_type == USB_APP_CDC_CARRIER_DEACTIVATED)
        {
            //start_transactions = FALSE;
        }
    }
    return;
}

/* EOF */
