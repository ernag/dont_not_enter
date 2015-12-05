/*
 * main.h
 *
 *  Created on: Jun 14, 2013
 *      Author: Chris
 */

#ifndef MAIN_H_
#define MAIN_H_

#include "mqx.h"

// Task Names
#define MAIN_TASK_NAME      "main"
#define CONSOLE_TASK_NAME   "cnsl"
#define BUTTON_TASK_NAME    "btndrv"
#define SYSEVENT_TASK_NAME  "syevt"
#define CDC_TASK_NAME       "CDC"
#define BTNMGR_TASK_NAME    "btn"
#define BT_TASK_NAME        "btdsc"
#define CONMGR_TASK_NAME    "con"
#define POST_TASK_NAME      "pst"
#define DUMP_TASK_NAME      "dmp"
#define UPLOAD_TASK_NAME    "upl"
#define ACC_TASK_NAME       "acc"
#define ACCOFF_TASK_NAME    "accoff"
#define DISPLAY_TASK_NAME   "dis"
#define PWR_TASK_NAME       "pwr"
#define PER_TASK_NAME       "per"
#define RTCW_TASK_NAME      "rtcw"
#define SHLK_TASK_NAME      "shlk"
#define FWU_TASK_NAME       "fwu"
#define BURNIN_TASK_NAME    "BI"
#define RTC_ALARM_TASK_NAME "rtcalm"


#if !DRIVER_CONFIG_INCLUDE_BMI
typedef enum whistle_task_template_index_type
{
    
    TEMPLATE_IDX_MAIN = 1,            // 1
    TEMPLATE_IDX_BUTTON_DRV,
    TEMPLATE_IDX_USBCDC,
    TEMPLATE_IDX_CORONA_CONSOLE,
    TEMPLATE_IDX_SYS_EVENT,           // 5
    TEMPLATE_IDX_CONMGR,
    TEMPLATE_IDX_EVTMGR_POST,
    TEMPLATE_IDX_EVTMGR_DUMP,
    TEMPLATE_IDX_EVTMGR_UPLOAD,       
    TEMPLATE_IDX_ACCMGR,              // 10
    TEMPLATE_IDX_ACCMGR_OFFLOAD,
    TEMPLATE_IDX_PWRMGR,
    TEMPLATE_IDX_SHERLOCK,            
    TEMPLATE_IDX_DISMGR,
    TEMPLATE_IDX_BTNMGR,              // 15
    TEMPLATE_IDX_FWUMGR,
    TEMPLATE_IDX_BTMGR_DISC,
    TEMPLATE_IDX_PERMGR,
    TEMPLATE_IDX_RTCWMGR,
    TEMPLATE_IDX_BURNIN,              // 20
    TEMPLATE_IDX_RTC_ALARM,

    WHISTLE_NUM_TASKS       // Must be last enum.
    
} wtemplate_idx_t;

#else
const TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
        /* Task Index, Function, Stack, Priority, Name, Attributes, Param, Time Slice */
        { TEMPLATE_IDX_MAIN, whistle_main_task, 2000, 9, "main", MQX_AUTO_START_TASK, 0, 0 },
        { TEMPLATE_IDX_FLASH_AGENT, Flash_Agent_Task, 2800, 9, "Flash", 0, 0, 0 },
        { TEMPLATE_IDX_USBCDC, USBCDC_Task, 768, 7L, "USB", 0, 0, 0 },
        { 0 } 
};
#endif

uint_32 whistle_task_create(wtemplate_idx_t idx);
void whistle_kill_all_tasks(void);
uint_32 whistle_task_destroy(const char *pTaskName);
TASK_TEMPLATE_STRUCT *whistle_get_task_template_list(void);

#endif /* MAIN_H_ */
