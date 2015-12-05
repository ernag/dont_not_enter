/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
* Copyright (c) 1989-2008 ARC International;
* All Rights Reserved
*
*************************************************************************** 
*
* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
* THE POSSIBILITY OF SUCH DAMAGE.
*
**************************************************************************
*
* $FileName: hidmouse.c$
* $Version : 3.6.29.0$
* $Date    : Aug-2-2010$
*
* Comments:
*
*   This file is an example of device drivers for a Mouse device.
*   It has been tested in a Dell and logitech USB 3 button wheel Mouse. The program
*   queues transfers on Interrupt USB pipe and waits till the data comes
*   back. It prints the data and queues another transfer on the same pipe.
*   See the code for details.
*
*END************************************************************************/
#include "mqx_host.h"
#include <usb_host_hub_sm.h>
#include "hidmouse.h"
#include "usb_host_hid.h"
#include <lwevent.h>


#if ! BSPCFG_ENABLE_IO_SUBSYSTEM
#error This application requires BSPCFG_ENABLE_IO_SUBSYSTEM defined non-zero in user_config.h. Please recompile BSP with this option.
#endif


#ifndef BSP_DEFAULT_IO_CHANNEL_DEFINED
#error This application requires BSP_DEFAULT_IO_CHANNEL to be not NULL. Please set corresponding BSPCFG_ENABLE_TTYx to non-zero in user_config.h and recompile BSP with this option.
#endif


/************************************************************************************
**
** Globals
************************************************************************************/
LWEVENT_STRUCT USB_Event;

#define USB_EVENT_CTRL 0x01
#define USB_EVENT_DATA 0x02

#define MAIN_TASK          (10)

/************************************************************************************
Table of driver capabilities this application wants to use. See Host API document for
details on How to define a driver info table. This table defines that this driver
supports a HID class, boot subclass and mouse protocol. 
************************************************************************************/
static  USB_HOST_DRIVER_INFO DriverInfoTable[] =
{
   {
      {0x00,0x00},                  /* Vendor ID per USB-IF             */
      {0x00,0x00},                  /* Product ID per manufacturer      */
      USB_CLASS_HID,                /* Class code                       */
      USB_SUBCLASS_HID_BOOT,        /* Sub-Class code                   */
      USB_PROTOCOL_HID_MOUSE,       /* Protocol                         */
      0,                            /* Reserved                         */
      usb_host_hid_mouse_event      /* Application call back function   */
   },
   /* USB 1.1 hub */
   {

      {0x00,0x00},                  /* Vendor ID per USB-IF             */
      {0x00,0x00},                  /* Product ID per manufacturer      */
      USB_CLASS_HUB,                /* Class code                       */
      USB_SUBCLASS_HUB_NONE,        /* Sub-Class code                   */
      USB_PROTOCOL_HUB_LS,          /* Protocol                         */
      0,                            /* Reserved                         */
      usb_host_hub_device_event     /* Application call back function   */
   },
   {
      {0x00,0x00},                  /* All-zero entry terminates        */
      {0x00,0x00},                  /* driver info list.                */
      0,
      0,
      0,
      0,
      NULL
   }
};

volatile DEVICE_STRUCT  hid_device = { 0 };  /* structure defined by this driver */

_usb_host_handle        host_handle;         /* global handle for calling host   */


extern void Main_Task(uint_32 param);
void process_mouse_buffer(uchar_ptr buffer);

TASK_TEMPLATE_STRUCT  MQX_template_list[] =
{
   { MAIN_TASK,      Main_Task,      3000L,  9L, "Main",      MQX_AUTO_START_TASK},
   { 0L,             0L,             0L,     0L, 0L,          0L }
};

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : main (Main_Task if using MQX)
* Returned Value : none
* Comments       :
*     Execution starts here
*
*END*--------------------------------------------------------------------*/

