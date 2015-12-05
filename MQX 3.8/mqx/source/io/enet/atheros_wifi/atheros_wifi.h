//------------------------------------------------------------------------------
// Copyright (c) 2011 Qualcomm Atheros, Inc.
// All Rights Reserved.
// Qualcomm Atheros Confidential and Proprietary.
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is
// hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================
#ifndef __ATHEROS_WIFI_H__
#define __ATHEROS_WIFI_H__

/* PORT_NOTE: atheros_wifi.h is intended to provide any system specific data 
 * 	structures and definitions required by the OS for a WIFI/Ethernet driver.
 * 	Hence everything in this file is Customized to the requirements of the 
 *	host OS. */

#include <wlan_config.h>
#include <io_gpio.h>
/* BSP_CONFIG_ATHEROS_PCB - represents the number of pre-allocated PCB's used
 * for interface rx packets.
 */
#define BSP_CONFIG_ATHEROS_PCB     WLAN_CONFIG_NUM_PRE_ALLOC_RX_BUFFERS

/* Structure used to configure Wifi Device,default parameters*/
#if NOT_WHISTLE
typedef struct param_wifi_struct {
   char_ptr                      SPI_DEVICE; // communication interface
   char_ptr                      GPIO_DEVICE; 
   GPIO_PIN_STRUCT               INT_PIN; // used by wifi chip to notify host 
   char_ptr                      PWD_DEVICE;   
   GPIO_PIN_STRUCT               PWD_PIN; // used to reset wifi device
}ATHEROS_PARAM_WIFI_STRUCT, * ATHEROS_PARAM_WIFI_STRUCT_PTR;
#else
typedef struct param_wifi_struct {
   char_ptr                      SPI_DEVICE; // communication interface
   GPIO_PIN_STRUCT               INT_PIN; // lwgpio pin used by wifi chip to notify host 
   GPIO_PIN_MUX                  INT_MUX; // lwgpio mux used by wifi chip to notify host 
   GPIO_PIN_STRUCT               PWD_PIN; // lwgpio pin used to reset wifi device
   GPIO_PIN_MUX                  PWD_MUX; // lwgpio mux used to reset wifi device
}ATHEROS_PARAM_WIFI_STRUCT, * ATHEROS_PARAM_WIFI_STRUCT_PTR;
#endif /* NOT_WHISTLE */

extern const ENET_MAC_IF_STRUCT ATHEROS_WIFI_IF;

#endif /* __ATHEROS_WIFI_H__ */
