/*
 * whistle_lwgpio_isr_mux.h
 *
 *  Created on: Mar 8, 2013
 *      Author: Chris
 */

#ifndef __whistle_lwgpio_isr_mux_h__
#define __whistle_lwgpio_isr_mux_h__ 1

#include <mqx.h>
#include <bsp.h>

typedef void (*whistle_lwgpio_isr_mux_isr_t)(void *pointer);

void whistle_lwgpio_isr_install(LWGPIO_STRUCT_PTR lwgpio, _int_priority priority, whistle_lwgpio_isr_mux_isr_t isr);

#endif