void Main_Task ( uint_32 param )
{
    USB_STATUS              status = USB_OK;
    _usb_pipe_handle        pipe;
    TR_INIT_PARAM_STRUCT    tr;
    HID_COMMAND_PTR         hid_com;
    uchar_ptr               buffer;
   
    /***************************************************************************
    ** Allocate memory for being able to send commands to HID class driver. Note 
    ** that USB_mem_alloc_uncached() allocates aligned buffers on cache line boundry.
        ** This is required if program is running with data cache on system.
    ***************************************************************************/
    hid_com = (HID_COMMAND_PTR) USB_mem_alloc_uncached(sizeof(HID_COMMAND));

    _int_disable();

    /* event for USB callback signaling */
    _lwevent_create(&USB_Event, LWEVENT_AUTO_CLEAR);

    _int_install_unexpected_isr();
	_usb_host_driver_install(HOST_CONTROLLER_NUMBER, (pointer)&_bsp_usb_host_callback_table);

    status = _usb_host_init(HOST_CONTROLLER_NUMBER, /* Use value in header file */
        MAX_FRAME_SIZE,            /* Frame size per USB spec  */
        &host_handle);             /* Returned pointer */

    if (status != USB_OK) {
        printf("\nUSB Host Initialization failed. STATUS: %x", status);
        fflush(stdout);
        exit(1);
    } /* Endif */

    /*
    ** since we are going to act as the host driver, register the driver
    ** information for wanted class/subclass/protocols
    */
    status = _usb_host_driver_info_register(host_handle, DriverInfoTable);
    if (status != USB_OK) {
          printf("\nDriver Registration failed. STATUS: %x", status);
          fflush(stdout);
          exit(1);
    }

    /* Allocate buffer to receive data from interrupt. It must be created on uncached heap,
    ** since some USB host controllers use DMA to access those buffers.
    */
    buffer = USB_mem_alloc_uncached(HID_BUFFER_SIZE);
    if (!buffer) {
        printf("\nError allocating buffer!");
        fflush(stdout);
        exit(1);
    } /* Endif */

    _int_enable();

    printf("\nMQX USB HID Mouse Demo\nWaiting for USB Mouse to be attached...\n");
    fflush(stdout);
   
    /*
    ** Infinite loop, waiting for events requiring action
    */
    for ( ; ; ) {
    
        // Wait for insertion or removal event
        _lwevent_wait_ticks(&USB_Event, USB_EVENT_CTRL, FALSE, 0);
        
        switch ( hid_device.DEV_STATE ) {
            case USB_DEVICE_IDLE:
                break;
            case USB_DEVICE_ATTACHED:
                printf("\nMouse device attached");
                fflush(stdout);
                hid_device.DEV_STATE = USB_DEVICE_SET_INTERFACE_STARTED;
                status = _usb_hostdev_select_interface(hid_device.DEV_HANDLE, hid_device.INTF_HANDLE, (pointer)&hid_device.CLASS_INTF);
                if (status != USB_OK) {
                    printf("\nError in _usb_hostdev_select_interface: %x", status);
                    fflush(stdout);
                    exit(1);
                } /* Endif */
                break;
            case USB_DEVICE_SET_INTERFACE_STARTED:
                break;
            case USB_DEVICE_INTERFACED:
                printf("Mouse interfaced, setting protocol...\n");
                /* now we will set the USB Hid standard boot protocol */
                hid_device.DEV_STATE = USB_DEVICE_SETTING_PROTOCOL;
            
                hid_com->CLASS_PTR = (CLASS_CALL_STRUCT_PTR)&hid_device.CLASS_INTF;
                hid_com->CALLBACK_FN = usb_host_hid_ctrl_callback;
                hid_com->CALLBACK_PARAM = 0;
            
                status = usb_class_hid_set_protocol(hid_com, USB_PROTOCOL_HID_MOUSE);
         
                if (status != USB_STATUS_TRANSFER_QUEUED) {
                      printf("\nError in usb_class_hid_set_protocol: %x", status);
                      fflush(stdout);
                }
                break;
            case USB_DEVICE_INUSE:
                pipe = _usb_hostdev_find_pipe_handle(hid_device.DEV_HANDLE, hid_device.INTF_HANDLE, USB_INTERRUPT_PIPE, USB_RECV);
                if(pipe){
                    printf("Mouse device ready, try to move the mouse\n");
                
                    while(1){
                        /******************************************************************
                        Initiate a transfer request on the interrupt pipe
                        ******************************************************************/
                        usb_hostdev_tr_init(&tr, usb_host_hid_recv_callback, NULL);
                        tr.RX_BUFFER = buffer;
                        tr.RX_LENGTH = HID_BUFFER_SIZE;                     
                        
                        status = _usb_host_recv_data(host_handle, pipe, &tr);
                        
                        if (status != USB_STATUS_TRANSFER_QUEUED) {
                            printf("\nError in _usb_host_recv_data: %x", status);
                            fflush(stdout);
                        }
                    
                        /* Wait untill we get the data from keyboard. */
                        _lwevent_wait_ticks(&USB_Event, USB_EVENT_CTRL | USB_EVENT_DATA, FALSE, 0);
                        
                        /* if not detached in the meanwhile */
                        if(hid_device.DEV_STATE == USB_DEVICE_INUSE) {
                            process_mouse_buffer((uchar *)buffer);
                        }
                        else {
                            /* kick the outer loop again to handle the CTRL event */
                            _lwevent_set(&USB_Event, USB_EVENT_CTRL);
                            break;
                        }
                    }
                }
                break;
            case USB_DEVICE_DETACHED:
                printf("Going to idle state\n");
                hid_device.DEV_STATE = USB_DEVICE_IDLE;
                break;
            case USB_DEVICE_OTHER:
                break;
             default:
                printf("Unknown Mouse Device State = %d\n", hid_device.DEV_STATE);
                fflush(stdout);
                break;
        } /* Endswitch */
    } /* Endfor */
} /* Endbody */


