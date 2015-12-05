/*
 * k22_irq.h
 *
 *  Created on: Jan 10, 2013
 *      Author: Ernie Aguilar
 */

#ifndef K22_IRQ_H_
#define K22_IRQ_H_

#include "PE_Types.h"

// K21/K22 IRQ's taken from the Freescale reference manual.
typedef enum k22_irq
{
	IRQ_SPI0   = 26,
	IRQ_SPI1   = 27,
	IRQ_UART2_STATUS = 35,
	IRQ_UART2_ERR = 36,
	IRQ_FTM0   = 42,
	IRQ_PORT_A = 59,
	IRQ_PORT_B = 60,
	IRQ_PORT_C = 61,
	IRQ_PORT_D = 62,
	IRQ_PORT_E = 63,
	IRQ_SPI2	= 65,
	IRQ_UART5_STATUS = 68,
	IRQ_UART5_ERR = 69,
	
} k22_irq_t;

// K21/K22 Vector numbers taken from the Freescale reference manual.
typedef enum k22_vector
{
	VECTOR_SPI0   = 42,
	VECTOR_SPI1   = 43,
	VECTOR_UART2_STATUS = 51,
	VECTOR_UART2_ERR = 52,
	VECTOR_FTM0   = 58,
	VECTOR_PORT_A = 75,
	VECTOR_PORT_B = 76,
	VECTOR_PORT_C = 77,
	VECTOR_PORT_D = 78,
	VECTOR_PORT_E = 79,
	VECTOR_SPI2		= 81,
	VECTOR_UART5_STATUS = 84,
	VECTOR_UART5_ERR = 85,
	
} k22_vector_t;

void k22_enable_irq(k22_irq_t irq_number, k22_vector_t vector_number, void *ISR_name);
void k22_disable_irq(k22_irq_t irq);
void k22_set_irq_priority(k22_irq_t irq, int prio);

#endif /* K22_IRQ_H_ */
