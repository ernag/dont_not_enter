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
* $FileName: ehci_shut.c$
* $Version : 3.8.7.1$
* $Date    : Dec-7-2011$
*
* Comments:
*
*   This file contains the low-level Host API functions specific to the VUSB 
*   chip for shutting down the host controller.
*
*END************************************************************************/
#include "hostapi.h"
#include "usbprv_host.h"
#include "ehci_shut.h"

#include "mqx_host.h"

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_hci_vusb20_shutdown
*  Returned Value : None
*  Comments       :
*     Shutdown and de-initialize the VUSB1.1 hardware
*
*END*-----------------------------------------------------------------*/

USB_STATUS _usb_hci_vusb20_shutdown
   (
      /* [IN] the USB Host state structure */
      _usb_host_handle        handle
   )
{ /* Body */
   USB_HOST_STATE_STRUCT_PTR         usb_host_ptr;
   VUSB20_REG_STRUCT_PTR             dev_ptr;
   ACTIVE_QH_MGMT_STRUCT_PTR         active_list_member_ptr, temp_ptr = NULL;
   
   usb_host_ptr = (USB_HOST_STATE_STRUCT_PTR)handle;
   
   dev_ptr      = (VUSB20_REG_STRUCT_PTR)usb_host_ptr->DEV_PTR;
   
   /* Disable interrupts */
   EHCI_REG_CLEAR_BITS(dev_ptr->REGISTERS.OPERATIONAL_HOST_REGISTERS.USB_INTR,VUSB20_HOST_INTR_EN_BITS);
      
   /* Stop the controller */
   EHCI_REG_CLEAR_BITS(dev_ptr->REGISTERS.OPERATIONAL_HOST_REGISTERS.USB_CMD,EHCI_CMD_RUN_STOP);
   
   /* Reset the controller to get default values */
   EHCI_REG_WRITE(dev_ptr->REGISTERS.OPERATIONAL_HOST_REGISTERS.USB_CMD,EHCI_CMD_CTRL_RESET);
   
   /**********************************************************
   ensure that periodic list in uninitilized.
   **********************************************************/
   usb_host_ptr->ITD_LIST_INITIALIZED = FALSE;
   usb_host_ptr->SITD_LIST_INITIALIZED = FALSE;
   usb_host_ptr->PERIODIC_LIST_INITIALIZED = FALSE;
   usb_host_ptr->ALIGNED_PERIODIC_LIST_BASE_ADDR = NULL;
   
   /**********************************************************
   Free all memory occupied by active transfers   
   **********************************************************/
   active_list_member_ptr = usb_host_ptr->ACTIVE_ASYNC_LIST_PTR;
   while(active_list_member_ptr) 
   {
      temp_ptr =  active_list_member_ptr;
      active_list_member_ptr = (ACTIVE_QH_MGMT_STRUCT_PTR)active_list_member_ptr->NEXT_ACTIVE_QH_MGMT_STRUCT_PTR;
      USB_mem_free(temp_ptr);
   }

   active_list_member_ptr = usb_host_ptr->ACTIVE_INTERRUPT_PERIODIC_LIST_PTR;
   while(active_list_member_ptr) 
   {
      temp_ptr =  active_list_member_ptr;
      active_list_member_ptr = (ACTIVE_QH_MGMT_STRUCT_PTR) \
                              active_list_member_ptr->NEXT_ACTIVE_QH_MGMT_STRUCT_PTR;
      USB_mem_free(temp_ptr);
      
   }

   /* Free all controller specific memory */
   USB_mem_free((pointer)usb_host_ptr->CONTROLLER_MEMORY);
   
#if 0      
   /* Free all controller specific memory */
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->PERIODIC_FRAME_LIST_BW_PTR);
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->ASYNC_LIST_BASE_ADDR);
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->ITD_BASE_PTR);
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->ITD_SCRATCH_STRUCT_BASE);
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->SITD_BASE_PTR);
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->QTD_BASE_PTR);
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->QTD_SCRATCH_STRUCT_BASE);
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->SITD_SCRATCH_STRUCT_BASE);
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->PERIODIC_LIST_BASE_ADDR);
   USB_memfree(__FILE__,__LINE__,(pointer)usb_host_ptr->QH_SCRATCH_STRUCT_BASE);
#endif

   return USB_OK;
   
} /* EndBody */

/* EOF */
