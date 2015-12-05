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
* $FileName: get_nsec.c$
* $Version : 3.7.2.0$
* $Date    : Feb-7-2011$
*
* Comments:
*
*   This file contains the function that reads the timer and returns
*   the number of nanoseconds elapsed since the last interrupt.
*
*END************************************************************************/

#include "mqx_inc.h"
#include "bsp.h"
#include "bsp_prv.h"

/*FUNCTION****************************************************************
*
* Function Name    : _time_get_nanoseconds
* Returned Value   : uint_32 nanoseconds
* Comments         :
*    This routine returns the number of nanoseconds that have elapsed
* since the last interrupt.
*
*END**********************************************************************/

uint_32 _time_get_nanoseconds
   (
      void
   )
{ /* Body */

   KERNEL_DATA_STRUCT_PTR kernel_data;
   uint_64                tmp64;
   uint_32                tmp32;

   _GET_KERNEL_DATA(kernel_data);
   
   tmp32 = _bsp_get_hwticks(0);
   
   /* Convert hardware ticks to ns. (H / (T/S * H/T) * 1000000000) */

   /* Convert using 2000000000 (extra * 2 for rounding) */
   tmp64 = (uint_64)tmp32 * 2000000000;
   tmp64 = tmp64 / ((uint_64)kernel_data->TICKS_PER_SECOND *
      kernel_data->HW_TICKS_PER_TICK);
   tmp64++;
   tmp32 = (uint_32)(tmp64 / 2); /* For rounding */
   return tmp32;

} /* Endbody */

