#ifndef __mqx_dev_h__
#define __mqx_dev_h__
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
* $FileName: mqx_dev.h$
* $Version : 3.8.11.0$
* $Date    : Aug-1-2011$
*
* Comments:
*
* Description:      
*  This file contains device-specifc defines for MQX RTOS.
*                                                               
*END*********************************************************************/

#include "mqx.h"
#include "psp.h"

#define  __USB_DEVICE__              1
#define  BSP_MAX_USB_DRIVERS         1

/* Add CPU specific macros */
#if (PSP_MQX_CPU_IS_MCF5445X || PSP_MQX_CPU_IS_MCF5227X || PSP_MQX_CPU_IS_MCF5441X||PSP_MQX_CPU_IS_MCF5301X||PSP_MQX_CPU_IS_MCF532X||PSP_MQX_CPU_IS_MPC512x||PSP_MQX_CPU_IS_MPC830x)
    #define HIGH_SPEED_DEVICE               (1)/*1:TRUE;0:FALSE*/
#else
    #define HIGH_SPEED_DEVICE               (0)/*1:TRUE;0:FALSE*/
#endif

/* The ARC is little-endian, just like USB */
#define USB_uint_16_low(x)          ((x) & 0xFF)
#define USB_uint_16_high(x)         (((x) >> 8) & 0xFF)

#include "devapi.h"
#if HIGH_SPEED_DEVICE
    #include "usbprv_hs.h"
#else
    #include "usbprv_fs.h"
#endif    

/*
**
** Low-level function list structure for USB
**
** This is the structure used to store chip specific functions to be called 
** by generic layer
*/
typedef struct usb_dev_callback_functions_struct
{
#ifdef __USB_DEVICE__
   /* The device number */
   uint_32             DEV_NUM;

   /* The Host/Device init function */
   uint_8 (_CODE_PTR_ DEV_INIT)(uint_8, _usb_device_handle);

   /* The function to send data */
   uint_8 (_CODE_PTR_ DEV_SEND)(_usb_device_handle, XD_STRUCT_PTR);

   /* The function to receive data */
   uint_8 (_CODE_PTR_ DEV_RECV)(_usb_device_handle, XD_STRUCT_PTR);
   
   /* The function to cancel the transfer */
   uint_8 (_CODE_PTR_ DEV_CANCEL_TRANSFER)(_usb_device_handle, uint_8, uint_8);
   
   uint_8 (_CODE_PTR_ DEV_INIT_ENDPOINT)(_usb_device_handle, XD_STRUCT_PTR);
   
   uint_8 (_CODE_PTR_ DEV_DEINIT_ENDPOINT)(_usb_device_handle, uint_8, uint_8);
   
   uint_8 (_CODE_PTR_ DEV_UNSTALL_ENDPOINT)(_usb_device_handle, uint_8, uint_8);
   
   uint_8 (_CODE_PTR_ DEV_GET_ENDPOINT_STATUS)(_usb_device_handle, uint_8, uint_16_ptr);
   
   uint_8 (_CODE_PTR_ DEV_SET_ENDPOINT_STATUS)(_usb_device_handle, uint_8, uint_16);

   uint_8 (_CODE_PTR_ DEV_GET_TRANSFER_STATUS)(_usb_device_handle, uint_8, uint_8);
   
   uint_8 (_CODE_PTR_ DEV_SET_ADDRESS)(_usb_device_handle, uint_8);
   
   uint_8 (_CODE_PTR_ DEV_SHUTDOWN)(_usb_device_handle);
   
   uint_8 (_CODE_PTR_ DEV_GET_SETUP_DATA)(_usb_device_handle, uint_8, uint_8_ptr);
   
   uint_8 (_CODE_PTR_ DEV_ASSERT_RESUME)(_usb_device_handle);
   
   uint_8 (_CODE_PTR_ DEV_STALL_ENDPOINT)(_usb_device_handle, uint_8, uint_8);
#endif

} USB_DEV_CALLBACK_FUNCTIONS_STRUCT, _PTR_ USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR;


extern USB_DEV_CALLBACK_FUNCTIONS_STRUCT _bsp_usb_dev_callback_table;

#ifdef __cplusplus
extern "C" {
#endif
extern uint_8 _usb_dev_driver_install(uint_32, pointer);
#ifdef __cplusplus
}
#endif

#endif
