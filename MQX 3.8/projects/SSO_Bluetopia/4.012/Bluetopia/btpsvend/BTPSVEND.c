/*****< btpsvend.c >***********************************************************/
/*      Copyright 2008 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  BTPSVEND - Vendor specific functions/definitions used to define a set of  */
/*             vendor specific functions supported by the Bluetopia Protocol  */
/*             Stack.  These functions may be unique to a given hardware      */
/*             platform.                                                      */
/*                                                                            */
/*  Author:  Dave Wooldridge                                                  */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   03/14/08  D. Wooldridge  Initial creation.                               */
/******************************************************************************/
#include "SS1BTPS.h"          /* Bluetopia API Prototypes/Constants.          */
#include "BTPSKRNL.h"
#include "BTPSVEND.h"
#include "Patch_PAN1316.h"

   /* Miscellaneous Type Declarations.                                  */
#define VENDOR_DEFAULT_BAUDRATE  115200

   /* The following is the binary representation of a patch to configure*/
   /* and enable HCILL.  It configure HCILL to the following parameters:*/
   /*   Inactivity Timeout                        = 100ms               */
   /*   WakeUp indication retransmission timeout = 500ms                */
   /*   RTS pulse width                           = 150us               */
static BTPSCONST unsigned char HCILLPatch[] =
{
   0x01, 0x2B, 0xFD, 0x05, 0x50, 0x00, 0x90, 0x01, 0x96,
   0x01, 0x0C, 0xFD, 0x09, 0x01, 0x01, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00
};

   /* The following stores the correct driver information for use by    */
   /* functions that need to reconfigure the driver.                    */
static HCI_COMMDriverInformation_t HCI_COMMDriverInformation;

   /* The following variable is used to track whether or not the Vendor */
   /* Specific Commands (and Patch RAM commands) have already been      */
   /* issued to the device.  This is done so that we do not issue them  */
   /* more than once for a single device (there is no need).  They could*/
   /* be potentially issued because the HCI Reset hooks (defined below) */
   /* are called for every HCI_Reset() that is issued.                  */
static Boolean_t VendorCommandsIssued;

   /* Internal Function Prototypes.                                     */
static Boolean_t DownloadPatch(unsigned int BluetoothStackID, unsigned int PatchLength, BTPSCONST unsigned char *PatchPointer);

   /* The following function is provided to allow a mechanism to        */
   /* download the specified Patch Data to the CC25xx device.  This     */
   /* function does not disable the co-processor.  This function returns*/
   /* TRUE if successful or FALSE if there was an error.                */
static Boolean_t DownloadPatch(unsigned int BluetoothStackID, unsigned int PatchLength, BTPSCONST unsigned char *PatchPointer)
{
   int            Result;
   int            Cnt;
   unsigned char  Status;
   unsigned char  OGF;
   unsigned char  Length;
   unsigned short OCF;
   Boolean_t      ret_val;
   unsigned char  Buffer[128];

   /* First, make sure the input parameters appear to be semi-valid.    */
   if((BluetoothStackID) && (PatchPointer))
   {
      ret_val = TRUE;

      /* Clear the status value and initialize all variables.           */
      Cnt      = 0;

      while((PatchLength) && (ret_val))
      {
         /* Confirm that the first parameter is what we expect.         */
         if(PatchPointer[0] == 0x01)
         {
            /* Get the OGF and OCF values an make a call to perform the */
            /* vendor specific function.                                */
            OGF     = PatchPointer[2] >> 2;
            OCF     = (READ_UNALIGNED_WORD_LITTLE_ENDIAN(&PatchPointer[1]) & 0x3FF);
            Length  = sizeof(Buffer);
            Result = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, PatchPointer[3], (unsigned char*)&PatchPointer[4], &Status, &Length, Buffer, TRUE);
            Cnt++;

            /* If the function was successful, update the count and     */
            /* pointer for the next command.                            */
            if((Result < 0) || (Status != 0))
            {
               DBG_MSG(DBG_ZONE_VENDOR, ("PatchDevice[%d] Result = %d Status %d\r\n", Cnt, ret_val, Status));
               DBG_DUMP(DBG_ZONE_VENDOR, (PatchPointer[3], &PatchPointer[4]));

               ret_val = FALSE;
            }

            /* Advance to the next Patch Entry.                         */
            PatchLength -= (PatchPointer[3] + 4);
            PatchPointer += (PatchPointer[3] + 4);
         }
         else
            ret_val = FALSE;
      }
   }
   else
      ret_val = FALSE;

   return(ret_val);
}

   /* The following function represents the vendor specific function to */
   /* change the baud rate.  It will format and send the vendor specific*/
   /* command to change the baseband's baud rate then issue a           */
   /* reconfigure command to the HCI driver to change the local baud    */
   /* rate.  This function will return a BOOLEAN TRUE upon success or   */
   /* FALSE if there is an error.                                       */
