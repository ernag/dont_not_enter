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

// Defines an entry into the ISR trigger tracker.
typedef struct corona_isr_util_entry
{
	const IRQInterruptIndex idx;   // IRQ number
	const char *pName;             // Corona-friendly name of signal/ISR
	uint32_t u32Count;             // How many times interrupt has happened since we started or cleared
	uint8_t  u8Pin;                // Pin number within a port, when applicable.  Use 0xFF when it doesn't matter.
}isr_uentry_t;

corona_err_t ciu_inc(IRQInterruptIndex idx, uint8_t pin);
corona_err_t ciu_clr(IRQInterruptIndex idx, uint8_t pin);
uint32_t     ciu_cnt(IRQInterruptIndex idx, uint8_t pin);

corona_err_t ciu_clr_str(const char *pName);
corona_err_t ciu_cnt_str(const char *pName, uint32_t *pCount);


#endif /* CORONA_ISR_UTIL_H_ */
