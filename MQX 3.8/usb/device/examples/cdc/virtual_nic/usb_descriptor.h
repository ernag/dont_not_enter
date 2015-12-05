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
* $FileName: usb_descriptor.h$
* $Version : 3.8.3.1$
* $Date    : Dec-7-2011$
*
* Comments:
*
* @brief The file contains defines and function definition for descriptor file
*
*****************************************************************************/

#ifndef _USB_DESCRIPTOR_H
#define _USB_DESCRIPTOR_H 1

/******************************************************************************
 * Includes
 *****************************************************************************/
#include "usb_class.h"
#include "usb_cdc.h"
/******************************************************************************
 * Constants - None
 *****************************************************************************/

/******************************************************************************
 * Macro's
 *****************************************************************************/ 
#define BCD_USB_VERSION                  (0x0200)

#define REMOTE_WAKEUP_SUPPORT            (0)/*TRUE : 1, FALSE : 0*/
#define REMOTE_WAKEUP_SHIFT              (5)
#define DATA_CLASS_SUPPORT               (1)/*TRUE*/
#define CIC_NOTIF_ELEM_SUPPORT           (1)/*TRUE*/

/* Communication Class SubClass Codes */
#define DIRECT_LINE_CONTROL_MODEL           (0x01)
#define ABSTRACT_CONTROL_MODEL              (0x02)
#define TELEPHONE_CONTROL_MODEL             (0x03)
#define MULTI_CHANNEL_CONTROL_MODEL         (0x04)
#define CAPI_CONTROL_MOPDEL                 (0x05)
#define ETHERNET_NETWORKING_CONTROL_MODEL   (0x06)
#define ATM_NETWORKING_CONTROL_MODEL        (0x07)
#define WIRELESS_HANDSET_CONTROL_MODEL      (0x08)
#define DEVICE_MANAGEMENT                   (0x09)
#define MOBILE_DIRECT_LINE_MODEL            (0x0A)
#define OBEX                                (0x0B)
#define ETHERNET_EMULATION_MODEL            (0x0C)

/* Communication Class Protocol Codes */
#define NO_CLASS_SPECIFIC_PROTOCOL  (0x00)/*also for Data Class Protocol Code*/
#define AT_250_PROTOCOL             (0x01)
#define AT_PCCA_101_PROTOCOL        (0x02)
#define AT_PCCA_101_ANNEX_O         (0x03)
#define AT_GSM_7_07                 (0x04)
#define AT_3GPP_27_007              (0x05)
#define AT_TIA_CDMA                 (0x06)
#define ETHERNET_EMULATION_PROTOCOL (0x07)
#define EXTERNAL_PROTOCOL           (0xFE)
#define VENDOR_SPECIFIC             (0xFF)/*also for Data Class Protocol Code*/

/* Data Class Protocol Codes */
#define PYHSICAL_INTERFACE_PROTOCOL    (0x30)
#define HDLC_PROTOCOL                  (0x31)
#define TRANSPARENT_PROTOCOL           (0x32)
#define MANAGEMENT_PROTOCOL            (0x50)
#define DATA_LINK_Q931_PROTOCOL        (0x51)
#define DATA_LINK_Q921_PROTOCOL        (0x52)
#define DATA_COMPRESSION_V42BIS        (0x90)
#define EURO_ISDN_PROTOCOL             (0x91)
#define RATE_ADAPTION_ISDN_V24         (0x92)
#define CAPI_COMMANDS                  (0x93)
#define HOST_BASED_DRIVER              (0xFD)
#define CDC_UNIT_FUNCTIONAL            (0xFE)

/* Descriptor SubType in Communications Class Functional Descriptors */
#define HEADER_FUNC_DESC              (0x00)
#define CALL_MANAGEMENT_FUNC_DESC     (0x01)
#define ABSTRACT_CONTROL_FUNC_DESC    (0x02)
#define DIRECT_LINE_FUNC_DESC         (0x03) 
#define TELEPHONE_RINGER_FUNC_DESC    (0x04)
#define TELEPHONE_REPORT_FUNC_DESC    (0x05) 
#define UNION_FUNC_DESC               (0x06)
#define COUNTRY_SELECT_FUNC_DESC      (0x07)
#define TELEPHONE_MODES_FUNC_DESC     (0x08)
#define USB_TERMINAL_FUNC_DESC        (0x09)
#define NETWORK_CHANNEL_FUNC_DESC     (0x0A)
#define PROTOCOL_UNIT_FUNC_DESC       (0x0B)
#define EXTENSION_UNIT_FUNC_DESC      (0x0C)
#define MULTI_CHANNEL_FUNC_DESC       (0x0D)
#define CAPI_CONTROL_FUNC_DESC        (0x0E)
#define ETHERNET_NETWORKING_FUNC_DESC (0x0F)
#define ATM_NETWORKING_FUNC_DESC      (0x10)
#define WIRELESS_CONTROL_FUNC_DESC    (0x11)
#define MOBILE_DIRECT_LINE_FUNC_DESC  (0x12)
#define MDLM_DETAIL_FUNC_DESC         (0x13)
#define DEVICE_MANAGEMENT_FUNC_DESC   (0x14)
#define OBEX_FUNC_DESC                (0x15)
#define COMMAND_SET_FUNC_DESC         (0x16)
#define COMMAND_SET_DETAIL_FUNC_DESC  (0x17)
#define TELEPHONE_CONTROL_FUNC_DESC   (0x18)
#define OBEX_SERVICE_ID_FUNC_DESC     (0x19)


