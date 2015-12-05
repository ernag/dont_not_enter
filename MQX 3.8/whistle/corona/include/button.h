/*
 * button.h
 *
 *  Created on: Mar 8, 2013
 *      Author: Ernie
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#include "app_errors.h"
#include <mqx.h>
#include <bsp.h>

#define ALL_BUTTON_EVENTS          \
    SYS_EVENT_BUTTON_SINGLE_PUSH | \
    SYS_EVENT_BUTTON_DOUBLE_PUSH | \
    SYS_EVENT_BUTTON_LONG_PUSH   | \
    SYS_EVENT_BUTTON_HOLD

typedef enum button_event
{
    BUTTON_DOWN = 0x1,
    BUTTON_UP   = 0x2
} button_event_t;


void button_init(void);
void button_handler_action( void );
button_event_t button_get_val(void);
/*
 * Button main task
 */
void BTNDRV_tsk(uint_32 dummy);
#endif /* BUTTON_H_ */
