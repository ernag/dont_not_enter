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
* $FileName: psp_tips.c$
* $Version : 3.8.3.1$
* $Date    : Oct-31-2011$
*
* Comments:
*
*   This file contains the functions for converting ticks to picoseconds
*
*END************************************************************************/

#include "mqx_inc.h"

/*FUNCTION*-----------------------------------------------------------------
* 
* Function Name    : _psp_ticks_to_picoseconds
* Returned Value   : uint_32 - number of picoseconds
* Comments         :
*    This function converts ticks into picoseconds, with rounding or truncating
*
*END*----------------------------------------------------------------------*/

static uint_32 __psp_ticks_to_picoseconds
   (
      /* [IN] Ticks to be converted */
      PSP_TICK_STRUCT_PTR tick_ptr,

      /* [OUT] pointer to where the overflow boolean is to be written */
      boolean _PTR_       overflow_ptr,
      
      /* [IN] boolean to control rounding or truncating */
      boolean             round
   )
{ /* Body */
   KERNEL_DATA_STRUCT_PTR kernel_data;
   uint_64                tmp;
   uint_64                ps_per_tick;

   _GET_KERNEL_DATA(kernel_data);

   tmp = ((uint_64)kernel_data->HW_TICKS_PER_TICK * tick_ptr->TICKS[0]) +
      tick_ptr->HW_TICKS[0];

   /* 
   ** Convert hardware ticks to ps. (H / (T/S * H/T) * 1000000000000)
   */
   /* Convert to hardware ticks 
   ** use 2000000000000000 (extra * 2 for rounding)
   */

   ps_per_tick = 2000000000000ULL / kernel_data->TICKS_PER_SECOND;
   if ((ps_per_tick * kernel_data->TICKS_PER_SECOND) == 2000000000000ULL) {
      /* Integral ps per tick */
      tmp = tmp * ps_per_tick;
   } else {
      tmp = tmp * 2000000000000ULL;
      tmp = tmp / kernel_data->TICKS_PER_SECOND;
   }/* Endif */

   tmp = tmp / kernel_data->HW_TICKS_PER_TICK;
   /* Round OR Truncate*/
   if (round)
   {
        tmp++;
   }
   tmp = tmp / 2; /* For rounding */
   *overflow_ptr = (boolean)(tmp > MAX_UINT_32);
   return (uint_32)tmp;

} /* Endbody */

/*FUNCTION*-----------------------------------------------------------------
* 
* Function Name    : _psp_ticks_to_picoseconds
* Returned Value   : uint_32 - number of picoseconds
* Comments         :
*    This function converts ticks into picoseconds, with rounding
*
*END*----------------------------------------------------------------------*/

uint_32 _psp_ticks_to_picoseconds
   (
      /* [IN] Ticks to be converted */
      PSP_TICK_STRUCT_PTR tick_ptr,

      /* [OUT] pointer to where the overflow boolean is to be written */
      boolean _PTR_       overflow_ptr
   )
{ /* Body */
    return __psp_ticks_to_picoseconds(tick_ptr, overflow_ptr, TRUE);
}

/*FUNCTION*-----------------------------------------------------------------
* 
* Function Name    : _psp_ticks_to_picoseconds
* Returned Value   : uint_32 - number of picoseconds
* Comments         :
*    This function converts ticks into picoseconds, with truncating
*
*END*----------------------------------------------------------------------*/

uint_32 _psp_ticks_to_picoseconds_truncate
   (
      /* [IN] Ticks to be converted */
      PSP_TICK_STRUCT_PTR tick_ptr,

      /* [OUT] pointer to where the overflow boolean is to be written */
      boolean _PTR_       overflow_ptr
   )
{ /* Body */
    return __psp_ticks_to_picoseconds(tick_ptr, overflow_ptr, FALSE);
}

/* EOF */
