/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
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
* $FileName: boot.c$
* $Version : 3.6.2.0$
* $Date    : Jun-4-2010$
*
* Comments:
*
*    This file contains the boot and exception code.
*
*END************************************************************************/

#include "mqx_inc.h"

extern unsigned long far __BOOT_STACK_ADDRESS[];
extern unsigned long __IPSBAR;
extern unsigned long __INTERNAL_FLASH_BASE;
extern unsigned long __INTERNAL_SRAM_BASE;

asm void __boot(void);
asm void __boot_exception(void);
void _start(void);
void mcf5225_init(void);

typedef void (*vector_entry)(void);

extern vector_entry __VECTOR_TABLE_ROM_START[]; // defined in linker command file
extern vector_entry __VECTOR_TABLE_RAM_START[]; // defined in linker command file

#if   __REGABI__
    __declspec(register_abi)
#elif __STDABI__
      __declspec(standard_abi)
#endif

asm void __boot(void) {
        // Assume we could start here via the debugger or jump here at any time
        move.w #0x2700, sr                  // Disable all interrupts
        move.l #__BOOT_STACK_ADDRESS, sp    // Establish a stack pointer

        move.l #__VECTOR_TABLE_ROM_START, d0
        movec d0, VBR

        // Initialize IPSBAR: peripheral base, default is 0x40000000
        move.l __IPSBAR, d0
        andi.l #0xC0000000, d0
        add.l #0x1, d0
        move.l d0, 0x40000000

        // Initialize FLASHBAR: locate internal Flash and validate it
        move.l #__INTERNAL_FLASH_BASE, d0   // May be code+data
        andi.l #0xFFF80000, d0
        add.l #0x61, d0
        movec d0, RAMBAR0

        // Initialize RAMBAR1: locate SRAM and validate it
        move.l #__INTERNAL_SRAM_BASE, d0    // May be code+data
        andi.l #0xFFF80000, d0
        add.l #0x21, d0
        movec d0, RAMBAR1

        // Initialize mcf5225 peripherals
        jsr mcf5225_init

        // Jump to the main process
        jmp _start
}

#if   __REGABI__
    __declspec(register_abi)
#elif __STDABI__
      __declspec(standard_abi)
#endif

asm void __boot_exception(void)
{
   halt
   rte
}
