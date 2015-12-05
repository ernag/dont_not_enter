/*****< HAL.h >****************************************************************/
/*      Copyright 2011 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  HAL - Hardware Abstraction functions for Stellaris LM3S9B96 Board         */
/*                                                                            */
/*  Author:  Tim Thomas                                                       */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   07/05/11  Tim Thomas     Initial creation.                               */
/******************************************************************************/
#ifndef HAL_H_
#define HAL_H_

#include <mqx.h>
#include <bsp.h>
#include <fio.h>

   /* The following function is used to retrieve data from the UART     */
   /* input queue.  the function receives a pointer to a buffer that    */
   /* will receive the UART characters a the length of the buffer.  The */
   /* function will return the number of characters that were returned  */
   /* in Buffer.                                                        */
int HAL_ConsoleRead(int Length, char *Buffer);

   /* The following function is used to send data to the UART output    */
   /* queue.  the function receives a pointer to a buffer that will     */
   /* contains the data to send and the length of the data.  The        */
   /* function will return the number of characters that were           */
   /* successfully saved in the output buffer.                          */
int HAL_ConsoleWrite(int Length, char *Buffer);

   /* The following function is used to retrieve a specific number of   */
   /* bytes from some Non Volatile memory.                              */
int HAL_NV_DataRead(int Length, unsigned char *Buffer);

   /* The following function is used to save a specific number of bytes */
   /* to some Non Volatile memory.                                      */
int HAL_NV_DataWrite(int Length, unsigned char *Buffer);

   /* The following function represents the samples main application    */
   /* thread to be started once the hardware is initialized.  It is not */
   /* defined within the platform but should instead exists within each */
   /* sample application.                                               */
void SampleMain(void *UserParameter);

#endif
