/*****< btvs.c >***************************************************************/
/*      Copyright 2011 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  BTVS - Vendor specific implementation of a set of vendor specific         */
/*         functions supported for a specific hardware platform.              */
/*                                                                            */
/*  Author:  Damon Lange                                                      */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   05/20/11  D. Lange       Initial creation.                               */
/******************************************************************************/
#include "SS1BTPS.h"          /* Bluetopia API Prototypes/Constants.          */
#include "HCIDRV.h"

#include "BTPSKRNL.h"

#include "SS1BTVS.h"          /* Vendor Specific Prototypes/Constants.        */

   /* Defines the size of buffer needed to hold the parameters for      */
   /* the VS Set Power Vector Command.                                  */
#define POWER_VECTOR_COMMAND_SIZE                                20

   /* Total Number of Power Vectors.                                    */
#define POWER_VECTOR_SIZE                                        (POWER_VECTOR_COMMAND_SIZE - 5)

   /* Number of steps in the Power Vector Section.                      */
#define POWER_VECTOR_STEP_SIZE                                   5

   /* Defines the size of buffer needed to hold the DRPB Tester Con Tx  */
   /* Command.                                                          */
#define DRPB_TESTER_CON_TX_SIZE                                  12

   /* Defines the size of buffer needed to hold the DRPB Tester Con Rx  */
   /* Command.                                                          */
#define DRPB_TESTER_CON_RX_SIZE                                  2

   /* Defines the size of buffer needed to hold the DRPB Tester Con RX  */
   /* Tx Command.                                                       */
#define DRPB_TESTER_PACKET_RX_TX_SIZE                            12

   /* Vendor Specific Command Opcodes.                                  */
#define VS_DRPB_SET_POWER_VECTOR_COMMAND_OPCODE                  ((Word_t)(0xFD82))
#define VS_DRPB_SET_SET_CLASS2_SINGLE_POWER_COMMAND_OPCODE       ((Word_t)(0xFD87))
#define VS_DRPB_SET_SET_RF_CALIBRATION_INFO_COMMAND_OPCODE       ((Word_t)(0xFD76))
#define VS_DRPB_SET_ENABLE_RF_CALIBRATION_COMMAND_OPCODE         ((Word_t)(0xFD80))
#define VS_DRPB_TESTER_CON_TX_COMMAND_OPCODE                     ((Word_t)(0xFD84))
#define VS_WRITE_HARDWARE_REGISTER_COMMAND_OPCODE                ((Word_t)(0xFF01))
#define VS_DRPB_TESTER_CON_RX_COMMAND_OPCODE                     ((Word_t)(0xFD17))
#define VS_DRPB_TESTER_PACKET_RX_RX_COMMAND_OPCODE               ((Word_t)(0xFD85))

#define VS_COMMAND_OGF(_CommandOpcode)                           ((Byte_t)((_CommandOpcode) >> 10))
#define VS_COMMAND_OCF(_CommandOpcode)                           ((Word_t)((_CommandOpcode) & (0x3FF)))

   /* Miscellaneous Type Declarations.                                  */

   /* Internal Variables to this Module (Remember that all variables    */
   /* declared static are initialized to 0 automatically by the         */
   /* compiler as part of standard C/C++).                              */

   /* Internal constant arrays used to hold parametes to the Power      */
   /* Vector commands.                                                  */

   /* Magical command for RF Calibration Command.                       */
static BTPSCONST Byte_t RF_Calibration_Info[] =
{
   0x01,
   0x21, 0x54, 0x00, 0x00,
   0x61, 0x57, 0x00, 0x00,

   20, 5, 10, 5, 0, 7, 6, 10, 4, 5, 8, 9,
   11, 12, 13, 14, 16, 16, 16, 16, 16, 16,
   16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
   16, 16, 16, 16, 0
};

   /* Internal Function Prototypes.                                     */

   /* The following function prototype represents the vendor specific   */
   /* function which is used to change the Bluetooth UART for the Local */
   /* Bluetooth Device specified by the Bluetooth Protocol Stack that   */
   /* is specified by the Bluetooth Protocol Stack ID. The second       */
   /* parameter specifies the new baud rate to set.  This change        */
   /* encompasses both changing the speed of the Bluetooth chip (by     */
   /* issuing the correct commands) and then, if successful, informing  */
   /* the HCI Driver of the change (so the driver can communicate with  */
   /* the Bluetooth device at the new baud rate).  This function returns*/
   /* zero if successful or a negative return error code if there was   */
   /* an error.                                                         */
