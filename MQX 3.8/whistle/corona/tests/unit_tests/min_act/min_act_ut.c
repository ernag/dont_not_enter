#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "min_act_ut.h"

#if (CALCULATE_MINUTES_ACTIVE)
extern uint16_t g_daily_min_active;
extern uint16_t g_prev_block_min;
extern uint16_t g_prev_block_day;
extern uint32_t g_points_this_min;


static int8_t g_test_array[] = {16, 32, 48, 
							    17, 34, 52, 
							    16, 33, 50, 
							    8, 17, 26, 
							    -9, -19, -28,
							    -24, -48, -72,
							    -4, -9, -14, 
							    26, 53, 80, 
							    -5, -10, -16, 
							    -22, -44, -66};
static size_t g_n_rows = 10;
static uint64_t g_device_milli = 61000;
static uint64_t g_day_milli = 8600000000;

void pnt_calc_ut(void){
	float point_diff;
	MIN_ACT_calc(g_test_array, g_n_rows * 3, 0, g_device_milli, g_day_milli);
	MIN_ACT_finish();
	if (g_prev_block_min != 1)
		corona_print("The previous block minute should have been 1 but was not!");
	if (g_prev_block_day != 99)
			corona_print("The previous block day should have been 99 but was not!");
	if (g_daily_min_active != 0)
		corona_print("The daily minutes active should have been 0 but was not!");
	point_diff = g_points_this_min - 18.984375;
	point_diff = point_diff < 0 ? -1 * point_diff : point_diff; 
	if (point_diff > 0.1)
			corona_print("The current point value should have been 18.984 but was not!");
}
#endif

#endif
