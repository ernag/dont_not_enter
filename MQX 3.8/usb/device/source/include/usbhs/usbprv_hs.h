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
* $FileName: usbprv_hs.h$
* $Version : 3.6.3.0$
* $Date    : Aug-25-2010$
*
* Comments:
*
*  This file contains the private defines, externs and
*  data structure definitions required by the VUSB_HS Device
*  driver.
*                                                               
*END*********************************************************************/

#ifndef __usbprv_hs_h__
#define __usbprv_hs_h__ 1

#include "dev_cnfg.h"
#include "usbhs_dev_main.h"

#define  MAX_EP_TR_DESCRS                    (32) 
#define  MAX_XDS_FOR_TR_CALLS                (32) 
#define  MAX_USB_DEVICES                     (1)
#define  USB_MAX_ENDPOINTS                   (4) 
#define ZERO_LENGTH                          (0) 

#define  USB_MAX_CTRL_PAYLOAD                (64)
 
/* Macro for aligning the EP queue head to 32 byte boundary */
#define USB_MEM32_ALIGN(n)                   ((n) + (-(n) & 31))

/* Macro for aligning the EP queue head to 1024 byte boundary */
#define USB_MEM1024_ALIGN(n)                 ((n) + (-(n) & 1023))

/* Macro for aligning the EP queue head to 1024 byte boundary */
#define USB_MEM2048_ALIGN(n)                 ((n) + (-(n) & 2047))

#define USB_XD_QADD(head,tail,XD)      \
   if ((head) == NULL) {         \
      (head) = (XD);            \
   } else {                      \
      (tail)->SCRATCH_PTR->PRIVATE = (XD);   \
   } /* Endif */                 \
   (tail) = (XD);               \
   (XD)->SCRATCH_PTR->PRIVATE = NULL
   
#define USB_XD_QGET(head,tail,XD)      \
   (XD) = (head);               \
   if (head) {                   \
      (head) = (XD_STRUCT_PTR)((head)->SCRATCH_PTR->PRIVATE);  \
      if ((head) == NULL) {      \
         (tail) = NULL;          \
      } /* Endif */              \
   } /* Endif */

#define EHCI_DTD_QADD(head,tail,dTD)      \
   if ((head) == NULL) {         \
      (head) = (dTD);            \
   } else {                      \
      (tail)->SCRATCH_PTR->PRIVATE = (void *) (dTD);   \
   } /* Endif */                 \
   (tail) = (dTD);               \
   (dTD)->SCRATCH_PTR->PRIVATE = NULL
   
#define EHCI_DTD_QGET(head,tail,dTD)      \
   (dTD) = (head);               \
   if (head) {                   \
      (head) = (head)->SCRATCH_PTR->PRIVATE;  \
      if ((head) == NULL) {      \
         (tail) = NULL;          \
      } /* Endif */              \
   } /* Endif */

/***************************************
**
** Data structures
**
*/

typedef struct xd_struct 
{
   uchar          EP_NUM;           /* Endpoint number */
   uchar          BDIRECTION;       /* Direction : Send/Receive */
   uint_8         EP_TYPE;          /* Type of the endpoint: Ctrl, Isoch, Bulk, 
                                    ** Int 
                                    */
   uchar          BSTATUS;          /* Current transfer status */
   uint_8_ptr      WSTARTADDRESS;    /* Address of first byte */
   uint_32        WTOTALLENGTH;     /* Number of bytes to send/recv */
   uint_32        WSOFAR;           /* Number of bytes recv'd so far */
   uint_16        WMAXPACKETSIZE;   /* Max Packet size */
   boolean        DONT_ZERO_TERMINATE;
   uint_8         MAX_PKTS_PER_UFRAME;
   SCRATCH_STRUCT_PTR SCRATCH_PTR;
} XD_STRUCT, _PTR_ XD_STRUCT_PTR;

