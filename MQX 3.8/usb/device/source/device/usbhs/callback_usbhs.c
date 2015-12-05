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
* $FileName: callback_usbhs.c$
* $Version : 3.8.1.1$
* $Date    : Dec-7-2011$
*
* Comments:
*
*   This file contains the default callback function for the usbhs 
*   and device.
*                                                               
*END*********************************************************************/

#include "mqx_dev.h"
#include "usbprv_hs.h"
#include "usb_bsp.h"

USB_DEV_CALLBACK_FUNCTIONS_STRUCT _bsp_usb_dev_callback_table =
{
   0,
   /* The Host/Device init function */
   _usb_dci_usbhs_init,

   /* The function to send data */
   _usb_dci_usbhs_send_data,

   /* The function to receive data */
   _usb_dci_usbhs_recv_data,
   
   /* The function to cancel the transfer */
   _usb_dci_usbhs_cancel_transfer,
         
   _usb_dci_usbhs_init_endpoint,
   
   _usb_dci_usbhs_deinit_endpoint,
   
   _usb_dci_usbhs_unstall_endpoint,
   
   _usb_dci_usbhs_get_endpoint_status,
   
   _usb_dci_usbhs_set_endpoint_status,

   _usb_dci_usbhs_get_transfer_status,
   
   _usb_dci_usbhs_set_address,
   
   _usb_dci_usbhs_shutdown,
   
   _usb_dci_usbhs_get_setup_data,
   
   _usb_dci_usbhs_assert_resume,
   
   _usb_dci_usbhs_stall_endpoint,
};

/* EOF */