int BTPSAPI VS_Update_UART_Baud_Rate(unsigned int BluetoothStackID, DWord_t BaudRate)
{
   int                              ret_val;
   Byte_t                           Length;
   Byte_t                           Status;
   NonAlignedDWord_t                _BaudRate;

   union
   {
      Byte_t                        Buffer[16];
      HCI_Driver_Reconfigure_Data_t DriverReconfigureData;
   } Data;

   /* Before continuing, make sure the input parameters appear to be    */
   /* semi-valid.                                                       */
   if((BluetoothStackID) && (BaudRate))
   {
      /* Write the Baud Rate.                                           */
      ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD(&_BaudRate, BaudRate);

      /* Next, write the command to the device.                         */
      Length  = sizeof(Data.Buffer);
      ret_val = HCI_Send_Raw_Command(BluetoothStackID, 0x3F, 0x0336, sizeof(NonAlignedDWord_t), (Byte_t *)&_BaudRate, &Status, &Length, Data.Buffer, TRUE);
      if((!ret_val) && (!Status))
      {
         /* We were successful, now we need to change the baud rate of  */
         /* the driver.                                                 */
         BTPS_MemInitialize(&(Data.DriverReconfigureData), 0, sizeof(HCI_Driver_Reconfigure_Data_t));

         Data.DriverReconfigureData.ReconfigureCommand = HCI_COMM_DRIVER_RECONFIGURE_DATA_COMMAND_CHANGE_PARAMETERS;
         Data.DriverReconfigureData.ReconfigureData    = (void *)BaudRate;

         ret_val = HCI_Reconfigure_Driver(BluetoothStackID, FALSE, &(Data.DriverReconfigureData));
      }
   }
   else
      ret_val = BTPS_ERROR_INVALID_PARAMETER;

   wassert(0 == ret_val);

   /* Return the result the caller.                                     */
   return(ret_val);
}

   /* The following function prototype represents the vendor specific   */
   /* function which is used to change the output power for the Local   */
   /* Bluetooth Device specified by the Bluetooth Protocol Stack that   */
   /* is specified by the Bluetooth Protocol Stack ID. The second       */
   /* parameter is the max output power to set. This function returns   */
   /* zero if successful or a negative return error code if there was   */
   /* an error.                                                         */
   /* * NOTE * The maximum output power is specified from 0 to 12 and   */
   /*          it specifies 4 dBm steps.                                */
