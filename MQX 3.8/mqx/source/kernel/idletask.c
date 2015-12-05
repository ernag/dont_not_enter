/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
* Copyright (c) 2004-2008 Embedded Access Inc.;
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
* $FileName: idletask.c$
* $Version : 3.0.3.0$
* $Date    : Nov-21-2008$
*
* Comments:
*
*   This file contains the idle task.
*
*END************************************************************************/

#include "mqx_inc.h"

static void k60_wait(void)
{
    /* Clear the SLEEPDEEP bit to make sure we go into WAIT (sleep)
     * mode instead of deep sleep.
     */
    SCB_SCR &= ~SCB_SCR_SLEEPDEEP_MASK;
    
    /* WFI instruction will start entry into WAIT mode */
     asm("WFI");
}

#if MQX_USE_IDLE_TASK
/*TASK*---------------------------------------------------------------------
* 
* Function Name    : _mqx_idle_task
* Returned Value   : none
* Comments         :
*    This function is the code for the idle task.
* It implements a simple 128 counter.  This can be read from the debugger,
* and calibrated so that the idle CPU time usage can be calculated.
*
*END*----------------------------------------------------------------------*/

void _mqx_idle_task
   ( 
      /* [IN] parameter passed to the task when created */
      uint_32 parameter
   )
{ /* Body */
        volatile KERNEL_DATA_STRUCT _PTR_ kernel_data;
        
        _GET_KERNEL_DATA(kernel_data);
        
        while (1) 
        {
        
             /*
              *   Since we are idle, go into a WAIT state, until we are interrupted.
              */
              k60_wait();
              
              if (++kernel_data->IDLE_LOOP1 == 0) 
              {
                    if (++kernel_data->IDLE_LOOP2 == 0) 
                    {
                        if (++kernel_data->IDLE_LOOP3 == 0) 
                        {
                           ++kernel_data->IDLE_LOOP4;
                        } /* Endif */
                    
                    } /* Endif */
             
               } /* Endif */

         } /* Endwhile */

} /* Endbody */
#endif /* MQX_USE_IDLE_TASK */

/* EOF */

