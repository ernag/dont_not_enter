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
* $FileName: msd_commands.c$
* $Version : 3.8.31.1$
* $Date    : Dec-7-2011$
*
* Comments:
*
*   This file contains device driver for mass storage class. This code tests
*   the UFI set of commands.
*
*END************************************************************************/

/**************************************************************************
Include the OS and BSP dependent files that define IO functions and
basic types. You may like to change these files for your board and RTOS 
**************************************************************************/
/**************************************************************************
Include the USB stack header files.
**************************************************************************/
#include "types.h"
#ifdef __USB_OTG__
#include "otgapi.h"
#include "devapi.h"
#else
#include "hostapi.h"
#endif

#include "mqx_host.h"
#include "bsp.h"
#include "fio.h"

volatile uint_32 _mqx_monitor_type = MQX_MONITOR_TYPE_BDM;
/**************************************************************************
Local header files for this application
**************************************************************************/
#include "msd_commands.h"

/**************************************************************************
Class driver files for this application
**************************************************************************/

#include "usb_host_msd_bo.h"
#include "usb_host_msd_ufi.h"
#include <usb_host_hub_sm.h>


#if ! BSPCFG_ENABLE_IO_SUBSYSTEM
#error This application requires BSPCFG_ENABLE_IO_SUBSYSTEM defined non-zero in user_config.h. Please recompile BSP with this option.
#endif


#ifndef BSP_DEFAULT_IO_CHANNEL_DEFINED
#error This application requires BSP_DEFAULT_IO_CHANNEL to be not NULL. Please set corresponding BSPCFG_ENABLE_TTYx to non-zero in user_config.h and recompile BSP with this option.
#endif


/**************************************************************************
A driver info table defines the devices that are supported and handled
by file system application. This table defines the PID, VID, class and
subclass of the USB device that this application listens to. If a device
that matches this table entry, USB stack will generate a attach callback.
As noted, this table defines a UFI class device and a USB
SCSI class device (e.g. high-speed hard disk) as supported devices.
see the declaration of structure USB_HOST_DRIVER_INFO for details
or consult the software architecture guide.
**************************************************************************/


static  USB_HOST_DRIVER_INFO DriverInfoTable[] =
{
   /* Floppy drive */
   {
      {0x00, 0x00},                 /* Vendor ID per USB-IF             */
      {0x00, 0x00},                 /* Product ID per manufacturer      */
      USB_CLASS_MASS_STORAGE,       /* Class code                       */
      USB_SUBCLASS_MASS_UFI,        /* Sub-Class code                   */
      USB_PROTOCOL_MASS_BULK,       /* Protocol                         */
      0,                            /* Reserved                         */
      usb_host_mass_device_event    /* Application call back function   */
   },

   /* USB 2.0 hard drive */
   {

      {0x00, 0x00},                 /* Vendor ID per USB-IF             */
      {0x00, 0x00},                 /* Product ID per manufacturer      */
      USB_CLASS_MASS_STORAGE,       /* Class code                       */
      USB_SUBCLASS_MASS_SCSI,       /* Sub-Class code                   */
      USB_PROTOCOL_MASS_BULK,       /* Protocol                         */
      0,                            /* Reserved                         */
      usb_host_mass_device_event    /* Application call back function   */
   },

   /* USB 1.1 hub */
   {

      {0x00, 0x00},                 /* Vendor ID per USB-IF             */
      {0x00, 0x00},                 /* Product ID per manufacturer      */
      USB_CLASS_HUB,                /* Class code                       */
      USB_SUBCLASS_HUB_NONE,        /* Sub-Class code                   */
      USB_PROTOCOL_HUB_LS,          /* Protocol                         */
      0,                            /* Reserved                         */
      usb_host_hub_device_event     /* Application call back function   */
   },

   {
      {0x00,0x00},                  /* All-zero entry terminates        */
      {0x00,0x00},                  /*    driver info list.             */
      0,
      0,
      0,
      0,
      NULL
   }

};