#define CIC_SUBCLASS_CODE                ABSTRACT_CONTROL_MODEL
#define CIC_PROTOCOL_CODE                VENDOR_SPECIFIC
#define DIC_PROTOCOL_CODE                NO_CLASS_SPECIFIC_PROTOCOL
#define CIC_ENDP_COUNT                   ((CIC_NOTIF_ELEM_SUPPORT & 0x01)*1)
#define DIC_ENDP_COUNT                   (2)

#define CIC_NOTIF_ENDPOINT               (1)
#define CIC_NOTIF_ENDP_PACKET_SIZE       (16)
#define DIC_BULK_IN_ENDPOINT             (2)
#define DIC_BULK_IN_ENDP_PACKET_SIZE     (64)
#define DIC_BULK_OUT_ENDPOINT            (3)
/*
 * In RedStripe IP DMA Transaction fails 
 * if Packet Size is configured more than 160.
 * As per USB 2.0 spec MAX packet Size has to be configured to 512 bytes.
 * Hence compliance fails.
 */
#define DIC_BULK_OUT_ENDP_PACKET_SIZE    (64)

#if (!HIGH_SPEED_DEVICE)
    #if((DIC_BULK_OUT_ENDP_PACKET_SIZE > 64) || (DIC_BULK_IN_ENDP_PACKET_SIZE > 64))
        #error "Bulk Endpoint Packet Size greater than 64 is not allowed for NON-HIGH SPEED DEVICES"
    #endif
#else
    #if((DIC_BULK_OUT_ENDP_PACKET_SIZE > 512) || (DIC_BULK_IN_ENDP_PACKET_SIZE > 512))
        #error "Bulk Endpoint Packet Size greater than 512 is not allowed for HIGH SPEED DEVICES"
    #endif
#endif

#if HIGH_SPEED_DEVICE 
    /* Here Other speed Configuration is for FULL SPEED */  
    #define OTHER_SPEED_DIC_BULK_IN_ENDP_PACKET_SIZE     (64)/* max supported is 64 for FS and 512 for HS*/ 
    #define OTHER_SPEED_DIC_BULK_OUT_ENDP_PACKET_SIZE    (64)/* max supported is 64 and 512 for HS*/ 
#endif

/* Various descriptor sizes */ 
#define DEVICE_DESCRIPTOR_SIZE            (18)
#define CONFIG_ONLY_DESC_SIZE             (9)
#define CONFIG_DESC_SIZE                  (CONFIG_ONLY_DESC_SIZE + 28 + CIC_NOTIF_ELEM_SUPPORT * 7 + DATA_CLASS_SUPPORT * 23)
#define IFACE_ONLY_DESC_SIZE              (9)   
#define ENDP_ONLY_DESC_SIZE               (7)
#define CDC_HEADER_FUNC_DESC_SIZE         (5)
#define CDC_CALL_MANAG_DESC_SIZE          (5)
#define CDC_ABSTRACT_DESC_SIZE            (4)
#define CDC_UNION_FUNC_DESC_SIZE          (5)

#if HIGH_SPEED_DEVICE
    #define DEVICE_QUALIFIER_DESCRIPTOR_SIZE    (10)
    #define OTHER_SPEED_CONFIG_DESCRIPTOR_SIZE  (CONFIG_DESC_SIZE)  
#endif

/* Max descriptors provided by the Application */
#define USB_MAX_STD_DESCRIPTORS           (7) 

/* Max configuration supported by the Application */
#define USB_MAX_CONFIG_SUPPORTED          (1)

/* Max string descriptors supported by the Application */
#define USB_MAX_STRING_DESCRIPTORS        (3)

/* Max language codes supported by the USB */
#define USB_MAX_LANGUAGES_SUPPORTED       (1)
 
/* string descriptors sizes */
#define USB_STR_DESC_SIZE (2)
#define USB_STR_0_SIZE  (2)
#define USB_STR_1_SIZE  (56)
#define USB_STR_2_SIZE  (40)
#define USB_STR_n_SIZE  (32)

/* descriptors codes */
#define USB_DEVICE_DESCRIPTOR     (1)
#define USB_CONFIG_DESCRIPTOR     (2)
#define USB_STRING_DESCRIPTOR     (3)
#define USB_IFACE_DESCRIPTOR      (4)
#define USB_ENDPOINT_DESCRIPTOR   (5)
#define USB_CS_INTERFACE          (0x24)
#define USB_CS_ENDPOINT           (0x25)

