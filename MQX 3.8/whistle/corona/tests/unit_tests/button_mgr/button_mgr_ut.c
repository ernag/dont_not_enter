/*
 * button_mgr_ut.c
 *
 *  Created on: Mar 8, 2013
 *      Author: Ernie
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "button_mgr_ut.h"
#include "button.h"
#include "sys_event.h"
#include "wassert.h"

void unit_test_cbk0(sys_event_t sys_event);
void unit_test_cbk1(sys_event_t sys_event);
void unit_test_cbk2(sys_event_t sys_event);
void unit_test_cbk3(sys_event_t sys_event);
void unit_test_cbk4(sys_event_t sys_event);
void unit_test_cbk5(sys_event_t sys_event);
void unit_test_cbk6(sys_event_t sys_event);
void unit_test_cbk7(sys_event_t sys_event);
void unit_test_cbk8(sys_event_t sys_event);
void unit_test_cbk9(sys_event_t sys_event);


void print_outcome(uint_8 cbk_number, sys_event_t sys_event)
{
    if (sys_event & SYS_EVENT_BUTTON_SINGLE_PUSH)
    {
        corona_print("Callback %d received SINGLE_PUSH event.\n", cbk_number);
    }
    else if (sys_event & SYS_EVENT_BUTTON_DOUBLE_PUSH)
    {
        corona_print("Callback %d received DOUBLE_PUSH event.\n", cbk_number);
    }
    else if (sys_event & SYS_EVENT_BUTTON_LONG_PUSH)
    {
        corona_print("Callback %d received BUTTON_LONG_PUSH event.\n", cbk_number);
    }
    else if (sys_event & SYS_EVENT_BUTTON_LONG_PUSH)
    {
        corona_print("Callback %d received BUTTON_HOLD event.\n", cbk_number);
    }
    else
    {
        corona_print("ERROR! Callback %d has an unexpected event: 0x%x\n", cbk_number);
        wassert(0);
    }
    fflush(stdout);
}


void unit_test_cbk0(sys_event_t sys_event)
{
    print_outcome(0, sys_event);
}


void unit_test_cbk1(sys_event_t sys_event)
{
    print_outcome(1, sys_event);
}


void unit_test_cbk2(sys_event_t sys_event)
{
    print_outcome(2, sys_event);
}


void unit_test_cbk3(sys_event_t sys_event)
{
    print_outcome(3, sys_event);
}


void unit_test_cbk4(sys_event_t sys_event)
{
    print_outcome(4, sys_event);
}


void unit_test_cbk5(sys_event_t sys_event)
{
    print_outcome(5, sys_event);
}


void unit_test_cbk6(sys_event_t sys_event)
{
    print_outcome(6, sys_event);
}


void unit_test_cbk7(sys_event_t sys_event)
{
    print_outcome(7, sys_event);
}


void unit_test_cbk8(sys_event_t sys_event)
{
    print_outcome(8, sys_event);
}


void unit_test_cbk9(sys_event_t sys_event)
{
    print_outcome(9, sys_event);
}


/*
 *   Register a bunch of button callbacks, and see if we can capture all of them.
 */
void button_mgr_ut(void)
{
	sys_event_register_cbk(unit_test_cbk0, ALL_BUTTON_EVENTS);
	sys_event_register_cbk(unit_test_cbk1, ALL_BUTTON_EVENTS);
	sys_event_register_cbk(unit_test_cbk2, ALL_BUTTON_EVENTS);
	sys_event_register_cbk(unit_test_cbk3, ALL_BUTTON_EVENTS);
	sys_event_register_cbk(unit_test_cbk4, ALL_BUTTON_EVENTS);
	sys_event_register_cbk(unit_test_cbk5, ALL_BUTTON_EVENTS);
	sys_event_register_cbk(unit_test_cbk6, ALL_BUTTON_EVENTS);
	sys_event_register_cbk(unit_test_cbk7, ALL_BUTTON_EVENTS);
	sys_event_register_cbk(unit_test_cbk8, ALL_BUTTON_EVENTS);
    sys_event_register_cbk(unit_test_cbk9, ALL_BUTTON_EVENTS);

#if 0
    sys_event_unregister_cbk(unit_test_cbk4);   // unregister just one, to make sure we can...
#endif
}
#endif
