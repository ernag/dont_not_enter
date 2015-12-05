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
#include "mqx.h"
#include "bsp.h"
#include "fio.h"

#include "HAL.h"                 /* Function for Hardware Abstraction.       */
#include "BTPSKRNL.h"            /* BTPS Kernel Header.                      */

#define VIRTUAL_FLASH_SIZE               512

#define HAL_PORT_SCG                     SIM_SCGC5_PORTA_MASK

#define HAL_LED_PORT                     PORTA_BASE_PTR
#define HAL_LED_GPIO                     PTA_BASE_PTR
#define HAL_LED_PIN                      29

static char VirtualFlash[VIRTUAL_FLASH_SIZE];

   /* Application Tasks.                                                */
static void MainApplicationTask(uint_32 initial_data);

   /* Note the following variable needs to be declared as global (and   */
   /* not static) because it is linked directly by the MQX libraries.   */

   /* Array that the MQX Framework uses to start Auto-executing task    */
   /* creation (all tasks with the MQX_AUTO_START_TASK attribute set    */
   /* will be started).  See the MQX User Guide for more information.   */
BTPSCONST TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
   /* Task Index, Function,            Stack,  Priority, Name,                Attributes,          Param, Time Slice */
   { 1,           MainApplicationTask, 2048,   5,        "BTMainApplication", MQX_AUTO_START_TASK, 0,     5},
   { 0 }
};

   /* The following function is used to illuminate an LED.  The number  */
   /* of LEDs on a board is board specific.  If the LED_ID provided does*/
   /* not exist on the hardware platform then nothing is done.          */
void HAL_LedOn(int LED_ID)
{
   if(LED_ID == 0)
      GPIO_PCOR_REG(HAL_LED_GPIO) |= (1 << HAL_LED_PIN);
}

   /* The following function is used to extinguish an LED.  The number  */
   /* of LEDs on a board is board specific.  If the LED_ID provided does*/
   /* not exist on the hardware platform then nothing is done.          */
void HAL_LedOff(int LED_ID)
{
   if(LED_ID == 0)
      GPIO_PSOR_REG(HAL_LED_GPIO) |= (1 << HAL_LED_PIN);
}

   /* The following function is used to toggle the state of an LED.  The*/
   /* number of LEDs on a board is board specific.  If the LED_ID       */
   /* provided does not exist on the hardware platform then nothing is  */
   /* done.                                                             */
void HAL_LedToggle(int LED_ID)
{
   if(LED_ID == 0)
      GPIO_PTOR_REG(HAL_LED_GPIO) |= (1 << HAL_LED_PIN);
}

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
   int i;

   for(i = 0; i < Length; i ++)
      putchar(Buffer[i]);

   return(Length);
}

   /* The following function is used to retrieve a specific number of   */
   /* bytes from some Non Volatile memory.                              */
int HAL_NV_DataRead(int Length, unsigned char *Buffer)
{
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
static void MainApplicationTask(uint_32 initial_data)
{
   PORT_PCR_REG(HAL_LED_PORT, HAL_LED_PIN) = PORT_PCR_MUX(1);
   GPIO_PDDR_REG(HAL_LED_GPIO) |= (1 << HAL_LED_PIN);
   GPIO_PSOR_REG(HAL_LED_GPIO) |= (1 << HAL_LED_PIN);

   SampleMain(NULL);

   BTPS_Delay(BTPS_INFINITE_WAIT);

   _mqx_exit(0);
}

