/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
* Copyright 1989-2008 ARC International
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
*   This file contains main initialization for your application
*   and infinite loop
*
*
*END************************************************************************/
#ifndef __main_h_
#define __main_h_
#include <mqx.h>
#include <bsp.h>
$mainh_mfs_includes$
$mainh_usbhost_includes$
$mainh_usbdevice_includes$
$mainh_shell_includes$
$mainh_rtcs_includes$
$usbhost_usb_otg$

#define MAIN_TASK   1
$mfs_ram_disk_size$
$mfs_ramdisk_defines$
$rtcs_enet_device$
$rtcs_dhcp$

$rtcs_ip_address$
$rtcs_ip_mask$
$rtcs_gateway$
$rtcs_ppp$

$usbhost_max_frame_size$    

extern void Main_task (uint32_t);

$mfs_ram_disk_start_function_prototype$
$rtcs_init_function_prototype$
$usbd_init_function_prototype$
$usbh_init_function_prototype$

$usbhost_hostevent_function_prototype$

/* PPP device must be set manually and 
** must be different from the default IO channel (BSP_DEFAULT_IO_CHANNEL) 
*/
#define PPP_DEVICE      "ittyb:" 

/*
** Define PPP_DEVICE_DUN only when using PPP to communicate
** to Win9x Dial-Up Networking over a null-modem
** This is ignored if PPP_DEVICE is not #define'd
*/
#define PPP_DEVICE_DUN  1

/*
** Define the local and remote IP addresses for the PPP link
** These are ignored if PPP_DEVICE is not #define'd
*/
#define PPP_LOCADDR     IPADDR(192,168,0,216)
#define PPP_PEERADDR    IPADDR(192,168,0,217)

/*
** Define a default gateway
*/
#define GATE_ADDR       IPADDR(192,168,0,1)

#endif /* __main_h_ */

