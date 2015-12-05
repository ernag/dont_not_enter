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
#include <stdio.h>
#include "boot2_mgr.h"

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
extern uint8_t g_sherlock_in_progress;
void usb_reset(void);

/****************************************************************************
 * Global Variables
 ****************************************************************************/
uint32_t usb_send_err = 0;
/*****************************************************************************
 * Local Types - None
 *****************************************************************************/

/*****************************************************************************
 * Local Functions Prototypes
 *****************************************************************************/
static void USB_App_Callback(uint8_t controller_ID,
                        uint8_t event_type, void* val);
static void USB_Notify_Callback(uint8_t controller_ID,
                        uint8_t event_type, void* val);
static void virtual_com_recv(void);
/*****************************************************************************
 * Local Variables
 *****************************************************************************/
/* Virtual COM Application start Init Flag */
static volatile boolean start_app = FALSE;
/* Virtual COM Application Carrier Activate Flag */
//static volatile boolean start_transactions = FALSE;
static volatile boolean start_transactions = TRUE;
/* Receive Buffer */
static uint8_t g_curr_recv_buf[USB_BUF_SIZE];
/* Send Buffer */
static uint8_t g_curr_send_buf[USB_BUF_SIZE];
/* Receive Data Size */
static uint8_t g_recv_size;

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
 *   @name        usb_app_init
 *
 *   @brief       This function is the entry for the Virtual COM app
 *
 *   @param       None
 *
 *   @return      None
 *****************************************************************************
 * This function starts the USB Virtual COM application
 *****************************************************************************/

void usb_app_init(void)
{
    uint8_t   status;

    usb_send_err = 0;
    g_recv_size = 0;
    DisableInterrupts;		

    /* Initialize the USB interface */
    status = USB_Class_CDC_Init(CONTROLLER_ID,USB_App_Callback,
                                NULL,USB_Notify_Callback, TRUE);
    if(status != USB_OK)
    {
        wassert_reboot();
    }
    EnableInterrupts;
}

/******************************************************************************
 *
 *   @name        usb_task
 *
 *   @brief       Application task function. It is called from the main loop
 *
 *   @param       None
 *
 *   @return      None
 *
 *****************************************************************************
 * USB Application task function. It is called from the main loop
 *****************************************************************************/
void usb_task(void)
{
    
    if( 0 == g_sherlock_in_progress )   // cannot rx USB data while we are dumping out log data.
    {
        /* call the periodic task function */
        USB_Class_CDC_Periodic_Task();
        
        /*check whether enumeration is complete or not */
        if((start_app==TRUE) && (start_transactions==TRUE))
        {
            virtual_com_recv();
        }
    }
}

void delay (int dcount)
{
  volatile uint32_t dummy = 0xDEADBEEF;
  int i;
  for (i = 0; i < dcount; i++)
  {
    dummy += 0xBEEFBEE5;
  }
}

/******************************************************************************
 *
 *    @name       virtual_com_recv
 *
 *    @brief      Handles received data from USB Host
 *
 *    @param      None
 *
 *    @return     None
 *
 *****************************************************************************
 * Handles received data from USB Host
 *****************************************************************************/

static void virtual_com_recv(void)
{
  /* Check if received usb data */
  if(g_recv_size)
  {
    b2_recv_data (g_curr_recv_buf, g_recv_size);
    // virtual_com_send (g_curr_recv_buf, g_recv_size);
    g_recv_size = 0;
  }
  return;
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
    if ( status == 0xC1 /*USBERR_DEVICE_BUSY*/ )
    {
        wassert_reboot();
    }
  } while ((status == 0xC1) && (usb_retries-- != 0)); // 0xC1 is USBERR_DEVICE_BUSY

  return status;
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
    uint8_t controller_ID,   /* [IN] Controller ID */
    uint8_t event_type,      /* [IN] value of the event */
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
        start_app=TRUE;
    }
    else if((event_type == USB_APP_DATA_RECEIVED)&&
            (start_transactions == TRUE))
    {
        /* Copy Received Data buffer to Application Buffer */
        USB_PACKET_SIZE BytesToBeCopied;
        APP_DATA_STRUCT* dp_rcv = (APP_DATA_STRUCT*)val;
        uint8_t index;
        BytesToBeCopied = (USB_PACKET_SIZE)((dp_rcv->data_size > USB_BUF_SIZE) ?
                                                                 USB_BUF_SIZE:dp_rcv->data_size);
        for(index = 0; index<BytesToBeCopied ; index++)
        {
            g_curr_recv_buf[index]= dp_rcv->data_ptr[index];
        }
        g_recv_size = index;
    }
    else if((event_type == USB_APP_SEND_COMPLETE) && (start_transactions == TRUE))
    {
        /* Previous Send is complete. Queue next receive */
        (void)USB_Class_CDC_Interface_DIC_Recv_Data(CONTROLLER_ID, NULL, 0);
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
    uint8_t controller_ID,   /* [IN] Controller ID */
    uint8_t event_type,      /* [IN] PSTN Event Type */
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
