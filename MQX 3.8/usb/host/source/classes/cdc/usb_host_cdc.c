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
* $FileName: usb_host_cdc.c$
* $Version : 3.8.14.2$
* $Date    : Nov-30-2011$
*
* Comments:
*
*   This file Contains the implementation of class driver for CDC devices.
*
*END************************************************************************/

#include "usb_host_cdc.h"

static void usb_class_cdc_cntrl_callback(pointer, pointer, uchar_ptr, uint_32, USB_STATUS  status);
static USB_STATUS usb_class_cdc_cntrl_common(CDC_COMMAND_PTR, USB_ACM_CLASS_INTF_STRUCT_PTR, uint_8, uint_8, uint_16, uint_16, uchar_ptr);
static void usb_class_cdc_int_acm_callback(_usb_pipe_handle, pointer, pointer, uint_32, uint_32);
static void usb_class_cdc_ctrl_acm_callback(_usb_pipe_handle, pointer, pointer, uint_32, uint_32);
static void usb_class_cdc_in_data_callback(_usb_pipe_handle, pointer, pointer, uint_32, uint_32);

static _mqx_int _io_cdc_serial_char_avail(MQX_FILE_PTR fd_ptr);
static _mqx_int _io_cdc_serial_open(MQX_FILE_PTR, char _PTR_, char _PTR_);
static _mqx_int _io_cdc_serial_close(MQX_FILE_PTR);
static _mqx_int _io_cdc_serial_read(MQX_FILE_PTR, char_ptr, _mqx_int);
static _mqx_int _io_cdc_serial_write(MQX_FILE_PTR, char_ptr, _mqx_int);
static _mqx_int _io_cdc_serial_ioctl(MQX_FILE_PTR, _mqx_uint, pointer);
static _mqx_int _io_cdc_serial_uninstall(IO_DEVICE_STRUCT_PTR);

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_cdc_cntrl_callback
* Returned Value : USB_OK if command has been passed on USB.
* Comments       :
*     This is the callback used when ACM information is sent or received.
*
*END*--------------------------------------------------------------------*/

static void usb_class_cdc_cntrl_callback
   (
      /* [IN] Unused */
      pointer     pipe,
      /* [IN] Pointer to the class interface instance */
      pointer     param,
      /* [IN] Data buffer */
      uchar_ptr   buffer,
      /* [IN] Length of buffer */
      uint_32     len,
      /* [IN] Error code (if any) */
      USB_STATUS  status
   )
{ /* Body */
    USB_ACM_CLASS_INTF_STRUCT_PTR      if_ptr;
 
    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_cntrl_callback");
    #endif
 
    /*
    ** There is no need for USB_lock in the callback function, and there is also no
    ** need to check if the device is still attached (otherwise this callback
    ** would never have been called!
    */
 
    /* Get class interface handle, reset IN_SETUP and call callback */
    if_ptr = (USB_ACM_CLASS_INTF_STRUCT_PTR)param;
 
    if (if_ptr->CDC_G.USER_CALLBACK) {
        if_ptr->CDC_G.USER_CALLBACK(pipe, if_ptr->CDC_G.USER_PARAM, buffer, len, status);
    } /* Endif */
 
    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_cntrl_callback, SUCCESSFUL");
    #endif
} /* Endbody */


/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_cdc_cntrl_common
* Returned Value : USB_OK if command has been passed on USB.
* Comments       :
*     This function is used to send a control request.
*     It must be run with USB locked.
*
*END*--------------------------------------------------------------------*/

static USB_STATUS usb_class_cdc_cntrl_common
   (
      /* [IN] The communication device common command structure */
      CDC_COMMAND_PTR           com_ptr,
      /* [IN] The communication device control interface */
      USB_ACM_CLASS_INTF_STRUCT_PTR if_ctrl_ptr,
      /* [IN] Bitmask of the request type */
      uint_8                    bmrequesttype,
      /* [IN] Request code */
      uint_8                    brequest,
      /* [IN] Value to copy into WVALUE field of the REQUEST */
      uint_16                   wvalue,
      /* [IN] Length of the data associated with REQUEST */
      uint_16                   wlength,
      /* [IN] Pointer to data buffer used to send/recv */
      uchar_ptr                 data
   )
{ /* Body */
    USB_CDC_GENERAL_CLASS_PTR        if_ptr;
    USB_SETUP                        req;
    USB_STATUS                       status = USBERR_NO_INTERFACE;
 
    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_cntrl_common");
    #endif
    
    if_ptr = (USB_CDC_GENERAL_CLASS_PTR) com_ptr->CLASS_PTR->class_intf_handle;
    
    /* Save the higher level callback and ID */
    if_ptr->USER_CALLBACK = com_ptr->CALLBACK_FN;
    if_ptr->USER_PARAM = com_ptr->CALLBACK_PARAM;
 
    /* Setup the request */
    req.BMREQUESTTYPE = bmrequesttype;
    req.BREQUEST = brequest;
    *(uint_16*)req.WINDEX = HOST_TO_LE_SHORT(if_ctrl_ptr->CDC_G.IFNUM); /* number of control interface */
    *(uint_16*)req.WVALUE = HOST_TO_LE_SHORT(wvalue);
    *(uint_16*)req.WLENGTH = HOST_TO_LE_SHORT(wlength);
    
    /* Since this function is general, we must distinguish here interface class to perform
    ** appropriate action.
    ** Only ACM devices are supported now.
    */
    switch (((INTERFACE_DESCRIPTOR_PTR) if_ctrl_ptr->CDC_G.G.intf_handle)->bInterfaceSubClass) 
    {
        case USB_SUBCLASS_COM_DIRECT:
            break;
        case USB_SUBCLASS_COM_ABSTRACT:
            if (USB_STATUS_TRANSFER_QUEUED == (status = _usb_hostdev_cntrl_request(if_ptr->G.dev_handle, &req, data,
                usb_class_cdc_cntrl_callback, if_ptr)))
                status = USB_OK;
            break;
        case USB_SUBCLASS_COM_TELEPHONE:
            break;
        case USB_SUBCLASS_COM_MULTICHAN:
            break;
        case USB_SUBCLASS_COM_CAPI:
            break;
        case USB_SUBCLASS_COM_ETHERNET:
            break;
        case USB_SUBCLASS_COM_ATM_NET:
            break;  
    }
 
    #ifdef _HOST_DEBUG_
    if (!status)
        DEBUG_LOG_TRACE("usb_class_cdc_cntrl_common, SUCCESSFUL");
    else
        DEBUG_LOG_TRACE("usb_class_cdc_cntrl_common, FAILED");
    #endif
    
    return status;
} /* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_cdc_get_acm_line_coding
* Returned Value : Success as USB_OK
* Comments       :
*     This function is used to get parameters of current line (baudrate,
*     HW control...)
*     This function cannot be run in ISR
* NOTE!!!
*     DATA instance communication structure is passed here as parameter,
*     not control interface.
*
*END*--------------------------------------------------------------------*/

