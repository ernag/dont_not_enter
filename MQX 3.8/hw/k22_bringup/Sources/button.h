/*
 * button.h
 *
 *  Created on: Jan 10, 2013
 *      Author: Ernie Aguilar
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#include "k2x_errors.h"
#include "PE_Types.h"

// Note that the Interrupt Vector Table between the Mk20DN512VMC10 (what this PE project is based on)
//   and the K21/K22 that we use on Corona is different.  Therefore, we have to use enable_irq(XYZ) for
//   the NVIC configuration, as well as change the vectortable entry for PTA29 (our button) to:
//   /* 0x4B  0x0000012C   -   ivINT_CMP0   (or IRQ=59).
//   PE will overwrite the vector table when we re-generate software, so we will either have to re-add
//   our isr_Button each time manually, or come up with some clever way to map the ivINT_CMP0 handler to isr_Button.

k2x_error_t button_init(void);

#endif /* BUTTON_H_ */