int BTPSAPI VS_Set_Max_Output_Power(unsigned int BluetoothStackID, SByte_t LEMaxPower, SByte_t CBMaxPower)
{
   int          ret_val;
   int          PowerVector;
   int          FourDbmDifference;
   int          SmallestDifference;
   Byte_t       Length;
   Byte_t       Status;
   Byte_t       ReturnBuffer[8];
   Byte_t       CommandBuffer[POWER_VECTOR_COMMAND_SIZE];
   Byte_t       OGF;
   Word_t       OCF;
   SByte_t      MaxPower;
   unsigned int Class2PowerLevel[3];
   unsigned int Index;
   unsigned int FormatIndex;

   /* Before continuing, make sure the input parameters appear to be    */
   /* semi-valid.                                                       */
   if((BluetoothStackID) && (LEMaxPower >= -23) && (LEMaxPower <= 10) && (CBMaxPower >= -23) && (CBMaxPower <= 12))
   {
      /* Send the HCI_DRPb_Set_Power_Vector commands for each           */
      /* modulation type.                                               */
      for(Index = 0; Index < 3; Index++)
      {
         /* Format the Command.                                         */
         ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[0], (Byte_t)(Index));
         ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[1], (Byte_t)(-50 * 2));

         /* Format the data that is dependent on the requested max      */
         /* power.                                                      */

         /* Set the max power to the CB Max Power (LE is handled        */
         /* seperately).                                                */
         MaxPower = CBMaxPower;

         if((Index > 0) && (MaxPower > 12))
            MaxPower = 10;

         SmallestDifference      = ((MaxPower - 4) >= 0) ? (MaxPower - 4):(-(MaxPower - 4));
         Class2PowerLevel[Index] = POWER_VECTOR_SIZE;
         for(FormatIndex = 0; FormatIndex < POWER_VECTOR_SIZE; FormatIndex++)
         {
            if(FormatIndex < (POWER_VECTOR_SIZE - POWER_VECTOR_STEP_SIZE))
            {
               /* We are only going to program the chip to use          */
               /* POWER_VECTOR_STEP_SIZE discrete power levels, thus    */
               /* for 12 dBm max power we have 12, 7,2,-3,-8 levels     */
               /* so the first 10 slots will be -8.                     */
               PowerVector       =  (((int)(MaxPower)) - (5 * (POWER_VECTOR_STEP_SIZE - 1)));
               FourDbmDifference = ((PowerVector - 4) >= 0)?(PowerVector - 4):(-(PowerVector - 4));
            }
            else
            {
               /* Calculate the vector in the next region.              */
               PowerVector       = ((int)(MaxPower)) - (((POWER_VECTOR_SIZE - FormatIndex)-1)*5);
               FourDbmDifference = ((PowerVector - 4) >= 0)?(PowerVector - 4):(-(PowerVector - 4));
            }

            /* If the current difference for the current vector is      */
            /* smaller than the current difference then we should save  */
            /* the index of this level for usage in the Set Class2      */
            /* Single Power command.                                    */
            if(FourDbmDifference < SmallestDifference)
            {
               SmallestDifference      = FourDbmDifference;
               Class2PowerLevel[Index] = FormatIndex + 1;
            }

            /* Assign the Power Vector into the command (note for GFSK  */
            /* the BLE Max Output power is in the N=1 position).        */
            if((Index == 0) && (FormatIndex == 0))
            {
               ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&(CommandBuffer[2 + FormatIndex]), (Byte_t)(LEMaxPower * 2));
            }
            else
            {
               ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&(CommandBuffer[2 + FormatIndex]), (Byte_t)(PowerVector * 2));
            }
         }

         ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&(CommandBuffer[FormatIndex + 2]), (Byte_t)(0xFF));
         ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(&(CommandBuffer[FormatIndex + 3]), 0);

         /* Now actually send the command now that it is formatted.     */
         Length  = sizeof(ReturnBuffer);
         OGF     = VS_COMMAND_OGF(VS_DRPB_SET_POWER_VECTOR_COMMAND_OPCODE);
         OCF     = VS_COMMAND_OCF(VS_DRPB_SET_POWER_VECTOR_COMMAND_OPCODE);

         ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, sizeof(CommandBuffer), (Byte_t *)(CommandBuffer), &Status, &Length, ReturnBuffer, TRUE);
         if((ret_val) || (Status) || (Length < 1) || ((Length >= 1) && (ReturnBuffer[0])))
         {
            /* If Send Raw returns an error return, or an error status  */
            /* OR the chip returns less than the expected number of     */
            /* bytes OR the chip returns a Non-Zero status              */
            /* (ReturnBuffer[0] will contain the command status from    */
            /* the HCI Command Complete Event) we should return an      */
            /* error code.                                              */
            ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;

            break;
         }
      }

      /* Only continue if no error occurred.                            */
      if(!ret_val)
      {
         /* Format the VS_DRPb_Set_Class2_Single_Power Command.     */
         ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[0],(Byte_t)(Class2PowerLevel[0]));
         ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[1],(Byte_t)(Class2PowerLevel[1]));
         ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[2],(Byte_t)(Class2PowerLevel[2]));

         /* Send the VS_DRPb_Set_Class2_Single_Power Vendor         */
         /* Specific Command.                                           */
         Length  = sizeof(ReturnBuffer);
         OGF     = VS_COMMAND_OGF(VS_DRPB_SET_SET_CLASS2_SINGLE_POWER_COMMAND_OPCODE);
         OCF     = VS_COMMAND_OCF(VS_DRPB_SET_SET_CLASS2_SINGLE_POWER_COMMAND_OPCODE);

         ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, 3, (Byte_t *)(CommandBuffer), &Status, &Length, ReturnBuffer, TRUE);
         if((!ret_val) && (!Status) && (Length >= 1) && (!ReturnBuffer[0]))
         {
            /* Send the VS_DRPb_Set_RF_Calibration_Info Vendor      */
            /* Specific Command.                                        */
            Length  = sizeof(ReturnBuffer);
            OGF     = VS_COMMAND_OGF(VS_DRPB_SET_SET_RF_CALIBRATION_INFO_COMMAND_OPCODE);
            OCF     = VS_COMMAND_OCF(VS_DRPB_SET_SET_RF_CALIBRATION_INFO_COMMAND_OPCODE);

            ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, sizeof(RF_Calibration_Info), (Byte_t *)(RF_Calibration_Info), &Status, &Length, ReturnBuffer, TRUE);
            if((!ret_val) && (!Status) && (Length >= 1) && (!ReturnBuffer[0]))
            {
               /* Send the first of two VS_DRPb_Enable_Calibration  */
               /* Vendor Specific Commands.                             */
               ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[0],0x00);
               ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD(&CommandBuffer[1],0x00000001);
               ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[5],0x01);

               Length  = sizeof(ReturnBuffer);
               OGF     = VS_COMMAND_OGF(VS_DRPB_SET_ENABLE_RF_CALIBRATION_COMMAND_OPCODE);
               OCF     = VS_COMMAND_OCF(VS_DRPB_SET_ENABLE_RF_CALIBRATION_COMMAND_OPCODE);

               ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, 6, (Byte_t *)(CommandBuffer), &Status, &Length, ReturnBuffer, TRUE);
               if((!ret_val) && (!Status) && (Length >= 1) && (!ReturnBuffer[0]))
               {
                  /* Send the second of two                             */
                  /* VS_DRPb_Enable_Calibration Vendor Specific     */
                  /* Commands.                                          */
                  ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[0],60);
                  ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD(&CommandBuffer[1],0x00005ff0);
                  ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[5],0x00);

                  Length  = sizeof(ReturnBuffer);
                  OGF     = VS_COMMAND_OGF(VS_DRPB_SET_ENABLE_RF_CALIBRATION_COMMAND_OPCODE);
                  OCF     = VS_COMMAND_OCF(VS_DRPB_SET_ENABLE_RF_CALIBRATION_COMMAND_OPCODE);

                  ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, 6, (Byte_t *)(CommandBuffer), &Status, &Length, ReturnBuffer, TRUE);
                  if((!ret_val) && (!Status) && (Length >= 1) && (!ReturnBuffer[0]))
                  {
                     /* Return success to the caller.                   */
                     ret_val = 0;
                  }
                  else
                     ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
               }
               else
                  ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
            }
            else
               ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
         }
         else
            ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
      }
   }
   else
      ret_val = BTPS_ERROR_INVALID_PARAMETER;

   wassert(0 == ret_val);

   /* Return the result the caller.                                     */
   return(ret_val);
}

   /* The following function prototype represents the vendor function   */
   /* which is used to put the specified by the Bluetooth Protocol Stack*/
   /* that is specified by the Bluetooth Protocol Stack ID into FCC Test*/
   /* Mode.  The second parameter specifies the Modulation Type to use  */
   /* in the test mode, the third parameter specifies the Test Pattern  */
   /* to transmit in the test mode, the fourth parameter specifies the  */
   /* Frequency Channel to trasmit on, and the fifth parameter specifies*/
   /* the Power Level to use while transmitting.  The final two         */
   /* parameters are only used when the Test_Pattern parameter is set to*/
   /* VS_TEST_PATTERN_USER_DEFINED.  This function returns zero if      */
   /* successful or a negative return error code if there was an error. */