Boolean_t HCI_VS_SetBaudRate(unsigned int BluetoothStackID, unsigned int NewBuadRate)
{
   Boolean_t                     ret_val;
   int                           Result;
   Byte_t                        Length;
   Byte_t                        Status;
   Byte_t                        DataBuffer[sizeof(DWord_t)];
   Byte_t                        ReceiveBuffer[16];
   HCI_Driver_Reconfigure_Data_t DriverReconfigureData;


   /* First check to see if the parameters required for the execution of*/
   /* this function appear to be semi-valid.                            */
   if((BluetoothStackID) && (NewBuadRate))
   {
      /* Write the Baud Rate.                                           */
      ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD(DataBuffer, NewBuadRate);

      /* Next, write the command to the device.                         */
      Length  = sizeof(ReceiveBuffer);
      Result = HCI_Send_Raw_Command(BluetoothStackID, 0x3F, 0x0336, sizeof(DWord_t), DataBuffer, &Status, &Length, ReceiveBuffer, TRUE);

      if((!Result) && (!Status))
      {
         /* We were successful, now we need to change the baudrate of   */
         /* the driver.                                                 */
         HCI_COMMDriverInformation.BaudRate = NewBuadRate;
         DriverReconfigureData.ReconfigureCommand = HCI_COMM_DRIVER_RECONFIGURE_DATA_COMMAND_CHANGE_PARAMETERS;
         DriverReconfigureData.ReconfigureData    = (void *)&HCI_COMMDriverInformation;

         Result = HCI_Reconfigure_Driver(BluetoothStackID, FALSE, &(DriverReconfigureData));

         if(Result >= 0)
         {
            DBG_MSG(DBG_ZONE_VENDOR, ("HCI_VS_SetBaudRate Success.\r\n"));
            ret_val = TRUE;
         }
         else
         {
            DBG_MSG(DBG_ZONE_VENDOR, ("HCI_VS_SetBaudRate Failed: Failed to reconfigure the driver.\r\n"));
            ret_val = FALSE;
         }
      }
      else
      {
         DBG_MSG(DBG_ZONE_VENDOR, ("HCI_VS_SetBaudRate Failed: Failed to configure the controller.\r\n"));
         ret_val = FALSE;
      }
   }
   else
      ret_val = FALSE;

   return(ret_val);
}

   /* The following function prototype represents the vendor specific   */
   /* function which is used to implement any needed Bluetooth device   */
   /* vendor specific functionality that needs to be performed before   */
   /* the HCI Communications layer is opened.  This function is called  */
   /* immediately prior to calling the initialization of the HCI        */
   /* Communications layer.  This function should return a BOOLEAN TRUE */
   /* indicating successful completion or should return FALSE to        */
   /* indicate unsuccessful completion.  If an error is returned the    */
   /* stack will fail the initialization process.                       */
   /* * NOTE * The parameter passed to this function is the exact       */
   /*          same parameter that was passed to BSC_Initialize() for   */
   /*          stack initialization.  If this function changes any      */
   /*          members that this pointer points to, it will change the  */
   /*          structure that was originally passed.                    */
   /* * NOTE * No HCI communication calls are possible to be used in    */
   /*          this function because the driver has not been initialized*/
   /*          at the time this function is called.                     */
