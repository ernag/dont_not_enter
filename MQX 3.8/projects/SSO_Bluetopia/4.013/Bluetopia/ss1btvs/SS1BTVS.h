/*****< btvsapi.h >************************************************************/
/*      Copyright 2011 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  BTVSAPI - Vendor specific functions/definitions/constants used to define  */
/*            a set of vendor specific functions supported for a specific     */
/*            hardware platform.                                              */
/*                                                                            */
/*  Author:  Damon Lange                                                      */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   05/20/11  D. Lange       Initial creation.                               */
/******************************************************************************/
#ifndef __BTVSAPIH__
#define __BTVSAPIH__

#include "SS1BTPS.h"            /* Bluetopia API Prototypes/Constants.        */

   /* The following enumerated type represents the different modulation */
   /* types that may be specified in VS_Enable_FCC_Test_Mode() function.*/
typedef enum
{
   mtContinuousWave = 0x00,
   mtGFSK           = 0x01,
   mtEDR2           = 0x02,
   mtEDR3           = 0x03
} VS_Modulation_Type_t;

   /* The following types represent the different Test Patterns that may*/
   /* be specified in VS_Enable_FCC_Test_Mode.                          */
#define VS_TEST_PATTERN_PN9                              0x00
#define VS_TEST_PATTERN_PN15                             0x01
#define VS_TEST_PATTERN_Z0Z0                             0x02
#define VS_TEST_PATTERN_ALL_ONE                          0x03
#define VS_TEST_PATTERN_ALL_ZERO                         0x04
#define VS_TEST_PATTERN_F0F0                             0x05
#define VS_TEST_PATTERN_FF00                             0x06
#define VS_TEST_PATTERN_USER_DEFINED                     0x07

   /* The following types represent the Minimum and Maxium BT Frequency */
   /* Channel that may be specified in VS_Enable_FCC_Test_Mode.         */
#define VS_MINIMUM_BT_FREQUENCY_CHANNEL                  0
#define VS_MAXIMUM_BT_FREQUENCY_CHANNEL                  78

   /* The following types represent the Minimum and Maximum Power Levels*/
   /* that may be specified in VS_Enable_FCC_Test_Mode.                 */
#define VS_MINIMUM_POWER_LEVEL                           0
#define VS_MAXIMUM_POWER_LEVEL                           15

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
BTPSAPI_DECLARATION int BTPSAPI VS_Update_UART_Baud_Rate(unsigned int BluetoothStackID, DWord_t BaudRate);

#ifdef INCLUDE_BLUETOOTH_API_PROTOTYPES
   typedef int (BTPSAPI *PFN_VS_Update_UART_Baud_Rate_t)(unsigned int BluetoothStackID, DWord_t BaudRate);
#endif

   /* The following function prototype represents the vendor specific   */
   /* function which is used to change the output power for the Local   */
   /* Bluetooth Device specified by the Bluetooth Protocol Stack that   */
   /* is specified by the Bluetooth Protocol Stack ID. The second       */
   /* parameter is the max output power to set. This function returns   */
   /* zero if successful or a negative return error code if there was   */
   /* an error.                                                         */
   /* * NOTE * The maximum output power is specified from 0 to 12 and   */
   /*          it specifies 4 dBm steps.                                */
BTPSAPI_DECLARATION int BTPSAPI VS_Set_Max_Output_Power(unsigned int BluetoothStackID, SByte_t LEMaxPower, SByte_t CBMaxPower);

#ifdef INCLUDE_BLUETOOTH_API_PROTOTYPES
   typedef int (BTPSAPI *PFN_VS_Set_Max_Output_Power_t)(unsigned int BluetoothStackID, SByte_t LEMaxPower, SByte_t CBMaxPower);
