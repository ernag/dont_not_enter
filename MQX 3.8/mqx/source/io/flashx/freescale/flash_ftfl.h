#ifndef __flash_ftfl_h__
#define __flash_ftfl_h__
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
* $FileName: flash_ftfl.h$
* $Version : 3.8.B.1$
* $Date    : Mar-9-2012$
*
* Comments:
*
*   The file contains function prototypes and defines for the internal 
*   flash driver.
*
*END************************************************************************/

/* FlexNVM IOCTL codes */
#define FLEXNVM_IOCTL_READ_RESOURCE         _IO(IO_TYPE_FLASH, 0x14)
#define FLEXNVM_IOCTL_GET_PARTITION_CODE    _IO(IO_TYPE_FLASH, 0x15)
#define FLEXNVM_IOCTL_SET_PARTITION_CODE    _IO(IO_TYPE_FLASH, 0x16)
#define FLEXNVM_IOCTL_GET_FLEXRAM_FN        _IO(IO_TYPE_FLASH, 0x17)
#define FLEXNVM_IOCTL_SET_FLEXRAM_FN        _IO(IO_TYPE_FLASH, 0x18)
#define FLEXNVM_IOCTL_WAIT_EERDY            _IO(IO_TYPE_FLASH, 0x19)
#define FLEXNVM_IOCTL_GET_EERDY             _IO(IO_TYPE_FLASH, 0x1a)

/* FlexNVM partition code values */
#define FLEXNVM_PART_CODE_DATA256_EE0       (0)
#define FLEXNVM_PART_CODE_DATA224_EE32      (3)
#define FLEXNVM_PART_CODE_DATA192_EE64      (4)
#define FLEXNVM_PART_CODE_DATA128_EE128     (5)
#define FLEXNVM_PART_CODE_DATA32_EE224      (0x0b)
#define FLEXNVM_PART_CODE_DATA64_EE192      (0x0c)
#define FLEXNVM_PART_CODE_DATA0_EE256       (8)
#define FLEXNVM_PART_CODE_NOPART            (0x0f)

/* FlexNVM EEPROM size values */
#define FLEXNVM_EE_SIZE_4096                (2)
#define FLEXNVM_EE_SIZE_2048                (3)
#define FLEXNVM_EE_SIZE_1024                (4)
#define FLEXNVM_EE_SIZE_512                 (5)
#define FLEXNVM_EE_SIZE_256                 (6)
#define FLEXNVM_EE_SIZE_128                 (7)
#define FLEXNVM_EE_SIZE_64                  (8)
#define FLEXNVM_EE_SIZE_32                  (9)
#define FLEXNVM_EE_SIZE_0                   (0x0f)

/* add definition for K20 P1 */
/* FlexNVM partition code values */
#define FLEXNVM_PART_CODE_DATA32_EE0        (0)
#define FLEXNVM_PART_CODE_DATA24_EE8        (1)
#define FLEXNVM_PART_CODE_DATA16_EE16       (2)
#define FLEXNVM_PART_CODE_DATA0_EE32        (3)
#define FLEXNVM_PART_CODE_DATA8_EE24        (9)

/* FlexNVM EEPROM / FlexRAM choice */
#define FLEXNVM_FLEXRAM_RAM                 (0xff)
#define FLEXNVM_FLEXRAM_EE                  (0)

/* FlexNVM split values */
#define FLEXNVM_EE_SPLIT_1_7                (0)
#define FLEXNVM_EE_SPLIT_1_3                (0x10)
#define FLEXNVM_EE_SPLIT_1_1                (0x30)

/*----------------------------------------------------------------------*/
/* 
**              EXPORTED TYPES
*/

typedef struct flexnvm_prog_part_struct {
    uint_8 EE_DATA_SIZE_CODE;
    uint_8 FLEXNVM_PART_CODE;
} FLEXNVM_PROG_PART_STRUCT, _PTR_ FLEXNVM_PROG_PART_STRUCT_PTR;

typedef struct flexnvm_read_rsrc_struct {
    uint_32 ADDR;
    uint_8 RSRC_CODE;
    uint_32 RD_DATA;
} FLEXNVM_READ_RSRC_STRUCT, _PTR_ FLEXNVM_READ_RSRC_STRUCT_PTR;

/*----------------------------------------------------------------------*/
/* 
**              DEFINED VARIABLES
*/

extern const FLASHX_BLOCK_INFO_STRUCT _flashx_kinetisN_block_map[];
extern const FLASHX_BLOCK_INFO_STRUCT _flashx_kinetisX_block_map[];
extern const FLASHX_BLOCK_INFO_STRUCT _flashx_mcf51xx_plus_block_map[];

extern const FLASHX_DEVICE_IF_STRUCT _flashx_ftfl_if;

#endif //__flash_ftfl_h__
