#ifndef __usb_mqx_h__
#define __usb_mqx_h__
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
* $FileName: usb_mqx.h$
* $Version : 3.8.11.0$
* $Date    : Sep-19-2011$
*
* Comments:
*
*   This file contains MQX-specific code used by the USB stack
*
*END************************************************************************/

#include "mqx.h"

#include "fio.h"
#include "mqx_dll.h"

#ifndef MQX_DISABLE_CONFIG_CHECK
# define MQX_DISABLE_CONFIG_CHECK
# include "message.h"
# include "timer.h"
# undef MQX_DISABLE_CONFIG_CHECK
#else
# include "message.h"
# include "timer.h"
#endif

#define USB_RTOS_HOST_NUM_PIPE_MESSAGES         32

#define USB_RTOS_HOST_TASK_NUM_MESSAGES         32

#define USB_RTOS_HOST_TASK_TEMPLATE_INDEX       0
#define USB_RTOS_HOST_TASK_ADDRESS              _usb_rtos_host_task
#define USB_RTOS_HOST_TASK_STACKSIZE            2048
#define USB_RTOS_HOST_TASK_PRIORITY             3
#define USB_RTOS_HOST_TASK_NAME                 "USB Host Task"
#define USB_RTOS_HOST_TASK_ATTRIBUTES           0
#define USB_RTOS_HOST_TASK_CREATION_PARAMETER   USB_RTOS_HOST_TASK_NUM_MESSAGES
#define USB_RTOS_HOST_TASK_DEFAULT_TIME_SLICE   0

#define USB_RTOS_HOST_IO_TASK_NUM_MESSAGES         32

#define USB_RTOS_HOST_IO_TASK_TEMPLATE_INDEX       0
#define USB_RTOS_HOST_IO_TASK_ADDRESS              _usb_rtos_host_io_task
#define USB_RTOS_HOST_IO_TASK_STACKSIZE            2048
#define USB_RTOS_HOST_IO_TASK_PRIORITY             2
#define USB_RTOS_HOST_IO_TASK_NAME                 "USB Int Task"
#define USB_RTOS_HOST_IO_TASK_ATTRIBUTES           0
#define USB_RTOS_HOST_IO_TASK_CREATION_PARAMETER   0
#define USB_RTOS_HOST_IO_TASK_DEFAULT_TIME_SLICE   0

#define USB_log_error(f,l,e)  USB_log_error_internal( f,l, e)
typedef struct usb_rtos_task_struct
{
   _task_id task_id;
} USB_RTOS_TASK_STRUCT, _PTR_ USB_RTOS_TASK_STRUCT_PTR;

typedef struct usb_rtos_msg_queue_struct
{
   _queue_id   msg_queue_id;
} USB_RTOS_MSG_QUEUE_STRUCT, _PTR_ USB_RTOS_MSG_QUEUE_STRUCT_PTR;

typedef struct usb_rtos_timer_struct
{
   _timer_id   timer_id;
} USB_RTOS_TIMER_STRUCT, _PTR_ USB_RTOS_TIMER_STRUCT_PTR;

typedef struct usb_rtos_host_task_state_struct
{
   _pool_id                   msg_pool_id;
   USB_RTOS_TASK_STRUCT       task;
   USB_RTOS_MSG_QUEUE_STRUCT  msg_queue;
} USB_RTOS_HOST_TASK_STATE_STRUCT;

typedef struct usb_rtos_host_state_struct
{
   USB_RTOS_HOST_TASK_STATE_STRUCT io_task_state;
   USB_RTOS_HOST_TASK_STATE_STRUCT host_task_state;
} USB_RTOS_HOST_STATE_STRUCT, _PTR_ USB_RTOS_HOST_STATE_STRUCT_PTR;

/*
 * USB RTOS Host IO Message structure
 *
 * Used to offload time-consuming driver work from interrupt to task level.
 */

typedef struct usb_rtos_host_io_msg_struct
{
   MESSAGE_HEADER_STRUCT msg_hdr;
   USB_STATUS  (*func_ptr) (void * param0, void * param1, void * param2, void * param3);
   void *      param0;
   void *      param1;
   void *      param2;
   void *      param3;
} USB_RTOS_HOST_IO_MSG_STRUCT, _PTR_ USB_RTOS_HOST_IO_MSG_STRUCT_PTR;

/*
 * USB RTOS Host Request structure
 *
 * Common structure used by all USB host task requests
 */

