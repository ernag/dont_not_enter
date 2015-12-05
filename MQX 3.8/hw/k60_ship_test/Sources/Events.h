/** ###################################################################
**     Filename    : Events.h
**     Project     : ProcessorExpert
**     Processor   : MK60DN512VMC10
**     Component   : Events
**     Version     : Driver 01.00
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2013-02-28, 14:00, # CodeGen: 0
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
#include "PORTB_GPIO.h"
#include "PORTC_GPIO.h"
#include "PORTE_GPIO.h"
#include "PORTD_GPIO.h"
#include "PE_LDD.h"

void Cpu_OnNMIINT(void);
/*
** ===================================================================
**     Event       :  Cpu_OnNMIINT (module Events)
**
**     Component   :  Cpu [MK60DN512LQ10]
**     Description :
**         This event is called when the Non maskable interrupt had
**         occurred. This event is automatically enabled when the <NMI
**         interrrupt> property is set to 'Enabled'.
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/


void Cpu_OnLLSWakeUpINT(void);
/*
** ===================================================================
**     Event       :  Cpu_OnLLSWakeUpINT (module Events)
**
**     Component   :  Cpu [MK60DN512MC10]
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
