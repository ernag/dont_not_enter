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
* $FileName: usb_mqx.c$
* $Version : 3.6.19.0$
* $Date    : Aug-25-2010$
*
* Comments:
*
*   This file contains MQX-specific code to install low-level device
*   driver functions
*
*END************************************************************************/

#include "usb.h"

#include "hostapi.h"
#include "usbprv_host.h"

#ifdef __USB_OTG__
#include "otgapi.h"
#include "usbprv_dev.h"
#include "usbprv_host.h"
#include "usbprv_otg.h"
#endif

#include "mqx_host.h"
#include "usb_mqx.h"

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_host_driver_install
*  Returned Value : None
*  Comments       :
*        Installs the device
*END*-----------------------------------------------------------------*/
uint_8 _usb_host_driver_install
(
   /* [IN] device number */
   uint_32  device_number,
         
   /* [IN] address if the callback functions structure */
   pointer  callback_struct_ptr
)
{ /* Body */
   pointer callback_struct_table_ptr;
   
   if (device_number >= USBCFG_MAX_DRIVERS)
       return USBERR_DRIVER_INSTALL_FAILED;
   
   callback_struct_table_ptr = _mqx_get_io_component_handle(IO_USB_COMPONENT);
   
   if (!callback_struct_table_ptr) {
      callback_struct_table_ptr = 
         USB_mem_alloc_zero(USBCFG_MAX_DRIVERS*
            sizeof(USB_HOST_CALLBACK_FUNCTIONS_STRUCT));
      
      if (!callback_struct_table_ptr) {
         return (uint_8) USB_log_error(__FILE__,__LINE__,USBERR_DRIVER_INSTALL_FAILED);
      } /* Endif */
      _mem_set_type(callback_struct_table_ptr, MEM_TYPE_USB_CALLBACK_STRUCT);
      USB_mem_zero((uchar_ptr)callback_struct_table_ptr, 
         sizeof(callback_struct_table_ptr));
      
      _mqx_set_io_component_handle(IO_USB_COMPONENT, 
         callback_struct_table_ptr);
   } /* Endif */

   *((USB_HOST_CALLBACK_FUNCTIONS_STRUCT_PTR)\
      (((USB_HOST_CALLBACK_FUNCTIONS_STRUCT_PTR)callback_struct_table_ptr) + 
      device_number)) = 
      *((USB_HOST_CALLBACK_FUNCTIONS_STRUCT_PTR)callback_struct_ptr);

   return USB_OK;

} /* EndBody */


#if (!defined __USB_HOST_KHCI__ && !defined __USB_HOST_EHCI__)

USB_STATUS _usb_rtos_timer_create_oneshot_and_start_after_milliseconds
(
   void (_CODE_PTR_ func_ptr)
      (uint_32 id, pointer data_ptr, uint_32 seconds, uint_32 milliseconds), 
   pointer data_ptr,
   uint_32 milliseconds,
   USB_RTOS_TIMER_STRUCT_PTR timer_ptr
)
{
   timer_ptr->timer_id = _timer_start_oneshot_after (func_ptr, data_ptr, TIMER_ELAPSED_TIME_MODE, milliseconds);
   
   if (timer_ptr->timer_id == 0)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }
   
   return USB_OK;
}

USB_STATUS _usb_rtos_timer_cancel
(
   USB_RTOS_TIMER_STRUCT_PTR timer_ptr
)
{
   _mqx_uint status;
   
   status = _timer_cancel (timer_ptr->timer_id);
   
   if (status != MQX_OK)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }
   
   return USB_OK;
}

USB_STATUS _usb_rtos_msgq_open
(
   uint_32 msg_max_bytes,
   uint_32 unused2,
   USB_RTOS_MSG_QUEUE_STRUCT_PTR msg_queue_ptr
)
{
   _queue_id msg_queue_id;

   /* msg_max_bytes must fit within _msg_size */
   msg_queue_id = _msgq_open (MSGQ_FREE_QUEUE, (_msg_size)msg_max_bytes);

   if (msg_queue_id == MSGQ_NULL_QUEUE_ID) {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }

   msg_queue_ptr->msg_queue_id = msg_queue_id;
   
   return USB_OK;
}

USB_STATUS _usb_rtos_msgq_close
(
   USB_RTOS_MSG_QUEUE_STRUCT_PTR msg_queue_ptr
)
{
   boolean result;

   result = _msgq_close (msg_queue_ptr->msg_queue_id);
   
   if (result != TRUE)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }
   
   return USB_OK;
}