int BTPSAPI VS_Enable_FCC_Test_Mode_TX(unsigned int BluetoothStackID, VS_Modulation_Type_t Modulation_Type, Byte_t Test_Pattern, Byte_t Frequency_Channel, Byte_t Power_Level, DWord_t Generator_Init_Value, DWord_t EDR_Generator_Mask)
{
   int    ret_val;
   Byte_t Length;
   Byte_t Status;
   Byte_t OGF;
   Word_t OCF;
   Byte_t CommandBuffer[DRPB_TESTER_CON_TX_SIZE];
   Byte_t ReturnBuffer[1];

   /* Before continuing, make sure the input parameters appears to be   */
   /* semi-valid.                                                       */
   if((BluetoothStackID) && (Modulation_Type >= mtContinuousWave) && (Modulation_Type <= mtEDR3) && (Test_Pattern >= VS_TEST_PATTERN_PN9) && (Test_Pattern <= VS_TEST_PATTERN_USER_DEFINED) && (Frequency_Channel <= VS_MAXIMUM_BT_FREQUENCY_CHANNEL) && (Power_Level <= VS_MAXIMUM_POWER_LEVEL))
   {
      /* Format the HCI VS DRPB Tester Con Tx Command.                  */
      BTPS_MemInitialize(CommandBuffer, 0, DRPB_TESTER_CON_TX_SIZE);

      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[0], Modulation_Type);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[1], Test_Pattern);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[2], Frequency_Channel);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[3], Power_Level);

      if(Test_Pattern == VS_TEST_PATTERN_USER_DEFINED)
      {
         ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD(&CommandBuffer[4], Generator_Init_Value);
         ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD(&CommandBuffer[8], EDR_Generator_Mask);
      }

      Length  = sizeof(ReturnBuffer);
      OGF     = VS_COMMAND_OGF(VS_DRPB_TESTER_CON_TX_COMMAND_OPCODE);
      OCF     = VS_COMMAND_OCF(VS_DRPB_TESTER_CON_TX_COMMAND_OPCODE);
      ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, DRPB_TESTER_CON_TX_SIZE, (Byte_t *)CommandBuffer, &Status, &Length, ReturnBuffer, TRUE);
      if((!ret_val) && (!Status) && (Length >= 1) && (ReturnBuffer[0] == HCI_ERROR_CODE_NO_ERROR))
      {
         /* The next step is to send a HCI VS Write Hardware Register   */
         /* Command.  If the Modulation Type is mtContinuousWave we will*/
         /* skip this step.                                             */
         if(Modulation_Type != mtContinuousWave)
         {
            /* Format the HCI VS Write Hardware Register Command.       */
            ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD(&CommandBuffer[0], 0x0019180C);
            ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(&CommandBuffer[4], 0x0101);

            Length  = sizeof(ReturnBuffer);
            OGF     = VS_COMMAND_OGF(VS_WRITE_HARDWARE_REGISTER_COMMAND_OPCODE);
            OCF     = VS_COMMAND_OCF(VS_WRITE_HARDWARE_REGISTER_COMMAND_OPCODE);
            ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, (DWORD_SIZE+WORD_SIZE), (Byte_t *)CommandBuffer, &Status, &Length, ReturnBuffer, TRUE);
            if((ret_val) || (Status) || (Length < 1) || ((Length >= 1) && (ReturnBuffer[0])))
            {
               /* If Send Raw returns an error return, or an error      */
               /* status OR the chip returns less than the expected     */
               /* number of bytes OR the chip returns a Non-Zero status */
               /* (ReturnBuffer[0] will contain the command status from */
               /* the HCI Command Complete Event) we should return an   */
               /* error code.                                           */
               ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
            }
         }

         /* We will move to the next step only if no error has occured. */
         /* If no error occurred then the next step is to Send the HCI  */
         /* VS Enable RF Calibration Command.                           */
         if(!ret_val)
         {
            /* Format the HCI VS DRPB Enable RF Calibration Command.    */
            ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[0], 0xFF);
            ASSIGN_HOST_DWORD_TO_LITTLE_ENDIAN_UNALIGNED_DWORD(&CommandBuffer[1], 0xFFFFFFFF);
            ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[5], 0x01);

            Length  = sizeof(ReturnBuffer);
            OGF     = VS_COMMAND_OGF(VS_DRPB_SET_ENABLE_RF_CALIBRATION_COMMAND_OPCODE);
            OCF     = VS_COMMAND_OCF(VS_DRPB_SET_ENABLE_RF_CALIBRATION_COMMAND_OPCODE);
            ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, (BYTE_SIZE+DWORD_SIZE+BYTE_SIZE), (Byte_t *)CommandBuffer, &Status, &Length, ReturnBuffer, TRUE);
            if((ret_val) || (Status) || (Length < 1) || ((Length >= 1) && (ReturnBuffer[0])))
               ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
         }
      }
      else
         ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
   }
   else
      ret_val = BTPS_ERROR_INVALID_PARAMETER;

   wassert(0 == ret_val);

   /* Return the result the caller.                                     */
   return(ret_val);
}

   /* The following function prototype represents the vendor function   */
   /* which is used to put the specified by the Bluetooth Protocol Stack*/
   /* that is specified by the Bluetooth Protocol Stack ID into FCC Test*/
   /* Mode.  The second parameter specifies the Frequency Channel to    */
   /* receive data on.  The final parameter specifies whether to use    */
   /* open or closed loop.  This function returns zero if successful or */
   /* a negative return error code if there was an error.               */
