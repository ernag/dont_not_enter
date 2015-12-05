/*
 * main implementation: use this 'C' sample to create your own application
 *
 */

#define PE_MCUINIT

#include <stdio.h>

#include "derivative.h" /* include peripheral declarations */

void MCU_init(void); /* Device initialization function declaration */

int main(void)
{
	int counter = 0;
	
	printf("Pin Mux Application\n");
	
	MCU_init(); /* call device initialization */
	
	
	for(;;) {	   
	   	counter++;
	}
	
	return 0;
}
