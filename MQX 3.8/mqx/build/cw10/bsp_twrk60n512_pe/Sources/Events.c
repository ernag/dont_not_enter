/** ###################################################################
**     Filename  : Events.c
**     Project   : ProcessorExpert
**     Processor : MK60DN512ZVMD10
**     Component : Events
**     Version   : Driver 01.00
**     Compiler  : CodeWarrior ARM C Compiler
**     Date/Time : 2011-11-23, 16:45, # CodeGen: 0
**     Abstract  :
**         This is user's event module.
**         Put your event handler code here.
**     Settings  :
**     Contents  :
**         PWM_OnCounterRestart  - void PWM_OnCounterRestart(LDD_TUserData *UserDataPtr);
**         DA1_OnBufferWatermark - void DA1_OnBufferWatermark(LDD_TUserData *UserDataPtr);
**         DA1_OnBufferEnd       - void DA1_OnBufferEnd(LDD_TUserData *UserDataPtr);
**         DA1_OnBufferStart     - void DA1_OnBufferStart(LDD_TUserData *UserDataPtr);
**         TRG_OnCounterRestart  - void TRG_OnCounterRestart(LDD_TUserData *UserDataPtr);
**
** ###################################################################*/
/* MODULE Events */

#include "Cpu.h"
#include "Events.h"

/* User includes (#include below this line is not maintained by Processor Expert) */

/*
** ===================================================================
**     Event       :  PWM_OnCounterRestart (module Events)
**
**     Component   :  PWM [TimerUnit_LDD]
**     Description :
**         Called if counter overflow/underflow or counter is
**         reinitialized by modulo or compare register matching.
**         OnCounterRestart event and Timer unit must be enabled. See
**         <SetEventMask> and <GetEventMask> methods. This event is
**         available only if a <Interrupt> is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer passed as
**                           the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void PWM_OnCounterRestart(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Event       :  DA1_OnBufferWatermark (module Events)
**
**     Component   :  DA1 [DAC_LDD]
**     Description :
**         Buffer read pointer reached the watermark level. This event
**         can be enabled only if Interrupt service is enabled and Data
**         buffer is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data.
**     Returns     : Nothing
** ===================================================================
*/
void DA1_OnBufferWatermark(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Event       :  DA1_OnBufferEnd (module Events)
**
**     Component   :  DA1 [DAC_LDD]
**     Description :
**         Buffer read pointer reached buffer top position. This event
**         can be enabled only if Interrupt service is enabled and Data
**         buffer is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data.
**     Returns     : Nothing
** ===================================================================
*/
void DA1_OnBufferEnd(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Event       :  DA1_OnBufferStart (module Events)
**
**     Component   :  DA1 [DAC_LDD]
**     Description :
**         Buffer read pointer reached buffer bottom position. This
**         event can be enabled only if Interrupt service is enabled
**         and Data buffer is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data.
**     Returns     : Nothing
** ===================================================================
*/
void DA1_OnBufferStart(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Event       :  TRG_OnCounterRestart (module Events)
**
**     Component   :  TRG [TimerUnit_LDD]
**     Description :
**         Called if counter overflow/underflow or counter is
**         reinitialized by modulo or compare register matching.
**         OnCounterRestart event and Timer unit must be enabled. See
**         <SetEventMask> and <GetEventMask> methods. This event is
**         available only if a <Interrupt> is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer passed as
**                           the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void TRG_OnCounterRestart(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

/* END Events */

/*
** ###################################################################
**
**     This file was created by Processor Expert 5.2 [04.49]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
