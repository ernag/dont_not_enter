/** ###################################################################
**     Filename    : Events.h
**     Project     : ProcessorExpert
**     Processor   : MK20DN512VMC10
**     Component   : Events
**     Version     : Driver 01.00
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2013-01-10, 15:34, # CodeGen: 0
**     Abstract    :
**         This is user's event module.
**         Put your event handler code here.
**     Settings    :
**     Contents    :
**         Cpu_OnNMIINT - void Cpu_OnNMIINT(void);
**
** ###################################################################*/

#ifndef __Events_H
#define __Events_H
/* MODULE Events */

#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"
#include "PORTD_GPIO.h"
#include "PORTC_GPIO.h"
#include "PORTE_GPIO.h"
#include "PORTA_GPIO.h"
#include "SPI1_Accel.h"
#include "UART5_DBG.h"
#include "SPI0_WIFI.h"
#include "I2C_Display.h"
#include "Sleep_Timer_LDD.h"
#include "TU1.h"
#include "UART2_BT_UART.h"
#include "BT_PWDN_B.h"
#include "WIFI_PD_B.h"
#include "ADC0_Vsense.h"
#include "SPI2_Flash.h"
#include "PORTB_GPIO.h"
#include "PE_LDD.h"

void Cpu_OnNMIINT(void);
/*
** ===================================================================
**     Event       :  Cpu_OnNMIINT (module Events)
**
**     Component   :  Cpu [MK20DN512MC10]
**     Description :
**         This event is called when the Non maskable interrupt had
**         occurred. This event is automatically enabled when the <NMI
**         interrrupt> property is set to 'Enabled'.
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/


void SPI1_Accel_OnBlockSent(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  SPI1_Accel_OnBlockSent (module Events)
**
**     Component   :  SPI1_Accel [SPIMaster_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter. This event is
**         available only if the SendBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method. 
**     Returns     : Nothing
** ===================================================================
*/

void SPI1_Accel_OnBlockReceived(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  SPI1_Accel_OnBlockReceived (module Events)
**
**     Component   :  SPI1_Accel [SPIMaster_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer. This method is available only if
**         the ReceiveBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method. 
**     Returns     : Nothing
** ===================================================================
*/

void SPI1_Accel_OnError(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  SPI1_Accel_OnError (module Events)
**
**     Component   :  SPI1_Accel [SPIMaster_LDD]
**     Description :
**         This event is called when a channel error (not the error
**         returned by a given method) occurs. The errors can be read
**         using <GetError> method. This event is enabled when SPI
**         device supports error detect.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method. 
**     Returns     : Nothing
** ===================================================================
*/

void UART5_DBG_OnBlockReceived(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  UART5_DBG_OnBlockReceived (module Events)
**
**     Component   :  UART5_DBG [Serial_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void UART5_DBG_OnBlockSent(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  UART5_DBG_OnBlockSent (module Events)
**
**     Component   :  UART5_DBG [Serial_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter. 
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void SPI0_WIFI_OnBlockSent(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  SPI0_WIFI_OnBlockSent (module Events)
**
**     Component   :  SPI0_WIFI [SPIMaster_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter. This event is
**         available only if the SendBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method. 
**     Returns     : Nothing
** ===================================================================
*/

void SPI0_WIFI_OnBlockReceived(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  SPI0_WIFI_OnBlockReceived (module Events)
**
**     Component   :  SPI0_WIFI [SPIMaster_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer. This method is available only if
**         the ReceiveBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method. 
**     Returns     : Nothing
** ===================================================================
*/

void SPI0_WIFI_OnError(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  SPI0_WIFI_OnError (module Events)
**
**     Component   :  SPI0_WIFI [SPIMaster_LDD]
**     Description :
**         This event is called when a channel error (not the error
**         returned by a given method) occurs. The errors can be read
**         using <GetError> method. This event is enabled when SPI
**         device supports error detect.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method. 
**     Returns     : Nothing
** ===================================================================
*/

void I2C_Display_OnMasterBlockSent(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  I2C_Display_OnMasterBlockSent (module Events)
**
**     Component   :  I2C_Display [I2C_LDD]
**     Description :
**         This event is called when I2C in master mode finishes the
**         transmission of the data successfully. This event is not
**         available for the SLAVE mode and if MasterSendBlock is
**         disabled. 
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void I2C_Display_OnMasterBlockReceived(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  I2C_Display_OnMasterBlockReceived (module Events)
**
**     Component   :  I2C_Display [I2C_LDD]
**     Description :
**         This event is called when I2C is in master mode and finishes
**         the reception of the data successfully. This event is not
**         available for the SLAVE mode and if MasterReceiveBlock is
**         disabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void I2C_Display_OnError(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  I2C_Display_OnError (module Events)
**
**     Component   :  I2C_Display [I2C_LDD]
**     Description :
**         This event is called when an error (e.g. Arbitration lost)
**         occurs. The errors can be read with GetError method.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void Sleep_Timer_LDD_OnInterrupt(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  Sleep_Timer_LDD_OnInterrupt (module Events)
**
**     Component   :  Sleep_Timer_LDD [TimerInt_LDD]
**     Description :
**         Called if periodic event occur. Component and OnInterrupt
**         event must be enabled. See <SetEventMask> and <GetEventMask>
**         methods. This event is available only if a <Interrupt
**         service/event> is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer passed as
**                           the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void UART2_BT_UART_OnBlockReceived(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  UART2_BT_UART_OnBlockReceived (module Events)
**
**     Component   :  UART2_BT_UART [Serial_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void UART2_BT_UART_OnBlockSent(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  UART2_BT_UART_OnBlockSent (module Events)
**
**     Component   :  UART2_BT_UART [Serial_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter. 
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void SPI2_Flash_OnBlockSent(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  SPI2_Flash_OnBlockSent (module Events)
**
**     Component   :  SPI2_Flash [SPIMaster_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter. This event is
**         available only if the SendBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method. 
**     Returns     : Nothing
** ===================================================================
*/

void SPI2_Flash_OnBlockReceived(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  SPI2_Flash_OnBlockReceived (module Events)
**
**     Component   :  SPI2_Flash [SPIMaster_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer. This method is available only if
**         the ReceiveBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method. 
**     Returns     : Nothing
** ===================================================================
*/

void SPI2_Flash_OnError(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  SPI2_Flash_OnError (module Events)
**
**     Component   :  SPI2_Flash [SPIMaster_LDD]
**     Description :
**         This event is called when a channel error (not the error
**         returned by a given method) occurs. The errors can be read
**         using <GetError> method. This event is enabled when SPI
**         device supports error detect.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method. 
**     Returns     : Nothing
** ===================================================================
*/

/* END Events */
#endif /* __Events_H*/

/*
** ###################################################################
**
**     This file was created by Processor Expert 5.3 [05.01]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