typedef struct usb_rtos_host_request_struct
{
   MESSAGE_HEADER_STRUCT   msg_hdr;
   uint_32                 request_type;
} USB_RTOS_HOST_REQUEST_STRUCT, _PTR_ USB_RTOS_HOST_REQUEST_STRUCT_PTR;

#define USB_HOST_REQUEST_CALLBACK            0

/*
 * USB RTOS Host Request Callback structure
 *
 * Used to offload USB stack processing work from Interrupt task.
 */
 
typedef struct usb_rtos_host_request_callback_struct
{
   USB_RTOS_HOST_REQUEST_STRUCT  request_hdr;
   USB_STATUS  (*func_ptr) (void * param0, void * param1, void * param2, void * param3);
   void *      param0;
   void *      param1;
   void *      param2;
   void *      param3;
} USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT, _PTR_ USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT_PTR;

/*
 * USB RTOS Host Request union
 *
 * Used to ensure the request message pool contains messages
 * with sizes to handle any given message
 */
 
typedef union usb_rtos_host_request_union
{
   USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT           a;
} USB_RTOS_HOST_REQUEST_UNION, _PTR_ USB_RTOS_HOST_REQUEST_UNION_PTR;

#ifdef __cplusplus
extern "C" {
#endif

extern USB_STATUS _usb_rtos_task_create
(
   _processor_number    processor_number,
   _mqx_uint            template_index,
   uint_32              parameter,
   USB_RTOS_TASK_STRUCT_PTR task_ptr
);

extern USB_STATUS _usb_rtos_task_destroy
(
   USB_RTOS_TASK_STRUCT_PTR task_ptr
);

extern USB_STATUS _usb_rtos_msgq_open
(
   uint_32 msg_max_bytes,
   uint_32 unused2,
   USB_RTOS_MSG_QUEUE_STRUCT_PTR msg_queue_ptr
);

extern USB_STATUS _usb_rtos_msgq_close
(
   USB_RTOS_MSG_QUEUE_STRUCT_PTR msg_queue_ptr
);

extern USB_STATUS _usb_rtos_msgq_receive
(
   USB_RTOS_MSG_QUEUE_STRUCT_PTR msg_queue_ptr,
   uint_32        timeout_ticks,
   pointer _PTR_  msg_ptr_ptr
);

extern USB_STATUS _usb_rtos_msgq_send
(
   USB_RTOS_MSG_QUEUE_STRUCT_PTR msg_queue_ptr,
   pointer msg_ptr,
   uint_32 msg_num_bytes
);

extern uint_8 _usb_host_driver_install
(
   uint_32,
   pointer
);

extern USB_STATUS _usb_rtos_timer_create_oneshot_and_start_after_milliseconds
(
   void (_CODE_PTR_ func_ptr)(uint_32, pointer, uint_32, uint_32), 
   pointer,
   uint_32,
   USB_RTOS_TIMER_STRUCT_PTR
);

extern USB_STATUS _usb_rtos_timer_cancel
(
   USB_RTOS_TIMER_STRUCT_PTR
);

extern USB_STATUS _usb_rtos_host_init
(
   void
);

extern USB_STATUS _usb_rtos_host_shutdown
(
   void
);

extern USB_STATUS _usb_host_transaction_complete_callback
(
   pointer param0,
   pointer param1,
   pointer param2,
   pointer param3
);

extern USB_STATUS _usb_rtos_host_task_offload
(
   USB_STATUS (_PTR_ func_ptr) (pointer param0, pointer param1, pointer param2, pointer param3),
   pointer param0,
   pointer param1,
   pointer param2,
   pointer param3
);

extern USB_STATUS _usb_rtos_host_io_task_offload
(
   USB_STATUS (_PTR_ func_ptr) (pointer param0, pointer param1, pointer param2, pointer param3),
   pointer param0,
   pointer param1,
   pointer param2,
   pointer param3
);

extern USB_STATUS _usb_rtos_host_task_destroy
(
   void
);

extern USB_STATUS _usb_rtos_host_io_task_destroy
(
   void
);

extern volatile USB_RTOS_HOST_STATE_STRUCT usb_rtos_host_state;

extern uint_32 USB_log_error_internal(char_ptr file, uint_32 line, uint_32 error);
extern uint_32 USB_set_error_ptr(char_ptr file, uint_32 line, uint_32_ptr error_ptr, uint_32 error);

#ifdef __cplusplus
}
#endif

#endif
