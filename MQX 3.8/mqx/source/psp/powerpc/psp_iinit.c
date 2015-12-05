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
* $FileName: psp_iinit.c$
* $Version : 3.5.3.0$
* $Date    : Feb-24-2010$
*
* Comments:
*
*   This file contains the functions for initializaing the handling of 
*   interrupts.
*
*END************************************************************************/

#include "mqx_inc.h"


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _psp_int_init
* Returned Value   : void
* Comments         :
*    This function initializes the kernel interrupt tables.
*
*END*----------------------------------------------------------------------*/

_mqx_uint _psp_int_init
   (
      /* [IN] the first (lower) user ISR vector number */
      _mqx_uint first_user_isr_vector_number,

      /* [IN] the last user ISR vector number */
      _mqx_uint last_user_isr_vector_number

   )
   { /* Body */
   uint_32            error;

   /* Install kernel interrupt services */
   error = _int_init(first_user_isr_vector_number, last_user_isr_vector_number);

   /* Install PSP interrupt services */
#if !(defined(PSP_PPC440) || defined(PSP_E500_CORE)) // fixed at link time
   if (error == MQX_OK) {
      _psp_int_install();
   } /* Endif */
#endif

   return error;
      
} /* Endbody */

/* EOF */
