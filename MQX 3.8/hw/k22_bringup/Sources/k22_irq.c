/*
 * k22_irq.c
 *
 *  Created on: Jan 10, 2013
 *      Author: Ernie Aguilar
 */

#include "k22_irq.h"
#include "Cpu.h"
#include "Events.h"

/* Including shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"

/* User includes (#include below this line is not maintained by Processor Expert) */
#define ARM_INTERRUPT_LEVEL_BITS          4

/***********************************************************************/
/*
 * Initialize the NVIC to disable the specified IRQ.
 * 
 * NOTE: The function only initializes the NVIC to disable a single IRQ. 
 * If you want to disable all interrupts, then use the DisableInterrupts
 * macro instead. 
 *
 * Parameters:
 * irq    irq number to be disabled (the irq number NOT the vector number)
 */
void k22_disable_irq(k22_irq_t irq)
{
    int div;
    
    /* Make sure that the IRQ is an allowable number. Right now up to 91 is 
     * used.
     */
    if (irq > 91)
        printf("\nERR! Invalid IRQ value passed to disable irq function!\n");
    
    /* Determine which of the NVICICERs corresponds to the irq */
    div = irq/32;
    
    switch (div)
    {
    	case 0x0:
               NVICICER0 = 1 << (irq%32);
              break;
    	case 0x1:
              NVICICER1 = 1 << (irq%32);
              break;
    	case 0x2:
              NVICICER2 = 1 << (irq%32);
              break;
    }              
}
/***********************************************************************/
/*
 * Initialize the NVIC to set specified IRQ priority.
 * 
 * NOTE: The function only initializes the NVIC to set a single IRQ priority. 
 * Interrupts will also need to be enabled in the ARM core. This can be 
 * done using the EnableInterrupts macro.
 *
 * Parameters:
 * irq    irq number to be enabled (the irq number NOT the vector number)
 * prio   irq priority. 0-15 levels. 0 max priority
 */

void k22_set_irq_priority(k22_irq_t irq, int prio)
{
    /*irq priority pointer*/
    uint8 *prio_reg;
    
    /* Make sure that the IRQ is an allowable number. Right now up to 91 is 
     * used.
     */
    if (irq > 91)
        printf("\nERR! Invalid IRQ value passed to priority irq function!\n");

    if (prio > 15)
        printf("\nERR! Invalid priority value passed to priority irq function!\n");
    
    /* Determine which of the NVICIPx corresponds to the irq */
    prio_reg = (uint8 *)(((uint32)&NVICIP0) + irq);
    /* Assign priority to IRQ */
    *prio_reg = ( (prio&0xF) << (8 - ARM_INTERRUPT_LEVEL_BITS) );             
}

// Set up the NVIC for K22.
// k22_Vector is the Vector number (labeled 'No.' in the Vector.c vector table).
// ISR_name is the name (address of) of the ISR

void k22_enable_irq(k22_irq_t irq, k22_vector_t vector, void *ISR_name)
{
    int div;
    
    /* Make sure that the IRQ is an allowable number. Right now up to 91 is 
     * used.
     */
    if (irq > 91)
        printf("\nERR! Invalid IRQ value passed to enable irq function!\n");
    
    /* Determine which of the NVICISERs corresponds to the irq */
    div = irq/32;
    
    switch (div)
    {
    	case 0x0:
              NVICICPR0 |= 1 << (irq%32);
              NVICISER0 |= 1 << (irq%32);
              break;
    	case 0x1:
              NVICICPR1 |= 1 << (irq%32);
              NVICISER1 |= 1 << (irq%32);
              break;
    	case 0x2:
              NVICICPR2 |= 1 << (irq%32);
              NVICISER2 |= 1 << (irq%32);
              break;
    }

    *((void **)((char *)&__vect_table.__fun[vector-1])) = ISR_name;
}