int BTPSAPI VS_Enable_FCC_Test_Mode_RX(unsigned int BluetoothStackID, Byte_t Frequency_Channel, Boolean_t OpenLoop)
{
   int    ret_val;
   Byte_t Length;
   Byte_t Status;
   Byte_t OGF;
   Word_t OCF;
   Byte_t CommandBuffer[DRPB_TESTER_CON_TX_SIZE];
   Byte_t ReturnBuffer[1];

   /* Before continuing, make sure the input parameters appears to be   */
   /* semi-valid.                                                       */
   if((BluetoothStackID) && (Frequency_Channel <= VS_MAXIMUM_BT_FREQUENCY_CHANNEL))
   {
      /* Format the HCI VS DRPB Tester Con Rx Command.                  */
      BTPS_MemInitialize(CommandBuffer, 0, DRPB_TESTER_CON_RX_SIZE);

      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[0], Frequency_Channel);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[1], (Byte_t)(OpenLoop?0x02:0x01));

      Length  = sizeof(ReturnBuffer);
      OGF     = VS_COMMAND_OGF(VS_DRPB_TESTER_CON_RX_COMMAND_OPCODE);
      OCF     = VS_COMMAND_OCF(VS_DRPB_TESTER_CON_RX_COMMAND_OPCODE);
      ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, DRPB_TESTER_CON_RX_SIZE, (Byte_t *)CommandBuffer, &Status, &Length, ReturnBuffer, TRUE);
      if((!ret_val) && (!Status) && (Length >= 1) && (ReturnBuffer[0] == HCI_ERROR_CODE_NO_ERROR))
         ret_val = 0;
      else
         ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
   }
   else
      ret_val = BTPS_ERROR_INVALID_PARAMETER;

   wassert(0 == ret_val);

   /* Return the result the caller.                                     */
   return(ret_val);
}

   /* The following function prototype represents the vendor function   */
   /* which is used to put the specified by the Bluetooth Protocol Stack*/
   /* that is specified by the Bluetooth Protocol Stack ID into         */
   /* Continuous Packet Tx/Rx Mode.  This function returns zero if      */
   /* successful or a negative return error code if there was an error. */