Boolean_t BTPSAPI HCI_VS_InitializeBeforeHCIOpen(HCI_DriverInformation_t *HCI_DriverInformation)
{
   /* Flag that we have not issued the first Vendor Specific Commands   */
   /* before the first reset.                                           */
   VendorCommandsIssued = FALSE;

   /* Store the driver information for later use.                       */
   BTPS_MemCopy(&HCI_COMMDriverInformation, &(HCI_DriverInformation->DriverInformation.COMMDriverInformation), sizeof(HCI_COMMDriverInformation_t));

   /* Make sure that the driver is initially configured to the default  */
   /* baud rate of the controller.                                      */
   HCI_DriverInformation->DriverInformation.COMMDriverInformation.BaudRate = VENDOR_DEFAULT_BAUDRATE;

   return(TRUE);
}

   /* The following function prototype represents the vendor specific   */
   /* function which is used to implement any needed Bluetooth device   */
   /* vendor specific functionality after the HCI Communications layer  */
   /* is initialized (the driver only).  This function is called        */
   /* immediately after returning from the initialization of the HCI    */
   /* Communications layer (HCI Driver).  This function should return a */
   /* BOOLEAN TRUE indicating successful completion or should return    */
   /* FALSE to indicate unsuccessful completion.  If an error is        */
   /* returned the stack will fail the initialization process.          */
   /* * NOTE * No HCI layer function calls are possible to be used in   */
   /*          this function because the actual stack has not been      */
   /*          initialized at this point.  The only initialization that */
   /*          has occurred is with the HCI Driver (hence the HCI       */
   /*          Driver ID that is passed to this function).              */
Boolean_t BTPSAPI HCI_VS_InitializeAfterHCIOpen(unsigned int HCIDriverID)
{
   return(TRUE);
}

   /* The following function prototype represents the vendor specific   */
   /* function which is used to implement any needed Bluetooth device   */
   /* vendor specific functions after the HCI Communications layer AND  */
   /* the HCI Stack layer has been initialized.  This function is called*/
   /* after all HCI functionality is established, but before the initial*/
   /* HCI Reset is sent to the stack.  The function should return a     */
   /* BOOLEAN TRUE to indicate successful completion or should return   */
   /* FALSE to indicate unsuccessful completion.  If an error is        */
   /* returned the stack will fail the initialization process.          */
   /* * NOTE * At the time this function is called HCI Driver and HCI   */
   /*          layer functions can be called, however no other stack    */
   /*          layer functions are able to be called at this time       */
   /*          (hence the HCI Driver ID and the Bluetooth Stack ID      */
   /*          passed to this function).                                */
Boolean_t BTPSAPI HCI_VS_InitializeBeforeHCIReset(unsigned int HCIDriverID, unsigned int BluetoothStackID)
{
   return(TRUE);
}

   /* The following function prototype represents the vendor specific   */
   /* function which is used to implement any needed Bluetooth device   */
   /* vendor specific functionality after the HCI layer has issued any  */
   /* HCI Reset as part of the initialization.  This function is called */
   /* after all HCI functionality is established, just after the initial*/
   /* HCI Reset is sent to the stack.  The function should return a     */
   /* BOOLEAN TRUE to indicate successful completion or should return   */
   /* FALSE to indicate unsuccessful completion.  If an error is        */
   /* returned the stack will fail the initialization process.          */
   /* * NOTE * At the time this function is called HCI Driver and HCI   */
   /*          layer functions can be called, however no other stack    */
   /*          layer functions are able to be called at this time (hence*/
   /*          the HCI Driver ID and the Bluetooth Stack ID passed to   */
   /*          this function).                                          */
