/** ###################################################################
**     Filename    : Events.c
**     Project     : ProcessorExpert
**     Processor   : MK60DN512VMD10
**     Component   : Events
**     Version     : Driver 01.00
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2012-03-06, 16:37, # CodeGen: 0
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

/*
** ===================================================================
**     Event       :  Cpu_OnLLSWakeUpINT (module Events)
**
**     Component   :  Cpu [MK60DN512MD10]
**     Description :
**         This event is called when Low Leakage WakeUp interrupt
**         occurs. LLWU flags indicating source of the wakeup can be
**         obtained by calling the <GetLLSWakeUpFlags> method. Flags
**         indicating the external pin wakeup source are automatically
**         cleared after this event is executed. It is responsibility
**         of user to clear flags corresponding to internal modules.
**         This event is automatically enabled when <LLWU settings> are
**         enabled.
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void Cpu_OnLLSWakeUpINT(void)
{
  /* Write your code here ... */
}

/* END Events */

/*
** ###################################################################
**
**     This file was created by Processor Expert 5.3 [05.01]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