int BTPSAPI VS_Tester_Packet_TX_RX(unsigned int BluetoothStackID, Byte_t FrequencyMode, Byte_t TxFrequencyIndex, Byte_t RxFrequencyIndex, Byte_t ACLPacketType, Byte_t ACLPacketDataPattern, Word_t ACLPacketDataLength, Byte_t PowerLevel, Byte_t DisableWhitening, Word_t PRBS9InitValue)
{
   int    ret_val;
   Byte_t Length;
   Byte_t Status;
   Byte_t OGF;
   Word_t OCF;
   Byte_t CommandBuffer[DRPB_TESTER_PACKET_RX_TX_SIZE];
   Byte_t ReturnBuffer[1];

   /* Before continuing, make sure the input parameters appears to be   */
   /* semi-valid.                                                       */
   if((BluetoothStackID)
       && ((FrequencyMode == 0x00) || (FrequencyMode == 0x03))
       && ((TxFrequencyIndex >= 0) && (TxFrequencyIndex <= 78))
       && (((RxFrequencyIndex >= 0) && (RxFrequencyIndex <= 78)) || (RxFrequencyIndex == 0xFF))
       && ((ACLPacketType >= 0x00) && (ACLPacketType <= 0x0B))
       && ((ACLPacketDataPattern >= 0) && (ACLPacketDataPattern <= 0x05))
       && ((PowerLevel >= 0) && (PowerLevel <= 15))
       && ((DisableWhitening >= 0) && (DisableWhitening <= 1))
       && ((PRBS9InitValue >= 0) && (PRBS9InitValue <= 0x01FF)))
   {
      /* Format the HCI VS DRPB Tester Con Rx Command.                  */
      BTPS_MemInitialize(CommandBuffer, 0, DRPB_TESTER_PACKET_RX_TX_SIZE);

      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[0], FrequencyMode);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[1], TxFrequencyIndex);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[2], RxFrequencyIndex);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[3], ACLPacketType);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[4], ACLPacketDataPattern);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[5], 0x00); /* Reserved.*/
      ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(&CommandBuffer[6], ACLPacketDataLength);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[8], PowerLevel);
      ASSIGN_HOST_BYTE_TO_LITTLE_ENDIAN_UNALIGNED_BYTE(&CommandBuffer[9], DisableWhitening);
      ASSIGN_HOST_WORD_TO_LITTLE_ENDIAN_UNALIGNED_WORD(&CommandBuffer[10], PRBS9InitValue);

      Length  = sizeof(ReturnBuffer);
      OGF     = VS_COMMAND_OGF(VS_DRPB_TESTER_PACKET_RX_RX_COMMAND_OPCODE);
      OCF     = VS_COMMAND_OCF(VS_DRPB_TESTER_PACKET_RX_RX_COMMAND_OPCODE);
      ret_val = HCI_Send_Raw_Command(BluetoothStackID, OGF, OCF, DRPB_TESTER_PACKET_RX_TX_SIZE, (Byte_t *)CommandBuffer, &Status, &Length, ReturnBuffer, TRUE);
      if((!ret_val) && (!Status) && (Length >= 1) && (ReturnBuffer[0] == HCI_ERROR_CODE_NO_ERROR))
         ret_val = 0;
      else
         ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
   }
   else
      ret_val = BTPS_ERROR_INVALID_PARAMETER;

   wassert(0 == ret_val);

   /* Return the result the caller.                                     */
   return(ret_val);
}

   /* The following function prototype represents the function which is */
   /* used to put the Local Bluetooth Device specified by the Bluetooth */
   /* Protocol Stack that is specified by the Bluetooth Protocol Stack  */
   /* ID into Bluetooth SIG RF Signal Test Mode.  This function returns */
   /* zero if successful or a negative return error code if there was an*/
   /* error.                                                            */
   /* * NOTE * Once the Local Bluetooth Device is in RF Signal Test Mode*/
   /*          it will remain in this mode until an HCI Reset is issued.*/
