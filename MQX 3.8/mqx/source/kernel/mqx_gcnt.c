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
* $FileName: mqx_gcnt.c$
* $Version : 3.5.4.0$
* $Date    : Dec-8-2009$
*
* Comments:
*
*   This file contains the function for returning a unique number
*   from the kernel.  This number is simply incremented.
*
*END************************************************************************/


#include "mqx_inc.h"

#if MQX_KD_HAS_COUNTER
/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _mqx_get_counter
* Returned Value   : _mqx_uint counter
* Comments         : 
*    This function increments the counter and then returns value of the counter.
*    This provides a unique number for whoever requires it.  
*    Note that this unique number will never be 0.
*
*END*----------------------------------------------------------------------*/

_mqx_uint _mqx_get_counter
   (
      void
   )
{ /* Body */
   register KERNEL_DATA_STRUCT_PTR  kernel_data;
            _mqx_uint                return_value;

   _GET_KERNEL_DATA(kernel_data);
   _INT_DISABLE();
 
   /*
   ** Increment counter, and ensure it is not zero. If it is zero, set it to
   ** one.
   */
   if ( ++kernel_data->COUNTER == 0 ) {
      kernel_data->COUNTER = 1;
   } /* Endif */
   return_value = kernel_data->COUNTER;
   _INT_ENABLE();
   return (return_value);

} /* Endbody */
#endif
/* EOF */
