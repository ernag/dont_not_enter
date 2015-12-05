/*****< iacptran.c >***********************************************************/
/*      Copyright 2001 - 2013 Stonestreet One.                                */
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
/*   03/01/13  S. Hishmeh     Ported to support MQX OS on TWR-K60N512.        */
/******************************************************************************/
#include <mqx.h>                 /* MQX RTOS includes.                        */
#include <bsp.h>                 /* MQX Board Support Package includes.       */
#include <i2c.h>                 /* MQX I2C driver.                           */
#include <i2c_ki2c.h>            /* MQX Kinetis Processor-Family I2C includes.*/
#include "wassert.h"
#include "BTPSKRNL.h"            /* Bluetooth Kernel Prototypes/Constants.    */
#include "IACPTRAN.h"            /* ACP Transport Prototypes/Constants.       */
#include "i2c1.h"                /* WHISTLE:  Play nice with the ROHM chip on same i2c bus */

   /* The following definition specifies the coprocessor's write address*/
   /* when the RST line is tied to ground.  The actual address is 0x20  */
   /* but the MQX I2C driver places this address in first 7 bits of the */
   /* address field, which essentially shifts if left one, so we shift  */
   /* right one here to account for the driver's behavior.              */
#define COPROCESSOR_WRITE_ADDRESS (0x22 >> 1)

   /* The maximum number of attempts to "ping" coprocessor and wait for */
   /* an acknowledge response.                                          */
#define MAX_NUM_ACK_ATTEMPTS  100

   /* The amount of time in microseconds to wait between attempts to    */
   /* ping the coprocessor.                                             */
#define NACK_WAIT_DELAY_US    500

   /* The following variable stores a pointer to the I2C device file.   */
static FILE *g_MFI_I2C1_fp = NULL;

   /* Helper functions for arbitrating I2C1 bus access between MFI & ROHM */
static void _MFI_I2C1_Start(void);
static void _MFI_I2C1_Cleanup(void);

   /* The following functions delays for the specified number of        */
   /* microseconds.                                                     */
   /* * NOTE * The accuracy of this function is limited by the          */
   /*          capabilities of the operating system and it may delay for*/
   /*          a longer amount of time than the requested number of     */
   /*          microseconds.  It will not, however, delay for a smaller */
   /*          amount of time than what was requested.                  */
static void DelayMicroseconds(unsigned int Microseconds)
{
   MQX_TICK_STRUCT Ticks = _mqx_zero_tick_struct;

   /* Add the specified number of microseconds to the ticks struct.     */
   _time_add_usec_to_ticks(&Ticks, Microseconds);

   /* Delay for the specified time.                                     */
   _time_delay_for(&Ticks);
}

   /* The following function checks if the coprocessor acknowledged the */
   /* last operation.  This function returns TRUE if the coprocessor    */
   /* acknowledged and FASLE if it did not.                             */
static Boolean_t CheckAckFromCoprocessor(void)
{
   Boolean_t Result;
   int       Acknowledge;
   
   /*
    *   We don't need to re-request access to I2C1 bus, but at least check that
    *   there's a valid pointer here.
    */
   wassert( g_MFI_I2C1_fp != NULL );

   /* Send the request to flush the output.                             */
   if(ioctl(g_MFI_I2C1_fp, IO_IOCTL_FLUSH_OUTPUT, &Acknowledge) == I2C_OK)
   {
      /* The driver writes the status of the flush, i.e. if the device  */
      /* acknowledged the operation, to the parameter of ioctl().       */
      Result = !Acknowledge;
   }
   else
   {
      Result = FALSE;
   }
   
   //wassert(Result); // Seems like the stack needs to retry, so don't make this fatal.

   return(Result);
}

/*
 *   Request and wait for access to the I2C1 bus.
 */
static void _MFI_I2C1_Start(void)
{
    I2C1_dibbs( &g_MFI_I2C1_fp );
}

/*
 *   Free access to I2C1 bus, so ROHM has a chance to use it.
 */
static void _MFI_I2C1_Cleanup(void)
{
    wassert( ERR_OK == I2C1_liberate( &g_MFI_I2C1_fp ) );
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
	/*
     *   Ernie:  We can't control when this gets called, so I'm using
     *           _MFI_I2C1_Start() instead to arbitrate access to the
     *           I2C1 bus, so we don't starve the ROHM chip.
     */
    return 0;
}

   /* The following function is responsible for performing any cleanup  */
   /* that may be required by the hardware or module.  This function    */
   /* returns no status.                                                */