/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_host_hid_mouse_event
* Returned Value : None
* Comments       :
*     Called when HID device has been attached, detached, etc.
*END*--------------------------------------------------------------------*/

void usb_host_hid_mouse_event
   (
      /* [IN] pointer to device instance */
      _usb_device_instance_handle      dev_handle,

      /* [IN] pointer to interface descriptor */
      _usb_interface_descriptor_handle intf_handle,

      /* [IN] code number for event causing callback */
      uint_32                          event_code
   )
{ /* Body */

   INTERFACE_DESCRIPTOR_PTR intf_ptr = (INTERFACE_DESCRIPTOR_PTR)intf_handle;
      
   switch (event_code) {
      case USB_ATTACH_EVENT:
            printf("----- Attach Event -----\n");
            /* Drop through into attach, same processing */
      case USB_CONFIG_EVENT:           
            printf("State = %d", hid_device.DEV_STATE);
            printf("  Class = %d", intf_ptr->bInterfaceClass);
            printf("  SubClass = %d", intf_ptr->bInterfaceSubClass);
            printf("  Protocol = %d\n", intf_ptr->bInterfaceProtocol);
            fflush(stdout);

         if (hid_device.DEV_STATE == USB_DEVICE_IDLE) {
            hid_device.DEV_HANDLE = dev_handle;
            hid_device.INTF_HANDLE = intf_handle;
            hid_device.DEV_STATE = USB_DEVICE_ATTACHED;
         } else {
            printf("HID device already attached - DEV_STATE = %d\n", hid_device.DEV_STATE);
            fflush(stdout);
         } /* Endif */
         
         break;
      case USB_INTF_EVENT:
         printf("----- Interfaced Event -----\n");
         hid_device.DEV_STATE = USB_DEVICE_INTERFACED;
         break ;
      case USB_DETACH_EVENT:
         /* Use only the interface with desired protocol */
         printf("----- Detach Event -----\n");
         printf("State = %d", hid_device.DEV_STATE);
         printf("  Class = %d", intf_ptr->bInterfaceClass);
         printf("  SubClass = %d", intf_ptr->bInterfaceSubClass);
         printf("  Protocol = %d\n", intf_ptr->bInterfaceProtocol);
         fflush(stdout);
         
         hid_device.DEV_HANDLE = NULL;
         hid_device.INTF_HANDLE = NULL;
         hid_device.DEV_STATE = USB_DEVICE_DETACHED;
         break;

      default:
         printf("HID Device state = %d??\n", hid_device.DEV_STATE);
         fflush(stdout);
         hid_device.DEV_STATE = USB_DEVICE_IDLE;
         break;
   } /* EndSwitch */

    /* notify application that status has changed */
    _lwevent_set(&USB_Event, USB_EVENT_CTRL);
   
} /* Endbody */