int BTPSAPI VS_Enable_SIG_RF_Signal_Test_Mode(unsigned int BluetoothStackID)
{
   int         ret_val;
   Byte_t      StatusResult;
   Condition_t ConditionStructure;

   /* Before continuing, make sure the input parameter appears to be    */
   /* semi-valid.                                                       */
   if(BluetoothStackID)
   {
      /* Send HCI Write Scan Enable Command.                            */
      if((!HCI_Write_Scan_Enable(BluetoothStackID, HCI_SCAN_ENABLE_INQUIRY_SCAN_ENABLED_PAGE_SCAN_ENABLED, &StatusResult)) && (StatusResult == HCI_ERROR_CODE_NO_ERROR))
      {
         /* Configure and send the HCI Set Event Filter Command.        */
         ConditionStructure.Condition.Connection_Setup_Filter_Type_All_Devices_Condition.Auto_Accept_Flag = HCI_AUTO_ACCEPT_FLAG_DO_AUTO_ACCEPT;

         if((!HCI_Set_Event_Filter(BluetoothStackID, HCI_FILTER_TYPE_CONNECTION_SETUP, HCI_FILTER_CONDITION_TYPE_CONNECTION_SETUP_ALL_DEVICES, ConditionStructure, &StatusResult)) && (StatusResult == HCI_ERROR_CODE_NO_ERROR))
         {
            /* Finally send the HCI Enable Device Under Test Command.   */
            if((!HCI_Enable_Device_Under_Test_Mode(BluetoothStackID, &StatusResult)) && (StatusResult == HCI_ERROR_CODE_NO_ERROR))
               ret_val = 0;
            else
               ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
         }
         else
            ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
      }
      else
         ret_val = BTPS_ERROR_DEVICE_HCI_ERROR;
   }
   else
      ret_val = BTPS_ERROR_INVALID_PARAMETER;

   wassert(0 == ret_val);

   /* Return the result the caller.                                     */
   return(ret_val);
}
