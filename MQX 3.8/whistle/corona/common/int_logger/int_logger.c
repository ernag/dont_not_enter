/*
 * int_enable_wrapper.c
 *
 *  Created on: Feb 3, 2014
 *      Author: Chris
 */

#include "main.h"
#include "rtc_alarm.h"
#include "wassert.h"
#include "bsp.h"
#include "app_errors.h"

#if INT_LOGGING

#define MAX_INT_DISABLE_TIME 1000

struct _int_log
{
	int int_disable_depth;
	uint64_t start_time;
	int high_water_mark_time_delta;
};

struct _int_log int_log[WHISTLE_NUM_TASKS] = {0};

void wint_dump(void)
{
	int i;
	TASK_TEMPLATE_STRUCT *task_template_list = whistle_get_task_template_list();
	
	corona_print("Index Name Depth High\n");
	
	for (i = 0; i < WHISTLE_NUM_TASKS - 1; ++i)
	{
		_mqx_uint t_idx = task_template_list[i].TASK_TEMPLATE_INDEX;
		if (0 != t_idx)
		{
			corona_print("%d %s %d %d\n", t_idx, task_template_list[i].TASK_NAME, int_log[t_idx].int_disable_depth, int_log[t_idx].high_water_mark_time_delta);
		}
	}
}

static _mqx_uint wint_get_idx()
{
	_mqx_uint idx = 0xffffffff;
	
	if ( 0 != _int_get_isr_depth() )
	{
		/* isr (not task) context */
		goto DONE;
	}
		
	idx = _task_get_index_from_id(_task_get_id());
	
	if ( WHISTLE_NUM_TASKS <= idx || 1 > idx)
	{
		/* not a task we know about */
		idx = 0xffffffff;
		goto DONE;
	}
	
DONE:
	return idx;
}

void wint_enable(void)
{
	_mqx_uint idx;
	int depth;
    uint64_t end_time;
    int time_delta = -1;
    
    idx = wint_get_idx();
    	
	if (0xffffffff == idx)
	{
		goto DONE;
	}	
	
	wassert(0 < int_log[idx].int_disable_depth);
	
	if (0 == --int_log[idx].int_disable_depth)
	{
		RTC_get_ms(&end_time);
		time_delta = end_time - int_log[idx].start_time;
	}
		
DONE:
	_int_enable();
	
	// if we're back to enabled, do some analysis
	// defer this until after ints are back enabled
	if (-1 != time_delta)
	{
		if (time_delta > int_log[idx].high_water_mark_time_delta)
		{
			int_log[idx].high_water_mark_time_delta = time_delta;
			
			if (MAX_INT_DISABLE_TIME < time_delta)
			{
				PROCESS_NINFO(ERR_INTERRUPT_DISABLED_TOO_LONG, "%d %d", idx, int_log[idx].high_water_mark_time_delta);
			}
			
			fprintf(stdout, "ihwm(%d) %d\n", idx, time_delta); // corona_print too expensive here
		}
	}
}

void wint_disable(void)
{
	int depth;
	_mqx_uint idx;
	
    idx = wint_get_idx();
    	
	if (0xffffffff == idx)
	{
		goto DONE;
	}

	wassert(0 <= int_log[idx].int_disable_depth);
	
	if (0 == int_log[idx].int_disable_depth++)
	{
		RTC_get_ms(&int_log[idx].start_time);
	}
		
DONE:
	_int_disable();
}
#endif