USB_STATUS usb_class_cdc_get_acm_line_coding
   (
      /* [IN] The communication device data instance structure */
      CLASS_CALL_STRUCT_PTR            ccs_ptr,
      /* [IN] Where to store coding */
      USB_CDC_UART_CODING_PTR          uart_coding_ptr
   )
{ /* Body */
    USB_DATA_CLASS_INTF_STRUCT_PTR if_data_ptr;
    USB_ACM_CLASS_INTF_STRUCT_PTR  if_acm_ptr;
    CLASS_CALL_STRUCT_PTR          acm_instance;
    USB_STATUS                     status = USBERR_NO_INTERFACE;
    LWEVENT_STRUCT_PTR             event;
    CDC_COMMAND                    cmd;
 
    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_get_acm_line_coding");
    #endif
 
    USB_lock();
    /* Validity checking: for data interface */
    if (usb_host_class_intf_validate(ccs_ptr)) {
        if_data_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR) ccs_ptr->class_intf_handle;
        acm_instance = if_data_ptr->BOUND_CONTROL_INTERFACE;
  
        if (usb_host_class_intf_validate(if_data_ptr->BOUND_CONTROL_INTERFACE)) {
            if_acm_ptr = (USB_ACM_CLASS_INTF_STRUCT_PTR) if_data_ptr->BOUND_CONTROL_INTERFACE->class_intf_handle;
         
            if (if_acm_ptr->acm_desc->bmCapabilities & USB_ACM_CAP_LINE_CODING) {
                event = &if_acm_ptr->acm_event;

                USB_unlock(); /* we must wait until ctrl pipe is used */

                _lwevent_wait_ticks(event, USB_ACM_CTRL_PIPE_FREE | USB_ACM_DETACH, FALSE, 0);

                USB_lock();
                if (usb_host_class_intf_validate(acm_instance)) {
                    cmd.CLASS_PTR = ccs_ptr;
                    cmd.CALLBACK_FN = (tr_callback) usb_class_cdc_ctrl_acm_callback;
                    cmd.CALLBACK_PARAM = acm_instance;
                    status = usb_class_cdc_cntrl_common(&cmd, if_acm_ptr,
                        REQ_TYPE_IN | REQ_TYPE_CLASS | REQ_TYPE_INTERFACE,
                        USB_CDC_GET_LINE_CODING, 0, 7 /* sizeof(USB_CDC_UART_CODING)*/, (uchar_ptr) uart_coding_ptr);
                }
            }
            else
               status = USBERR_INVALID_BMREQ_TYPE;
        } /* Endif */
    } /* Endif */

    USB_unlock();

    /* This piece of code cannot be executed if it is running in USB interrupt context */
    if (!status) {
        /* wait for command completion */
        _lwevent_wait_ticks(event, USB_ACM_CTRL_PIPE_FREE | USB_ACM_DETACH, FALSE, 0);
        /* and unlock control pipe as it is automatic event */
        _lwevent_set(event, USB_ACM_CTRL_PIPE_FREE);
    }

    #ifdef _HOST_DEBUG_
    if (!status)
        DEBUG_LOG_TRACE("usb_class_cdc_get_acm_line_coding, SUCCESSFUL");
    else
        DEBUG_LOG_TRACE("usb_class_cdc_get_acm_line_coding, FAILED");
    #endif
 
    return status;
} /* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_cdc_set_acm_line_coding
* Returned Value : Success as USB_OK
* Comments       :
*     This function is used to set parameters of current line (baudrate,
*     HW control...)
*     This function cannot be run in ISR
* NOTE!!!
*     DATA instance communication structure is passed here as parameter,
*     not control interface.
*
*END*--------------------------------------------------------------------*/

USB_STATUS usb_class_cdc_set_acm_line_coding
   (
      /* [IN] The communication device data instance structure */
      CLASS_CALL_STRUCT_PTR            ccs_ptr,
      /* [IN] Coding to set */
      USB_CDC_UART_CODING_PTR          uart_coding_ptr
   )
{ /* Body */
    USB_DATA_CLASS_INTF_STRUCT_PTR if_data_ptr;
    USB_ACM_CLASS_INTF_STRUCT_PTR  if_acm_ptr;
    CLASS_CALL_STRUCT_PTR          acm_instance;
    USB_STATUS                     status = USBERR_NO_INTERFACE;
    LWEVENT_STRUCT_PTR             event;
    CDC_COMMAND                    cmd;

    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_set_acm_line_coding");
    #endif

    USB_lock();
    /* Validity checking: for data interface */
    if (usb_host_class_intf_validate(ccs_ptr)) {
        if_data_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR)ccs_ptr->class_intf_handle;
        acm_instance = if_data_ptr->BOUND_CONTROL_INTERFACE;
  
        if (usb_host_class_intf_validate(acm_instance)) {
            if_acm_ptr = (USB_ACM_CLASS_INTF_STRUCT_PTR) if_data_ptr->BOUND_CONTROL_INTERFACE->class_intf_handle;
   
            if (if_acm_ptr->acm_desc->bmCapabilities & USB_ACM_CAP_LINE_CODING) {
                event = &if_acm_ptr->acm_event;
   
                USB_unlock(); /* we must wait until ctrl pipe is used */
   
                _lwevent_wait_ticks(event, USB_ACM_CTRL_PIPE_FREE | USB_ACM_DETACH, FALSE, 0);
   
                USB_lock();
                if (usb_host_class_intf_validate(acm_instance)) {
                    cmd.CLASS_PTR = ccs_ptr;
                    cmd.CALLBACK_FN = (tr_callback) usb_class_cdc_ctrl_acm_callback;
                    cmd.CALLBACK_PARAM = acm_instance;
                    status = usb_class_cdc_cntrl_common(&cmd, if_acm_ptr,
                        REQ_TYPE_OUT | REQ_TYPE_CLASS | REQ_TYPE_INTERFACE,
                        USB_CDC_SET_LINE_CODING, 0, 7 /* sizeof(USB_CDC_UART_CODING)*/, (uchar_ptr) uart_coding_ptr);
                }
            }
            else
               status = USBERR_INVALID_BMREQ_TYPE;
        } /* Endif */
    } /* Endif */
    
    USB_unlock();
 
    /* This piece of code cannot be executed if it is running in USB interrupt context */
    if (!status) {
        /* wait for command completion */
        _lwevent_wait_ticks(event, USB_ACM_CTRL_PIPE_FREE | USB_ACM_DETACH, FALSE, 0);
        /* and unlock control pipe as it is automatic event */
        _lwevent_set(event, USB_ACM_CTRL_PIPE_FREE);
    }

    #ifdef _HOST_DEBUG_
    if (!status)
        DEBUG_LOG_TRACE("usb_class_cdc_set_acm_line_coding, SUCCESSFUL");
    else
        DEBUG_LOG_TRACE("usb_class_cdc_set_acm_line_coding, FAILED");
    #endif
 
    return status;
} /* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_cdc_set_acm_ctrl_state
* Returned Value : Success as USB_OK
* Comments       :
*     This function is used to set parameters of current line (baudrate,
*     HW control...)
*     This function cannot be run in ISR
* NOTE!!!
*     DATA instance communication structure is passed here as parameter,
*     not control interface.
*
*END*--------------------------------------------------------------------*/