#endif

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
BTPSAPI_DECLARATION int BTPSAPI VS_Enable_FCC_Test_Mode_TX(unsigned int BluetoothStackID, VS_Modulation_Type_t Modulation_Type, Byte_t Test_Pattern, Byte_t Frequency_Channel, Byte_t Power_Level, DWord_t Generator_Init_Value, DWord_t EDR_Generator_Mask);

#ifdef INCLUDE_BLUETOOTH_API_PROTOTYPES
   typedef int (BTPSAPI *PFN_VS_Enable_FCC_Test_Mode_TX_t)(unsigned int BluetoothStackID, VS_Modulation_Type_t Modulation_Type, Byte_t Test_Pattern, Byte_t Frequency_Channel, Byte_t Power_Level, DWord_t Generator_Init_Value, DWord_t EDR_Generator_Mask);
#endif

   /* The following function prototype represents the vendor function   */
   /* which is used to put the specified by the Bluetooth Protocol Stack*/
   /* that is specified by the Bluetooth Protocol Stack ID into FCC Test*/
   /* Mode.  The second parameter specifies the Frequency Channel to    */
   /* receive data on.  The final parameter specifies whether to use    */
   /* open or closed loop.  This function returns zero if successful or */
   /* a negative return error code if there was an error.               */
BTPSAPI_DECLARATION int BTPSAPI VS_Enable_FCC_Test_Mode_RX(unsigned int BluetoothStackID, Byte_t Frequency_Channel, Boolean_t OpenLoop);

#ifdef INCLUDE_BLUETOOTH_API_PROTOTYPES
   typedef int (BTPSAPI *PFN_VS_Enable_FCC_Test_Mode_RX_t)(unsigned int BluetoothStackID, Byte_t Frequency_Channel, Boolean_t OpenLoop);
#endif

   /* The following function prototype represents the vendor function   */
   /* which is used to put the specified by the Bluetooth Protocol Stack*/
   /* that is specified by the Bluetooth Protocol Stack ID into         */
   /* Continuous Packet Tx/Rx Mode.  This function returns zero if      */
   /* successful or a negative return error code if there was an error. */
BTPSAPI_DECLARATION int BTPSAPI VS_Tester_Packet_TX_RX(unsigned int BluetoothStackID, Byte_t FrequencyMode, Byte_t TxFrequencyIndex, Byte_t RxFrequencyIndex, Byte_t ACLPacketType, Byte_t ACLPacketDataPattern, Word_t ACLPacketDataLength, Byte_t PowerLevel, Byte_t DisableWhitening, Word_t PRBS9InitValue);

#ifdef INCLUDE_BLUETOOTH_API_PROTOTYPES
   typedef int (BTPSAPI *PFN_VS_Tester_Packet_TX_RX_t)(unsigned int BluetoothStackID, Byte_t FrequencyMode, Byte_t TxFrequencyIndex, Byte_t RxFrequencyIndex, Byte_t ACLPacketType, Byte_t ACLPacketDataPattern, Word_t ACLPacketDataLength, Byte_t PowerLevel, Byte_t DisableWhitening, Word_t PRBS9InitValue);
#endif

   /* The following function prototype represents the function which is */
   /* used to put the Local Bluetooth Device specified by the Bluetooth */
   /* Protocol Stack that is specified by the Bluetooth Protocol Stack  */
   /* ID into Bluetooth SIG RF Signal Test Mode.  This function returns */
   /* zero if successful or a negative return error code if there was an*/
   /* error.                                                            */
   /* * NOTE * Once the Local Bluetooth Device is in RF Signal Test Mode*/
   /*          it will remain in this mode until an HCI Reset is issued.*/
BTPSAPI_DECLARATION int BTPSAPI VS_Enable_SIG_RF_Signal_Test_Mode(unsigned int BluetoothStackID);

#ifdef INCLUDE_BLUETOOTH_API_PROTOTYPES
   typedef int (BTPSAPI *PFN_VS_Enable_SIG_RF_Signal_Test_Mode_t)(unsigned int BluetoothStackID);
#endif

#endif
