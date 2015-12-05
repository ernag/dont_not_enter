//------------------------------------------------------------------------------
// Copyright (c) 2011 Qualcomm Atheros, Inc.
// All Rights Reserved.
// Qualcomm Atheros Confidential and Proprietary.
// Permission to use, copy, modify, and/or distribute this software for any purpose with or without fee is
// hereby granted, provided that the above copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
// INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
// USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================


#include "main.h"
#include "throughput.h"

/*
** MQX initialization information
*/

const TASK_TEMPLATE_STRUCT MQX_template_list[] =
{
   /* Task Index,   Function,         Stack,  Priority, Name,       Attributes,             Param,  Time Slice */    
#if FLASH_APP
    { FLASH_AGENT_TASK,    Flash_Agent_Task,         2800/*1700*/,    9,      "Flash",     MQX_AUTO_START_TASK,    0,      0           },
#else
    { WMICONFIG_TASK1,  wmiconfig_Task1,  3000,    9,      "WMICONFIG1",	MQX_AUTO_START_TASK,  0,      0           },   
    { WMICONFIG_TASK2,  wmiconfig_Task2,  3000,    9,      "WMICONFIG2", MQX_AUTO_START_TASK,  0,      0           },
#endif
#if DEMOCFG_ENABLE_SERIAL_SHELL
    { SHELL_TASK,   Shell_Task,        1500,   12,      "Shell",    MQX_AUTO_START_TASK,      0,      0           },
#endif

    {0}
};



 
/* EOF */