USB_STATUS usb_class_cdc_set_acm_ctrl_state
   (
      /* [IN] The communication device data instance structure */
      CLASS_CALL_STRUCT_PTR            ccs_ptr,
      /* [IN] DTR state to set */
      uint_8                           dtr,
      /* [IN] RTS state to set */
      uint_8                           rts
   )
{ /* Body */
    USB_DATA_CLASS_INTF_STRUCT_PTR if_data_ptr;
    USB_ACM_CLASS_INTF_STRUCT_PTR  if_acm_ptr;
    CLASS_CALL_STRUCT_PTR          acm_instance;
    USB_STATUS                     status = USBERR_NO_INTERFACE;
    LWEVENT_STRUCT_PTR             event;
    CDC_COMMAND                    cmd;

    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_set_acm_ctrl_state");
    #endif

    USB_lock();
    /* Validity checking: for data interface */
    if (usb_host_class_intf_validate(ccs_ptr)) {
        if_data_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR)ccs_ptr->class_intf_handle;
        acm_instance = if_data_ptr->BOUND_CONTROL_INTERFACE;
  
        if (usb_host_class_intf_validate(acm_instance)) {
            if_acm_ptr = (USB_ACM_CLASS_INTF_STRUCT_PTR) if_data_ptr->BOUND_CONTROL_INTERFACE->class_intf_handle;
   
            if (if_acm_ptr->acm_desc->bmCapabilities & USB_ACM_CAP_LINE_CODING) {
                event = &if_acm_ptr->acm_event;
   
                USB_unlock(); /* we must wait until ctrl pipe is used */
   
                _lwevent_wait_ticks(event, USB_ACM_CTRL_PIPE_FREE | USB_ACM_DETACH, FALSE, 0);
   
                USB_lock();
                if (usb_host_class_intf_validate(acm_instance)) {
                    cmd.CLASS_PTR = ccs_ptr;
                    cmd.CALLBACK_FN = (tr_callback) usb_class_cdc_ctrl_acm_callback;
                    cmd.CALLBACK_PARAM = acm_instance;
                    
                    if_acm_ptr->ctrl_state.bmStates[1]  = dtr ? USB_ACM_LINE_STATE_DTR : 0;
                    if_acm_ptr->ctrl_state.bmStates[1] |= rts ? USB_ACM_LINE_STATE_RTS : 0;
                    status = usb_class_cdc_cntrl_common(&cmd, if_acm_ptr,
                        REQ_TYPE_OUT | REQ_TYPE_CLASS | REQ_TYPE_INTERFACE,
                        USB_CDC_SET_CTRL_LINE_STATE, * ((uint_16 *) &if_acm_ptr->ctrl_state), 0, NULL);
                }
            }
            else
               status = USBERR_INVALID_BMREQ_TYPE;
        } /* Endif */
    } /* Endif */
    
    USB_unlock();
 
    /* This piece of code cannot be executed if it is running in USB interrupt context */
    if (!status) {
        /* wait for command completion */
        _lwevent_wait_ticks(event, USB_ACM_CTRL_PIPE_FREE | USB_ACM_DETACH, FALSE, 0);
        /* and unlock control pipe as it is automatic event */
        _lwevent_set(event, USB_ACM_CTRL_PIPE_FREE);
    }

    #ifdef _HOST_DEBUG_
    if (!status)
        DEBUG_LOG_TRACE("usb_class_cdc_set_acm_ctrl_state, SUCCESSFUL");
    else
        DEBUG_LOG_TRACE("usb_class_cdc_set_acm_ctrl_state, FAILED");
    #endif
 
    return status;
} /* Endbody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_acm_int_callback
* Returned Value : None
* Comments       :
*     Called on interrupt endpoint data reception.
*
*END*--------------------------------------------------------------------*/

static void usb_class_cdc_int_acm_callback
   (
      /* [IN] pointer to pipe */
      _usb_pipe_handle  pipe,

      /* [IN] user-defined parameter */
      pointer           param,

      /* [IN] buffer address */
      pointer           buffer,

      /* [IN] length of data transferred */
      uint_32           len,

      /* [IN] status, hopefully USB_OK or USB_DONE */
      uint_32           status
   )
{ /* Body */
    CLASS_CALL_STRUCT_PTR         acm_instance = (CLASS_CALL_STRUCT_PTR) param;
//    USB_ACM_CLASS_INTF_STRUCT_PTR if_ptr;
    
    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_int_acm_callback");
    #endif

//    if_ptr = (USB_ACM_CLASS_INTF_STRUCT_PTR) acm_instance->class_intf_handle;
    
    /* we do not use USB_ACM_INT_PIPE_FREE in this version at all */
//    _lwevent_set(&if_ptr->acm_event, USB_ACM_INT_PIPE_FREE); 

    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_int_acm_callback, SUCCESSFUL");
    #endif

} /*EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_acm_ctrl_callback
* Returned Value : None
* Comments       :
*     Called on ctrl endpoint data reception / send
*
*END*--------------------------------------------------------------------*/