/**************************************************************************
   Global variables
**************************************************************************/
volatile DEVICE_STRUCT       mass_device = { 0 };   /* mass storage device struct */
volatile boolean bCallBack = FALSE;
volatile USB_STATUS bStatus       = USB_OK;
volatile uint_32  dBuffer_length = 0;
boolean  Attach_Notification = FALSE;
_usb_host_handle        host_handle;         /* global handle for calling host   */


/* the following is the mass storage class driver object structure. This is
used to send commands down to  the class driver. See the Class API document
for details */

COMMAND_OBJECT_PTR pCmd;

/*some handles for communicating with USB stack */
_usb_host_handle     host_handle; /* host controller status etc. */

static uchar_ptr buff_out, buff_in;

/**************************************************************************
The following is the way to define a multi tasking system in MQX RTOS.
Remove this code and use your own RTOS way of defining tasks (or threads).
**************************************************************************/

#define MAIN_TASK          (10)
extern void Main_Task(uint_32 param);
TASK_TEMPLATE_STRUCT  MQX_template_list[] =
{
   { MAIN_TASK,      Main_Task,      3000L,  9L, "Main",      MQX_AUTO_START_TASK},
   { 0L,             0L,             0L,     0L, 0L,          0L }
};

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : main
* Returned Value : none
* Comments       :
*     Execution starts here
*
*END*--------------------------------------------------------------------*/

void Main_Task ( uint_32 param )
{ /* Body */
 
   USB_STATUS           status = USB_OK;

   _int_disable();

   _int_install_unexpected_isr();
   status = _usb_host_driver_install(HOST_CONTROLLER_NUMBER, (pointer)&_bsp_usb_host_callback_table);
   if (status) {
      _int_enable();
      printf("ERROR: %ls", status);
      exit(1);
   } /* Endif */

   status = _usb_host_init
      (HOST_CONTROLLER_NUMBER,    /* Use value in header file */
       MAX_FRAME_SIZE,            /* Frame size per USB spec  */
       &host_handle);             /* Returned pointer */

   if (status != USB_OK) {
      _int_enable();
      printf("\nUSB Host Initialization failed. STATUS: %x", status);
      fflush(stdout);
      goto Error_Cleanup;
   } /* Endif */

   /*
   ** Since we are going to act as the host driver, register
   ** the driver information for wanted class/subclass/protocols
   */
   status = _usb_host_driver_info_register(host_handle, DriverInfoTable);
   if (status != USB_OK) {
      _int_enable();
      printf("\nDriver Registration failed. STATUS: %x", status);
      fflush(stdout);
      goto Error_Cleanup;
   }

   _int_enable();

   pCmd = (COMMAND_OBJECT_PTR) USB_mem_alloc_zero(sizeof(COMMAND_OBJECT_STRUCT));
   
   if (pCmd == NULL)
   {
      printf ("\nUnable to allocate Command Object");
      fflush (stdout);
      goto Error_Cleanup;
   }

   pCmd->CBW_PTR = (CBW_STRUCT_PTR) USB_mem_alloc_zero(sizeof(CBW_STRUCT));
   
   if (pCmd->CBW_PTR == NULL)
   {
      printf ("\nUnable to allocate Command Block Wrapper");
      fflush (stdout);
      goto Error_Cleanup;
   }

   pCmd->CSW_PTR = (CSW_STRUCT_PTR) USB_mem_alloc_zero(sizeof(CSW_STRUCT));
   
   if (pCmd->CSW_PTR == NULL)
   {
      printf ("\nUnable to allocate Command Status Wrapper");
      fflush (stdout);
      goto Error_Cleanup;
   }

   buff_in = (uchar_ptr)USB_mem_alloc_uncached(0x400C);
   
   if (buff_in == NULL)
   {
      printf ("\nUnable to allocate Input Buffer");
      fflush (stdout);
      goto Error_Cleanup;
   }
   _mem_zero(buff_in, 0x400C);

   buff_out = (uchar_ptr) USB_mem_alloc_uncached(0x0F);
   
   if (buff_out == NULL)
   {
      printf ("\nUnable to allocate Output Buffer");
      fflush (stdout);
      goto Error_Cleanup;
   }
   _mem_zero(buff_out, 0x0F);

   printf("\nPlease insert Mass Storage Device.\n");
   /*----------------------------------------------------**
   ** Infinite loop, waiting for events requiring action **
   **----------------------------------------------------*/
   for ( ; ; ) {
      switch ( mass_device.dev_state ) {
         case USB_DEVICE_IDLE:
            break ;
         case USB_DEVICE_ATTACHED:
            printf( "Mass Storage Device Attached\n" );
            fflush(stdout);
            mass_device.dev_state = USB_DEVICE_SET_INTERFACE_STARTED;
            status = _usb_hostdev_select_interface(mass_device.dev_handle,
                 mass_device.intf_handle, (pointer)&mass_device.class_intf);
            break ;
         case USB_DEVICE_SET_INTERFACE_STARTED:
            break;
         case USB_DEVICE_INTERFACED:
            usb_host_mass_test_storage();
            mass_device.dev_state = USB_DEVICE_OTHER;
            break ;
         case USB_DEVICE_DETACHED:
            printf ( "\nMass Storage Device Detached\n" );
            fflush(stdout);
            mass_device.dev_state = USB_DEVICE_IDLE;
            break;
         case USB_DEVICE_OTHER:
            break ;
         default:
            printf ( "Unknown Mass Storage Device State = %d\n",\
               mass_device.dev_state );
            fflush(stdout);
            break ;
      } /* Endswitch */
   } /* Endfor */
   
   Error_Cleanup:
   {
      if (pCmd != NULL)
      {
         if (pCmd->CSW_PTR != NULL)
         {
            USB_mem_free(pCmd->CSW_PTR);
         }
         
         if (pCmd->CBW_PTR != NULL)
         {
            USB_mem_free(pCmd->CBW_PTR);
         }
         
         USB_mem_free(pCmd);
      }
      
      if (buff_in != NULL)
      {
         USB_mem_free(buff_in);
      }
      
      if (buff_out != NULL)
      {
         USB_mem_free(buff_out);
      }
      
      if (host_handle != NULL)
      {
         _usb_host_shutdown (host_handle);
      }
   }

} /* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_host_mass_device_event
* Returned Value : None
* Comments       :
*     called when mass storage device has been attached, detached, etc.
*END*--------------------------------------------------------------------*/

