/*****< HAL.c >***************************************************************/
/*      Copyright 2011 - 2013 Stonestreet One.                               */
/*      All Rights Reserved.                                                 */
/*                                                                           */
/*  HAL - Hardware Abstraction function for ST STM3240G-EVAL Board           */
/*                                                                           */
/*  Author:  Tim Cook                                                        */
/*                                                                           */
/*** MODIFICATION HISTORY ****************************************************/
/*                                                                           */
/*   mm/dd/yy  F. Lastname    Description of Modification                    */
/*   --------  -----------    -----------------------------------------------*/
/*   10/28/11  T. Cook        Initial creation.                              */
/*****************************************************************************/

/* Library includes. */
#include "wassert.h"
#include "mqx.h"
#include "bsp.h"
#include "fio.h"

#include "HAL.h"                 /* Function for Hardware Abstraction.       */
#include "BTPSKRNL.h"            /* BTPS Kernel Header.                      */

#define VIRTUAL_FLASH_SIZE               512

#define HAL_PORT_SCG                     SIM_SCGC5_PORTA_MASK

static char VirtualFlash[VIRTUAL_FLASH_SIZE];

   /* Application Tasks.                                                */
static void BTStackTask(uint_32 initial_data);

   /* The following function is used to retrieve data from the UART     */
   /* input queue.  the function receives a pointer to a buffer that    */
   /* will receive the UART characters a the length of the buffer.  The */
   /* function will return the number of characters that were returned  */
   /* in Buffer.                                                        */
int HAL_ConsoleRead(int Length, char *Buffer)
{
   int     ret_val;
   boolean CharAvailable;

   if((Length) && (Buffer))
   {
      /* Read characters into the buffer until none are available or    */
      /* the specified Length has been reached.                         */
      ret_val = 0;
      while((ret_val < Length) && (ioctl(stdin, IO_IOCTL_CHAR_AVAIL, &CharAvailable) == MQX_OK) && (CharAvailable))
      {
         *Buffer = (char)getchar();
         Buffer  ++;
         ret_val ++;
      }
   }
   else
      ret_val = 0;

   return(ret_val);
}

   /* The following function is used to send data to the UART output    */
   /* queue.  the function receives a pointer to a buffer that will     */
   /* contains the data to send and the length of the data.  The        */
   /* function will return the number of characters that were           */
   /* successfully saved in the output buffer.                          */
int HAL_ConsoleWrite(int Length, char *Buffer)
{
	wassert(Length == strlen(Buffer));
	corona_print("BT: %s", Buffer);

   return(Length);
}

   /* The following function is used to retrieve a specific number of   */
   /* bytes from some Non Volatile memory.                              */
int HAL_NV_DataRead(int Length, unsigned char *Buffer)
{
   wassert(0);

   if(Length > VIRTUAL_FLASH_SIZE)
      Length = VIRTUAL_FLASH_SIZE;

   BTPS_MemCopy(Buffer, VirtualFlash, Length);

   return(Length);
}

   /* The following function is used to save a specific number of bytes */
   /* to some Non Volatile memory.                                      */
int HAL_NV_DataWrite(int Length, unsigned char *Buffer)
{
   int ret_val;
   
   wassert(0);

   if(Length <= VIRTUAL_FLASH_SIZE)
   {
      BTPS_MemCopy(VirtualFlash, Buffer, Length);
      ret_val = 0;
   }
   else
      ret_val = -1;

   return(ret_val);
}

   /* The following function is the main user interface thread.  It     */
   /* opens the Bluetooth Stack and then drives the main user interface.*/
void BTSampleAppTask(uint_32 initial_data)
{
   wassert(0);

   ISampleMain(NULL);

   BTPS_Delay(BTPS_INFINITE_WAIT);

   _mqx_exit(0);
}