static void usb_class_cdc_ctrl_acm_callback
   (
      /* [IN] pointer to pipe */
      _usb_pipe_handle  pipe,

      /* [IN] user-defined parameter */
      pointer           param,

      /* [IN] buffer address */
      pointer           buffer,

      /* [IN] length of data transferred */
      uint_32           len,

      /* [IN] status, hopefully USB_OK or USB_DONE */
      uint_32           status
   )
{ /* Body */
    CLASS_CALL_STRUCT_PTR         acm_instance = (CLASS_CALL_STRUCT_PTR) param;
    USB_ACM_CLASS_INTF_STRUCT_PTR if_ptr;
    
    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_ctrl_acm_callback");
    #endif

    if_ptr = (USB_ACM_CLASS_INTF_STRUCT_PTR) acm_instance->class_intf_handle;

    _lwevent_set(&if_ptr->acm_event, USB_ACM_CTRL_PIPE_FREE); /* mark we are not using control pipe */

    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_ctrl_acm_callback, SUCCESSFUL");
    #endif

} /*EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_data_in_callback
* Returned Value : Success as USB_OK
* Comments       :
*     Called on interrupt endpoint data reception
*END*--------------------------------------------------------------------*/

static void usb_class_cdc_in_data_callback
   (
      /* [IN] pointer to pipe */
      _usb_pipe_handle  pipe,

      /* [IN] user-defined parameter */
      pointer           param,

      /* [IN] buffer address */
      pointer           buffer,

      /* [IN] length of data transferred */
      uint_32           len,

      /* [IN] status, hopefully USB_OK or USB_DONE */
      uint_32           status
   )
{ /* Body */
    MQX_FILE_PTR                   fd_ptr = (MQX_FILE_PTR) param;
    CLASS_CALL_STRUCT_PTR          data_instance = (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_PTR->DRIVER_INIT_PTR;
    USB_DATA_CLASS_INTF_STRUCT_PTR if_ptr;
    CLASS_CALL_STRUCT_PTR          acm_instance;
    USB_ACM_CLASS_INTF_STRUCT_PTR  acm_if_ptr;
    
    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_in_data_callback");
    #endif

    if_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR) data_instance->class_intf_handle;
    acm_instance = if_ptr->BOUND_CONTROL_INTERFACE;

    if ((acm_instance == NULL) || !usb_host_class_intf_validate(acm_instance))
        len = 0;
    else if (((CDC_SERIAL_INIT_PTR) fd_ptr->FLAGS)->FLAGS & USB_UART_HW_FLOW) {
        acm_if_ptr = (USB_ACM_CLASS_INTF_STRUCT_PTR) acm_instance->class_intf_handle;
        /* check the state of DCD signal for HW flow control files */
        if (!(acm_if_ptr->interrupt_buffer.bmStates & USB_ACM_STATE_RX_CARRIER))
           len = 0; /* ignore all received bytes if DCD is not set */
        /* check the state of DTR signal */
        if (!(acm_if_ptr->ctrl_state.bmStates[1] & USB_ACM_LINE_STATE_DTR))
           len = 0; /* ignore all sent bytes if DTR is not set */
    }
    
    /* in the case we have less data than expected, status is not USB_OK, but buffer is not NULL */
    if ((status != USB_OK) && (buffer == NULL)) /* if no data received */
        len = 0;
    if (if_ptr->RX_BUFFER_DRV != NULL)
        if_ptr->RX_BUFFER_DRV += len;
    if_ptr->RX_READ = len;

    _lwevent_set(&if_ptr->data_event, USB_DATA_READ_COMPLETE); /* signal that we have completed transfer on input pipe */

    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_in_data_callback, SUCCESSFUL");
    #endif

} /*EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_data_out_callback
* Returned Value : Success as USB_OK
* Comments       :
*     Called on interrupt endpoint data reception
*END*--------------------------------------------------------------------*/

static void usb_class_cdc_out_data_callback
   (
      /* [IN] pointer to pipe */
      _usb_pipe_handle  pipe,

      /* [IN] user-defined parameter */
      pointer           param,

      /* [IN] buffer address */
      pointer           buffer,

      /* [IN] length of data transferred */
      uint_32           len,

      /* [IN] status, hopefully USB_OK or USB_DONE */
      uint_32           status
   )
{ /* Body */
    MQX_FILE_PTR                   fd_ptr = (MQX_FILE_PTR) param;
    CLASS_CALL_STRUCT_PTR          data_instance = (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_PTR->DRIVER_INIT_PTR;
    USB_DATA_CLASS_INTF_STRUCT_PTR if_ptr;
    CLASS_CALL_STRUCT_PTR          acm_instance;
    USB_ACM_CLASS_INTF_STRUCT_PTR  acm_if_ptr;
    
    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_in_data_callback");
    #endif

    if_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR) data_instance->class_intf_handle;
    acm_instance = if_ptr->BOUND_CONTROL_INTERFACE;

    /* Prepare ACM control interface for future use */
    if ((acm_instance == NULL) || !usb_host_class_intf_validate(acm_instance))
        len = 0;
    else if (((CDC_SERIAL_INIT_PTR) fd_ptr->FLAGS)->FLAGS & USB_UART_HW_FLOW) {
        acm_if_ptr = (USB_ACM_CLASS_INTF_STRUCT_PTR) acm_instance->class_intf_handle;
        /* check the state of DSR signal for HW flow control files */
        if (!(acm_if_ptr->interrupt_buffer.bmStates & USB_ACM_STATE_TX_CARRIER))
        /* check the state of RTS signal */
        if (!(acm_if_ptr->ctrl_state.bmStates[1] & USB_ACM_LINE_STATE_RTS))
           len = 0; /* ignore all sent bytes if RTS is not set */
    }
    
    /* in the case we have less data than expected, status is not USB_OK, but buffer is not NULL */
    if ((status != USB_OK) && (buffer == NULL)) /* if no data received */
        len = 0;
    if_ptr->TX_SENT = len;

    _lwevent_set(&if_ptr->data_event, USB_DATA_SEND_COMPLETE); /* signal that we have completed transfer on output pipe */

    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_in_data_callback, SUCCESSFUL");
    #endif

} /*EndBody */

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_cdc_init_ipipe
* Returned Value : Success as USB_OK, no interrupt pipe (not error!) as 
* Comments       :
*     Starts interrupt endpoint to poll for interrupt on specified device.
*END*--------------------------------------------------------------------*/