/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_host_hid_ctrl_callback
* Returned Value : None
* Comments       :
*     Called when a command is completed
*END*--------------------------------------------------------------------*/

void usb_host_hid_ctrl_callback
   (
      /* [IN] pointer to pipe */
      _usb_pipe_handle  pipe_handle,

      /* [IN] user-defined parameter */
      pointer           user_parm,

      /* [IN] buffer address */
      uchar_ptr         buffer,

      /* [IN] length of data transferred */
      uint_32           buflen,

      /* [IN] status, hopefully USB_OK or USB_DONE */
      uint_32           status
   )
{ /* Body */
   if (status == USBERR_ENDPOINT_STALLED) {
      printf("\nHID Set_Protocol Request BOOT is not supported!\n");
      fflush(stdout);
   }
   else if (status) {
      printf("\nHID Set_Protocol Request BOOT failed!: 0x%x ... END!\n", status);
      fflush(stdout);
      exit(1);
   } /* Endif */

    if(hid_device.DEV_STATE == USB_DEVICE_SETTING_PROTOCOL)
        hid_device.DEV_STATE = USB_DEVICE_INUSE;
    
    /* notify application that status has changed */
    _lwevent_set(&USB_Event, USB_EVENT_CTRL);
   
} /* Endbody */


/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_host_hid_recv_callback
* Returned Value : None
* Comments       :
*     Called when data is received on a pipe
*END*--------------------------------------------------------------------*/

void usb_host_hid_recv_callback
   (
      /* [IN] pointer to pipe */
      _usb_pipe_handle  pipe_handle,

      /* [IN] user-defined parameter */
      pointer           user_parm,

      /* [IN] buffer address */
      uchar_ptr         buffer,

      /* [IN] length of data transferred */
      uint_32           buflen,

      /* [IN] status, hopefully USB_OK or USB_DONE */
      uint_32           status
   )
{ /* Body */

   /* notify application that data are available */
    _lwevent_set(&USB_Event, USB_EVENT_DATA);

}

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : process_mouse_buffer
* Returned Value : None
* Comments       :
*   Process the data from mouse buffer
*END*--------------------------------------------------------------------*/
void process_mouse_buffer(uchar_ptr buffer)
{
    
    if (buffer[0] & 0x01) 
        printf("Left Click ");
    else    
        printf("           ");
    if (buffer[0] & 0x02) 
        printf("Right Click ");
    else 
        printf("            ");
    if (buffer[0] & 0x04) 
        printf("Middle Click ");
    else 
        printf("             ");

    if(buffer[1]){
        if(buffer[1] > 127) 
            printf("Left  ");
        else 
            printf("Right ");
    }
    else { 
        printf("      ");
    }

    if(buffer[2]){
        if(buffer[2] > 127) 
            printf("UP   ");
        else 
            printf("Down ");
    }
    else { 
        printf("     ");
    }

    if(buffer[3]){
        if(buffer[3] > 127) 
            printf("Wheel Down");
        else 
            printf("Wheel UP  ");
    }
    else { 
        printf("          ");
    }

    printf("\n");
    fflush(stdout); 
    
}

/* EOF */

