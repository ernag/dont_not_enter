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
* $FileName: usbprv_host.h$
* $Version : 3.7.15.0$
* $Date    : Mar-11-2011$
*
* Comments:
*
*   This file contains the macros, function prototypes and data structure
*   definitions required by the KHCI USB device driver.
*
*END************************************************************************/
#ifndef __usbprv_host_h__
#define __usbprv_host_h__

#include <mqx.h>
#include <lwevent.h>
#include "host_cnfg.h"
#include "hostapi.h"    /* needed for USB_SERVICE_STRUCT_PTR */
#include <lwmsgq.h>

enum {
    TR_CTRL,
    TR_IN,
    TR_OUT,
    TR_OUT_HNDSK,
    TR_IN_HNDSK
};

typedef struct pipe_tr_struct {
   /* fields common to both pipe transfer structures and the structures
    * used to initialize new pipe transfer instances.
    */
   TR_INFO_FIELDS
   boolean  SEND_PHASE;
   USB_SETUP   setup_packet;
   uint_32  status;
   struct pipe_tr_struct _PTR_ next;
} PIPE_TR_STRUCT, _PTR_ PIPE_TR_STRUCT_PTR;

typedef struct pipe_descriptor_struct
{
   /* Fields common to both pipe descriptors and the structures used to
    * initialize new pipe instances.
    */
   PIPE_INFO_FIELDS

   PIPE_TR_STRUCT_PTR   tr_list_ptr;
   
} PIPE_DESCRIPTOR_STRUCT, _PTR_ PIPE_DESCRIPTOR_STRUCT_PTR;

/* Transaction type */
typedef enum {
    TR_MSG_UNKNOWN,     // unknow - not used
    TR_MSG_SETUP,       // setup transaction
    TR_MSG_SEND,        // send trnasaction
    TR_MSG_RECV         // receive transaction
} TR_MSG_TYPE;


/* Transaction message */
typedef struct {
    TR_MSG_TYPE type;                       // transaction type
    PIPE_DESCRIPTOR_STRUCT_PTR pipe_desc;   // pointer to pipe descriptor
    PIPE_TR_STRUCT_PTR pipe_tr;             // pointer to transaction
} TR_MSG_STRUCT;

/* Interrupt transaction message queue */
typedef struct tr_int_que_itm_struct {
    TIME_STRUCT   time;         /* next time to fire interrupt */
    _mqx_uint     period;       /* period in [ms] for interrupt endpoint */
    TR_MSG_STRUCT msg;          /* atom transaction message */
} TR_INT_QUE_ITM_STRUCT, _PTR_ TR_INT_QUE_ITM_STRUCT_PTR;

#define BDT_BASE                ((uint_32*)(bdt))
#define BD_PTR(ep, rxtx, odd)   (((uint_32)BDT_BASE) & 0xfffffe00 | ((ep & 0x0f) << 5) | ((rxtx & 1) << 4) | ((odd & 1) << 3))

#define BD_CTRL(ep,rxtx,odd)    (*((uint_32*)BD_PTR(ep, rxtx, odd)))
#define BD_CTRL_RX(ep, odd)     (*((uint_32*)BD_PTR(ep, 0, odd)))
#define BD_CTRL_TX(ep, odd)     (*((uint_32*)BD_PTR(ep, 1, odd)))

#define BD_ADDR(ep,rxtx,odd)    (*((uint_32*)BD_PTR(ep, rxtx, odd) + 1))
#define BD_ADDR_RX(ep, odd)     (*((uint_32*)BD_PTR(ep, 0, odd) + 1))
#define BD_ADDR_TX(ep, odd)     (*((uint_32*)BD_PTR(ep, 1, odd) + 1))

/* Define USB buffer descriptor definitions in case of their lack */
#ifndef USB_BD_BC
#   define USB_BD_BC(n)                 ((n & 0x3ff) << 16)
#   define USB_BD_OWN                   0x80
#   define USB_BD_DATA01(n)             ((n & 1) << 6)
#   define USB_BD_DATA0                 USB_BD_DATA01(0)
#   define USB_BD_DATA1                 USB_BD_DATA01(1)
#   define USB_BD_KEEP                  0x20
#   define USB_BD_NINC                  0x10
#   define USB_BD_DTS                   0x08
#   define USB_BD_STALL                 0x04
#   define USB_BD_PID(n)                ((n & 0x0f) << 2)
#endif

#ifndef USB_TOKEN_TOKENPID_SETUP
#   define USB_TOKEN_TOKENPID_OUT                   USB_TOKEN_TOKENPID(0x1)
#   define USB_TOKEN_TOKENPID_IN                    USB_TOKEN_TOKENPID(0x9)
#   define USB_TOKEN_TOKENPID_SETUP                 USB_TOKEN_TOKENPID(0xD)
#endif

/* Alignement of buffer for DMA transfer, needed in some cases,
** USB DMA bus could not possibly be intializes properly and 
** first data transfered is the one aligned at 4-byte boundary
*/
#define USB_MEM4_ALIGN(n)               ((n) + (-(n) & 3))
#define USB_DMA_ALIGN(n)                USB_MEM4_ALIGN(n)


typedef struct usb_host_state_struct
{
   /* driver-specific fields */
   /* Generic device fields */
   uint_32                             vector;
   boolean                             endian_swap;
   
   /* Generic USB host controller fields */
   uint_32                             device_type;

   /* Generic USB hub fields */
   MQX_DLL_LIST                        debounce_list;
   MQX_DLL_LIST                        reset_recovery_list;
   uint_32                             num_downstream_ports;
   uint_8                              power_on_to_power_good_time;
   boolean                             no_power_switching;
   uint_8                              power_switching_mode;

   /* fields common to both the VUSB32 and VUSBHS host device drivers */
   uint_32                             FRAME_LIST_SIZE;
   pointer                             DEV_PTR;
   pointer                             DEVICE_LIST_PTR;
   USB_SERVICE_STRUCT_PTR              SERVICE_HEAD_PTR;
   PIPE_DESCRIPTOR_STRUCT_PTR          PIPE_DESCRIPTOR_BASE_PTR;
   pointer                             CALLBACK_STRUCT_PTR;
   uint_8                              DEV_NUM;
   pointer                             DEVICE_INFO_TABLE;

   uint_32                             speed;

   /* RX/TX buffer descriptor toggle bits */
   uint_8                              rx_bd;
   uint_8                              tx_bd;
   /* event from KHCI peripheral */
   LWEVENT_STRUCT                      khci_event;
   /* Pipe, that had latest NAK respond, usefull to cut bandwidth for interrupt pipes sending no data */
   PIPE_DESCRIPTOR_STRUCT_PTR          last_to_pipe;
   /* Interrupt transactions */
   TR_INT_QUE_ITM_STRUCT               tr_int_que[USBCFG_KHCI_MAX_INT_TR];
   _mqx_max_type                       tr_que[(sizeof(LWMSGQ_STRUCT) + USBCFG_KHCI_TR_QUE_MSG_CNT * sizeof(TR_MSG_STRUCT)) / sizeof(_mqx_max_type) + 1];
} USB_HOST_STATE_STRUCT, _PTR_ USB_HOST_STATE_STRUCT_PTR;

#ifdef __cplusplus
extern "C" {
#endif

extern uint_8 _bsp_get_usb_vector(uint_8);
extern pointer _bsp_get_usb_base(uint_8);

#ifdef __cplusplus
}
#endif

#endif

/* EOF */
