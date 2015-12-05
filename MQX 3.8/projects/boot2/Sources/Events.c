/** ###################################################################
**     Filename    : Events.c
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
/* MODULE Events */

#include "Cpu.h"
#include "Events.h"
#include "sleep_timer.h"

/* User includes (#include below this line is not maintained by Processor Expert) */
extern uint8 g_SPI2_Flash_Wait_for_Send;
extern uint8 g_SPI2_Flash_Wait_for_Receive;

extern volatile bool  g_I2C1_DataReceivedFlg;
extern volatile bool  g_I2C1_DataTransmittedFlg;

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

extern volatile int OpFinishedFlg;
  
void FLASH1_OnOperationComplete(LDD_TUserData *UserDataPtr)
{
    OpFinishedFlg = TRUE;  /* Set DataWrittenFlg flag */
}

/*
** ###################################################################
**
**  The interrupt service routine(s) must be implemented
**  by user in one of the following user modules.
**
**  If the "Generate ISR" option is enabled, Processor Expert generates
**  ISR templates in the CPU event module.
**
**  User modules:
**      ProcessorExpert.c
**      Events.c
**
** ###################################################################
**/
PE_ISR(isr_PORTE)
{
	if(PORTE_ISFR & 0x2)
	{
		PORTE_ISFR |= 0x2; // clear (button) interrupt status bit.
	}
	
	if(PORTE_ISFR & 0x4)
	{
		PORTE_ISFR |= 0x4;
	}
}

/*
** ###################################################################
**
**  The interrupt service routine(s) must be implemented
**  by user in one of the following user modules.
**
**  If the "Generate ISR" option is enabled, Processor Expert generates
**  ISR templates in the CPU event module.
**
**  User modules:
**      ProcessorExpert.c
**      Events.c
**
** ###################################################################
*/
PE_ISR(isr_PORTD)
{
	if(PORTD_ISFR & 0x1)
	{
		PORTD_ISFR |= 0x1;
		//ciu_inc(INT_PORTA, 29); // \todo pick appropriate port/pin for CHG_B.
		printf("PORT D: CHG_B interrupt\n");
	}
}


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
void Cpu_OnNMIINT(void)
{
  /* Write your code here ... */
}

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
void SPI2_Flash_OnBlockSent(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_SPI2_Flash_Wait_for_Send = 0;
}

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
void SPI2_Flash_OnBlockReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_SPI2_Flash_Wait_for_Receive = 0;
}

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
void SPI2_Flash_OnError(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

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
void Sleep_Timer_LDD_OnInterrupt(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
  sleep_timer_tick();
}

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
void I2C2_OnMasterBlockSent(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
    g_I2C1_DataTransmittedFlg = TRUE;
}

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
void I2C2_OnMasterBlockReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
    g_I2C1_DataReceivedFlg = TRUE;
}

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
void I2C2_OnError(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

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
void whistle_random_OnError(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

/* END Events */

/*
** ###################################################################
**
**     This file was created by Processor Expert 10.0 [05.03]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