/* The USB Device State Structure */
typedef struct 
{
   boolean                          BUS_RESETTING;       /* Device is 
                                                         ** being reset 
                                                         */
   boolean                          TRANSFER_PENDING;    /* Transfer pending ? */

   USBHS_REG_STRUCT_PTR             DEV_PTR; /* Device Controller 
                                                         ** Register base 
                                                         ** address 
                                                         */

   pointer                          CALLBACK_STRUCT_PTR;
   
   SERVICE_STRUCT_PTR               SERVICE_HEAD_PTR;       /* Head struct 
                                                         ** address of 
                                                         ** registered services 
                                                         */
   XD_STRUCT_PTR                    TEMP_XD_PTR;         /* Temp xd for ep init */
   XD_STRUCT_PTR                    XD_BASE;
   XD_STRUCT_PTR                    XD_HEAD;             /* Head Transaction 
                                                         ** descriptors 
                                                         */
   XD_STRUCT_PTR                    XD_TAIL;             /* Tail Transaction 
                                                         ** descriptors 
                                                         */
   XD_STRUCT_PTR                    PENDING_XD_PTR;      /* pending transfer */
   uint_32                          XD_ENTRIES;
   USBHS_EP_QUEUE_HEAD_STRUCT_PTR  EP_QUEUE_HEAD_PTR;   /* Endpoint Queue 
                                                         ** head 
                                                         */   
   uint_8_ptr                       DRIVER_MEMORY;/* pointer to driver memory*/
   uint_32                          TOTAL_MEMORY; /* total memory occupied 
                                                     by driver */
   USBHS_EP_QUEUE_HEAD_STRUCT_PTR  EP_QUEUE_HEAD_BASE;
   USBHS_EP_TR_STRUCT_PTR          DTD_BASE_PTR;        /* Device transfer 
                                                         ** descriptor pool 
                                                         ** address 
                                                         */
   USBHS_EP_TR_STRUCT_PTR          DTD_ALIGNED_BASE_PTR;/* Aligned transfer 
                                                         ** descriptor pool 
                                                         ** address 
                                                         */
   USBHS_EP_TR_STRUCT_PTR          DTD_HEAD;
   USBHS_EP_TR_STRUCT_PTR          DTD_TAIL;
   USBHS_EP_TR_STRUCT_PTR          EP_DTD_HEADS[USB_MAX_ENDPOINTS * 2];
   USBHS_EP_TR_STRUCT_PTR          EP_DTD_TAILS[USB_MAX_ENDPOINTS * 2];
   SCRATCH_STRUCT_PTR               XD_SCRATCH_STRUCT_BASE;
   
   
   SCRATCH_STRUCT_PTR               SCRATCH_STRUCT_BASE;
   
   /* These fields are kept only for USB_shutdown() */
   void(_CODE_PTR_                  OLDISR_PTR)(pointer);
   pointer                          OLDISR_DATA;
   uint_16                          USB_STATE;
   uint_16                          USB_DEVICE_STATE;
   uint_16                          USB_SOF_COUNT;
   uint_16                          DTD_ENTRIES;
   uint_16                          ERRORS;
   uint_16                          USB_DEV_STATE_B4_SUSPEND;
   uint_8                           DEV_NUM;             /* USB device number 
                                                         ** on the board 
                                                         */
   uint_8                           DEV_VEC;             /* Interrupt vector 
                                                         ** number for USB OTG 
                                                         */
   uint_8                           SPEED;               /* Low Speed, 
                                                         ** High Speed, 
                                                         ** Full Speed 
                                                         */
   uint_8                           MAX_ENDPOINTS;       /* Max endpoints
                                                         ** supported by this
                                                         ** device
                                                         */
                                                         
   uint_16                          USB_CURR_CONFIG;                                                         
   uint_8                           DEVICE_ADDRESS;
} USB_DEV_STATE_STRUCT, _PTR_ USB_DEV_STATE_STRUCT_PTR;

/***************************************
**
** Prototypes
**
*/
#ifdef __cplusplus
extern "C" {
#endif
extern void _usb_dci_usbhs_isr(pointer);

extern uint_8 _usb_dci_usbhs_init(uint_8, _usb_device_handle);
extern void _usb_device_free_XD(_usb_device_handle ,pointer);
extern void _usb_dci_usbhs_free_dTD(_usb_device_handle, pointer);
extern uint_8 _usb_dci_usbhs_add_dTD(_usb_device_handle, XD_STRUCT_PTR);
extern uint_8 _usb_dci_usbhs_send_data(_usb_device_handle, XD_STRUCT_PTR);
extern uint_8 _usb_dci_usbhs_recv_data(_usb_device_handle, XD_STRUCT_PTR);
extern uint_8 _usb_dci_usbhs_cancel_transfer(_usb_device_handle, uint_8, uint_8);
extern uint_8 _usb_dci_usbhs_get_transfer_status(_usb_device_handle, uint_8, uint_8);
extern uint_8 _bsp_get_usb_otg_vector(uint_8);
extern pointer _bsp_get_usb_base(uint_8);
extern pointer _bsp_get_usb_capability_register_base(uint_8);
extern void _usb_dci_usbhs_process_tr_complete(_usb_device_handle);
extern void _usb_dci_usbhs_process_reset(_usb_device_handle);
extern void _usb_dci_usbhs_process_tr_complete(_usb_device_handle);
extern void _usb_dci_usbhs_process_suspend(_usb_device_handle);
extern void _usb_dci_usbhs_process_SOF(_usb_device_handle);
extern void _usb_dci_usbhs_process_port_change(_usb_device_handle);
extern void _usb_dci_usbhs_process_error(_usb_device_handle);
extern uint_8 _usb_dci_usbhs_shutdown(_usb_device_handle);
extern uint_8 _usb_dci_usbhs_set_address(_usb_device_handle, uint_8);
extern uint_8 _usb_dci_usbhs_get_setup_data(_usb_device_handle, 
   uint_8, uint_8_ptr);
extern uint_8 _usb_dci_usbhs_assert_resume(_usb_device_handle);
extern uint_8 _usb_dci_usbhs_init_endpoint(_usb_device_handle, XD_STRUCT_PTR);
extern uint_8 _usb_dci_usbhs_stall_endpoint(_usb_device_handle, uint_8, uint_8);
extern uint_8 _usb_dci_usbhs_unstall_endpoint(_usb_device_handle, uint_8, uint_8);
extern uint_8 _usb_dci_usbhs_deinit_endpoint(_usb_device_handle, uint_8, uint_8);
extern uint_8 _usb_dci_usbhs_set_endpoint_status(_usb_device_handle, uint_8, 
   uint_16);
extern uint_8 _usb_dci_usbhs_set_test_mode(_usb_device_handle, uint_16);   
extern uint_8  _usb_dci_usbhs_get_endpoint_status(_usb_device_handle, uint_8, uint_16_ptr);
extern void _usb_dci_usbhs_chip_initialize(_usb_device_handle);

#ifdef __cplusplus
}
#endif

#endif
