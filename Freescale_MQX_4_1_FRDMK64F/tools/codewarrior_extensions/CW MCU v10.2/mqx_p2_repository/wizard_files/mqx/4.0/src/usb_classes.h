/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other restrictions.
*****************************************************************************
*
* Comments:
*
*END************************************************************************/
#ifndef __usb_classes_h__
#define __usb_classes_h__

#warning Please uncomment following definitions for class to be used in your application and follow TODO in the generated files.

//#define USBCLASS_INC_HID
//#define USBCLASS_INC_MASS
//#define USBCLASS_INC_CDC
//#define USBCLASS_INC_PRINTER
//#define USBCLASS_INC_HUB

#ifdef USBCLASS_INC_MASS
#include "usb_host_msd_bo.h"
#endif

#ifdef USBCLASS_INC_PRINTER
#include "usb_host_printer.h"
#endif

#ifdef USBCLASS_INC_HID
#include "usb_host_hid.h"
#endif

#ifdef USBCLASS_INC_CDC
#include "usb_host_cdc.h"
#endif

#ifdef USBCLASS_INC_AUDIO
#include "usb_host_audio.h"
#endif

#ifdef USBCLASS_INC_PHDC
#include "usb_host_phdc.h"
#endif

/* here hub is considered as device from host point of view */
#ifdef USBCLASS_INC_HUB
#include "usb_host_hub.h"
#include "usb_host_hub_sm.h"
#endif

#endif
/* EOF */