#define USB_MAX_SUPPORTED_INTERFACES     (2)

#if HIGH_SPEED_DEVICE
    #define USB_DEVQUAL_DESCRIPTOR      (6)
    #define USB_OTHER_SPEED_DESCRIPTOR  (7)
#endif

#define CDC_CLASS                              (0x02)
#define DEVICE_DESC_DEVICE_CLASS               (0x02)
#define DEVICE_DESC_DEVICE_SUBCLASS            (0x00)
#define DEVICE_DESC_DEVICE_PROTOCOL            (0x00)
#define DEVICE_DESC_NUM_CONFIG_SUPPOTED        (0x01)
/* Keep the following macro Zero if you dont Support Other Speed Configuration
   If you suppoort Other Speeds make it 0x01 */
#define DEVICE_OTHER_DESC_NUM_CONFIG_SUPPOTED  (0x00) 
#define CONFIG_DESC_NUM_INTERFACES_SUPPOTED    (0x01+DATA_CLASS_SUPPORT)
#define CONFIG_DESC_CURRENT_DRAWN              (0x32)

/* Implementation Specific Macros */
#define LINE_CODING_SIZE              (0x07)
#define COMM_FEATURE_DATA_SIZE        (0x02)

#define LINE_CODE_DTERATE_IFACE      (115200) /*e.g 9600 is 0x00002580 */
#define LINE_CODE_CHARFORMAT_IFACE   (0x00)   /* 1 stop bit */
#define LINE_CODE_PARITYTYPE_IFACE   (0x00)  /* No Parity */
#define LINE_CODE_DATABITS_IFACE     (0x08)  /* Data Bits Format */

#define STATUS_ABSTRACT_STATE_IFACE  (0x0000) /* Disable Multiplexing ENDP in
                                                  this interface will continue
                                                  to accept/offer data*/
#define COUNTRY_SETTING_IFACE        (0x0000) /* Country Code in the format as 
                                                  defined in [ISO3166]- 
                                                  - PLEASE CHECK THESE VALUES*/                                                                                                   
/* Notifications Support */
#define PSTN_SUBCLASS_NOTIF_SUPPORT   (1)/*TRUE*/
#define WMC_SUBCLASS_NOTIF_SUPPORT    (0)/*FALSE*/
#define CDC_CLASS_NOTIF_SUPPORT       (0)/*FALSE*/

#define CDC_DESC_ENDPOINT_COUNT       (CIC_ENDP_COUNT+(DATA_CLASS_SUPPORT & 0x01) * DIC_ENDP_COUNT)

/******************************************************************************
 * Types
 *****************************************************************************/ 
    
/******************************************************************************
 * Global Functions
 *****************************************************************************/
extern uint_8 USB_Desc_Get_Descriptor( 
     uint_32 handle, 
     uint_8 type,
     uint_8 str_num, 
     uint_16 index,
     uint_8_ptr *descriptor,
     uint_32 *size);
      
extern uint_8 USB_Desc_Get_Interface(
                              uint_32 handle, 
                              uint_8 interface, 
                              uint_8_ptr alt_interface);


extern uint_8 USB_Desc_Set_Interface(
                              uint_32 handle, 
                              uint_8 interface, 
                              uint_8 alt_interface);

extern boolean USB_Desc_Valid_Configation(uint_32 handle, 
                                          uint_16 config_val); 

extern boolean USB_Desc_Remote_Wakeup(uint_32 handle); 

extern USB_ENDPOINTS *USB_Desc_Get_Endpoints(uint_32 handle);

extern uint_8 USB_Desc_Get_Line_Coding(uint_32 handle, 
                                       uint_8 interface, 
                                       uint_8_ptr *coding_data);

extern uint_8 USB_Desc_Set_Line_Coding(uint_32 handle, 
                                       uint_8 interface, 
                                       uint_8_ptr *coding_data);                                       

extern uint_8 USB_Desc_Get_Abstract_State(uint_32 handle, 
                                          uint_8 interface, 
                                          uint_8_ptr *feature_data);
extern uint_8 USB_Desc_Get_Country_Setting(uint_32 handle, 
                                          uint_8 interface, 
                                          uint_8_ptr *feature_data);
extern uint_8 USB_Desc_Set_Abstract_State(uint_32 handle, 
                                          uint_8 interface, 
                                          uint_8_ptr *feature_data);
extern uint_8 USB_Desc_Set_Country_Setting(uint_32 handle, 
                                          uint_8 interface, 
                                          uint_8_ptr *feature_data);                                                                                    
uint_8 USB_Desc_Get_feature(uint_32 handle,int_32 cmd, uint_8 in_data,uint_8_ptr * out_buf);
uint_8 USB_Desc_Set_feature(uint_32 handle,int_32 cmd, uint_8 in_data,uint_8_ptr * in_buf);

#endif

/* EOF */