USB_STATUS _usb_rtos_msgq_receive
(
   USB_RTOS_MSG_QUEUE_STRUCT_PTR msg_queue_ptr,
   uint_32        timeout_ticks,
   pointer _PTR_  msg_ptr_ptr
)
{
   pointer msg_ptr;

   msg_ptr = _msgq_receive (msg_queue_ptr->msg_queue_id, timeout_ticks);

   /* caller will not inherently block on a system message queue */
   if (msg_ptr == NULL)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }
   
   *msg_ptr_ptr = msg_ptr;

   return USB_OK;
}

USB_STATUS _usb_rtos_msgq_send
(
   USB_RTOS_MSG_QUEUE_STRUCT_PTR msg_queue_ptr,
   pointer msg_ptr,
   uint_32 msg_num_bytes
)
{
   boolean result;
   
   /* msg_max_bytes must fit within _msg_size */
   ((MESSAGE_HEADER_STRUCT_PTR)msg_ptr)->SOURCE_QID = 0;
   ((MESSAGE_HEADER_STRUCT_PTR)msg_ptr)->TARGET_QID = msg_queue_ptr->msg_queue_id;
   ((MESSAGE_HEADER_STRUCT_PTR)msg_ptr)->SIZE = (_msg_size)msg_num_bytes;

   result = _msgq_send (msg_ptr);
   
   if (result != TRUE)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }

   return USB_OK;
}

USB_STATUS _usb_rtos_task_create
(
   _processor_number    processor_number,
   _mqx_uint            template_index,
   uint_32              parameter,
   USB_RTOS_TASK_STRUCT_PTR task_ptr
)
{
   _task_id task_id;

   task_id = _task_create_blocked (processor_number, template_index, parameter);
   
   if (task_id == (_task_id)NULL)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }
   
   task_ptr->task_id = task_id;
   
   _task_ready (_task_get_td (task_id));

   return USB_OK;
}

USB_STATUS _usb_rtos_task_destroy
(
   USB_RTOS_TASK_STRUCT_PTR task_ptr
)
{
   _mqx_uint status;
   
   status = _task_destroy (task_ptr->task_id);
   
   if (status != MQX_OK)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }

   return USB_OK;
}

USB_STATUS _usb_host_transaction_complete_callback
(
   pointer param0,
   pointer param1,
   pointer param2,
   pointer param3
)
{
   PIPE_DESCRIPTOR_STRUCT_PTR pipe_desc_ptr = (PIPE_DESCRIPTOR_STRUCT_PTR)param0;
   PIPE_TR_STRUCT_PTR pipe_tr_ptr = (PIPE_TR_STRUCT_PTR)param1;
   USB_STATUS status = (USB_STATUS)param2;

   /* pipe may be freed before all transactions have completed */
   if ((pipe_desc_ptr != NULL) && (pipe_desc_ptr->DEV_INSTANCE != NULL) && (pipe_tr_ptr != NULL))
   {
      if (pipe_desc_ptr->PIPETYPE == USB_ISOCHRONOUS_PIPE)
      {
#ifndef __USB_HOST_OHCI__
         if (pipe_desc_ptr->DIRECTION == USB_SEND)
         {
            if (pipe_tr_ptr->CALLBACK != NULL)
            {
               pipe_tr_ptr->CALLBACK((pointer)pipe_desc_ptr,
                                       (pointer)pipe_tr_ptr->CALLBACK_PARAM,
                                       (uchar_ptr)pipe_tr_ptr->TX_BUFFER,
                                       (uint_32)pipe_tr_ptr->TX_LENGTH,
                                       (uint_32)status);
            }
         }
         else
         {
            if (pipe_tr_ptr->CALLBACK != NULL)
            {
               pipe_tr_ptr->CALLBACK((pointer)pipe_desc_ptr,
                                       (pointer)pipe_tr_ptr->CALLBACK_PARAM,
                                       pipe_tr_ptr->RX_BUFFER,
                                       pipe_tr_ptr->iso_rx_length,
                                       (uint_32)status);
            }
         }
      }
#else
         if (pipe_tr_ptr->ISO_CALLBACK != NULL)
         {
            pipe_tr_ptr->ISO_CALLBACK(pipe_desc_ptr,
                                       pipe_tr_ptr,
                                       status);
         }
#endif
      }
      else
      {
         if (pipe_desc_ptr->DIRECTION == USB_SEND)
         {
            if (pipe_tr_ptr->CALLBACK != NULL)
            {
               pipe_tr_ptr->CALLBACK((pointer)pipe_desc_ptr,
                                       (pointer)pipe_tr_ptr->CALLBACK_PARAM,
                                       (uchar_ptr)pipe_tr_ptr->TX_BUFFER,
                                       (uint_32)pipe_tr_ptr->TX_LENGTH,
                                       (uint_32)status);
            }
         }
         else
         {
            if (pipe_tr_ptr->CALLBACK != NULL)
            {
               pipe_tr_ptr->CALLBACK((pointer)pipe_desc_ptr,
                                       (pointer)pipe_tr_ptr->CALLBACK_PARAM,
                                       pipe_tr_ptr->RX_BUFFER,
                                       pipe_tr_ptr->RX_LENGTH,
                                       (uint_32)status);
            }
         }
      }
   }
   else
   {
   }

   /* mark the pipe transfer descriptor as unused */
   pipe_tr_ptr->TR_INDEX = 0;
   
   return USB_OK;
}


