/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved                       
*
* Copyright (c) 1989-2008 ARC International;
* All Rights Reserved
*
*************************************************************************** 
*
* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
* THE POSSIBILITY OF SUCH DAMAGE.
*
**************************************************************************
*
* $FileName: taskq.c$
* $Version : 3.8.9.0$
* $Date    : Oct-4-2011$
*
* Comments:
*
*   This file contains the source for the task queue example program.
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


/* Task IDs */
#define SERVICE_TASK 5
#define ISR_TASK     6

extern void simulated_ISR_task(uint_32);
extern void service_task(uint_32);

const TASK_TEMPLATE_STRUCT  MQX_template_list[] = 
{
   /* Task Index,   Function,           Stack, Priority,  Name,            Attributes,          Param, Time Slice */
    { SERVICE_TASK, service_task,       2000,  8,         "service",       MQX_AUTO_START_TASK, 0,     0 },
    { ISR_TASK,     simulated_ISR_task, 2000,  8,         "simulated_ISR", 0,                   0,     0 },
    { 0 }
};

pointer   my_task_queue;

/*TASK*-----------------------------------------------------
* 
* Task Name : simulated_ISR_task
* Comments  :
*   This task pauses and then resumes the task queue.
*END*-----------------------------------------------------*/

void simulated_ISR_task 
   (
      uint_32 initial_data
   )
{

   while (TRUE) {
      _time_delay_ticks(200);
      _taskq_resume(my_task_queue, FALSE);
   }
}


/*TASK*-----------------------------------------------------
* 
* Task Name : service_task
* Comments  :
*    This task creates a task queue and the simulated_ISR_task
*    task. Then it enters an infinite loop, printing "Tick" and
*    suspending the task queue.
*END*-----------------------------------------------------*/

void service_task
   (
      uint_32 initial_data
   )
{
   _task_id second_task_id;

   /* create a task queue */
   my_task_queue = _taskq_create(MQX_TASK_QUEUE_FIFO);
   if (my_task_queue == NULL) {
      _task_block();
   }

   /* create the ISR task */
   second_task_id = _task_create(0, ISR_TASK, 0);
   if (second_task_id == MQX_NULL_TASK_ID) {
      printf ("\n Could not create simulated_ISR_task\n");
      _task_block();
   }
   
   while (TRUE) {
      printf(" Tick \n");
      _taskq_suspend(my_task_queue);
   }

}

/* EOF */
