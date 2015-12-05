/*
 * periodic_timer.h
 *
 *  Created on: Jan 14, 2013
 *      Author: Chris
 */

#ifndef PERIODIC_TIMER_H_
#define PERIODIC_TIMER_H_

#include "k2x_errors.h"
#include "PE_Types.h"

// Note that the Interrupt Vector Table between the Mk20DN512VMC10 (what this PE project is based on)
//   and the K21/K22 that we use on Corona is different.  Therefore, we have to use enable_irq(XYZ) for
//   the NVIC configuration, as well as change the vectortable entry for FTM0 (sleep timer) to:
//   /* 0x3A  0x000000E8   -   ivINT_CAN1_Wake_Up   (or IRQ=42)
//   PE will overwrite the vector table when we re-generate software, so we will either have to re-add
//   our isr_Button each time manually, or come up with some clever way to map the ivINT_CAN1_Wake_Up handler to isr_Button.

k2x_error_t sleep_timer_init(void);

void sleep_timer_tick(void);
void sleep(int ms);

#endif /* PERIODIC_TIMER_H_ */