USB_STATUS usb_class_cdc_init_ipipe
    (
        /* ACM interface instance */
        CLASS_CALL_STRUCT_PTR      acm_instance
    )
{ /* Body */
    TR_INIT_PARAM_STRUCT           tr;
    USB_STATUS                     status = USBERR_NO_INTERFACE;
    USB_ACM_CLASS_INTF_STRUCT_PTR  if_ptr;

    #ifdef _HOST_DEBUG_
       DEBUG_LOG_TRACE("usb_class_cdc_init_ipipe");
    #endif

    USB_lock();
    /* Validity checking, always needed when passing data to lower API */
    if (usb_host_class_intf_validate(acm_instance)) {
        if_ptr = (USB_ACM_CLASS_INTF_STRUCT_PTR) acm_instance->class_intf_handle;
    
        if (if_ptr->interrupt_pipe != NULL) {
            usb_hostdev_tr_init(&tr, (tr_callback) usb_class_cdc_int_acm_callback, (pointer) acm_instance);
            tr.RX_BUFFER = (uchar_ptr) &if_ptr->interrupt_buffer;
            tr.RX_LENGTH = sizeof(if_ptr->interrupt_buffer);
            
            if (USB_STATUS_TRANSFER_QUEUED == (status = _usb_host_recv_data(
                if_ptr->CDC_G.G.host_handle,
                if_ptr->interrupt_pipe,
                &tr
            )))
            {
                status = USB_OK;
            }
        }
        else
            status = USBERR_OPEN_PIPE_FAILED;
    }

    USB_unlock();

    #ifdef _HOST_DEBUG_
    if (!status)
         DEBUG_LOG_TRACE("usb_class_cdc_init_ipipe, SUCCESSFUL");
    else
         DEBUG_LOG_TRACE("usb_class_cdc_init_ipipe, FAILED");
    #endif
   
    return status;
}

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_cdc_install_driver
* Returned Value : Success as USB_OK
* Comments       :
*     Adds / installs USB serial device driver to the MQX drivers
*END*--------------------------------------------------------------------*/

USB_STATUS usb_class_cdc_install_driver
    (
        CLASS_CALL_STRUCT_PTR    data_instance,
        char_ptr                 device_name
    )
{
    USB_DATA_CLASS_INTF_STRUCT_PTR  if_ptr;
    USB_STATUS      status = USBERR_NO_INTERFACE;

    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_install_driver");
    #endif
    
    USB_lock();
    /* Validity checking, always needed when passing data to lower API */
    if (usb_host_class_intf_validate(data_instance)) {
        if_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR) data_instance->class_intf_handle;
    
        status = _io_dev_install_ext(device_name, 
            _io_cdc_serial_open,
            _io_cdc_serial_close,
            _io_cdc_serial_read,
            _io_cdc_serial_write,
            _io_cdc_serial_ioctl,
            _io_cdc_serial_uninstall,
            data_instance); /* pass a parameter to the driver to associate this driver with data instance */
        if (status == IO_OK)
            if_ptr->device_name = device_name;
    }
    
    USB_unlock();

    #ifdef _HOST_DEBUG_
    if (!status)
         DEBUG_LOG_TRACE("usb_class_cdc_install_driver, SUCCESSFUL");
    else
         DEBUG_LOG_TRACE("usb_class_cdc_install_driver, FAILED");
    #endif

    return status;
}

/*FUNCTION*----------------------------------------------------------------
*
* Function Name  : usb_class_acm_uninstall_driver
* Returned Value : Success as USB_OK
* Comments       :
*     Removes USB serial device driver from the MQX drivers
*END*--------------------------------------------------------------------*/

USB_STATUS usb_class_cdc_uninstall_driver
    (
        CLASS_CALL_STRUCT_PTR data_instance
    )
{
    USB_DATA_CLASS_INTF_STRUCT_PTR  if_ptr;
    USB_STATUS      status = USBERR_NO_INTERFACE;

    #ifdef _HOST_DEBUG_
        DEBUG_LOG_TRACE("usb_class_cdc_uninstall_driver");
    #endif

    USB_lock();
    /* Validity checking, always needed when passing data to lower API */
    if (usb_host_class_intf_validate(data_instance)) {
        if_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR) data_instance->class_intf_handle;
    
        status = _io_dev_uninstall(if_ptr->device_name);
    }

    USB_unlock();

    #ifdef _HOST_DEBUG_
    if (!status)
         DEBUG_LOG_TRACE("usb_class_acm_uninstall_driver, SUCCESSFUL");
    else
         DEBUG_LOG_TRACE("usb_class_acm_uninstall_driver, FAILED");
    #endif

    return status;
}

/*FUNCTION****************************************************************
*
* Function Name    : _io_cdc_serial_open
* Returned Value   : _mqx_int error code
* Comments         :
*    This routine initializes a USB serial I/O channel. It acquires
*    memory, then stores information into it about the channel.
*    It then calls the hardware interface function to initialize the channel.
*
*END**********************************************************************/

static _mqx_int _io_cdc_serial_open
   (
      /* [IN] the file handle for the device being opened */
      MQX_FILE_PTR fd_ptr,

      /* [IN] the remaining portion of the name of the device */
      char       _PTR_ open_name_ptr,

      /* [IN] the flags to be used during operation:
      ** echo, translation, xon/xoff, encoded into a pointer.
      */
      char       _PTR_ flags
   )
{ /* Body */
    if (NULL == (pointer) (fd_ptr->FLAGS = (_mqx_uint) flags)) {
        fd_ptr->ERROR = MQX_INVALID_PARAMETER;
        return IO_ERROR;
    }
    
    if (((CDC_SERIAL_INIT_PTR) fd_ptr->FLAGS)->FLAGS & USB_UART_HW_FLOW) {
        /* HW flow control? If yes, then from our side we dont have any problem to send
        ** data any time. Inform device that we are prepared to send.
        */
        if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_set_acm_ctrl_state(
            (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_PTR->DRIVER_INIT_PTR,
            1, /* DTR set on */
            1  /* RTS set on */
        )))
            return IO_ERROR;
    }

    return IO_OK;
} /* Endbody */


/*FUNCTION****************************************************************
*
* Function Name    : _io_cdc_serial_close
* Returned Value   : _mqx_int error code
* Comments         :
*    This routine closes the USB serial I/O channel.
*
*END**********************************************************************/

