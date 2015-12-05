/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
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
* $FileName: usb_dcd_pol.c$
* $Version : 3.8.1.0$
* $Date    : Jul-27-2011$
*
* Comments:
*
*   This file contains the USB DCD polled I/O driver functions.
*
*END************************************************************************/


#include <mqx_inc.h>
#include <fio.h>
#include <fio_prv.h>
#include <io.h>
#include <io_prv.h>
#include "usb_dcd.h"
#include "usb_dcd_pol_prv.h"


/*FUNCTION****************************************************************
* 
* Function Name    : _io_usb_dcd_polled_install
* Returned Value   : MQX error code
* Comments         :
*    Install the USB_DCD device.
*
*END**********************************************************************/

_mqx_uint _io_usb_dcd_polled_install
   (
      /* [IN] A string that identifies the device for fopen */
      char_ptr               identifier,
  
      /* [IN] The I/O init function */
      _mqx_uint (_CODE_PTR_ init)(pointer, pointer _PTR_, char_ptr),
      
      /* [IN] The I/O de-init function */
      _mqx_uint (_CODE_PTR_ deinit)(pointer, pointer),
      
      /* [IN] The input function */
      _mqx_int  (_CODE_PTR_ recv)(pointer, char_ptr, _mqx_int),
     
      /* [IN] The output function */
      _mqx_int  (_CODE_PTR_ xmit)(pointer, char_ptr, _mqx_int),

      /* [IN] The I/O ioctl function */
      _mqx_int  (_CODE_PTR_ ioctl)(pointer, _mqx_uint, _mqx_uint_ptr),

      /* [IN] The I/O init data pointer */
      pointer               init_data_ptr
      
   )

{ /* Body */
   IO_USB_DCD_POLLED_DEVICE_STRUCT_PTR pol_io_dev_ptr;

   pol_io_dev_ptr = (IO_USB_DCD_POLLED_DEVICE_STRUCT_PTR)_mem_alloc_system_zero ((_mem_size)sizeof (IO_USB_DCD_POLLED_DEVICE_STRUCT));
   if (pol_io_dev_ptr == NULL) 
   {
      return MQX_OUT_OF_MEMORY;
   }
   _mem_set_type (pol_io_dev_ptr, MEM_TYPE_IO_USB_DCD_POLLED_DEVICE_STRUCT);            

   pol_io_dev_ptr->DEV_INIT          = init;
   pol_io_dev_ptr->DEV_DEINIT        = deinit;
   pol_io_dev_ptr->DEV_READ          = recv;
   pol_io_dev_ptr->DEV_WRITE         = xmit;
   pol_io_dev_ptr->DEV_IOCTL         = ioctl;
   pol_io_dev_ptr->DEV_INIT_DATA_PTR = init_data_ptr;
   
   return (_io_dev_install_ext (identifier,
      _io_usb_dcd_polled_open,  _io_usb_dcd_polled_close,
      _io_usb_dcd_polled_read,  _io_usb_dcd_polled_write,
      _io_usb_dcd_polled_ioctl, _io_usb_dcd_polled_uninstall,
      (pointer)pol_io_dev_ptr));

} /* Endbody */


/*FUNCTION****************************************************************
*
* Function Name    : _io_usb_dcd_polled_uninstall
* Returned Value   : MQX error code
* Comments         :
*    Uninstall a polled USB DCD device.
*
*END**********************************************************************/

_mqx_int _io_usb_dcd_polled_uninstall
   (
      /* [IN] The IO device structure for the device */
      IO_DEVICE_STRUCT_PTR   io_dev_ptr
   )
{ /* Body */
   IO_USB_DCD_POLLED_DEVICE_STRUCT_PTR pol_io_dev_ptr = io_dev_ptr->DRIVER_INIT_PTR;

   if (pol_io_dev_ptr->COUNT == 0) 
   {
      if (pol_io_dev_ptr->DEV_DEINIT) 
      {
         (*pol_io_dev_ptr->DEV_DEINIT)(pol_io_dev_ptr->DEV_INIT_DATA_PTR,pol_io_dev_ptr->DEV_INFO_PTR);
      }
      _mem_free (pol_io_dev_ptr);
      io_dev_ptr->DRIVER_INIT_PTR = NULL;
      return IO_OK;
   } else {
      return IO_ERROR_DEVICE_BUSY;
   }

} /* Endbody */


