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
* $FileName: usbinst.c$
* $Version : 3.8.2.1$
* $Date    : Dec-7-2011$
*
* Comments:
*
*   This file contains the function to install the low-level driver functions
*                                                               
*END*********************************************************************/

#include "types.h"
#include "mqx_dev.h"

#ifdef __USB_DEVICE__
#include "devapi.h"
#endif

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_dev_driver_install
*  Returned Value : None
*  Comments       :
*        Installs the device
*END*-----------------------------------------------------------------*/
uint_8 _usb_dev_driver_install
   (
      /* [IN] device number */
      uint_32  device_number,
            
      /* [IN] address if the callback functions structure */
      pointer  callback_struct_ptr
   )
{ /* Body */
   pointer callback_struct_table_ptr;
   
   callback_struct_table_ptr = _mqx_get_io_component_handle(IO_USB_COMPONENT);
   
   if (!callback_struct_table_ptr) 
   {
      callback_struct_table_ptr = 
         USB_mem_alloc_zero((BSP_MAX_USB_DRIVERS * sizeof(USB_DEV_CALLBACK_FUNCTIONS_STRUCT)));
      if (!callback_struct_table_ptr) 
      {
         #if _DEBUG
            printf("memalloc failed in _usb_driver_install\n");
         #endif 
         return USBERR_DRIVER_INSTALL_FAILED;
      } /* Endif */
      
      USB_mem_zero((uint_8_ptr)callback_struct_table_ptr, 
         sizeof(callback_struct_table_ptr));
      
      _mqx_set_io_component_handle(IO_USB_COMPONENT, 
         callback_struct_table_ptr);
   } /* Endif */

   *((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)\
      (((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)callback_struct_table_ptr) + 
      device_number)) = 
      *((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)callback_struct_ptr);

   return USB_OK;

} /* EndBody */

/* EOF */
