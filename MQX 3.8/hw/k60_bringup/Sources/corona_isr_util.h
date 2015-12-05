/*
 * corona_isr_util.h
 *
 *  Created on: Feb 12, 2013
 *      Author: Ernie Aguilar
 */

#ifndef CORONA_ISR_UTIL_H_
#define CORONA_ISR_UTIL_H_

// Use this define for "pin" input to all API's, where the pin within the port is not applicable.
#define PIN_NOT_APPLICABLE	0xFF

// Enumerates the configuration options in the K60 PORTx_PCRn IRQC 19:16 field.
//  This field is only applicable to GPIO interrupts.
typedef enum corona_isr_util_irqc
{
	IRQC_DISABLED       = 0b0000,
	IRQC_DMA_RISING     = 0b0001,
	IRQC_DMA_FALLING    = 0b0010,
	IRQC_DMA_EITHER     = 0b0011,
	IRQC_LOGIC_0        = 0b1000,
	IRQC_EDGE_RISING    = 0b1001,
	IRQC_EDGE_FALLING   = 0b1010,
	IRQC_EDGE_EITHER    = 0b1011,
	IRQC_LOGIC_1        = 0b1100,
	IRQC_NOT_APPLICABLE = 0b1110,
	IRQC_INVALID        = 0b1111,
	IRQC_REQUEST_STATUS = 0xDEADBEEF  // use this when all we care about is reading status.

}isr_irqc_t;

// Defines an entry into the ISR trigger tracker.
typedef struct corona_isr_util_entry
{
	const uint32_t   idx;       // IRQ number from "IRQInterruptIndex" table.
	const char       *pName;    // Corona-friendly name of signal/ISR
	uint32_t         u32Count;  // How many times interrupt has happened since we started or cleared
	const uint8_t    u8Pin;     // Pin number within a port, when applicable.  Use 0xFF when it doesn't matter.
	const isr_irqc_t irqc;      // IRQC status to set when enabling the interrupt.
}isr_uentry_t;

corona_err_t ciu_inc(uint32_t idx, uint8_t pin);
corona_err_t ciu_clr(uint32_t idx, uint8_t pin);
uint32_t     ciu_cnt(uint32_t idx, uint8_t pin);
void         ciu_status(isr_uentry_t *pEntry, volatile isr_irqc_t *pIrqc);
const char * ciu_status_str(volatile isr_irqc_t *pIrqc);
isr_uentry_t * get_uentry_str(const char *pName);

corona_err_t ciu_clr_str(const char *pName);
corona_err_t ciu_cnt_str(const char *pName, uint32_t *pCount);


#endif /* CORONA_ISR_UTIL_H_ */
