/*****< iacptran.c >***********************************************************/
/*      Copyright 2011 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  IACPTRAN - Stonestreet One Apple Authentication Coprocessor Transport     */
/*             Layer.                                                         */
/*                                                                            */
/*  Author:  Tim Thomas                                                       */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   01/13/11  T. Thomas      Initial creation.                               */
/******************************************************************************/
#include "BTPSKRNL.h"            /* Bluetooth Kernel Prootypes/Constants.     */
#include "IACPTRAN.h"            /* ACP Transport Prototypes/Constants.       */

   /* I2C 7-bit address for the Apple co-processor.  will be either 0x10*/
   /* or 0x11 depending on hardware configuration.                      */
#define IACPTR_MFI_ADDRESS 0x11

static int I2CWrite(unsigned char Address, unsigned int Length, unsigned char *Buffer);
static int I2CRead(unsigned char Address, unsigned int Length, unsigned char *Buffer);
static void DelayMicroseconds(unsigned int Microseconds);

   /* The following funciton is used to delay operation for the         */
   /* specified number of microseconds.                                 */
static void DelayMicroseconds(unsigned int Microseconds)
{
//xxx
}

   /* The following function performs a I2C write opertion.  The first  */
   /* paramter is the address to be written to, the second paramter is  */
   /* the length of the data to be written and the third paramter is the*/
   /* data to write.  This function returns 1 if successful or 0 if     */
   /* there was an error.                                               */
static int I2CWrite(unsigned char Address, unsigned int Length, unsigned char *Buffer)
{
//xxx
}

   /* The following function performs a I2C read opertion.  The first   */
   /* paramter is the address to read from, the second paramter is the  */
   /* length of the data to be read and the third paramter is the buffer*/
   /* to store the data read.  This function returned the number of     */
   /* bytes read or zero if there was an error.                         */
static int I2CRead(unsigned char Address, unsigned int Length, unsigned char *Buffer)
{
//xxx
}

   /* The following function is responsible for Initializing the ACP    */
   /* hardware.  The function is passed a pointer to an opaque object.  */
   /* Since the hardware platform is not know, it is intended that the  */
   /* parameter be used to pass hardware specific information to the    */
   /* module that would be needed for proper operation.  Each           */
   /* implementation will define what gets passed to this function.     */
   /* This function returns zero if successful or a negative value if   */
   /* the initialization fails.                                         */
int BTPSAPI IACPTR_Initialize(void *Parameters)
{
//xxx
   return(0);
}

   /* The following function is responsible for performing any cleanup  */
   /* that may be required by the hardware or module.  This function    */
   /* returns no status.                                                */
void BTPSAPI IACPTR_Cleanup(void)
{
//xxx
}

   /* The following function is responsible for Reading Data from a     */
   /* device via the ACP interface.  The first parameter to this        */
   /* function is the Register/Address that is to be read.  The second  */
   /* parameter indicates the number of bytes that are to be read from  */
   /* the device.  The third parameter is a pointer to a buffer where   */
   /* the data read is placed.  The size of the Read Buffer must be     */
   /* large enough to hold the number of bytes being read.  The last    */
   /* parameter is a pointer to a variable that will receive status     */
   /* information about the result of the read operation.  This function*/
   /* should return a non-negative return value if successful (and      */
   /* return IACP_STATUS_SUCCESS in the status parameter).  This        */
   /* function should return a negative return error code if there is an*/
   /* error with the parameters and/or module initialization.  Finally, */
   /* this function can also return success (non-negative return value) */
   /* as well as specifying a non-successful Status value (to denote    */
   /* that there was an actual error during the write process, but the  */
   /* module is initialized and configured correctly).                  */
   /* * NOTE * If this function returns a non-negative return value     */
   /*          then the caller will ALSO examine the value that was     */
   /*          placed in the Status parameter to determine the actual   */
   /*          status of the operation (success or failure).  This      */
   /*          means that a successful return value will be a           */
   /*          non-negative return value and the status member will     */
   /*          contain the value:                                       */
   /*             IACP_STATUS_SUCCESS                                   */
