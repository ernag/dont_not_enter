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
* $FileName: psp_timn.c$
* $Version : 3.5.3.0$
* $Date    : Dec-8-2009$
*
* Comments:
*
*   This file contains the functions for converting ticks to minutes
*
*END************************************************************************/

#include "mqx_inc.h"

#if MQX_HAS_TICK

/*FUNCTION*-----------------------------------------------------------------
* 
* Function Name    : _psp_ticks_to_minutes
* Returned Value   : uint_32 - number of minutes
* Comments         :
*    This function converts ticks into minutes
*
*END*----------------------------------------------------------------------*/

uint_32 _psp_ticks_to_minutes
   (
      /* [IN] Ticks to be converted */
      PSP_TICK_STRUCT_PTR tick_ptr,

      /* [OUT] pointer to where the overflow boolean is to be written */
      boolean _PTR_       overflow_ptr
   )
{ /* Body */
   uint_64                tmp;
   KERNEL_DATA_STRUCT_PTR kernel_data;

   _GET_KERNEL_DATA(kernel_data);

   tmp = tick_ptr->TICKS[0];

   if ((tmp != MAX_UINT_64) &&
      (tick_ptr->HW_TICKS[0] > (kernel_data->HW_TICKS_PER_TICK/2)))
   {
      tmp++;
   } /* Endif */

   tmp = (tmp / kernel_data->TICKS_PER_SECOND) / SECS_IN_MINUTE;
  
   *overflow_ptr = (boolean)(tmp > MAX_UINT_32);

   return (uint_32)tmp;

} /* Endbody */

#endif
/* EOF */