void BTPSAPI IACPTR_Cleanup(void)
{
    /*
     *   Ernie:  This function doesn't get called, and therefore blocks
     *           the ROHM chip, which shares the same bus.  I'm using
     *           _MFI_I2C1_Cleanup() instead.
     */
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
   /*          placed in the Status parameter to deterimine the actual  */
   /*          status of the operation (success or failure).  This      */
   /*          means that a successful return value will be a           */
   /*          non-negative return value and the status member will     */
   /*          contain the value:                                       */
   /*             IACP_STATUS_SUCCESS                                   */
int BTPSAPI IACPTR_Read(unsigned char Register, unsigned char BytesToRead, unsigned char *ReadBuffer, unsigned char *Status)
{
   int       Result;
   int       NumAttempts;
   Boolean_t Acknowledged;
   int       NumRead;
   int       Param;
   
   _MFI_I2C1_Start();

   /* Specify the coprocessor's device address to the MQX I2C driver.   */
   Param = COPROCESSOR_WRITE_ADDRESS;
   ioctl(g_MFI_I2C1_fp, IO_IOCTL_I2C_SET_DESTINATION_ADDRESS, &Param);

   /* Verify that the parameters passed in appear valid.                */
   if((BytesToRead) && (ReadBuffer) && (Status) && (g_MFI_I2C1_fp))
   {
      /* Initialize the function's return value.                        */
      Result = IACP_RESPONSE_TIMEOUT;

      /* Initialize the status value.                                   */
      *Status = IACP_STATUS_READ_FAILURE;

      NumAttempts = 0;

      do
      {
         if(NumAttempts > 0)
         {
            /* Send the repeated start sequence.  The first start       */
            /* sequence is automatically handled by the driver when the */
            /* coprocessor address is output on the I2C lines.          */
            ioctl(g_MFI_I2C1_fp, IO_IOCTL_I2C_REPEATED_START, NULL);
         }

         /* Send the write address of the coprocessor by writing 0 bytes*/
         /* to the device driver.                                       */
         (void)fwrite(&Param, 1, 0, g_MFI_I2C1_fp);

         /* Check for an acknowledgment from the coprocessor.           */
         Acknowledged = CheckAckFromCoprocessor();

         /* Delay if an acknowledgment was not received.                */
         if(!Acknowledged)
            DelayMicroseconds(NACK_WAIT_DELAY_US);

         NumAttempts++;

         /* Loop while the coprocessor has not acknowledged and we have */
         /* not exceeded the maximum number of attempts.                */
      } while((!Acknowledged) && (NumAttempts < MAX_NUM_ACK_ATTEMPTS));

      if(Acknowledged)
      {
         /* Send the register address at which to begin reading.        */
         (void)fwrite(&Register, 1, 1, g_MFI_I2C1_fp);

         NumAttempts = 0;

         do
         {
            /* Send the repeated start sequence.                        */
            ioctl(g_MFI_I2C1_fp, IO_IOCTL_I2C_REPEATED_START, NULL);

            /* Set up a read request for the driver.                    */
            Param = BytesToRead;
            ioctl(g_MFI_I2C1_fp, IO_IOCTL_I2C_SET_RX_REQUEST, &Param);

            /* Read the specified number of bytes.  This function       */
            /* handles sending the read address and clocking in the     */
            /* bytes.  If the coprocessor does not acknowledge its read */
            /* address this function will not attempt to clock in the   */
            /* data and will return 0.                                  */
            NumRead = fread(ReadBuffer, 1, BytesToRead, g_MFI_I2C1_fp);

            /* If the number of bytes read is not equal to the number of*/
            /* bytes specified to read then the device did not          */
            /* acknowledge its read address.                            */
            Acknowledged = NumRead == BytesToRead ? TRUE : FALSE;

            /* Delay if an acknowledgment was not received.             */
            if(!Acknowledged)
               DelayMicroseconds(NACK_WAIT_DELAY_US);

            NumAttempts++;

            /* Loop while the coprocessor has not acknowledged and we   */
            /* have not exceeded the maximum number of attempts.        */
         } while((!Acknowledged) && (NumAttempts < MAX_NUM_ACK_ATTEMPTS));

         /* Send I2C the stop sequence.                                 */
         ioctl(g_MFI_I2C1_fp, IO_IOCTL_I2C_STOP, NULL);

         if(NumRead == BytesToRead)
         {
            /* If the number of bytes read is equal to the number of    */
            /* bytes specified to read then flag the status byte as     */
            /* success.                                                 */
            *Status = IACP_STATUS_SUCCESS;
         }

         /* Set the return value to success.                            */
         Result = 0;
      }
   }
   else
      Result = IACP_INVALID_PARAMETER;

   _MFI_I2C1_Cleanup();
   
   wassert(0 == Result);

   return(Result);
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
   /*          placed in the Status parameter to deterimine the actual  */
   /*          status of the operation (success or failure).  This      */
   /*          means that a successful return value will be a           */
   /*          non-negative return value and the status member will     */
   /*          contain the value:                                       */
   /*             IACP_STATUS_SUCCESS                                   */
int BTPSAPI IACPTR_Write(unsigned char Register, unsigned char BytesToWrite, unsigned char *WriteBuffer, unsigned char *Status)
{
   int            Result;
   int            NumAttempts;
   int            NumWritten;
   unsigned char *TempBuffer;
   Boolean_t      Acknowledged;
   int            Address;
   
   _MFI_I2C1_Start();

   /* We have to add the register address to the send buffer so allocate*/
   /* a new buffer for this.                                            */
   TempBuffer = BTPS_AllocateMemory(BytesToWrite + 1);

   if(TempBuffer)
   {
      /* Copy the data to the new buffer.                               */
      TempBuffer[0] = Register;
      BTPS_MemCopy(&TempBuffer[1], WriteBuffer, BytesToWrite);
      BytesToWrite += 1;

      /* Specify the coprocessor's device address to the MQX I2C driver.*/
      Address = COPROCESSOR_WRITE_ADDRESS;
      ioctl(g_MFI_I2C1_fp, IO_IOCTL_I2C_SET_DESTINATION_ADDRESS, &Address);

      NumAttempts = 0;

      do
      {
         if(NumAttempts > 0)
         {
            /* Send the repeated start sequence.  The first start       */
            /* sequence is automatically handled by the driver when the */
            /* coprocessor address is output on the I2C lines.          */
            ioctl(g_MFI_I2C1_fp, IO_IOCTL_I2C_REPEATED_START, NULL);
         }

         /* Write the specified number of bytes.  This function handles */
         /* sending the write address and clocking out the bytes.  If   */
         /* the coprocessor does not acknowledge its write address this */
         /* function will not attempt to clock out the data and will    */
         /* return 0.                                                   */
         NumWritten = fwrite(TempBuffer, 1, BytesToWrite, g_MFI_I2C1_fp);

         /* If the number of bytes written is not equal to the number of*/
         /* bytes specified to write then the device did not acknowledge*/
         /* its write address.                                          */
         Acknowledged = NumWritten == BytesToWrite ? TRUE : FALSE;

         /* Delay if an acknowledgment was not received.                */
         if(!Acknowledged)
            DelayMicroseconds(NACK_WAIT_DELAY_US);

         NumAttempts++;

         /* Loop while the coprocessor has not acknowledged and we have */
         /* not exceeded the maximum number of attempts.                */
      } while((!Acknowledged) && (NumAttempts < MAX_NUM_ACK_ATTEMPTS));

      /* Send the stop sequence.                                        */
      ioctl(g_MFI_I2C1_fp, IO_IOCTL_I2C_STOP, NULL);

      /* Free the allocated buffer.                                     */
      BTPS_FreeMemory(TempBuffer);

      if(NumWritten == BytesToWrite)
      {
         /* If the number of bytes written is equal to the number of    */
         /* bytes specified to write then flag the status byte as       */
         /* success.                                                    */
         *Status = IACP_STATUS_SUCCESS;
      }
      else
      {
         /* If the number of bytes written is not equal to the number of*/
         /* bytes specified to write then flag the error in the status  */
         /* byte.                                                       */
         *Status = IACP_STATUS_WRITE_FAILURE;
      }

      /* Set the return value to success.                               */
      Result = 0;
   }
   else
      Result = IACP_ALLOCATION_ERROR;

   _MFI_I2C1_Cleanup();
   
   wassert(0 == Result);

   return(Result);
}

   /* The following function is responsible for resetting the ACP       */
   /* hardware. The implementation should not return until the reset is */
   /* complete.                                                         */
   /* * NOTE * This function only applies to chips version 2.0B and     */
   /*          earlier. For chip version 2.0C and newer, this function  */
   /*          should be implemented as a NO-OP.                        */
void BTPSAPI IACPTR_Reset(void)
{
}
