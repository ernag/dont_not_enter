/** ###################################################################
**     Filename    : Events.h
**     Project     : ProcessorExpert
**     Processor   : MK60DN512VMC10
**     Component   : Events
**     Version     : Driver 01.00
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2013-02-15, 13:20, # CodeGen: 0
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
#include "USB_OTG1.h"
#include "FLASH1.h"
#include "SPI2_Flash.h"
#include "Sleep_Timer_LDD.h"
#include "TU2.h"
#include "ADC0_Vsense.h"
#include "PORTA_GPIO.h"
#include "PORTE_GPIO.h"
#include "PORTC_GPIO.h"
#include "PORTD_GPIO.h"
#include "CRC1.h"
#include "whistle_random.h"
#include "I2C2.h"
#include "PE_LDD.h"

void FLASH1_OnOperationComplete(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  FLASH1_OnOperationComplete (module Events)
**
**     Component   :  FLASH1 [FLASH_LDD]
**     Description :
**         Called at the end of the whole write / erase operation. if
**         the event is enabled. See SetEventMask() and GetEventMask()
**         methods.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/

void Cpu_OnNMIINT(void);
/*
** ===================================================================
**     Event       :  Cpu_OnNMIINT (module Events)
**
**     Component   :  Cpu [MK60DN512MD10]
**     Description :
**         This event is called when the Non maskable interrupt had
**         occurred. This event is automatically enabled when the <NMI
**         interrrupt> property is set to 'Enabled'.
**     Parameters  : None
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

void I2C2_OnMasterBlockSent(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  I2C2_OnMasterBlockSent (module Events)
**
**     Component   :  I2C2 [I2C_LDD]
*/
/*!
**     @brief
**         This event is called when I2C in master mode finishes the
**         transmission of the data successfully. This event is not
**         available for the SLAVE mode and if MasterSendBlock is
**         disabled. 
**     @param
**         UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
*/
/* ===================================================================*/

void I2C2_OnMasterBlockReceived(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  I2C2_OnMasterBlockReceived (module Events)
**
**     Component   :  I2C2 [I2C_LDD]
*/
/*!
**     @brief
**         This event is called when I2C is in master mode and finishes
**         the reception of the data successfully. This event is not
**         available for the SLAVE mode and if MasterReceiveBlock is
**         disabled.
**     @param
**         UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
*/
/* ===================================================================*/

void I2C2_OnError(LDD_TUserData *UserDataPtr);
/*
** ===================================================================
**     Event       :  I2C2_OnError (module Events)
**
**     Component   :  I2C2 [I2C_LDD]
*/
/*!
**     @brief
**         This event is called when an error (e.g. Arbitration lost)
**         occurs. The errors can be read with GetError method.
**     @param
**         UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
*/
/* ===================================================================*/

/*
** ===================================================================
**     Event       :  whistle_random_OnError (module Events)
**
**     Component   :  whistle_random [RNG_LDD]
*/
/*!
**     @brief
**         Called when empty FIFO is read (interrupt must be enabled
**         and no mask applied). See SetEventMask() and GetEventMask()
**         methods.
**     @param
**         UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
*/
/* ===================================================================*/
void whistle_random_OnError(LDD_TUserData *UserDataPtr);

/* END Events */
#endif /* __Events_H*/

/*
** ###################################################################
**
**     This file was created by Processor Expert 10.0 [05.03]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
