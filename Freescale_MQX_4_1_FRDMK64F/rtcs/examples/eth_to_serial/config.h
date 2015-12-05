/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
* Copyright 2004-2008 Embedded Access Inc.
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
* See license agreement file for full license terms including other
* restrictions.
*****************************************************************************
*
* Comments:
*
*   Configurable information for the RTCS examples.
*
*
*END************************************************************************/

/*
 *    With This Configuration, I can get it to work over telnet:
 *    
 *    Connect FRDM board to ClownCar network via Ethernet.
 *    From MAC OS X (connected via WIFI) do, telnet 192.168.1.202 23
 *    
 *    ENET_IPADDR IPADDR(192,168,1,202)
 *    ENET_IPMASK  IPADDR(255,255,255,0) 
 *    ENET_IPGATEWAY  IPADDR(0,0,0,0) 
 *    #define ENET_MAC  {0x00,0xA7,0xC5,0xF1,0x11,0x90}
 */

/*
 *   Steps For Running Demo:
 *   
 *   1.  Connect door and Proto board up to each other.
 *   2.  Connect Airport Router to Ethernet.
 *   3.  Connect Macbook Pro to Airport Router.
 *   4.  Connect FRDM to Airport Router.
 *   5.  IP Address of 192.168.168.254:51837 worked for FRDM.
 *   6.  Start ./doorbridge.py -d 192.168.168.254:51837
 *   7.  Make note of MAC address on your laptop (which is what Justin needs I think).
 *   8.  At this point, on the MAC, "telnet <IP of Macbook> 51837 will work to comm with FRDM board from Macbook.
 */

/*
** Define IP address and IP network mask
*/
#ifndef ENET_IPADDR

//192.168.2.1
   #define ENET_IPADDR IPADDR(192,168,168,254)    // IP Address:  10.42.105.104
#endif

#ifndef ENET_IPMASK
   #define ENET_IPMASK  IPADDR(255,255,255,0) 
#endif

#ifndef ENET_IPGATEWAY
   #define ENET_IPGATEWAY  IPADDR(0,0,0,0) 
#endif

#ifndef ENET_MAC
   #define ENET_MAC  {0x00,0xA7,0xC5,0xF1,0x11,0x90}
#endif

#ifndef DEMO_PORT
    #define DEMO_PORT 51837 //IPPORT_TELNET
#endif

#define SERIAL_DEVICE BSP_DEFAULT_IO_CHANNEL
/* EOF */
