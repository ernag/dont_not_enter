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
* $FileName: init_flashx.c$
* $Version : 3.8.0.2$
* $Date    : Mar-6-2012$
*
* Comments:
*
*   This file contains the global generic settings for FLASHX driver.
*
*END************************************************************************/

#include "mqx.h"
#include "bsp.h"

const FLASHX_FILE_BLOCK _bsp_flashx_file_blocks[] = {
    { "bank0",                 BSP_INTERNAL_FLASH_BASE, BSP_INTERNAL_FLASH_SIZE - 1 },
    { "flexram0", (_mem_size) BSP_INTERNAL_FLEXRAM_BASE, INTERNAL_FLEXRAM_BASE + BSP_INTERNAL_FLEXRAM_SIZE - 1 },
    { ""     ,         (_mem_size) __FLASHX_START_ADDR, (uint_32) __FLASHX_END_ADDR },
    { NULL   ,                             0,                           0 }
};

const FLASHX_INIT_STRUCT _bsp_flashx_init = {
    0x00000000, /* BASE_ADDR should be 0 for internal flashes */
    _flashx_kinetisX_block_map, /* HW block map for KinetisX devices */
    _bsp_flashx_file_blocks, /* Files on the device defined by the BSP */
    &_flashx_ftfl_if, /* Interface for low level driver */
    32, /* For external devices, data lines for the flash. Not used for internal flash devices. */
    1,  /* Number of parallel external flash devices. Not used for internal flash devices. */
    0,  /* 0 if the write verify is requested, non-zero otherwise */
    NULL /* low level driver specific data */
};

/* EOF */
