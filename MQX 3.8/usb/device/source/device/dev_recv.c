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
* $FileName: dev_recv.c$
* $Version : 3.8.4.0$
* $Date    : Sep-19-2011$
*
* Comments:
*
*  This file contains USB device API specific function to receive 
*  data.
*                                                               
*END*********************************************************************/

#include "devapi.h"
#include "mqx_dev.h"

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_recv_data
*  Returned Value : USB_OK or error code
*  Comments       :
*        Receives data on a specified endpoint.
*
*END*-----------------------------------------------------------------*/
uint_8 _usb_device_recv_data
   (
      /* [IN] the USB_USB_dev_initialize state structure */
      _usb_device_handle         handle,
            
      /* [IN] the Endpoint number */
      uint_8                     ep_num,
            
      /* [IN] buffer to receive data */
      uint_8_ptr                  buff_ptr,
            
      /* [IN] length of the transfer */
      uint_32                    size
   )
{ /* Body */
   uint_8                           error = USB_OK;
   XD_STRUCT_PTR                    xd_ptr;
   USB_DEV_STATE_STRUCT_PTR         usb_dev_ptr;
   
   if (handle == NULL)
   {
      return USBERR_ERROR;
   }
   
   usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)handle;
   
   #if PSP_HAS_DATA_CACHE
      /********************************************************
       If system has a data cache, it is assumed that buffer
       passed to this routine will be aligned on a cache line
       boundry. The following code will invalidate the
       buffer before passing it to hardware driver.   
      ********************************************************/
       USB_dcache_invalidate_mlines((pointer)buff_ptr,size);   
   #endif

         
   USB_lock();

   if (!usb_dev_ptr->XD_ENTRIES) 
   {
      USB_unlock();
      return USB_STATUS_TRANSFER_IN_PROGRESS;
   } /* Endif */

   /* Get a transfer descriptor for the specified endpoint 
   ** and direction 
   */
   USB_XD_QGET(usb_dev_ptr->XD_HEAD, usb_dev_ptr->XD_TAIL, xd_ptr);
   
   usb_dev_ptr->XD_ENTRIES--;

   /* Initialize the new transfer descriptor */      
   xd_ptr->EP_NUM = ep_num;
   xd_ptr->BDIRECTION = USB_RECV;
   xd_ptr->WTOTALLENGTH = size;
   xd_ptr->WSTARTADDRESS = buff_ptr;
   xd_ptr->WSOFAR = 0;
   
   xd_ptr->BSTATUS = USB_STATUS_TRANSFER_ACCEPTED;
   

   if (((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)\
      usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_RECV != NULL)
   {
     error = ((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)\
      usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_RECV(handle, xd_ptr);  
   }
   else
   {
      #if _DEBUG
        printf("_usb_device_recv_data: DEV_RECV is NULL\n");                      
      #endif    
      return USBERR_ERROR;
   }
       
   USB_unlock();
   
   if (error) 
   {
      return USBERR_RX_FAILED;
   } /* Endif */
   
   return error;
} /* EndBody */

/* EOF */
