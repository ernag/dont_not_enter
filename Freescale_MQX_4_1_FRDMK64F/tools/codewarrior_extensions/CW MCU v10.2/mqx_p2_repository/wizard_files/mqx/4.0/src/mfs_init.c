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
*END************************************************************************/
#include "main.h"

/* allocate RAM DISK storage memory */
#if defined(RAM_DISK_SIZE) && !defined(RAM_DISK_BASE)
static unsigned char ram_disk[RAM_DISK_SIZE];
#define RAM_DISK_BASE ram_disk
#endif

/*FUNCTION*-----------------------------------------------------
* 
* Task Name    : Ram_disk_start
* Comments     :
*    This function installs and initializes ramdisk.
*
*END*-----------------------------------------------------*/

void Ram_disk_start(void)
{
   MQX_FILE_PTR               dev_handle1, a_fd_ptr;
   int32_t                     error_code;
   _mqx_uint                  mqx_status;

   a_fd_ptr = 0;

   /* Install device */
   mqx_status = _io_mem_install("mfs_ramdisk:", (unsigned char *)RAM_DISK_BASE, (_file_size)RAM_DISK_SIZE);
   if ( mqx_status != MQX_OK ) 
   {
      printf("Error installing memory device (0x%x)\n", mqx_status);
      _task_block();
   }

   /* Open the device which MFS will be installed on */
   dev_handle1 = fopen("mfs_ramdisk:", 0);
   if ( dev_handle1 == NULL ) 
   {
      printf("Unable to open Ramdisk device\n");
      _task_block();
   }

   /* Install MFS  */
   mqx_status = _io_mfs_install(dev_handle1, "a:", (_file_size)0);
   if (mqx_status != MFS_NO_ERROR) 
   {
      printf("Error initializing a:\n");
      _task_block();
   } 
   else 
   {
      printf("Ramdisk initialized to a:\\\n");
   }

   /* Open the filesystem and detect, if format is required */
   a_fd_ptr = fopen("a:", NULL);
   error_code = ferror(a_fd_ptr);
   if ((error_code != MFS_NO_ERROR) && (error_code != MFS_NOT_A_DOS_DISK))
   {
      printf("Error while opening a:\\ (%s)\n", MFS_Error_text((uint32_t)(uint32_t)error_code));
      _task_block();
   }
   if (error_code == MFS_NOT_A_DOS_DISK) 
   {
      printf("NOT A DOS DISK! You must format to continue.\n");
   }
}