static void usb_host_mass_device_event
   (
      /* [IN] pointer to device instance */
      _usb_device_instance_handle      dev_handle,

      /* [IN] pointer to interface descriptor */
      _usb_interface_descriptor_handle intf_handle,

      /* [IN] code number for event causing callback */
      uint_32           event_code
   )
{ /* Body */
   INTERFACE_DESCRIPTOR_PTR   intf_ptr =
      (INTERFACE_DESCRIPTOR_PTR)intf_handle;

   fflush(stdout);
   switch (event_code) {
      case USB_CONFIG_EVENT:
         /* Drop through into attach, same processing */
      case USB_ATTACH_EVENT:
         if (mass_device.dev_state == USB_DEVICE_IDLE) {
            mass_device.dev_handle = dev_handle;
            mass_device.intf_handle = intf_handle;
            mass_device.dev_state = USB_DEVICE_ATTACHED;
         } else {
            printf("Mass Storage Device Is Already Attached\n");
            fflush(stdout);
         } /* EndIf */
         break;
      case USB_INTF_EVENT:
         mass_device.dev_state = USB_DEVICE_INTERFACED;
         break;
      case USB_DETACH_EVENT:
         if (mass_device.dev_state != USB_DEVICE_IDLE) {
            mass_device.dev_handle = NULL;
            mass_device.intf_handle = NULL;
            mass_device.dev_state = USB_DEVICE_DETACHED;
         } else {
            printf("Mass Storage Device Is Not Attached\n");
            fflush(stdout);
         } /* EndIf */
         break;
      default:
         mass_device.dev_state = USB_DEVICE_IDLE;
         break;
   } /* EndSwitch */
} /* Endbody */


