/**HEADER**********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
* All Rights Reserved
*
*******************************************************************************
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
*******************************************************************************
*
* $FileName: user_config.h$
* $Version : 3.8.0.2$
* $Date    : Mar-6-2012$
*
* Comments:
*
*   User configuration for MQX components
*
*END**************************************************************************/

#ifndef __user_config_h__
#define __user_config_h__

/* mandatory CPU identification */
#define MQX_CPU                 PSP_CPU_MK20DX256

/*
** BSP settings - for defaults see mqx\source\bsp\<board_name>\<board_name>.h
*/
#define BSPCFG_ENABLE_TTYA       0
#define BSPCFG_ENABLE_ITTYA      0
#define BSPCFG_ENABLE_TTYB       1
#define BSPCFG_ENABLE_ITTYB      0
#define BSPCFG_ENABLE_TTYC       0
#define BSPCFG_ENABLE_ITTYC      0
#define BSPCFG_ENABLE_TTYD       0
#define BSPCFG_ENABLE_ITTYD      0
#define BSPCFG_ENABLE_TTYE       0
#define BSPCFG_ENABLE_ITTYE      0
#define BSPCFG_ENABLE_I2C0       1
#define BSPCFG_ENABLE_II2C0      1
#define BSPCFG_ENABLE_I2C1       0
#define BSPCFG_ENABLE_II2C1      0

// EA:  Dave wired up BMA280 accelermoter to SPI1, so enabling SPI1 instead of SPI0.
#define BSPCFG_ENABLE_SPI0       0//1
#define BSPCFG_ENABLE_SPI1       1//0
#define BSPCFG_ENABLE_ISPI0      0//1
#define BSPCFG_ENABLE_ISPI1      1//0

#define BSPCFG_ENABLE_GPIODEV    0
#define BSPCFG_ENABLE_RTCDEV     1
#define BSPCFG_ENABLE_IODEBUG    0

/* pccard shares CS signal with MRAM - opening pccard in user application disables MRAM */
#define BSPCFG_ENABLE_PCFLASH    0

#define BSPCFG_ENABLE_ADC0       0
#define BSPCFG_ENABLE_ADC1       0//1
#define BSPCFG_ENABLE_FLASHX     0

/*
** board-specific MQX settings - see for defaults mqx\source\include\mqx_cnfg.h
*/

#define MQX_USE_IDLE_TASK        1
#define MQX_ENABLE_LOW_POWER     0

/*
** board-specific Shell settings
*/
#define RTCSCFG_ENABLE_TCP       1
#define SHELLCFG_USES_RTCS       1

/*
** include common settings
*/

/* use the rest of defaults from small-RAM-device profile */
#include "small_ram_config.h"

/* and enable verification checks in kernel */
#include "verif_enabled_config.h"

#endif /* __user_config_h__ */
