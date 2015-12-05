/*
 * exp_handler_ut.c
 *
 *  Created on: May 14, 2013
 *      Author: Ernie
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <mqx.h>
#include <stdio.h>
#include "corona_console.h"

static void expt_frm_dump(pointer ext_frm_ptr)  
{  
    static char *expt_name[] = {  
                                    "None",  
                                    "Reset",  
                                    "NMI",  
                                    "HardFault",  
                                    "MemManage",  
                                    "BusFault",  
                                    "UsageFault",  
                                    "Rsvd",  
                                    "Rsvd",  
                                    "Rsvd",  
                                    "SVCall",  
                                    "Debug Monitor",  
                                    "Rsvd",  
                                    "PendSV",  
                                    "SysTick"  
    };  
    uint_32 excpt_num = __get_PSR() & 0x1FF;  
    
    corona_print("Opps, bad thing happened.\n");  
    if(excpt_num < 16)
    {  
        corona_print("The exception [%s] invoked in TASK 0x%x\n",  
             expt_name[excpt_num] , _task_get_id());  
        corona_print("Dump the exception frame as :\n");  
        corona_print("R0:\t0x%08x\n", *((uint_32 *)ext_frm_ptr));  
        corona_print("R1:\t0x%08x\n", *((uint_32 *)ext_frm_ptr + 1));  
        corona_print("R2:\t0x%08x\n", *((uint_32 *)ext_frm_ptr + 2));  
        corona_print("R3:\t0x%08x\n", *((uint_32 *)ext_frm_ptr + 3));  
        corona_print("R12:\t0x%08x\n", *((uint_32 *)ext_frm_ptr + 4));  
        corona_print("LR:\t0x%08x\n", *((uint_32 *)ext_frm_ptr + 5));  
        corona_print("PC:\t0x%08x\n", *((uint_32 *)ext_frm_ptr + 6));  
        corona_print("PSR:\t0x%08x\n", *((uint_32 *)ext_frm_ptr + 7));  
    }
    else
    {  
        corona_print("The external interrupt %d occured while no handler to serve it.\n");
    }
    
    fflush(stdout);
}

void whistle_exception_handler(_mqx_uint para, pointer stack_ptr)  
{  
    pointer expt_frm_ptr = (pointer)__get_PSP();  
    expt_frm_dump(expt_frm_ptr);  
}

/*
 *   Cause an exception on purpose and see the results.
 */
void exp_handler_ut(void)  
{  
    unsigned int *p_bad = (unsigned int *)0; 
    
    corona_print("Install the _int_exception_isr to replace the _int_default_isr\n");
    _int_install_exception_isr();
    
    /* Set the exception handler of the task */  
    _task_set_exception_handler(_task_get_id(), whistle_exception_handler);
    
    *p_bad = 0;     // an access to address 0  
    //_task_block();
}  
#endif