/*FUNCTION****************************************************************
* 
* Function Name    : _io_usb_dcd_polled_open
* Returned Value   : MQX error code
* Comments         :
*    This routine initializes the USB DCD I/O channel. It acquires
*    memory, then stores information into it about the channel.
*    It then calls the hardware interface function to initialize the channel.
* 
*END**********************************************************************/

_mqx_int _io_usb_dcd_polled_open
   (
      /* [IN] the file handle for the device being opened */
      FILE_DEVICE_STRUCT_PTR          fd_ptr,
       
      /* [IN] the remaining portion of the name of the device */
      char_ptr                        open_name_ptr,

      /* [IN] the flags to be used during operation */
      char_ptr                        flags
   )
{ /* Body */
   IO_DEVICE_STRUCT_PTR                   io_dev_ptr;
   IO_USB_DCD_POLLED_DEVICE_STRUCT_PTR    pol_io_dev_ptr;
   _mqx_int                               result = MQX_OK;

   io_dev_ptr     = (IO_DEVICE_STRUCT_PTR)fd_ptr->DEV_PTR;
   pol_io_dev_ptr = (pointer)(io_dev_ptr->DRIVER_INIT_PTR);
   
   _int_disable ();
   if (pol_io_dev_ptr->COUNT) 
   {
      /* Device is already opened */
      _int_enable ();
      return MQX_IO_OPERATION_NOT_AVAILABLE;
   }
   pol_io_dev_ptr->COUNT = 1;
   _int_enable ();
      
   pol_io_dev_ptr->FLAGS = (_mqx_uint)flags;
   fd_ptr->FLAGS         = (_mqx_uint)flags;
            
   result = (*pol_io_dev_ptr->DEV_INIT)((pointer)pol_io_dev_ptr->DEV_INIT_DATA_PTR, &pol_io_dev_ptr->DEV_INFO_PTR, open_name_ptr);
   if (result != MQX_OK) 
   {
      pol_io_dev_ptr->COUNT = 0;
   }
   
   return result;

} /* Endbody */


/*FUNCTION****************************************************************
* 
* Function Name    : _io_usb_dcd_polled_close
* Returned Value   : MQX error code
* Comments         :
*    This routine closes the USB DCD I/O channel.
* 
*END**********************************************************************/

_mqx_int _io_usb_dcd_polled_close
   (
      /* [IN] the file handle for the device being closed */
      FILE_DEVICE_STRUCT_PTR       fd_ptr
   )
{ /* Body */
   IO_DEVICE_STRUCT_PTR                  io_dev_ptr;
   IO_USB_DCD_POLLED_DEVICE_STRUCT_PTR   pol_io_dev_ptr;
   _mqx_int                              result = MQX_OK;

   io_dev_ptr     = (IO_DEVICE_STRUCT_PTR)fd_ptr->DEV_PTR;
   pol_io_dev_ptr = (pointer)io_dev_ptr->DRIVER_INIT_PTR;

   _int_disable ();
   if (--pol_io_dev_ptr->COUNT == 0) 
   {
      if (pol_io_dev_ptr->DEV_DEINIT) 
      {
         result = (*pol_io_dev_ptr->DEV_DEINIT)(pol_io_dev_ptr,pol_io_dev_ptr->DEV_INFO_PTR);
      }
   }
   _int_enable ();
   return result;

} /* Endbody */

  
/*FUNCTION****************************************************************
* 
* Function Name    : _io_usb_dcd_polled_read
* Returned Value   : MQX_OK
* Comments         :
*    This routine calls the appropriate read routine.
*
*END*********************************************************************/