static _mqx_int _io_cdc_serial_close
   (
      /* [IN] the file handle for the device being closed */
      MQX_FILE_PTR fd_ptr
   )
{ /* Body */
    return IO_OK;
}

/*FUNCTION****************************************************************
*
* Function Name    : _io_cdc_serial_read
* Returned Value   : _mqx_int number of characters read
* Comments         :
*    This routine reads characters from the USB serial I/O channel
*    device, converting carriage return ('\r') characters to newlines,
*    and then echoing the input characters.
*
*END*********************************************************************/

static _mqx_int _io_cdc_serial_read
   (
      /* [IN] the file handle for the device */
      MQX_FILE_PTR fd_ptr,

      /* [IN] where the characters are to be stored */
      char_ptr   data_ptr,

      /* [IN] the number of characters to input */
      _mqx_int   num
   )
{ /* Body */
    CLASS_CALL_STRUCT_PTR          data_instance;
    USB_DATA_CLASS_INTF_STRUCT_PTR if_ptr;
    TR_INIT_PARAM_STRUCT           tr;
    USB_STATUS                     status;
    _mqx_int                       num_left, block_len;
    LWEVENT_STRUCT_PTR             event;
    
    if (0 == (num_left = num))
       return 0;
    
    data_instance = (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_PTR->DRIVER_INIT_PTR;

    USB_lock();
    /* Validity checking, always needed when passing data to lower API */
    if (!usb_host_class_intf_validate(data_instance)) {
        USB_unlock();
        fd_ptr->ERROR = USBERR_NO_INTERFACE;
        return IO_ERROR;
    }
    if_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR) data_instance->class_intf_handle;
    
    /* Does the device use IN pipe? */
    if (if_ptr->in_pipe == NULL) {
        USB_unlock();
        fd_ptr->ERROR = USBERR_OPEN_PIPE_FAILED;
        return IO_ERROR;
    }
    
    block_len = if_ptr->RX_BUFFER_SIZE;
    event = &if_ptr->data_event;

    /* If we have USB_UART_NO_BLOCKING, we want to be sure that we will not be waiting for the pipe.
    ** We just check if the pipe is busy (event USB_DATA_READ_PIPE_FREE is not set). If it is, we should return,
    ** There is no other way to check the status of event without waiting than to access its structure.
    ** Known issue:
    ** This approach takes the risk that the 2 tasks access USB_UART_NO_BLOCKING file, then both can read "free pipe"
    ** and next on waiting for USB_DATA_READ_PIPE_FREE one of them takes the event and the other one waits blocked
    ** until the pipe is free (and waiting is not accepted with USB_UART_NO_BLOCKING).
    */
    if ((((CDC_SERIAL_INIT_PTR) fd_ptr->FLAGS)->FLAGS & USB_UART_NO_BLOCKING) && !(event->VALUE & USB_DATA_READ_PIPE_FREE)) {
        USB_unlock();
        return 0;
    }
    
    USB_unlock();
    
    _lwevent_wait_ticks(event, USB_DATA_READ_PIPE_FREE | USB_DATA_DETACH, FALSE, 0);
    USB_lock();
    /* Validity checking, always needed when passing data to lower API */
    if (!usb_host_class_intf_validate(data_instance)) {
        USB_unlock();
        fd_ptr->ERROR = USBERR_NO_INTERFACE;
        return IO_ERROR;
    }

    while (if_ptr->RX_BUFFER_APP < if_ptr->RX_BUFFER_DRV) {
        *data_ptr = *if_ptr->RX_BUFFER_APP++;
        num_left--;
        if (!num_left)
            break;
    }
    
    while (num_left) {
        usb_hostdev_tr_init(&tr, (tr_callback) usb_class_cdc_in_data_callback, (pointer) fd_ptr);
        if (num_left > block_len) {
            tr.RX_BUFFER = (uchar_ptr) data_ptr;
            tr.RX_LENGTH = block_len * (num_left / block_len);
            if_ptr->RX_BUFFER_DRV = NULL;
        }
        else {
            /* last packet must be always written to the local buffer, because device can send more bytes than we need to read */
            if_ptr->RX_BUFFER_APP = if_ptr->RX_BUFFER;
            if_ptr->RX_BUFFER_DRV = if_ptr->RX_BUFFER;
            tr.RX_BUFFER = (uchar_ptr) if_ptr->RX_BUFFER;
            tr.RX_LENGTH = block_len;
        }

        _lwevent_clear(event, USB_DATA_READ_COMPLETE);
        status = _usb_host_recv_data(if_ptr->CDC_G.G.host_handle, if_ptr->in_pipe, &tr);
        if (status == USB_STATUS_TRANSFER_QUEUED) {
    
            USB_unlock();
            /* serialize requests by waiting for pipe */
            /* wait while data will be available */
            _lwevent_wait_ticks(event, USB_DATA_READ_COMPLETE | USB_DATA_DETACH, FALSE, 0);
            
            USB_lock();
            
            /* Validity checking, always needed when passing data to lower API */
            if (!usb_host_class_intf_validate(data_instance)) {
                USB_unlock();
                fd_ptr->ERROR = USBERR_NO_INTERFACE;
                return IO_ERROR;
            }
            
            if (tr.RX_BUFFER != (uchar_ptr) data_ptr) {
                while ((if_ptr->RX_BUFFER_APP < if_ptr->RX_BUFFER_DRV) && num_left) {
                    *data_ptr++ = *if_ptr->RX_BUFFER_APP++;
                    num_left--;
                }
            }
            else {
                data_ptr += if_ptr->RX_READ;
                num_left -= if_ptr->RX_READ;
            }
           
            if (num_left && !(((CDC_SERIAL_INIT_PTR) fd_ptr->FLAGS)->FLAGS & USB_UART_NO_BLOCKING)) {
                /* wait 10 ticks to next read */
                USB_unlock();
                _lwevent_wait_ticks(event, USB_DATA_DETACH, FALSE, 10);
                USB_lock();
                /* Validity checking, always needed when passing data to lower API */
                if (!usb_host_class_intf_validate(data_instance)) {
                    USB_unlock();
                    fd_ptr->ERROR = USBERR_NO_INTERFACE;
                    return IO_ERROR;
                }
            }
            else
                break;
        }
    }
    
read_done:
    _lwevent_set(event, USB_DATA_READ_PIPE_FREE); /* mark we are not using input pipe */

    USB_unlock();
    
    return num - num_left;

} /* Endbody */