static void _usb_rtos_host_task
(
   uint_32 num_messages
)
{
   _mqx_uint status;
   USB_RTOS_HOST_REQUEST_STRUCT_PTR msg_ptr;
   
   /* allocate message pool */
   /* num_messages must be fit in 16 bits */
   usb_rtos_host_state.host_task_state.msg_pool_id = _msgpool_create (sizeof (USB_RTOS_HOST_REQUEST_UNION), (uint_16)num_messages, 0, 0);
   
   if (usb_rtos_host_state.host_task_state.msg_pool_id == MSGPOOL_NULL_POOL_ID)
   {
      return;
   }

   /* allocate message queue used by the USB OHCI interrupt task */
   status = _usb_rtos_msgq_open (sizeof (USB_RTOS_HOST_REQUEST_UNION), 0,
                                 (USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.host_task_state.msg_queue);

   if (status != USB_OK)
   {
      _msgpool_destroy (usb_rtos_host_state.host_task_state.msg_pool_id);

      return;
   }

   while (1)
   {
      status = _usb_rtos_msgq_receive ((USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.host_task_state.msg_queue,
                                    0,
                                    (pointer _PTR_)&msg_ptr);
      
      if (status != USB_OK)
      {
         break;
      }

      switch (msg_ptr->request_type)
      {
         case USB_HOST_REQUEST_CALLBACK:
         {
            (*((USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT_PTR)msg_ptr)->func_ptr)
                     (((USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT_PTR)msg_ptr)->param0,
                      ((USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT_PTR)msg_ptr)->param1,
                      ((USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT_PTR)msg_ptr)->param2,
                      ((USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT_PTR)msg_ptr)->param3);
            break;
         }
         
         default:
         {
            status = _usb_rtos_msgq_close ((USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.host_task_state.msg_queue);

            if (status != TRUE)
            {
            }

            return;
         }
      }

       /* free the message */
      _msg_free (msg_ptr);
   }

   status = _usb_rtos_msgq_close ((USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.host_task_state.msg_queue);

   if (status != TRUE)
   {
   }

   return;
}

static void _usb_rtos_host_io_task
(
   uint_32  dev_inst_ptr
)
{
   _mqx_uint status;
   USB_RTOS_HOST_IO_MSG_STRUCT_PTR msg_ptr;
   DEV_INSTANCE_PTR dev_instance_ptr = (DEV_INSTANCE_PTR)dev_inst_ptr;
   
   /* allocate message pool */
   usb_rtos_host_state.io_task_state.msg_pool_id = _msgpool_create (sizeof (USB_RTOS_HOST_IO_MSG_STRUCT), USB_RTOS_HOST_IO_TASK_NUM_MESSAGES, 0, 0);
   
   if (usb_rtos_host_state.io_task_state.msg_pool_id == MSGPOOL_NULL_POOL_ID)
   {
      return;
   }

   /* allocate message queue used by the device's task */
   status = _usb_rtos_msgq_open (sizeof (USB_RTOS_HOST_IO_MSG_STRUCT), 0,
                                 (USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.io_task_state.msg_queue);

   if (status != USB_OK)
   {
      _msgpool_destroy (usb_rtos_host_state.io_task_state.msg_pool_id);

      return;
   }

   while (1)
   {
      status = _usb_rtos_msgq_receive ((USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.io_task_state.msg_queue,
                                    0,
                                    (pointer _PTR_)&msg_ptr);
      
      if (status != USB_OK)
      {
         break;
      }
      
      (*msg_ptr->func_ptr) (msg_ptr->param0, msg_ptr->param1, msg_ptr->param2, msg_ptr->param3);
      
       /* free the message */
      _msg_free (msg_ptr);
   }

   status = _usb_rtos_msgq_close ((USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.io_task_state.msg_queue);

   if (status != TRUE)
   {
   }

   return;
}

static USB_STATUS _usb_rtos_host_task_create
(
   void
)
{
   USB_STATUS status;
   TASK_TEMPLATE_STRUCT task_template;

   /* create task for processing interrupt deferred work */
   task_template.TASK_TEMPLATE_INDEX = USB_RTOS_HOST_TASK_TEMPLATE_INDEX;
   task_template.TASK_ADDRESS = USB_RTOS_HOST_TASK_ADDRESS;
   task_template.TASK_STACKSIZE = USB_RTOS_HOST_TASK_STACKSIZE;
   task_template.TASK_PRIORITY = USB_RTOS_HOST_TASK_PRIORITY;
   task_template.TASK_NAME = USB_RTOS_HOST_TASK_NAME;
   task_template.TASK_ATTRIBUTES = USB_RTOS_HOST_TASK_ATTRIBUTES;
   task_template.CREATION_PARAMETER = USB_RTOS_HOST_TASK_CREATION_PARAMETER;
   task_template.DEFAULT_TIME_SLICE = USB_RTOS_HOST_TASK_DEFAULT_TIME_SLICE;

   status = _usb_rtos_task_create (0, 0, (uint_32)&task_template,
                                    (USB_RTOS_TASK_STRUCT_PTR)&usb_rtos_host_state.host_task_state.task);

   if (status != USB_OK)
   {
      return USB_log_error(__FILE__,__LINE__,status);
   }

   return USB_OK;
}

static USB_STATUS _usb_rtos_host_io_task_create
(
   void
)
{
   USB_STATUS status;
   TASK_TEMPLATE_STRUCT task_template;

   /* create task for processing interrupt deferred work */
   task_template.TASK_TEMPLATE_INDEX = USB_RTOS_HOST_IO_TASK_TEMPLATE_INDEX;
   task_template.TASK_ADDRESS = USB_RTOS_HOST_IO_TASK_ADDRESS;
   task_template.TASK_STACKSIZE = USB_RTOS_HOST_IO_TASK_STACKSIZE;
   task_template.TASK_PRIORITY = USB_RTOS_HOST_IO_TASK_PRIORITY;
   task_template.TASK_NAME = USB_RTOS_HOST_IO_TASK_NAME;
   task_template.TASK_ATTRIBUTES = USB_RTOS_HOST_IO_TASK_ATTRIBUTES;
   task_template.CREATION_PARAMETER = USB_RTOS_HOST_IO_TASK_CREATION_PARAMETER;
   task_template.DEFAULT_TIME_SLICE = USB_RTOS_HOST_IO_TASK_DEFAULT_TIME_SLICE;

   status = _usb_rtos_task_create (0, 0, (uint_32)&task_template,
                                    (USB_RTOS_TASK_STRUCT_PTR)&usb_rtos_host_state.io_task_state.task);

   if (status != USB_OK)
   {
      return USB_log_error(__FILE__,__LINE__,status);
   }

   return USB_OK;
}

USB_STATUS _usb_rtos_host_task_destroy
(
   void
)
{
   _mqx_uint status;

   /* prevent interrupts first */
   _msgpool_destroy (usb_rtos_host_state.host_task_state.msg_pool_id);

   /* destroy _usb_host_task first */
   _usb_rtos_msgq_close ((USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.host_task_state.msg_queue);
   
   status = _usb_rtos_task_destroy ((USB_RTOS_TASK_STRUCT_PTR)&usb_rtos_host_state.host_task_state.task);
   
   if (status != MQX_OK)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }
   
   return USB_OK;
}

USB_STATUS _usb_rtos_host_io_task_destroy
(
   void
)
{
   _mqx_uint status;

   /* prevent interrupts first */
   _msgpool_destroy (usb_rtos_host_state.io_task_state.msg_pool_id);

   /* destroy _usb_host_task first */
   _usb_rtos_msgq_close ((USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.io_task_state.msg_queue);
   
   status = _usb_rtos_task_destroy ((USB_RTOS_TASK_STRUCT_PTR)&usb_rtos_host_state.io_task_state.task);
   
   if (status != MQX_OK)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }
   
   return USB_OK;
}

USB_STATUS _usb_rtos_host_shutdown
(
   void
)
{
   USB_STATUS status;
   
   status = _usb_rtos_host_task_destroy ();
   
   if (status != USB_OK)
   {
   }

   status = _usb_rtos_host_io_task_destroy ();
   
   if (status != USB_OK)
   {
   }
   
   return USB_OK;
}

USB_STATUS _usb_rtos_host_init
(
   void
)
{
   USB_STATUS status;

   status = _usb_rtos_host_task_create ();
   
   if (status != USB_OK)
   {
      return USB_log_error(__FILE__,__LINE__,status);
   }

   status = _usb_rtos_host_io_task_create ();
   
   if (status != USB_OK)
   {
      _usb_rtos_host_task_destroy ();
      return USB_log_error(__FILE__,__LINE__,status);
   }
   
   return USB_OK;
}

USB_STATUS _usb_rtos_host_task_offload
(
   USB_STATUS (*func_ptr) (void * param0, void * param1, void * param2, void * param3),
   void * param0,
   void * param1,
   void * param2,
   void * param3
)
{
   USB_STATUS status;
   USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT_PTR msg_ptr;

   msg_ptr = (USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT_PTR)_msg_alloc (usb_rtos_host_state.host_task_state.msg_pool_id);

   if (msg_ptr == NULL)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }

   msg_ptr->request_hdr.request_type = USB_HOST_REQUEST_CALLBACK;

   msg_ptr->func_ptr = func_ptr;
   msg_ptr->param0   = param0;
   msg_ptr->param1   = param1;
   msg_ptr->param2   = param2;
   msg_ptr->param3   = param3;

   status = _usb_rtos_msgq_send ((USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.host_task_state.msg_queue,
                              msg_ptr, sizeof (USB_RTOS_HOST_REQUEST_CALLBACK_STRUCT));

   if (status != USB_OK)
   {
      _msg_free (msg_ptr);
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }
   
   return USB_OK;
}

USB_STATUS _usb_rtos_host_io_task_offload
(
   USB_STATUS (*func_ptr) (void * param0, void * param1, void * param2, void * param3),
   void * param0,
   void * param1,
   void * param2,
   void * param3
)
{
   USB_STATUS status;
   USB_RTOS_HOST_IO_MSG_STRUCT_PTR msg_ptr;

   msg_ptr = (USB_RTOS_HOST_IO_MSG_STRUCT_PTR)_msg_alloc (usb_rtos_host_state.io_task_state.msg_pool_id);

   if (msg_ptr == NULL)
   {
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }

   msg_ptr->msg_hdr.SOURCE_QID = 0;
   msg_ptr->msg_hdr.TARGET_QID = usb_rtos_host_state.io_task_state.msg_queue.msg_queue_id;
   msg_ptr->msg_hdr.SIZE = sizeof (USB_RTOS_HOST_IO_MSG_STRUCT);

   msg_ptr->func_ptr = func_ptr;
   msg_ptr->param0   = param0;
   msg_ptr->param1   = param1;
   msg_ptr->param2   = param2;
   msg_ptr->param3   = param3;

   status = _usb_rtos_msgq_send ((USB_RTOS_MSG_QUEUE_STRUCT_PTR)&usb_rtos_host_state.io_task_state.msg_queue,
                              msg_ptr, sizeof (USB_RTOS_HOST_IO_MSG_STRUCT));

   if (status != USB_OK)
   {
      _msg_free (msg_ptr);
      return USB_log_error(__FILE__,__LINE__,USBERR_UNKNOWN_ERROR);
   }
   
   return USB_OK;
}

#endif //(!defined __USB_HOST_KHCI__ && !defined __USB_HOST_EHCI__)

uint_32 USB_log_error_internal(char_ptr file, uint_32 line, uint_32 error)
{
   if ((error != USB_OK) && (error != USB_STATUS_TRANSFER_QUEUED)) {
   }
   return error;
}
uint_32 USB_set_error_ptr(char_ptr file, uint_32 line, uint_32_ptr error_ptr, uint_32 error)
{
   if ((error != USB_OK) && (error != USB_STATUS_TRANSFER_QUEUED)) {
      USB_log_error(file, line, error);
   }
   if (error_ptr != NULL) {
      *error_ptr = error;
   }
   return error;
}