/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_host_mass_ctrl_callback
* Returned Value : None
* Comments       :
*     Called on completion of a control-pipe transaction.
*
*END*--------------------------------------------------------------------*/

static void usb_host_mass_ctrl_callback
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

   bCallBack = TRUE;
   bStatus = status;

} /* Endbody */


/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_host_mass_bulk_callback
* Returned Value : None
* Comments       :
*     Called on completion of a bulk transaction.
*
*END*--------------------------------------------------------------------*/

void usb_host_mass_bulk_callback
   (
      /* [IN] Status of this command */
      USB_STATUS status,

      /* [IN] pointer to USB_MASS_BULK_ONLY_REQUEST_STRUCT*/
      pointer p1,

      /* [IN] pointer to the command object*/
      pointer p2,

      /* [IN] Length of data transmitted */
      uint_32 buffer_length
   )
{ /* Body */

   dBuffer_length = buffer_length;
   bCallBack = TRUE;
   bStatus = status;

} /* Endbody */



/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_host_mass_test_storage
* Returned Value : None
* Comments       :
*     Calls the UFI command set for testing
*END*--------------------------------------------------------------------*/

static void usb_host_mass_test_storage
   (
      void
   )
{ /* Body */
   USB_STATUS                                 status = USB_OK;
   uint_8                                     bLun = 0;
   INQUIRY_DATA_FORMAT                        inquiry;
   CAPACITY_LIST                              capacity_list;
   #if 0
   MODE_SELECT_PARAMETER_LIST                 md_list;
   #endif
   MASS_STORAGE_READ_CAPACITY_CMD_STRUCT_INFO read_capacity;
   REQ_SENSE_DATA_FORMAT                      req_sense;
   FORMAT_UNIT_PARAMETER_BLOCK                formatunit = { 0};
   _mqx_int                                   block_len;
   DEVICE_DESCRIPTOR_PTR                      pdd;
   CBW_STRUCT_PTR cbw_ptr = pCmd->CBW_PTR;
   CSW_STRUCT_PTR csw_ptr = pCmd->CSW_PTR;

   USB_mem_zero(buff_out,0x0F);
   USB_mem_zero(buff_in,0x400C);
   USB_mem_zero(pCmd->CSW_PTR,sizeof(CSW_STRUCT));
   USB_mem_zero(pCmd->CBW_PTR,sizeof(CBW_STRUCT));
   USB_mem_zero(pCmd,sizeof(COMMAND_OBJECT_STRUCT));
   
   pCmd->CBW_PTR  = cbw_ptr;
   pCmd->CSW_PTR  = csw_ptr;
   pCmd->LUN      = bLun;
   pCmd->CALL_PTR = (pointer)&mass_device.class_intf;
   pCmd->CALLBACK = usb_host_mass_bulk_callback;

   printf("\n =============START OF A NEW SESSION==================");

   if (USB_OK != _usb_hostdev_get_descriptor(
      mass_device.dev_handle,
      NULL,
      USB_DESC_TYPE_DEV,
      0,
      0,
      (pointer *) &pdd))
   {
      printf ("\nCould not retrieve device descriptor.");
      return;
   }
   else
      printf("\nVID = 0x%04x, PID = 0x%04x", SHORT_LE_TO_HOST(*(uint_16*)pdd->idVendor), SHORT_LE_TO_HOST(*(uint_16*)pdd->idProduct));

   /* Test the GET MAX LUN command */
   printf("\nTesting: GET MAX LUN Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_class_mass_getmaxlun_bulkonly(
      (pointer)&mass_device.class_intf, &bLun,
      usb_host_mass_ctrl_callback);

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack) {;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }

   /* Test the TEST UNIT READY command */
   printf("Testing: TEST UNIT READY Command");
   fflush(stdout);

   bCallBack = FALSE;

   status =  usb_mass_ufi_test_unit_ready(pCmd);
   
   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }

   /* Test the REQUEST SENSE command */
   printf("Testing: REQUEST SENSE Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_request_sense(pCmd, &req_sense,
      sizeof(REQ_SENSE_DATA_FORMAT));

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }

   /* Test the INQUIRY command */
   printf("Testing: INQUIRY Command");
   fflush(stdout);

   bCallBack = FALSE;

   status = usb_mass_ufi_inquiry(pCmd, (uchar_ptr) &inquiry,
      sizeof(INQUIRY_DATA_FORMAT));

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the REQUEST SENSE command */
   printf("Testing: REQUEST SENSE Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_request_sense(pCmd, &req_sense,
      sizeof(REQ_SENSE_DATA_FORMAT));

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the READ FORMAT CAPACITY command */
   printf("Testing: READ FORMAT CAPACITIES Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_format_capacity(pCmd, (uchar_ptr)&capacity_list,
      sizeof(CAPACITY_LIST));

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the REQUEST SENSE command */
   printf("Testing: REQUEST SENSE Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_request_sense(pCmd, &req_sense,
      sizeof(REQ_SENSE_DATA_FORMAT));

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the READ CAPACITY command */
   printf("Testing: READ CAPACITY Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_read_capacity(pCmd, (uchar_ptr)&read_capacity,
      sizeof(MASS_STORAGE_READ_CAPACITY_CMD_STRUCT_INFO));

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   block_len = HOST_TO_BE_LONG(* (uint_32 *) &read_capacity.BLENGTH);