Boolean_t BTPSAPI HCI_VS_InitializeAfterHCIReset(unsigned int HCIDriverID, unsigned int BluetoothStackID)
{
   Boolean_t  ret_val;

   /* Verify that the parameters that were passed in appear valid.      */
   if((!VendorCommandsIssued) && (HCIDriverID) && (BluetoothStackID))
   {
      DBG_MSG(DBG_ZONE_VENDOR, ("HCI_VS_InitializeAfterHCIReset\r\n"));

      if(HCI_COMMDriverInformation.BaudRate != VENDOR_DEFAULT_BAUDRATE)
      {
         /* First, change the baudrate to what was specified in the     */
         /* initialization structure.                                   */
         ret_val = HCI_VS_SetBaudRate(BluetoothStackID, HCI_COMMDriverInformation.BaudRate);

         BTPS_Delay(500);
      }
      else
         ret_val = TRUE;

     if(ret_val)
      {
         /* Next download the patch.                                    */
         ret_val = DownloadPatch(BluetoothStackID, sizeof(BasePatch), BasePatch);

#ifdef __SUPPORT_LOW_ENERGY__

         if(ret_val)
         {
            /* Download the LE Patch.                                */
            ret_val = DownloadPatch(BluetoothStackID, sizeof(LowEnergyPatch), LowEnergyPatch);
         }

#endif

         if((ret_val) && ((HCI_COMMDriverInformation.Protocol == cpHCILL) || (HCI_COMMDriverInformation.Protocol == cpHCILL_RTS_CTS)))
         {
            /* Now configure and enable HCILL                           */
            ret_val = DownloadPatch(BluetoothStackID, sizeof(HCILLPatch), HCILLPatch);
         }
      }

      VendorCommandsIssued = ret_val;
   }
   else
      ret_val = FALSE;

   /* Print Success/Failure status.                                     */
   DBG_MSG(DBG_ZONE_VENDOR, ("HCI_VS_InitializeAfterHCIReset %s\r\n", (ret_val) ? "Success" : "Failure"));

   return(ret_val);
}

   /* The following function prototype represents the vendor specific   */
   /* function which would is used to implement any needed Bluetooth    */
   /* device vendor specific functionality before the HCI layer is      */
   /* closed.  This function is called at the start of the HCI_Cleanup()*/
   /* function (before the HCI layer is closed), at which time all HCI  */
   /* functions are still operational.  The caller is NOT able to call  */
   /* any other stack functions other than the HCI layer and HCI Driver */
   /* layer functions because the stack is being shutdown (i.e.         */
   /* something has called BSC_Shutdown()).  The caller is free to      */
   /* return either success (TRUE) or failure (FALSE), however, it will */
   /* not circumvent the closing down of the stack or of the HCI layer  */
   /* or HCI Driver (i.e. the stack ignores the return value from this  */
   /* function).                                                        */
   /* * NOTE * At the time this function is called HCI Driver and HCI   */
   /*          layer functions can be called, however no other stack    */
   /*          layer functions are able to be called at this time (hence*/
   /*          the HCI Driver ID and the Bluetooth Stack ID passed to   */
   /*          this function).                                          */
Boolean_t BTPSAPI HCI_VS_InitializeBeforeHCIClose(unsigned int HCIDriverID, unsigned int BluetoothStackID)
{
   Boolean_t ret_val;
   unsigned char HCILLDisablePatch[sizeof(HCILLPatch)];
   unsigned int Length;
   unsigned int Offset;

   if((HCI_COMMDriverInformation.Protocol == cpHCILL) || (HCI_COMMDriverInformation.Protocol == cpHCILL_RTS_CTS))
   {
      /* Disable HCILL.                                                 */

      /* Use the size of the configuration portion of the HCILL patch as*/
      /* the offset for the enable/disable portion.                     */
      Offset = HCILLPatch[3] + 4;
      Length = sizeof(HCILLPatch) - Offset;

      BTPS_MemCopy(HCILLDisablePatch, (HCILLPatch + Offset), Length);
      HCILLDisablePatch[5] = 0;
      ret_val = DownloadPatch(BluetoothStackID, Length, HCILLDisablePatch);
   }
   else
      ret_val = TRUE;

   if(ret_val)
   {
      if(HCI_COMMDriverInformation.BaudRate != VENDOR_DEFAULT_BAUDRATE);
      {
         /* Set the Buadrate back to the baseband's default.            */
         ret_val = HCI_VS_SetBaudRate(BluetoothStackID, VENDOR_DEFAULT_BAUDRATE);
      }
   }

   /* Flag that we need to re-download the Patch.                       */
   VendorCommandsIssued = FALSE;

   return(ret_val);
}

   /* The following function prototype represents the vendor specific   */
   /* function which is used to implement any needed Bluetooth device   */
   /* vendor specific functionality after the entire Bluetooth Stack is */
   /* closed.  This function is called during the HCI_Cleanup()         */
   /* function, after the HCI Driver has been closed.  The caller is    */
   /* free return either success (TRUE) or failure (FALSE), however, it */
   /* will not circumvent the closing down of the stack as all layers   */
   /* have already been closed.                                         */
   /* * NOTE * No Stack calls are possible in this function because the */
   /*          entire stack has been closed down at the time this       */
   /*          function is called.                                      */
Boolean_t BTPSAPI HCI_VS_InitializeAfterHCIClose(void)
{
   return(TRUE);
}

