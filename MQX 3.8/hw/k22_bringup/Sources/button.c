/*
 * button.c
 *
 *  Created on: Jan 10, 2013
 *      Author: Ernie Aguilar
 */

#include "button.h"
#include "k22_irq.h"
#include "IO_Map.h"
#include "PE_Types.h"
#include "corona_isr_util.h"

// We have to explicitly init the button ISR stuff, since PE for M20KN512VMC10 
//   doesn't place the ISR vector table in the right place.
k2x_error_t button_init(void)
{
	PE_ISR(isr_Button);
	
	printf("Initializing Button IRQ for PTA29\n");
	k22_enable_irq(IRQ_PORT_A, VECTOR_PORT_A, isr_Button); // 59 is the button IRQ
	PORTA_ISFR = 0x20000000; // clear PTA29 (button) ISR, and wait for interrupt event to occur.
	return SUCCESS;
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
PE_ISR(isr_Button)   // Note you have to put this in the 0x12C entry of the vectortable for this to work.
{
	if(PORTA_ISFR & 0x20000000)
	{
		PORTA_ISFR = 0x20000000; // clear PTA29 (button) interrupt status bit.
		ciu_inc(INT_PORTA, 29);
		//printf("Button Pressed\n");
	}
#if 0
	/*
	 *  Unit testing for CIU module.
	 */
	if(ciu_cnt(INT_PORTA, 29) == 15)
	{
		printf("Button Count = 15!\n");
		ciu_clr(INT_PORTA, 29);
	}
#endif
}
