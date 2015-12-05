/*
 * sleep_timer_test.c
 *
 *  Created on: Jan 14, 2013
 *      Author: Chris
 */

#include "sleep_timer.h"

k2x_error_t sleep_timer_test(void)
{
  printf("sleeping 5000 ms...\n");
  sleep(5000);
  printf("done\n");
}
