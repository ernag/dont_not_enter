/** ###################################################################
**     Filename    : ProcessorExpert.c
**     Project     : ProcessorExpert
**     Processor   : MK60DN512VMC10
**     Version     : Driver 01.01
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2013-02-28, 14:00, # CodeGen: 0
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/* MODULE ProcessorExpert */


/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "Events.h"
#include "PORTB_GPIO.h"
#include "PORTC_GPIO.h"
#include "PORTE_GPIO.h"
#include "PORTD_GPIO.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"

/* User includes (#include below this line is not maintained by Processor Expert) */

/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */
	int i = 100;
	//uint32 *pMDM_AP = (uint32 *)0x01000004; // MDM-AP Control Register

	
  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  /* For example: for(;;) { } */

  printf("Put Corona in _SHIP_ mode...\n");
  //printf("SIM_SCGC1: 0x%x\n", SIM_SCGC1);
  //printf("SIM_SCGC2: 0x%x\n", SIM_SCGC2);
  //printf("SIM_SCGC3: 0x%x\n", SIM_SCGC3);
  //printf("SIM_SCGC4: 0x%x\n", SIM_SCGC4);
  //printf("SIM_SCGC5: 0x%x\n", SIM_SCGC5); // We do not want to clock gate the PORTS in LLWU mode.
  //printf("SIM_SCGC6: 0x%x\n", SIM_SCGC6); // fully optimized.
  
  SIM_SCGC7 = 0;
  //printf("SIM_SCGC7: 0x%x\n", SIM_SCGC7);  // fully optimized.
  
  SIM_CLKDIV1 |=  (0xFFFF0000); // maximum divides for all clocks.
  //printf("SIM_CLKDIV1: 0x%x\n", SIM_CLKDIV1); // fully optimized now.
  
  //printf("MDM-AP Control Register: 0x%x\n", *pMDM_AP);
  
  llwu_configure(0x1, 0x2, 0x1);  // PTE1 (button) can wake up out of VLLS1 state, this "0xFF" probably isn't quite right...
  while(i-- != 0){} // a completely idiotic form of a (potentially unnecessary) delay.
  
  MCG_C6 &= ~MCG_C6_CME0_MASK; // disables the clock monitor
  PORTA_PCR2 = PORT_PCR_PE_MASK | PORT_PCR_PS_MASK; // enables pull-up on TDO (they do this in prepapreForLowestPower() in demo).
  
  enter_vlls1();
  
  // We shouldn't really be able to get here, until maybe after we press the button !
  for(;;)
  {
	  printf(".");
  }

  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END ProcessorExpert */
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.0 [05.03]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