#if 0
   /* Test the MODE SELECT command */
   printf("Testing: MODE SELECT Command");
   fflush(stdout);
   bCallBack = FALSE;

   //md_list. Fill parameters here
   USB_mem_zero (&md_list, sizeof (MODE_SELECT_PARAMETER_LIST));
   md_list.MODE_PARAM_HEADER.BLENGTH[1] = 0x06;
   
   status = usb_mass_ufi_mode_select(pCmd, &md_list, 8);

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
#endif

   /* Test the REQUEST SENSE command */
   printf("Testing: REQUEST SENSE Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_request_sense(pCmd, &req_sense,
      sizeof(REQ_SENSE_DATA_FORMAT));

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the READ(10) command */
   printf("Testing: READ(10) Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_read_10(pCmd, 0, buff_in, block_len, 1);

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the MODE SENSE command */
   printf("Testing: MODE SENSE Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_mode_sense(pCmd,
      2, //PC
      0x3F, //page code
      buff_in,
      (uint_32)0x08);

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the PREVENT ALLOW command */
   printf("Testing: PREVENT-ALLOW MEDIUM REMOVAL Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_prevent_allow_medium_removal(
      pCmd,
      1 // prevent
      );

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the REQUEST SENSE command */
   printf("Testing: REQUEST SENSE Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_request_sense(pCmd, &req_sense,
      sizeof(REQ_SENSE_DATA_FORMAT));

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the VERIFY command */
   printf("Testing: VERIFY Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_verify(
      pCmd,
      0x400, // block address
      1 //length to be verified
      );

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }
   
   /* Test the WRITE(10) command */
   printf("Testing: WRITE(10) Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_write_10(pCmd, 8, buff_out, block_len, 1);

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }

   /* Test the REQUEST SENSE command */
   printf("Testing: REQUEST SENSE Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_request_sense(pCmd, &req_sense,
      sizeof(REQ_SENSE_DATA_FORMAT));

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }

   /* Test the START-STOP UNIT command */
   printf("Testing: START-STOP UNIT Command");
   fflush(stdout);
   bCallBack = FALSE;

   status = usb_mass_ufi_start_stop(pCmd, 0, 1);

   if ((status != USB_OK) && (status != USB_STATUS_TRANSFER_QUEUED))
   {
      printf ("\n...ERROR");
      return;
   }
   else
   {
      /* Wait till command comes back */
      while (!bCallBack){;}
      if (!bStatus) {
         printf("...OK\n");
      }
      else {
         printf("...Unsupported by device (bStatus=0x%x)\n", bStatus);
      }
   }

   printf("\nTest done!");
   fflush(stdout);
} /* Endbody */


/* EOF */
