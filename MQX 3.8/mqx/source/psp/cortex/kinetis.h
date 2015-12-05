/**HEADER***********************************************************************
*
* Copyright (c) 2010 Freescale Semiconductor;
* All Rights Reserved
*
********************************************************************************
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
********************************************************************************
*
* $FileName: kinetis.h$
* $Version : 3.8.B.5$
* $Date    : Mar-19-2012$
*
* Comments:
*
*   This file contains the type definitions for the Kinetis microcontrollers.
*
*END***************************************************************************/

#ifndef __kinetis_h__
#define __kinetis_h__

#define __kinetis_h__version "$Version:3.8.B.5$"
#define __kinetis_h__date    "$Date:Mar-19-2012$"

#ifndef __ASM__

/* Include header file for specific Kinetis platform */
#if     (MQX_CPU == PSP_CPU_MK10DX128Z) || \
        (MQX_CPU == PSP_CPU_MK10DX256Z) || \
        (MQX_CPU == PSP_CPU_MK10DN512Z)
    #include "MK10DZ10.h"
#elif   (MQX_CPU == PSP_CPU_MK10F12)
    #include "MK10F12.h"
#elif   (MQX_CPU == PSP_CPU_MK20DX128Z) || \
        (MQX_CPU == PSP_CPU_MK20DX256Z) || \
        (MQX_CPU == PSP_CPU_MK20DN512Z)
    #include "MK20DZ10.h"
#elif   (MQX_CPU == PSP_CPU_MK20F12)
    #include "MK20F12.h"
#elif   (MQX_CPU == PSP_CPU_MK30DX128Z) || \
        (MQX_CPU == PSP_CPU_MK30DX256Z) || \
        (MQX_CPU == PSP_CPU_MK30DN512Z)
    #include "MK30DZ10.h"
#elif   (MQX_CPU == PSP_CPU_MK40DX128Z) || \
        (MQX_CPU == PSP_CPU_MK40DX256Z) || \
        (MQX_CPU == PSP_CPU_MK40DN512Z)
    #include "MK40DZ10.h"
#elif   (MQX_CPU == PSP_CPU_MK50DX256Z) || \
        (MQX_CPU == PSP_CPU_MK50DN512Z)
    #include "MK50DZ10.h"
#elif   (MQX_CPU == PSP_CPU_MK51DX256Z) || \
        (MQX_CPU == PSP_CPU_MK51DN256Z) || \
        (MQX_CPU == PSP_CPU_MK51DN512Z)
    #include "MK51DZ10.h"
#elif   (MQX_CPU == PSP_CPU_MK52DN512Z)
    #include "MK52DZ10.h"
#elif   (MQX_CPU == PSP_CPU_MK53DX256Z) || \
        (MQX_CPU == PSP_CPU_MK53DN512Z)
    #include "MK53DZ10.h"
#elif   (MQX_CPU == PSP_CPU_MK60DX256Z) || \
        (MQX_CPU == PSP_CPU_MK60DN256Z) || \
        (MQX_CPU == PSP_CPU_MK60DN512Z)
    #include "MK60DZ10.h"
#elif   (MQX_CPU == PSP_CPU_MK60DN512)
    #include "MK60D10.h"
#elif   (MQX_CPU == PSP_CPU_MK60DF120M)
    #include "MK60F15.h"
#elif   (MQX_CPU == PSP_CPU_MK70F120M)
    #include "MK70F12.h"
#elif   (MQX_CPU == PSP_CPU_MK70F150M)
    #include "MK70F15.h"
#else
    #error "Wrong CPU definition"
#endif

#include "kinetis_mpu.h"

#endif /* __ASM__ */

#ifdef __cplusplus
extern "C" {
#endif


/*
*******************************************************************************
**
**                  CONSTANT DEFINITIONS
**
*******************************************************************************
*/
#ifndef __CWARM__

#if   (MQX_CPU == PSP_CPU_MK70F120M)
    /* enable FPU for this platform */
    #define PSP_HAS_CODE_CACHE                      0
    #define PSP_HAS_DATA_CACHE                      0
    #define PSP_HAS_FPU                             1
#elif   (MQX_CPU == PSP_CPU_MK70F150M)
    /* enable FPU for this platform */
    #define PSP_HAS_CODE_CACHE                      0
    #define PSP_HAS_DATA_CACHE                      0
    #define PSP_HAS_FPU                             1
#endif

#else
//workaround for CW 10.1 and less (CW10.1 - dont know, how to parse long line...)
//#message "This conditionally compiled section can be removed in CW10.2"
#endif

/* Cache and MMU definition values */
#ifndef PSP_HAS_MMU
#define PSP_HAS_MMU                             0
#endif

#ifndef PSP_HAS_CODE_CACHE
#define PSP_HAS_CODE_CACHE                      0
#endif

#ifndef PSP_HAS_DATA_CACHE
#define PSP_HAS_DATA_CACHE                      0
#endif

#ifndef PSP_HAS_FPU
#define PSP_HAS_FPU                             0
#endif

#define PSP_CACHE_LINE_SIZE                     (0x10)

#ifndef __ASM__

/* Standard cache macros */
#define _DCACHE_FLUSH()
#define _DCACHE_FLUSH_LINE(p)
#define _DCACHE_FLUSH_MBYTES(p, m)
#define _DCACHE_INVALIDATE()
#define _DCACHE_INVALIDATE_LINE(p)
#define _DCACHE_INVALIDATE_MBYTES(p, m)

#define _CACHE_ENABLE(n)
#define _CACHE_DISABLE()

#define _ICACHE_INVALIDATE()
#define _ICACHE_INVALIDATE_LINE(p)
#define _ICACHE_INVALIDATE_MBYTES(p, m)

#define PSP_INTERRUPT_TABLE_INDEX               IRQInterruptIndex

/*
*******************************************************************************
**
**                  TYPE DEFINITIONS
**
*******************************************************************************
*/



/*
*******************************************************************************
**
**              FUNCTION PROTOTYPES AND GLOBAL EXTERNS
**
*******************************************************************************
*/

#define _psp_mem_check_access(addr, size, flags)    \
                    _kinetis_mpu_sw_check(addr, size, flags)

#define _psp_mem_check_access_mask(addr, size, flags, mask) \
                    _kinetis_mpu_sw_check_mask(addr, size, flags, mask)

#define _psp_mpu_add_region(start, end, flags)  \
                    _kinetis_mpu_add_region(start, end, flags)


#endif /* __ASM__ */

#ifdef __cplusplus
}
#endif

#endif /* __kinetis_h__ */
