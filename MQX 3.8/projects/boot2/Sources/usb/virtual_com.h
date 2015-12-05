/******************************************************************************
 *
 * Freescale Semiconductor Inc.
 * (c) Copyright 2004-2009 Freescale Semiconductor, Inc.
 * ALL RIGHTS RESERVED.
 *
 **************************************************************************//*!
 *
 * @file virtual_com.h
 *
 * @author 
 *
 * @version 
 *
 * @date May-28-2009
 *
 * @brief The file contains Macro's and functions required for Virtual COM  
 *        Loopback Application
 *
 *****************************************************************************/

#ifndef _VIRTUAL_COM_H
#define _VIRTUAL_COM_H

#include "types.h"

/******************************************************************************
 * Constants - None
 *****************************************************************************/

/******************************************************************************
 * Macro's
 *****************************************************************************/
#define  CONTROLLER_ID      (0)   /* ID to identify USB CONTROLLER */ 

/* 
   USB_BUF_SIZE should be greater than or equal to the endpoint buffer size, 
   otherwise there will be data loss.
*/
#define  USB_BUF_SIZE     (64)

/*****************************************************************************
 * Global variables
 *****************************************************************************/

/*****************************************************************************
 * Global Functions
 *****************************************************************************/
extern void b2_recv_data (uint_8* recv_buf, uint_8 size);


void usb_app_init(void);
void usb_task(void);
int virtual_com_send (uint_8* sData, uint_8 dataLen);

#endif 
