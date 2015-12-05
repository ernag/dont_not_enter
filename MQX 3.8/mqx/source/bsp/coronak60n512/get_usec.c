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
* $FileName: get_usec.c$
* $Version : 3.7.2.0$
* $Date    : Feb-7-2011$
*
* Comments:
*
*   This file contains the function that reads the timer and returns
*   the number of microseconds elapsed since the last interrupt.
*
*END************************************************************************/

#include "mqx_inc.h"
#include "bsp.h"
#include "bsp_prv.h"

/*FUNCTION****************************************************************
*
* Function Name    : _time_get_microseconds
* Returned Value   : uint_16 microseconds
* Comments         :
*    This routine returns the number of microseconds that have elapsed
*    since the last interrupt.
*
*END**********************************************************************/

uint_16 _time_get_microseconds
   (
      void
   )
{ /* Body */
   KERNEL_DATA_STRUCT_PTR kernel_data;
   uint_32                tmp32;
   uint_32 hwticks = _bsp_get_hwticks(0);

   _GET_KERNEL_DATA(kernel_data);
   
   /* Convert hardware ticks to us. (H / (T/S * H/T) * 1000000) */
   tmp32 = (uint_32)(kernel_data->TICKS_PER_SECOND * kernel_data->HW_TICKS_PER_TICK)/1000000;   
   tmp32 = hwticks/ tmp32;   
   
   return (uint_16)tmp32;

} /* Endbody */

/* EOF */
