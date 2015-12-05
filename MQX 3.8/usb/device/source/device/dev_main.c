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
* $FileName: dev_main.c$
* $Version : 3.8.6.0$
* $Date    : Sep-19-2011$
*
* Comments:
*
*  This file contains the main USB device API functions that will be 
*  used by most applications.
*                                                               
*END*********************************************************************/

#include "devapi.h"
#include "mqx_dev.h"
#include "usb_bsp.h"

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_free_XD
*  Returned Value : void
*  Comments       :
*        Enqueues a XD onto the free XD ring.
*
*END*-----------------------------------------------------------------*/

void _usb_device_free_XD
   (
     _usb_device_handle  handle,
      /* [IN] the dTD to enqueue */
      pointer  xd_ptr
   )
{ /* Body */
    USB_DEV_STATE_STRUCT_PTR usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)
        handle;

   /*
   ** This function can be called from any context, and it needs mutual
   ** exclusion with itself.
   */
   USB_lock();

   /*
   ** Add the XD to the free XD queue (linked via PRIVATE) and
   ** increment the tail to the next descriptor
   */
   USB_XD_QADD(usb_dev_ptr->XD_HEAD, usb_dev_ptr->XD_TAIL, 
      (XD_STRUCT_PTR)xd_ptr);
   usb_dev_ptr->XD_ENTRIES++;

   USB_unlock();
} /* Endbody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_init
*  Returned Value : USB_OK or error code
*  Comments       :
*        Initializes the USB device specific data structures and calls 
*  the low-level device controller chip initialization routine.
*
*END*-----------------------------------------------------------------*/
uint_8 _usb_device_init
   (
      /* [IN] the USB device controller to initialize */
      uint_8                    devnum,

      /* [OUT] the USB_USB_dev_initialize state structure */
      _usb_device_handle _PTR_  handle,
            
      /* [IN] number of endpoints to initialize */
      uint_8                    endpoints 
   )
{ /* Body */
    USB_DEV_STATE_STRUCT_PTR         usb_dev_ptr;
    XD_STRUCT_PTR                    xd_ptr;
    uint_8                           temp, i, error;
    SCRATCH_STRUCT_PTR               temp_scratch_ptr;
    
    USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR call_back_table_ptr;
    _mqx_int mqx_status;
    _int_install_unexpected_isr();  
    _usb_dev_driver_install(devnum, &_bsp_usb_dev_callback_table);  
       
    if (devnum > MAX_USB_DEVICES) 
    {
        return USBERR_INVALID_DEVICE_NUM;
    }
   
    /* Allocate memory for the state structure */
    usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)
        USB_mem_alloc_zero(sizeof(USB_DEV_STATE_STRUCT));
      
    if (usb_dev_ptr == NULL) 
    {
        #if _DEBUG
            printf("1 memalloc failed in _usb_device_init\n");
        #endif  
        return USBERR_ALLOC_STATE;
    } /* Endif */
   
    /* Zero out the internal USB state structure */
    USB_mem_zero(usb_dev_ptr,sizeof(USB_DEV_STATE_STRUCT));
    
    call_back_table_ptr = (USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)
      _mqx_get_io_component_handle(IO_USB_COMPONENT);
      
    usb_dev_ptr->CALLBACK_STRUCT_PTR = (call_back_table_ptr + devnum);
      
    if (!usb_dev_ptr->CALLBACK_STRUCT_PTR) 
    {
      return USBERR_DRIVER_NOT_INSTALLED;
    } /* Endif */
    mqx_status = _bsp_usb_dev_init ((pointer)devnum);

    if (mqx_status != 0)
    {     
      return USBERR_UNKNOWN_ERROR;
    }

   /* Multiple devices will have different base addresses and 
   ** interrupt vectors (For future)
   */   
   usb_dev_ptr->DEV_PTR = _bsp_get_usb_base(devnum); 
   usb_dev_ptr->DEV_VEC = _bsp_get_usb_vector(devnum);
   usb_dev_ptr->USB_STATE = USB_STATE_UNKNOWN;
   
   usb_dev_ptr->MAX_ENDPOINTS = endpoints;
   
   temp = (uint_8)(usb_dev_ptr->MAX_ENDPOINTS * 2);

   /* Allocate MAX_XDS_FOR_TR_CALLS */
   xd_ptr = (XD_STRUCT_PTR)
      USB_mem_alloc_zero(sizeof(XD_STRUCT) * MAX_XDS_FOR_TR_CALLS);
      
   if (xd_ptr == NULL) 
   {
      #if _DEBUG
        printf("2 memalloc failed in _usb_device_init\n");
      #endif    
      return USBERR_ALLOC_TR;
   } /* Endif */
   
   usb_dev_ptr->XD_BASE = xd_ptr;
   
   // USB_mem_zero(xd_ptr, sizeof(XD_STRUCT) * MAX_XDS_FOR_TR_CALLS); // already done before

   /* Allocate memory for internal scratch structure */   
   usb_dev_ptr->XD_SCRATCH_STRUCT_BASE = (SCRATCH_STRUCT_PTR)
      USB_mem_alloc_zero(sizeof(SCRATCH_STRUCT)*MAX_XDS_FOR_TR_CALLS);
   
   if (usb_dev_ptr->XD_SCRATCH_STRUCT_BASE == NULL) 
   {
      #if _DEBUG
        printf("3 memalloc failed in _usb_device_init\n");
      #endif    
      return USBERR_ALLOC;
   } /* Endif */

   temp_scratch_ptr = usb_dev_ptr->XD_SCRATCH_STRUCT_BASE;
   usb_dev_ptr->XD_HEAD = NULL;
   usb_dev_ptr->XD_TAIL = NULL;
   usb_dev_ptr->XD_ENTRIES = 0;

   /* Enqueue all the XDs */   
   for (i=0;i<MAX_XDS_FOR_TR_CALLS;i++) 
   {
      xd_ptr->SCRATCH_PTR = temp_scratch_ptr;
      xd_ptr->SCRATCH_PTR->FREE = _usb_device_free_XD;
      xd_ptr->SCRATCH_PTR->PRIVATE = (pointer)usb_dev_ptr;
      _usb_device_free_XD(usb_dev_ptr,(pointer)xd_ptr);
      xd_ptr++;
      temp_scratch_ptr++;
   } /* Endfor */

   usb_dev_ptr->TEMP_XD_PTR = (XD_STRUCT_PTR)USB_mem_alloc_zero(sizeof(XD_STRUCT));
   if(usb_dev_ptr->TEMP_XD_PTR == NULL)
   {
        #if _DEBUG
            printf("4 memalloc failed in _usb_device_init\n");
        #endif  
        return USBERR_ALLOC;
   }
   USB_mem_zero(usb_dev_ptr->TEMP_XD_PTR, sizeof(XD_STRUCT));

   /* Initialize the USB controller chip */
   if (((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)
      usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_INIT != NULL) 
   {
       error = ((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)\
          usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_INIT(devnum, usb_dev_ptr);     
   }
   else
   {
        #if _DEBUG
            printf("_usb_device_init: DEV_INIT is NULL\n");                   
        #endif  
        return USBERR_ERROR;
   }

   if (error) 
   {
      return USBERR_INIT_FAILED;
   } /* Endif */
   
   *handle = usb_dev_ptr;  
   return error;
} /* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_init_endpoint
*  Returned Value : USB_OK or error code
*  Comments       :
*     Initializes the endpoint and the data structures associated with the 
*  endpoint
*
*END*-----------------------------------------------------------------*/
uint_8 _usb_device_init_endpoint
   (
    /* [IN] the USB_USB_dev_initialize state structure */
    _usb_device_handle         handle,

    /* [IN] the endpoint structure, include members such as endpoint number, 
     * endpoint type, endpoint direction and the max packet size 
     */                  
    USB_EP_STRUCT_PTR    ep_ptr, 
    
    /* [IN] After all data is transfered, should we terminate the transfer
     * with a zero length packet if the last packet size == MAX_PACKET_SIZE?
     */
      uint_8                     flag
   )
{ /* Body */
   uint_8         error = 0;
   USB_DEV_STATE_STRUCT_PTR      usb_dev_ptr;
   
   usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)handle;

   /* Initialize the transfer descriptor */
   usb_dev_ptr->TEMP_XD_PTR->EP_NUM = ep_ptr->ep_num;
   usb_dev_ptr->TEMP_XD_PTR->BDIRECTION = ep_ptr->direction;
   usb_dev_ptr->TEMP_XD_PTR->WMAXPACKETSIZE = (uint_16)(ep_ptr->size & 0x0000FFFF);
   usb_dev_ptr->TEMP_XD_PTR->EP_TYPE = ep_ptr->type;
   usb_dev_ptr->TEMP_XD_PTR->DONT_ZERO_TERMINATE = flag;

   if (((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)
      usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_INIT_ENDPOINT != NULL) 
   {
        error=((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)\
          usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_INIT_ENDPOINT(handle, 
          usb_dev_ptr->TEMP_XD_PTR);
   }
   else
   {
        #if _DEBUG
            printf("_usb_device_init_endpoint: DEV_INIT_ENDPOINT is NULL\n");                     
        #endif  
        return USBERR_ERROR;
   }
   
   return error;
} /* EndBody */

/*FUNCTION*----------------------------------------------------------------
* 
* Function Name  : _usb_device_register_service
* Returned Value : USB_OK or error code
* Comments       :
*     Registers a callback routine for a specified event or endpoint.
* 
*END*--------------------------------------------------------------------*/
uint_8 _usb_device_register_service
   (
      /* [IN] Handle to the USB device */
      _usb_device_handle         handle,
      
      /* [IN] type of event or endpoint number to service */
      uint_8                     type,
      
      /* [IN] Pointer to the service's callback function */
      void(_CODE_PTR_ service)(PTR_USB_EVENT_STRUCT, pointer),
      
      /*[IN] User Argument to be passed to Services when invoked.*/
      pointer arg
   )
{ /* Body */
   USB_DEV_STATE_STRUCT_PTR   usb_dev_ptr;
   SERVICE_STRUCT_PTR         service_ptr;
   SERVICE_STRUCT_PTR _PTR_   search_ptr;

   if (handle == NULL)
   {
        return USBERR_ERROR;
   }
   usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)handle;
   /* Needs mutual exclusion */
   USB_lock();
   
   /* Search for an existing entry for type */
   for (search_ptr = &usb_dev_ptr->SERVICE_HEAD_PTR;
      *search_ptr;
      search_ptr = &(*search_ptr)->NEXT) 
   {
      if ((*search_ptr)->TYPE == type) 
      {
         /* Found an existing entry */
         USB_unlock();
         return USBERR_OPEN_SERVICE;
      } /* Endif */
   } /* Endfor */
   
   /* No existing entry found - create a new one */
   service_ptr = (SERVICE_STRUCT_PTR)USB_mem_alloc_zero(sizeof(SERVICE_STRUCT));
   
   if (!service_ptr) 
   {
      USB_unlock();
      #if _DEBUG
        printf("memalloc failed in _usb_device_register_service\n");
      #endif    
      return USBERR_ALLOC;
   } /* Endif */
   
   service_ptr->TYPE = type;
   service_ptr->SERVICE = service;
   service_ptr->arg = arg;
   service_ptr->NEXT = NULL;
   *search_ptr = service_ptr;
   
   USB_unlock();
   return USB_OK;
} /* EndBody */

/*FUNCTION*----------------------------------------------------------------
* 
* Function Name  : _usb_device_unregister_service
* Returned Value : USB_OK or error code
* Comments       :
*     Unregisters a callback routine for a specified event or endpoint.
* 
*END*--------------------------------------------------------------------*/
uint_8 _usb_device_unregister_service
   (
      /* [IN] Handle to the USB device */
      _usb_device_handle         handle,

      /* [IN] type of event or endpoint number to service */
      uint_8                     type
   )
{ /* Body */
   USB_DEV_STATE_STRUCT_PTR   usb_dev_ptr;
   SERVICE_STRUCT_PTR         service_ptr;
   SERVICE_STRUCT_PTR _PTR_   search_ptr;

   if (handle == NULL)
   {
      return USBERR_ERROR;
   }

   usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)handle;
   /* Needs mutual exclusion */
   USB_lock();
   
   /* Search for an existing entry for type */
   for (search_ptr = &usb_dev_ptr->SERVICE_HEAD_PTR;
      *search_ptr;
      search_ptr = &(*search_ptr)->NEXT) 
   {
      if ((*search_ptr)->TYPE == type) 
      {
         /* Found an existing entry - delete it */
         break;
      } /* Endif */
   } /* Endfor */
   
   /* No existing entry found */
   if (!*search_ptr) 
   {
      USB_unlock();
      return USBERR_CLOSED_SERVICE;
   } /* Endif */
   
   service_ptr = *search_ptr;
   *search_ptr = service_ptr->NEXT;

   USB_mem_free((pointer)service_ptr);
   
   USB_unlock();
   return USB_OK;

} /* EndBody */

