#ifndef __psp_comp_h__
#define __psp_comp_h__
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
* $FileName: psp_comp.h$
* $Version : 3.6.4.0$
* $Date    : May-28-2010$
*
* Comments:
*
*   This file determines which compiler is running, then includes
*   the compiler specific header file
*   Revision History:
*   Date          Version        Changes
*   Mar 1, 1997   2.40           Convert to MQX2.40 from MQX2.33
*
*END************************************************************************/

#if defined(__DCC__)
#include "diab_cf.h"

#elif defined(__CODEWARRIOR__)
#include "cw_comp.h"

#elif defined(__ICCCF__)
#include "iar_comp.h"

#else
#error "COMPILER TYPE NOT DEFINED"
#endif

#if PSP_ABI == PSP_ABI_REG
   #define PSP_TASK_PARAM(stack_start_ptr) (stack_start_ptr->INITIAL_CONTEXT.REGISTERS.D0)
#else
   #define PSP_TASK_PARAM(stack_start_ptr) (stack_start_ptr->PARAMETER)
#endif

#endif
/* EOF */
