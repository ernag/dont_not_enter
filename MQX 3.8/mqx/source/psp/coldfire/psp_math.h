#ifndef __psp_math_h__
#define __psp_math_h__
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
* $FileName: psp_math.h$
* $Version : 3.5.5.0$
* $Date    : Feb-2-2010$
*
* Comments:
*
*   This file contains the definitions for functions that provide
*   mathematics for working on 64 bit, 96 bit and 128 bit numbers.
*   (Needed by the time conversion functions)
*   The operations on the numbers are performed uing the 16-bit array
*   representation of the numbers.
*
*END************************************************************************/

/* Macro to add a single uint_32 to an array of uint_16s */
#define PSP_ADD_ELEMENT_TO_ARRAY(a_ptr, v, r_ptr, size) \
   _psp_add_element_to_array((uint_16_ptr)((pointer)(a_ptr)), \
      (uint_16)(v), \
      (uint_16_ptr)((pointer)(r_ptr)), \
      (_mqx_uint)(size))

/*-----------------------------------------------------------------------*/
/*
** PSP 64 BIT UNION
**
** The representation of a 64 bit number
**
*/
typedef union psp_64_bit_union
{
   uint_64  LLW;
   uint_32  LW[2];
   uint_16  W[4];
   uint_8   B[8];
} PSP_64_BIT_UNION, _PTR_ PSP_64_BIT_UNION_PTR;

/*-----------------------------------------------------------------------*/
/*
** PSP 96 BIT UNION
**
** The representation of a 96 bit number
**
*/
typedef union psp_96_bit_union
{
   uint_32         LW[3];
   uint_16         W[6];
   uint_8          B[12];
   PSP_TICK_STRUCT TICKS;
} PSP_96_BIT_UNION, _PTR_ PSP_96_BIT_UNION_PTR;

/*-----------------------------------------------------------------------*/
/*
** PSP 128 BIT UNION
**
** The representation of a 128 bit number
**
*/
typedef union psp_128_bit_union
{
   uint_64  LLW[2];
   uint_32  LW[4];
   uint_16  W[8];
   uint_8   B[16];
} PSP_128_BIT_UNION, _PTR_ PSP_128_BIT_UNION_PTR;


/*--------------------------------------------------------------------------*/
/*
**                  PROTOTYPES OF PSP FUNCTIONS
*/

#ifdef __cplusplus
extern "C" {
#endif

extern uint_32 _psp_add_element_to_array(uint_32_ptr, uint_32, uint_32, uint_32_ptr);
extern uint_32 _psp_div_128_by_32(PSP_128_BIT_UNION_PTR, uint_32, PSP_128_BIT_UNION_PTR);
extern uint_32 _psp_mul_128_by_32(PSP_128_BIT_UNION_PTR, uint_32, PSP_128_BIT_UNION_PTR);
extern uint_64 _psp_mul_32_32(uint_32, uint_32);
extern uint_32 _psp_div_64_32(uint_64, uint_32);

#ifdef __cplusplus
}
#endif

#endif
/* EOF */
