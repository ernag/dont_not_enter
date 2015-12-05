#ifndef __usb_host_hub_sm_h__
#define __usb_host_hub_sm_h__
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
* $FileName: usb_host_hub_sm.h$
* $Version : 3.8.9.0$
* $Date    : Sep-19-2011$
*
* Comments:
*
*   This file defines structures used by the implementation of hub SM on host.
*
*END************************************************************************/

/* System Include files */
#include "usb.h"
#include <hostapi.h>
#include <host_ch9.h>
#include <host_dev_list.h>
#include <host_common.h>
#include <usbprv.h>

/*
** Following structs contain all states and pointers
** used by the application to control/operate devices.
*/

typedef struct hub_device_port_struct {
/* status read from GetPortStatus transfer */
   uint_16                          STATUS;
#define HUB_PORT_ATTACHED           (0x01) /* flag informs that application started attach process */
#define HUB_PORT_REMOVABLE          (0x02) /* flag informs that application started attach process */
   uint_8                           APP_STATUS;
} HUB_PORT_STRUCT, _PTR_ HUB_PORT_STRUCT_PTR;

typedef struct hub_device_struct {
   CLASS_CALL_STRUCT                CCS;
   /* below the hub specific data */
   enum {
      HUB_IDLE,
      HUB_GET_DESCRIPTOR_TINY_PROCESS,
      HUB_GET_DESCRIPTOR_PROCESS,
      HUB_SET_PORT_FEATURE_PROCESS,
      HUB_CLEAR_PORT_FEATURE_PROCESS,
      HUB_GET_PORT_STATUS_PROCESS,
      HUB_GET_PORT_STATUS_ASYNC,
      HUB_RESET_DEVICE_PORT_PROCESS,
      HUB_ADDRESS_DEVICE_PORT_PROCESS,
      HUB_DETACH_DEVICE_PORT_PROCESS,
      HUB_GET_STATUS_ASYNC,
      HUB_NONE
   }                                STATE;
   uchar                            HUB_PORT_NR;
   _mqx_int                         port_iterator;
   HUB_PORT_STRUCT_PTR              HUB_PORTS;
} HUB_DEVICE_STRUCT,  _PTR_ HUB_DEVICE_STRUCT_PTR;

/* List of Function Prototypes */

#ifdef __cplusplus
extern "C" {
#endif

void usb_host_hub_device_event(_usb_device_instance_handle, _usb_interface_descriptor_handle, uint_32);

#ifdef __cplusplus
}
#endif

#endif
