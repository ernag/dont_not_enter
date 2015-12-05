/*HEADER**********************************************************************
*
* Copyright 2008 Freescale Semiconductor, Inc.
* Copyright 1989-2008 ARC International
*
* This software is owned or controlled by Freescale Semiconductor.
* Use of this software is governed by the Freescale MQX RTOS License
* distributed with this Material.
* See the MQX_RTOS_LICENSE file distributed for more details.
*
* Brief License Summary:
* This software is provided in source form for you to use free of charge,
* but it is not open source software. You are allowed to use this software
* but you cannot redistribute it or derivative works of it in source form.
* The software may be used only in connection with a product containing
* a Freescale microprocessor, microcontroller, or digital signal processor.
* See license agreement file for full license terms including other
* restrictions.
*****************************************************************************
*
* Comments:
*
*   This file contains the source for the isr example program.
*
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>


#if ! BSPCFG_ENABLE_IO_SUBSYSTEM
#error This application requires BSPCFG_ENABLE_IO_SUBSYSTEM defined non-zero in user_config.h. Please recompile BSP with this option.
#endif


#ifndef BSP_DEFAULT_IO_CHANNEL_DEFINED
#error This application requires BSP_DEFAULT_IO_CHANNEL to be not NULL. Please set corresponding BSPCFG_ENABLE_TTYx to non-zero in user_config.h and recompile BSP with this option.
#endif


#define MAIN_TASK 10
extern void main_task(uint32_t);
extern void new_tick_isr(void *);




const TASK_TEMPLATE_STRUCT  MQX_template_list[] = 
{
   /* Task Index, Function,  Stack, Priority, Name,        Attributes,          Param, Time Slice */
    { MAIN_TASK,  main_task, 2000,  8,        "Main",      MQX_AUTO_START_TASK, 0,     0 },
    { 0 }
};

typedef struct my_isr_struct
{
   void                 *OLD_ISR_DATA;
   void      (_CODE_PTR_ OLD_ISR)(void *);
   _mqx_uint             TICK_COUNT;
} MY_ISR_STRUCT, * MY_ISR_STRUCT_PTR;

/*ISR*-----------------------------------------------------------
*
* ISR Name : new_tick_isr
* Comments :
*   This ISR replaces the existing timer ISR, then calls the 
*   old timer ISR.
*END*-----------------------------------------------------------*/

void new_tick_isr
   (
      void   *user_isr_ptr
   )
{
   MY_ISR_STRUCT_PTR  isr_ptr;

   isr_ptr = (MY_ISR_STRUCT_PTR)user_isr_ptr;
   isr_ptr->TICK_COUNT++;

   /* Chain to the previous notifier */
   (*isr_ptr->OLD_ISR)(isr_ptr->OLD_ISR_DATA);
}

/*TASK*----------------------------------------------------------
*
* Task Name : main_task
* Comments  : 
*   This task installs a new ISR to replace the timer ISR.
*   It then waits for some time, finally printing out the
*   number of times the ISR ran.
*END*-----------------------------------------------------------*/

void main_task
   (
      uint32_t initial_data
   )
{
   MY_ISR_STRUCT_PTR  isr_ptr;

   isr_ptr = _mem_alloc_zero((_mem_size)sizeof(MY_ISR_STRUCT));

   isr_ptr->TICK_COUNT   = 0;
   isr_ptr->OLD_ISR_DATA =
      _int_get_isr_data(BSP_TIMER_INTERRUPT_VECTOR);
   isr_ptr->OLD_ISR      =
      _int_get_isr(BSP_TIMER_INTERRUPT_VECTOR);

   _int_install_isr(BSP_TIMER_INTERRUPT_VECTOR, new_tick_isr,
      isr_ptr);

   _time_delay_ticks(200);

   printf("\nTick count = %d\n", isr_ptr->TICK_COUNT);

   _task_block();

}

/* EOF */