_mqx_int _io_usb_dcd_polled_read
   (
      /* [IN] the handle returned from _fopen */
      FILE_DEVICE_STRUCT_PTR       fd_ptr,

      /* [IN] where the characters are to be stored */
      char_ptr                     data_ptr,

      /* [IN] the number of bytes to read */
      _mqx_int                     n
   )
{  /* Body */

   IO_DEVICE_STRUCT_PTR                  io_dev_ptr;
   IO_USB_DCD_POLLED_DEVICE_STRUCT_PTR   pol_io_dev_ptr;
   
   io_dev_ptr     = (IO_DEVICE_STRUCT_PTR)fd_ptr->DEV_PTR;
   pol_io_dev_ptr = (pointer)io_dev_ptr->DRIVER_INIT_PTR;

   return (*pol_io_dev_ptr->DEV_READ)(pol_io_dev_ptr, data_ptr, n);

}  /* Endbody */


/*FUNCTION****************************************************************
* 
* Function Name    : _io_usb_dcd_polled_write
* Returned Value   : number of bytes written
* Comments         :
*    This routine calls the appropriate write routine.
*
*END**********************************************************************/

_mqx_int _io_usb_dcd_polled_write
   (
      /* [IN] the handle returned from _fopen */
      FILE_DEVICE_STRUCT_PTR       fd_ptr,

      /* [IN] where the bytes are stored */
      char_ptr                     data_ptr,

      /* [IN] the number of bytes to output */
      _mqx_int                     n
   )
{  /* Body */
   IO_DEVICE_STRUCT_PTR			            	io_dev_ptr;
   IO_USB_DCD_POLLED_DEVICE_STRUCT_PTR		 	pol_io_dev_ptr;
   
   io_dev_ptr     = (IO_DEVICE_STRUCT_PTR)fd_ptr->DEV_PTR;
   pol_io_dev_ptr = (pointer)io_dev_ptr->DRIVER_INIT_PTR;
               
   return (*pol_io_dev_ptr->DEV_WRITE)(pol_io_dev_ptr, data_ptr, n);
              
} /* Endbody */


/*FUNCTION*****************************************************************
* 
* Function Name    : _io_usb_dcd_polled_ioctl
* Returned Value   : MQX error code
* Comments         :
*    Returns result of USB_DCD ioctl operation.
*
*END*********************************************************************/

_mqx_int _io_usb_dcd_polled_ioctl
   (
      /* [IN] the file handle for the device */
      FILE_DEVICE_STRUCT_PTR       fd_ptr,

      /* [IN] the ioctl command */
      _mqx_uint                    cmd,

      /* [IN] the ioctl parameters */
      pointer                      input_param_ptr
   )
{ /* Body */
   IO_DEVICE_STRUCT_PTR  		          io_dev_ptr;
   IO_USB_DCD_POLLED_DEVICE_STRUCT_PTR  pol_io_dev_ptr;
   _mqx_int                        		 result = MQX_OK;
   _mqx_uint_ptr                   		 param_ptr = (_mqx_uint_ptr)input_param_ptr;

   io_dev_ptr     = (IO_DEVICE_STRUCT_PTR)fd_ptr->DEV_PTR;
   pol_io_dev_ptr = (pointer)io_dev_ptr->DRIVER_INIT_PTR;

    switch (cmd) {
        case IO_IOCTL_DEVICE_IDENTIFY :
            /* return the device identify */
            param_ptr[0] = IO_DEV_TYPE_PHYS_USB_DCD_POLLED;
            param_ptr[1] = 0;
            param_ptr[2] = IO_DEV_ATTR_POLL | IO_DEV_ATTR_READ | IO_DEV_ATTR_WRITE;
            result = MQX_OK;    
            break;
        default:
            if (pol_io_dev_ptr->DEV_IOCTL != NULL) {
                result = (*pol_io_dev_ptr->DEV_IOCTL)(pol_io_dev_ptr->DEV_INFO_PTR, cmd, param_ptr);
            }
    }
   
   return result;
} /* Endbody */

/* EOF */