int BTPSAPI IACPTR_Read(unsigned char Register, unsigned char BytesToRead, unsigned char *ReadBuffer, unsigned char *Status)
{
   int Result;
   int ret_val;
   int RetryCount;

   /* Verify that the parameters passed in appear valid.                */
   if((BytesToRead) && (ReadBuffer) && (Status))
   {
      *Status = IACP_STATUS_READ_FAILURE;

      RetryCount = 100;
      do
      {
         if((Result = I2CWrite(IACPTR_MFI_ADDRESS, 1, &Register)) == 0)
            DelayMicroseconds(500);
      }
      while((RetryCount--) && (!Result);

      if(Result)
      {
         RetryCount = 100;
         do
         {
            if((Result = I2CRead(IACPTR_MFI_ADDRESS, BytesToRead, ReadBuffer)) == 0)
               DelayMicroseconds(500);
         }
         while((RetryCount--) && (!Result));

         if(Result)
            *Status = 0;
         else
            *Status = IACP_STATUS_READ_FAILURE;
      }
      else
         *Status = IACP_STATUS_READ_FAILURE;

      ret_val = 0;
   }
   else
      ret_val = IACP_INVALID_PARAMETER;

   return(ret_val);
}

   /* The following function is responsible for Writing Data to a device*/
   /* via the ACP interface.  The first parameter to this function is   */
   /* the Register/Address where the write operation is to start.  The  */
   /* second parameter indicates the number of bytes that are to be     */
   /* written to the device.  The third parameter is a pointer to a     */
   /* buffer where the data to be written is placed.  The last parameter*/
   /* is a pointer to a variable that will receive status information   */
   /* about the result of the write operation.  This function should    */
   /* return a non-negative return value if successful (and return      */
   /* IACP_STATUS_SUCCESS in the status parameter).  This function      */
   /* should return a negative return error code if there is an error   */
   /* with the parameters and/or module initialization.  Finally, this  */
   /* function can also return success (non-negative return value) as   */
   /* well as specifying a non-successful Status value (to denote that  */
   /* there was an actual error during the write process, but the       */
   /* module is initialized and configured correctly).                  */
   /* * NOTE * If this function returns a non-negative return value     */
   /*          then the caller will ALSO examine the value that was     */
   /*          placed in the Status parameter to determine the actual   */
   /*          status of the operation (success or failure).  This      */
   /*          means that a successful return value will be a           */
   /*          non-negative return value and the status member will     */
   /*          contain the value:                                       */
   /*             IACP_STATUS_SUCCESS                                   */
int BTPSAPI IACPTR_Write(unsigned char Register, unsigned char BytesToWrite, unsigned char *WriteBuffer, unsigned char *Status)
{
   int Result;
   int ret_val;
   int RetryCount;

   /* Verify that the parameters passed in appear valid.                */
   if((BytesToWrite) && (WriteBuffer) && (Status))
   {
      RetryCount = 100;
      Result = !NU_SUCCESS;
      do
      {
         if((Result = I2CWrite(IACPTR_MFI_ADDRESS, 1, &Register)) == 0)
            DelayMicroseconds(500);
      }
      while((RetryCount--) && (!Result));

      if(Result)
         *Status = 0;
      else
         *Status = IACP_STATUS_WRITE_FAILURE;

      ret_val = 0;
   }
   else
      ret_val = IACP_INVALID_PARAMETER;

   return(ret_val);
}

   /* The following function is responsible for resetting the ACP       */
   /* hardware. The implementation should not return until the reset is */
   /* complete.                                                         */
   /* * NOTE * This function only applies to chips version 2.0B and     */
   /*          earlier. For chip version 2.0C and newer, this function  */
   /*          should be implemented as a NO-OP.                        */
void BTPSAPI IACPTR_Reset(void)
{
//xxx
}

