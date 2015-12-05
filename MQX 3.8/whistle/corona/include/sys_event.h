/*
 * sys_event.h
 *
 *  Created on: Mar 21, 2013
 *      Author: Chris
 */

#ifndef SYS_EVENT_H_
#define SYS_EVENT_H_

#include <mqx.h>
#include <bsp.h>
#include "app_errors.h"

/*
 *   This type represents an event, or an OR'd combination of events,
 *    defined in this file below.
 */
typedef uint_32   sys_event_t;

/*
 *   Defines the different types of system events.  For button, these should correspond to the requirements here:
 *    https://whistle.atlassian.net/wiki/display/COR/Device+User+Interfaces
 *
 *    System events can be OR'd together so you can register and be notified simultaneously for
 *     different events.  It is up to the caller/callback to decode which events have occured.
 *     Check out button_mgr_ut.c for example usage of sys_event callback service.
 */
#define SYS_EVENT_BUTTON_SINGLE_PUSH    0x1
#define SYS_EVENT_BUTTON_DOUBLE_PUSH    0x2
#define SYS_EVENT_BUTTON_LONG_PUSH      0x4
#define SYS_EVENT_BUTTON_HOLD           0x8
#define SYS_EVENT_WIFI_DOWN             0x10
#define SYS_EVENT_BT_DOWN               0x20
#define SYS_EVENT_USB_PLUGGED_IN        0x40
#define SYS_EVENT_USB_UNPLUGGED         0x80
#define SYS_EVENT_SYS_SHUTDOWN          0x100
#define SYS_EVENT_CHGB_FAULT            0x200
#define SYS_EVENT_CHARGED               0x400
#define SYS_EVENT_UNUSED_9              0x800
#define SYS_EVENT_UNUSED_10             0x1000
#define SYS_EVENT_UNUSED_11             0x2000
#define SYS_EVENT_UNUSED_12             0x4000
#define SYS_EVENT_UNUSED_13             0x8000
#define SYS_EVENT_UNUSED_14             0x10000
#define SYS_EVENT_UNUSED_15             0x20000
#define SYS_EVENT_UNUSED_16             0x40000
#define SYS_EVENT_UNUSED_17             0x80000
#define SYS_EVENT_UNUSED_18             0x100000
#define SYS_EVENT_UNUSED_19             0x200000
#define SYS_EVENT_UNUSED_20             0x400000
#define SYS_EVENT_UNUSED_21             0x800000
#define SYS_EVENT_UNUSED_22             0x1000000
#define SYS_EVENT_UNUSED_23             0x2000000
#define SYS_EVENT_UNUSED_24             0x4000000
#define SYS_EVENT_UNUSED_25             0x8000000
#define SYS_EVENT_UNUSED_26             0x10000000
#define SYS_EVENT_UNUSED_27             0x20000000
#define SYS_EVENT_UNUSED_28             0x40000000
#define SYS_EVENT_UNUSED_29             0x80000000

/*
 *   Callback Interface that callback subscribers must use for registering.
 */
typedef void (*sys_event_cbk_t)(sys_event_t sysEvent);

/*
 *   Callback registration API
 */
err_t sys_event_register_cbk(sys_event_cbk_t pCallback, sys_event_t event);

/*
 *   Callback UN-registration API
 */
err_t sys_event_unregister_cbk(sys_event_cbk_t pCallback);

void sys_event_init(void);

/*
 * Main task
 */
void SYSEVT_tsk(uint_32 dummy);
#endif /* SYS_EVENT_H_ */