/*FUNCTION****************************************************************
*
* Function Name    : _io_cdc_serial_char_avail
* Returned Value   : TRUE if char is available, FALSE if not
* Comments         :
*    This routine reads characters from the USB serial I/O channel
*    device, converting carriage return ('\r') characters to newlines,
*    and then echoing the input characters.
*
*END*********************************************************************/

static _mqx_int _io_cdc_serial_char_avail
   (
      /* [IN] the file handle for the device */
      MQX_FILE_PTR fd_ptr
   )
{ /* Body */
    CLASS_CALL_STRUCT_PTR          data_instance;
    USB_DATA_CLASS_INTF_STRUCT_PTR if_ptr;
    TR_INIT_PARAM_STRUCT           tr;
    USB_STATUS                     status;
    LWEVENT_STRUCT_PTR             event;
    
    data_instance = (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_PTR->DRIVER_INIT_PTR;

    USB_lock();
    /* Validity checking, always needed when passing data to lower API */
    if (!usb_host_class_intf_validate(data_instance)) {
        USB_unlock();
        return USBERR_NO_INTERFACE;
    }
    if_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR) data_instance->class_intf_handle;

    if (if_ptr->in_pipe == NULL) {
        USB_unlock();
        fd_ptr->ERROR = USBERR_OPEN_PIPE_FAILED;
        return IO_ERROR;
    }

    event = &if_ptr->data_event;

    if (!(event->VALUE & USB_DATA_READ_PIPE_FREE)) {
        /* any other task is reading => no character available for us now */
        USB_unlock();
        return FALSE;
    }
    if (if_ptr->RX_BUFFER_APP != if_ptr->RX_BUFFER_DRV) {
        /* cache is not empty => char is available */
        USB_unlock();
        return TRUE;
    }
    
    /* following command never waits as USB_DATA_READ_PIPE_FREE is set (tested above) */
    _lwevent_wait_ticks(event, USB_DATA_READ_PIPE_FREE, FALSE, 0);
    
    /* We are going to try reading data. We will see if some data is available or not. */
    
    usb_hostdev_tr_init(&tr, (tr_callback) usb_class_cdc_in_data_callback, (pointer) fd_ptr);
    if_ptr->RX_BUFFER_APP = if_ptr->RX_BUFFER;
    if_ptr->RX_BUFFER_DRV = if_ptr->RX_BUFFER;
    tr.RX_BUFFER = (uchar_ptr) if_ptr->RX_BUFFER;
    tr.RX_LENGTH = if_ptr->RX_BUFFER_SIZE;

    _lwevent_clear(event, USB_DATA_READ_COMPLETE);
    status = _usb_host_recv_data(if_ptr->CDC_G.G.host_handle, if_ptr->in_pipe, &tr);

    if (status == USB_STATUS_TRANSFER_QUEUED) {
        USB_unlock();
        /* serialize requests by waiting for pipe */
        /* wait while data will be available */
        _lwevent_wait_ticks(event, USB_DATA_READ_COMPLETE | USB_DATA_DETACH, FALSE, 0);
            
        USB_lock();
        /* Validity checking, always needed when passing data to lower API */
        if (!usb_host_class_intf_validate(data_instance)) {
            USB_unlock();
            return FALSE;
        }
        
        status = (if_ptr->RX_READ != 0 ? TRUE : FALSE);
    }
    else
        status = FALSE;
    
    _lwevent_set(event, USB_DATA_READ_PIPE_FREE); /* mark we are not using input pipe */

    USB_unlock();

    return status;
    
} /* Endbody */

/*FUNCTION****************************************************************
*
* Function Name    : _io_cdc_serial_write
* Returned Value   : _mqx_int number of characters written
* Comments         :
*    This routine writes characters to the USB serial I/O channel.
*
*END**********************************************************************/