/*FUNCTION*----------------------------------------------------------------
* 
* Function Name  : _usb_device_call_service
* Returned Value : USB_OK or error code
* Comments       :
*     Calls the appropriate service for the specified type, if one is
*     registered. Used internally only.
* 
*END*--------------------------------------------------------------------*/
uint_8 _usb_device_call_service
   (
       /* [IN] Type of service or endpoint */    
      uint_8                  type,
      
      /* [IN] pointer to event structure  */ 
      PTR_USB_EVENT_STRUCT    event            
   )
{ /* Body */
   USB_DEV_STATE_STRUCT_PTR      usb_dev_ptr;
   SERVICE_STRUCT _PTR_          service_ptr;

   usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)event->handle;
   /* Needs mutual exclusion */
   USB_lock();
   
   /* Search for an existing entry for type */
   for (service_ptr = usb_dev_ptr->SERVICE_HEAD_PTR;
      service_ptr;
      service_ptr = service_ptr->NEXT) 
   {
      if (service_ptr->TYPE == type) 
      {
         service_ptr->SERVICE(event,service_ptr->arg);
         USB_unlock();
         return USB_OK;
      } /* Endif */
      
   } /* Endfor */

   USB_unlock();

   return USBERR_CLOSED_SERVICE;
} /* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_get_transfer_status
*  Returned Value : Status of the transfer
*  Comments       :
*        returns the status of the transaction on the specified endpoint.
*
*END*-----------------------------------------------------------------*/
uint_8 _usb_device_get_transfer_status
   (
      /* [IN] the USB_USB_dev_initialize state structure */
      _usb_device_handle         handle,
            
      /* [IN] the Endpoint number */
      uint_8                     ep_num,
            
      /* [IN] direction */
      uint_8                     direction
   )
{ /* Body */
   uint_8   error;
   USB_DEV_STATE_STRUCT_PTR      usb_dev_ptr;
   
   usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)handle;
   
   USB_lock();

   if (((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)
      usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_GET_TRANSFER_STATUS != NULL) 
   {
       error = ((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)
          usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_GET_TRANSFER_STATUS(handle, ep_num,
          direction);
   }
   else
   {
        #if _DEBUG
            printf("_usb_device_get_transfer_status: DEV_GET_TRANSFER_STATUS is NULL\n");                     
        #endif  
        return USBERR_ERROR;
   }
   
   USB_unlock();
   /* Return the status of the last queued transfer */
   return error;

} /* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_read_setup_data
*  Returned Value : USB_OK or error code
*  Comments       :
*        Reads the setup data from the hardware
*
*END*-----------------------------------------------------------------*/
uint_8 _usb_device_read_setup_data
   (
      /* [IN] the USB_USB_dev_initialize state structure */
      _usb_device_handle         handle,
            
      /* [IN] the Endpoint number */
      uint_8                     ep_num,
            
      /* [IN] buffer for receiving Setup packet */
      uint_8_ptr                  buff_ptr
   )
{ /* Body */
   USB_DEV_STATE_STRUCT_PTR      usb_dev_ptr;
   uint_8 error = 0;
  
   if (handle == NULL)
   {
      return USBERR_ERROR;
   }
   usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)handle;

   if (((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)
      usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_GET_SETUP_DATA != NULL) 
   {
        error = ((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)
            usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_GET_SETUP_DATA(handle, 
            ep_num, buff_ptr);
   }
   else
   {
        #if _DEBUG
            printf("_usb_device_read_setup_data: DEV_GET_SETUP_DATA is NULL\n");                      
        #endif  
        return USBERR_ERROR;
   }
  
   return error;
} /* EndBody */

/*FUNCTION*-------------------------------------------------------------
*
*  Function Name  : _usb_device_set_address
*  Returned Value : USB_OK or error code
*  Comments       :
*        Sets the device address as assigned by the host during enumeration
*
*END*-----------------------------------------------------------------*/
uint_8 _usb_device_set_address
   (
      /* [IN] the USB_USB_dev_initialize state structure */
      _usb_device_handle         handle,
      
      /* [IN] the USB address to be set in the hardware */
      uint_8                     address
   )
{ 
   USB_DEV_STATE_STRUCT_PTR      usb_dev_ptr;
   uint_8 error;
   
   if (handle == NULL)
   {
      return USBERR_ERROR;
   }
   usb_dev_ptr = (USB_DEV_STATE_STRUCT_PTR)handle;

    if (((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)
      usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_SET_ADDRESS != NULL) 
    {
        error = ((USB_DEV_CALLBACK_FUNCTIONS_STRUCT_PTR)
            usb_dev_ptr->CALLBACK_STRUCT_PTR)->DEV_SET_ADDRESS(handle, address);
    }
    else
    {
        #if _DEBUG
            printf("_usb_device_read_setup_data: DEV_GET_SETUP_DATA is NULL\n");                      
        #endif  
        return USBERR_ERROR;
    }

   return error;
} 

/* EOF */
