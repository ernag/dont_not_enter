;/**HEADER********************************************************************
;* 
;* Copyright (c) 2010-2011 Freescale Semiconductor;
;* All Rights Reserved                       
;*
;*************************************************************************** 
;*
;* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR 
;* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
;* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  
;* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
;* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
;* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
;* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
;* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
;* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
;* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF 
;* THE POSSIBILITY OF SUCH DAMAGE.
;*
;**************************************************************************
;*
;* $FileName: cortex_boot.s$
;* $Version : 3.8.10.3$
;* $Date    : Dec-2-2011$
;*
;* Comments:
;*
;*
;*END************************************************************************/

#include <asm_mac.h>
#include "mqx_cnfg.h"
#include "types.s"

#define __ASM__

#ifdef __CWARM__
/*workaround for CW 10.1 and less (CW10.1 - dont know, how to parse long line...)*/
//#message "We must temporary include kinetis.h (instead of psp_cpu.h) - bug in CW10.1 - dont know, how to parse long line. This conditionally compiled section can be removed in CW10.2"
#include "kinetis.h"

#else
#include "psp_cpu.h"
#endif // __CWARM__

#undef __ASM__

;FUNCTION*-------------------------------------------------------------------
; 
; Function Name    : __boot
; Returned Value   : 
; Comments         : startup sequence
;
;END*----------------------------------------------------------------------

 ASM_CODE_SECTION(.text)
        
 ASM_PUBLIC(__boot)
 ASM_PUBLIC(__set_MSP)

ASM_PUBLIC_BEGIN(__boot)
ASM_PUBLIC_FUNC(__boot)
ASM_LABEL(__boot)
        ; disable interrupts and clear pending flags
        ldr r0, =0xe000e180     ; NVIC_ICER0 - Interrupt Clear-Enable Registers
        ldr r1, =0xe000e280     ; NVIC_ICPR0 - Interrupt Clear-Pending Registers
        ldr r2, =0xffffffff
        mov r3, #8
        
ASM_LABEL(_boot_loop)
        cbz r3, _boot_loop_end
        str r2, [r0], #4        ; NVIC_ICERx - clear enable IRQ register
        str r2, [r1], #4        ; NVIC_ICPRx - clear pending IRQ register
        sub r3, r3, #1
        b _boot_loop

ASM_LABEL(_boot_loop_end)
        
        ; prepare process stack pointer
        mrs r0, MSP
        msr PSP, r0
        
        ; switch to proccess stack (PSP)
        mrs r0, CONTROL
        orr r0, r0, #2
        msr CONTROL, r0

#if MQXCFG_ENABLE_FP && PSP_HAS_FPU
          ; CPACR is located at address 0xE000ED88
        LDR.W   R0, =0xE000ED88
        ; Read CPACR
        LDR     R1, [R0]
        ; Set bits 20-23 to enable CP10 and CP11 coprocessors
        ORR     R1, R1, #(0xF << 20)
        ; Write back the modified value to the CPACR
        STR     R1, [R0]
        ; turn off fpu register stacking in exception entry

        ldr r0, =0xE000EF34     ; FPCCR
        mov r1, #0
        str r1, [r0]
#endif
 
#if defined(__CODEWARRIOR__)
        ASM_EXTERN(__thumb_startup)
        b ASM_PREFIX(__thumb_startup)
#elif defined(__IAR_SYSTEMS_ICC__)  || defined (__IASMARM__) 
        ASM_EXTERN(__iar_program_start)
        b ASM_PREFIX(__iar_program_start)
#elif defined(__CC_ARM)
        ASM_EXTERN(init_hardware)
        ASM_EXTERN(__main)
        bl ASM_PREFIX(init_hardware)
        b ASM_PREFIX(__main)
#endif


ASM_PUBLIC_END(__boot)
        
ASM_PUBLIC_BEGIN(__set_MSP)
ASM_PUBLIC_FUNC(__set_MSP)
ASM_LABEL(__set_MSP)
        msr MSP, r0
        bx lr
ASM_PUBLIC_END(__set_MSP)

        ASM_ALIGN(4)
        ASM_END