static _mqx_int _io_cdc_serial_write
   (
      /* [IN] the file handle for the device */
      MQX_FILE_PTR fd_ptr,

      /* [IN] where the characters are */
      char_ptr  data_ptr,

      /* [IN] the number of characters to output */
      _mqx_int  num
   )
{ /* Body */
    CLASS_CALL_STRUCT_PTR          data_instance;
    USB_DATA_CLASS_INTF_STRUCT_PTR if_ptr;
    TR_INIT_PARAM_STRUCT           tr;
    USB_STATUS                     status = USBERR_NO_INTERFACE;
    uint_32                        num_left;
    LWEVENT_STRUCT_PTR             event;
    
    if (0 == (num_left = num))
       return 0;
    
    data_instance = (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_PTR->DRIVER_INIT_PTR;

    USB_lock();
    /* Validity checking, always needed when passing data to lower API */
    if (!usb_host_class_intf_validate(data_instance)) {
        USB_unlock();
        fd_ptr->ERROR = USBERR_NO_INTERFACE;
        return IO_ERROR;
    }
    if_ptr = (USB_DATA_CLASS_INTF_STRUCT_PTR) data_instance->class_intf_handle;
    
    /* Do the device use OUT pipe? */
    if (if_ptr->out_pipe == NULL) {
        USB_unlock();
        return USBERR_OPEN_PIPE_FAILED;
    }
    
    event = &if_ptr->data_event;

    if ((((CDC_SERIAL_INIT_PTR) fd_ptr->FLAGS)->FLAGS & USB_UART_NO_BLOCKING) && !(event->VALUE & USB_DATA_SEND_PIPE_FREE)) {
        USB_unlock();
        return 0;
    }

    USB_unlock();
    
    _lwevent_wait_ticks(event, USB_DATA_SEND_PIPE_FREE | USB_DATA_DETACH, FALSE, 0);
    USB_lock();

    /* Validity checking, always needed when passing data to lower API */
    if (!usb_host_class_intf_validate(data_instance)) {
        USB_unlock();
        fd_ptr->ERROR = USBERR_NO_INTERFACE;
        return IO_ERROR;
    }

    while (num_left) {
        usb_hostdev_tr_init(&tr, (tr_callback) usb_class_cdc_out_data_callback, (pointer) fd_ptr);
        tr.TX_BUFFER = (uchar_ptr) data_ptr;
        tr.TX_LENGTH = num_left;

        _lwevent_clear(event, USB_DATA_SEND_COMPLETE);
        status = _usb_host_send_data(if_ptr->CDC_G.G.host_handle, if_ptr->out_pipe, &tr);
        if (status == USB_STATUS_TRANSFER_QUEUED) {
    
            USB_unlock();
            /* serialize requests by waiting for pipe */
            /* wait while data will be sent */
            _lwevent_wait_ticks(event, USB_DATA_SEND_COMPLETE | USB_DATA_DETACH, FALSE, 0);
            
            USB_lock();

            /* Validity checking, always needed when passing data to lower API */
            if (!usb_host_class_intf_validate(data_instance)) {
                USB_unlock();
                fd_ptr->ERROR = USBERR_NO_INTERFACE;
                return IO_ERROR;
            }

            data_ptr += if_ptr->TX_SENT;
            num_left -= if_ptr->TX_SENT;

            if (num_left && !(((CDC_SERIAL_INIT_PTR) fd_ptr->FLAGS)->FLAGS & USB_UART_NO_BLOCKING)) {
                /* wait 10 ticks to next write */
                USB_unlock();
                _lwevent_wait_ticks(event, USB_DATA_DETACH, FALSE, 10);
                USB_lock();
                /* Validity checking, always needed when passing data to lower API */
                if (!usb_host_class_intf_validate(data_instance)) {
                    USB_unlock();
                    fd_ptr->ERROR = USBERR_NO_INTERFACE;
                    return IO_ERROR;
                }
            }
            else
                break;
        }
    }
    
    _lwevent_set(event, USB_DATA_SEND_PIPE_FREE); /* mark we are not using input pipe */

    USB_unlock();

    return num - num_left;
} /* Endbody */


/*FUNCTION*****************************************************************
*
* Function Name    : _io_cdc_serial_ioctl
* Returned Value   : _mqx_int
* Comments         :
*    Returns result of ioctl operation.
*
*END*********************************************************************/

static _mqx_int _io_cdc_serial_ioctl
   (
      /* [IN] the file handle for the device */
      MQX_FILE_PTR   fd_ptr,

      /* [IN] the ioctl command */
      _mqx_uint      cmd,

      /* [IN] the ioctl parameters */
      pointer        param_ptr
   )
{ /* Body */
    _mqx_int _PTR_ bparam_ptr;
    uint_32  _PTR_ pparam_ptr;
    USB_CDC_UART_CODING uart_coding;
   
    switch (cmd) {
        case IO_IOCTL_CHAR_AVAIL:
            bparam_ptr  = (_mqx_int _PTR_) param_ptr;
            fd_ptr->ERROR = _io_cdc_serial_char_avail(fd_ptr);
            if ((fd_ptr->ERROR != (_mqx_int) TRUE) && (fd_ptr->ERROR != (_mqx_int) FALSE))
                return IO_ERROR;
            *bparam_ptr = fd_ptr->ERROR;
            break;
         
        case IO_IOCTL_SERIAL_GET_FLAGS:
            pparam_ptr  = (_mqx_uint_ptr) param_ptr;
            *pparam_ptr = fd_ptr->FLAGS;
            break;

        case IO_IOCTL_SERIAL_SET_FLAGS:
            pparam_ptr  = (_mqx_uint_ptr) param_ptr;
            fd_ptr->FLAGS = *pparam_ptr;
            break;
      
        case IO_IOCTL_SERIAL_GET_STOP_BITS:
            if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                return IO_ERROR;
            pparam_ptr = (_mqx_uint_ptr) param_ptr;
            switch (uart_coding.stopbits) {
                case 1:
                    *pparam_ptr = IO_SERIAL_STOP_BITS_1;
                    break;
                case 2:
                    *pparam_ptr = IO_SERIAL_STOP_BITS_2;
                    break;
                case 3:
                    *pparam_ptr = IO_SERIAL_STOP_BITS_1_5;
                    break;
                default:
                    fd_ptr->ERROR = MQX_INVALID_PARAMETER;
                    return IO_ERROR;
            }
            break;

        case IO_IOCTL_SERIAL_SET_STOP_BITS:
            /* Tell user to change terminal settings */
            pparam_ptr  = (_mqx_uint_ptr) param_ptr;
            switch (*pparam_ptr) {
                case IO_SERIAL_STOP_BITS_1:
                    if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                        return IO_ERROR;
                    uart_coding.stopbits = 1;
                    if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                        return IO_ERROR;

                case IO_SERIAL_STOP_BITS_1_5:
                    if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                        return IO_ERROR;
                    uart_coding.stopbits = 3;
                    if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                        return IO_ERROR;

                case IO_SERIAL_STOP_BITS_2:
                    if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                        return IO_ERROR;
                    uart_coding.stopbits = 2;
                    if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                        return IO_ERROR;

            } /* Endswitch */
            return MQX_INVALID_PARAMETER;

        case IO_IOCTL_SERIAL_GET_DATA_BITS:
            if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                return IO_ERROR;
            pparam_ptr = (_mqx_uint_ptr) param_ptr;
            *pparam_ptr = uart_coding.databits;
            break;

        case IO_IOCTL_SERIAL_SET_DATA_BITS:
            if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                return IO_ERROR;
            pparam_ptr = (_mqx_uint_ptr) param_ptr;
            if (*pparam_ptr > 16)
                return IO_ERROR;
            uart_coding.databits = *pparam_ptr;
            if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_set_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                return IO_ERROR;
            break;

        case IO_IOCTL_SERIAL_GET_BAUD:
            if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                return IO_ERROR;
            pparam_ptr = (_mqx_uint_ptr) param_ptr;
            *pparam_ptr = uart_coding.baudrate;
            break;

        case IO_IOCTL_SERIAL_SET_BAUD:
            if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_get_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                return IO_ERROR;
            pparam_ptr = (_mqx_uint_ptr) param_ptr;
            uart_coding.baudrate = *pparam_ptr;
            if (USB_OK != (fd_ptr->ERROR = usb_class_cdc_set_acm_line_coding( (CLASS_CALL_STRUCT_PTR) fd_ptr->DEV_DATA_PTR, &uart_coding)))
                return IO_ERROR;
            break;

        default:
            return IO_ERROR_INVALID_IOCTL_CMD;
    } /* Endswitch */
   
    return IO_OK;

} /* Endbody */

/*FUNCTION*****************************************************************
*
* Function Name    : _io_cdc_serial_uninstall
* Returned Value   : _mqx_int
* Comments         :
*    Does nothing. Must be implemented to allow device to be uninstalled.
*
*END*********************************************************************/

static _mqx_int _io_cdc_serial_uninstall
   (
       IO_DEVICE_STRUCT_PTR dev
   )
{ /* Body */
    return IO_OK;
} /* Endbody */

/* EOF */
