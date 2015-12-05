/** ###################################################################
**     Filename    : Events.c
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
/* MODULE Events */

#include "Cpu.h"
#include "Events.h"

/* User includes (#include below this line is not maintained by Processor Expert) */

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
void Cpu_OnNMIINT(void)
{
  /* Write your code here ... */
}

PE_ISR(PORTE_ISR)
{
	if(PORTE_ISFR & 0x2) // PTE1 is button.
	{
		printf("\nPORT E (BUTTON) Interrupt\n");
		//exit_vlpr();
		PORTE_ISFR |= 0x2; // PTE1 is button.
		//enter_vlls1();
	}
}

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
void Cpu_OnLLSWakeUpINT(void)
{
  /* Write your code here ... */
	printf("Low Leakage Wake-up has occurred!\n");
	/*
	 * 
	 *    TODO - twinkle a debug LED here.
	 */
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
