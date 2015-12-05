/** ###################################################################
**     THIS COMPONENT MODULE IS GENERATED BY THE TOOL. DO NOT MODIFY IT.
**     Filename    : PORTA_GPIO.h
**     Project     : ProcessorExpert
**     Processor   : MK20DN512VMC10
**     Component   : Init_GPIO
**     Version     : Component 01.003, Driver 01.03, CPU db: 3.00.000
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2013-02-07, 11:29, # CodeGen: 57
**     Abstract    :
**          This file implements the GPIO (PTA) module initialization
**          according to the Peripheral Initialization settings, and
**          defines interrupt service routines prototypes.
**     Settings    :
**          Component name                                 : PORTA_GPIO
**          Device                                         : PTA
**          Pins                                           : 
**            Pin 0                                        : Disabled
**            Pin 1                                        : Disabled
**            Pin 2                                        : Disabled
**            Pin 3                                        : Disabled
**            Pin 4                                        : Disabled
**            Pin 5                                        : Disabled
**            Pin 10                                       : Disabled
**            Pin 11                                       : Disabled
**            Pin 12                                       : Disabled
**            Pin 13                                       : Disabled
**            Pin 14                                       : Disabled
**            Pin 15                                       : Disabled
**            Pin 16                                       : Disabled
**            Pin 17                                       : Disabled
**            Pin 18                                       : Disabled
**            Pin 19                                       : Disabled
**            Pin 29                                       : Enabled
**              Pin                                        : PTA29/FB_AD19/FB_A24
**              Pin signal                                 : Button
**              Pin output                                 : Disabled
**              Output value                               : No initialization
**              Open drain enable                          : No initialization
**              Pull enable                                : Enabled
**              Pull select                                : Pull Up
**              Slew rate                                  : No initialization
**              Drive strength                             : No initialization
**              Interrupt configuration                    : Interrupt on falling
**              Digital filter enable                      : No initialization
**              Passive filter enable                      : Enabled
**              Lock                                       : No initialization
**            Digital filter clock source                  : Bus clock
**            Digital filter width                         : 0
**          Interrupts                                     : 
**            Port interrupt                               : Enabled
**              Interrupt                                  : INT_PORTA
**              Interrupt request                          : Enabled
**              Interrupt priority                         : 0 (Highest)
**              ISR Name                                   : isr_Button
**          Initialization                                 : 
**            Call Init method                             : yes
**     Contents    :
**         Init - void PORTA_GPIO_Init(void);
**
**     Copyright : 1997 - 2012 Freescale, Inc. All Rights Reserved.
**     
**     http      : www.freescale.com
**     mail      : support@freescale.com
** ###################################################################*/

#ifndef PORTA_GPIO_H_
#define PORTA_GPIO_H_

/* MODULE PORTA_GPIO. */

/* Including shared modules, which are used in the whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"
#include "Cpu.h"


void PORTA_GPIO_Init(void);
/*
** ===================================================================
**     Method      :  PORTA_GPIO_Init (component Init_GPIO)
**
**     Description :
**         This method initializes registers of the GPIO module
**         according to the Peripheral Initialization settings.
**         Call this method in user code to initialize the module. By
**         default, the method is called by PE automatically; see "Call
**         Init method" property of the component for more details.
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
/*
** ===================================================================
** The interrupt service routine must be implemented by user in one
** of the user modules (see PORTA_GPIO.c file for more information).
** ===================================================================
*/
PE_ISR(isr_Button);


/* END PORTA_GPIO. */
#endif /* #ifndef __PORTA_GPIO_H_ */
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.0 [05.03]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/