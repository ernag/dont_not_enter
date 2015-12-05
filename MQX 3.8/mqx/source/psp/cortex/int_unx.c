/**HEADER********************************************************************
* 
* Copyright (c) 2010 Freescale Semiconductor;
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
* $FileName: int_unx.c$
* $Version : 3.8.8.0$
* $Date    : Aug-30-2011$
*
* Comments:
*
*   This file contains the function for the unexpected
*   exception handling function for MQX, which will display on the
*   console what exception has ocurred.  
*
*   NOTE: the default I/O for the current task is used, since a printf
*   is being done from an ISR.  
*   This default I/O must NOT be an interrupt drive I/O channel.
*
*END************************************************************************/

#include "mqx_inc.h"
#include "fio.h"
#include <assert.h>


/*
 *   The application can use a callback of this type so the PSP can call it back, 
 *      if registered.
 */
unexpected_isr_app_cbk g_unexp_isr_app_cbk = NULL;

void _install_unexpected_isr_app_cbk(void *pCbk)
{
    g_unexp_isr_app_cbk = (unexpected_isr_app_cbk)pCbk;
}

void reboot_now(void)
{
    uint_32 read_value = SCB_AIRCR;

    // Issue a Kinetis Software System Reset
    read_value &= ~SCB_AIRCR_VECTKEY_MASK;
    read_value |= SCB_AIRCR_VECTKEY(0x05FA);
    read_value |= SCB_AIRCR_SYSRESETREQ_MASK;

    //_int_disable();
    SCB_AIRCR = read_value;
    while (1)
    {
    }
}

/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _int_unexpected_isr
* Returned Value   : void
* Comments         :
*    A default handler for all unhandled interrupts.
*    It determines the type of interrupt or exception and prints out info.
*
*END*----------------------------------------------------------------------*/

void _int_unexpected_isr
   (
      /* [IN] the parameter passed to the default ISR, the vector */
      pointer parameter
   )
{ /* Body */
   KERNEL_DATA_STRUCT_PTR     kernel_data;
   TD_STRUCT_PTR              td_ptr;
   uint_32                    fail_pc = 0, fail_lr = 0;
   
   __asm
   {
       mov fail_pc, PC
       mov fail_lr, LR
   }
   
#if 0
   fail_lr = __get_LR();
   fail_pc = __get_PC();
#endif
   
   /*
    *   Ernie:  05/14/2013
    *   
    *           Putting an assert here, to call attention to unhandled interrupts.
    *           It would have been preferable to pipe all the useful data below to stdout,
    *           however, I found that printf() just hung indefinitely and nothing was being
    *           printed anyway (this was for the hard fault (vector #3) case).
    *           
    *           I recommend that once you realize this is happening, to get rid of the assert, and
    *           put a breakpoint here.  See what "parameter"'s value is.  This is the vector number
    *           inside of the ISR table.  This should tell you which ISR is getting invoked.
    *           
    *           Previously, we were seeing vector #3, hard fault, get invoked.  The reason was related
    *           to byte packing / alignment.  I reverted all of the "#pragma pack(1)" in corona to fix.
    *           
    *           assert(0);
    *           
    *   Ernie:  10/25/2013
    *   
    *           The above approach is no longer used.  Now we use a combination of crumbs and a callback
    *           to a function in the application, which allows us to save some of the context and upload
    *           to the server later.
    */

   _GET_KERNEL_DATA(kernel_data);
   td_ptr = kernel_data->ACTIVE_PTR;
   
   if( NULL != g_unexp_isr_app_cbk )
   {
       g_unexp_isr_app_cbk( (uint_32)td_ptr->TASK_ID, (uint_32)parameter, fail_pc, fail_lr, NULL, td_ptr->TASK_TEMPLATE_PTR->TASK_ADDRESS );
   }
   
   // In theory, should not get here, but hard reboot just in case.
   reboot_now();

#if 0
   printf("\n\r*** UNHANDLED INTERRUPT ***\n\r");  fflush(stdout);
   printf("Vector #: 0x%02x Task Id: 0x%0x Td_ptr 0x%x\n\r",
      (uint_32)parameter, (uint_32)td_ptr->TASK_ID, (uint_32)td_ptr); fflush(stdout);

   psp = __get_PSP();
   msp = __get_MSP();
   fail_pc = __get_PC();

   printf("PC: 0x%08x LR: 0x%08x PSP: 0x%08x MSP: 0x%08x PSR: 0x%08x\n\r", __get_PC(), __get_LR(), psp, msp, __get_PSR());
   fflush(stdout);
   
   printf("\n\r\n\rMemory dump:\n\r");
   for (i = 0; i < 32; i += 4) 
   {
       printf("0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n\r", psp + i * 4, ((uint_32*)psp)[i], ((uint_32*)psp)[i + 1], ((uint_32*)psp)[i + 2], ((uint_32*)psp)[i + 3]);
   }
   fflush(stdout);

   printf("\n\r\n\rMemory dump:\n\r");
   for (i = 0; i < 32; i += 4) 
   {
       printf("0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n\r", msp + i * 4, ((uint_32*)msp)[i], ((uint_32*)msp)[i + 1], ((uint_32*)msp)[i + 2], ((uint_32*)msp)[i + 3]);
   }
   fflush(stdout);

   _INT_ENABLE();
   if (td_ptr->STATE != UNHANDLED_INT_BLOCKED) 
   {
      td_ptr->STATE = UNHANDLED_INT_BLOCKED;
      td_ptr->INFO  = (_mqx_uint)parameter;

      _QUEUE_UNLINK(td_ptr);
   } /* Endif */
   _INT_DISABLE();
#endif
} /* Endbody */

/* EOF */
