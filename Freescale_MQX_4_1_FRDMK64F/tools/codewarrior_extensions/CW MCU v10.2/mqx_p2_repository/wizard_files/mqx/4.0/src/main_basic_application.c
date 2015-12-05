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

#include "main.h"

$mainc_mfs_ifdef_usesmfs$
$mainc_mfs_ifdef_ramdisksize$
$mainc_shell_cmdstruct_begin$
$mainc_mfs_shellcmds$
$mainc_rtcs_shellcmds$
$mainc_shell_cmdstruct_end$
$mainc_rtcs_tellnet_shellcmdstruct$

TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
/*  Task number, Entry point, Stack, Pri, String, Auto? */
   {MAIN_TASK,   Main_task,   2000,  9,   "main", MQX_AUTO_START_TASK},
   {0,           0,           0,     0,   0,      0,                 }
};

/*TASK*-----------------------------------------------------------------
*
* Function Name  : Main_task
* Comments       :
*    This task initializes MFS and starts SHELL.
*
*END------------------------------------------------------------------*/

void Main_task(uint32_t initial_data)
{
   $mainc_mfs_ramdiskstart_call$  
   $mainc_rtcs_init_call$   
   $mainc_usbd_init_call$    
   $mainc_usbh_init_call$

   /*******************************
   * 
   * START YOUR CODING HERE
   *
   ********************************/   
   $mainc_shell_for$
   $mainc_not_shell$
}
/* EOF */
