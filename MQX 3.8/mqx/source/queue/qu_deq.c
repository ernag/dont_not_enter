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
* $FileName: qu_deq.c$
* $Version : 3.0.4.0$
* $Date    : Nov-21-2008$
*
* Comments:
*
*   This file contains the function for removing an element from a queue.
*   Note that the QUEUE macros can be found in mqx_prv.h
*
*END************************************************************************/

/* Start CR 312 */
#define MQX_FORCE_USE_INLINE_MACROS 1
/* End CR 312 */

#include "mqx_inc.h"


/*FUNCTION*-----------------------------------------------------
* 
* Function Name    : _queue_dequeue
* Returned Value   : QUEUE_ELEMENT_STRUCT_PTR
* Comments         :
*    Dequeue an element from the head of the queue
*
*END*--------------------------------------------------------*/

QUEUE_ELEMENT_STRUCT_PTR  _queue_dequeue
   ( 
      /* [IN] the queue to use */
      QUEUE_STRUCT_PTR q_ptr 
   )
{ /* Body */
   QUEUE_ELEMENT_STRUCT_PTR e_ptr;
   
   _int_disable();
   if (q_ptr->SIZE == 0) {
      _int_enable();
      return(NULL);
   } /* Endif */
   
   _QUEUE_DEQUEUE(q_ptr, e_ptr);
   _int_enable();
   return(e_ptr);

} /* Endbody */

/* EOF */
