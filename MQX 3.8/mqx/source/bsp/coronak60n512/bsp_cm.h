/**HEADER***********************************************************************
*
* Copyright (c) 2011 Freescale Semiconductor;
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
* $FileName: bsp_cm.h$
* $Version : 3.8.3.1$
* $Date    : Dec-9-2011$
*
* Comments:
*   Clock manager Kinetis BSP specific definitions and function prototypes.
*
*       _bsp_initialize_hardware();
*       _bsp_set_clock_configuration();
*       _bsp_get_clock_configuration();
*       _bsp_get_clock();
*       _bsp_osc_autotrim();
*
*END***************************************************************************/

#ifndef __bsp_cm_h__
    #define __bsp_cm_h__


#ifndef PE_LDD_VERSION

#if 0
    // EA:  Commenting out since these are not even used by anyone.
    #define CPU_BUS_CLK_HZ                  0x02DC6C00U /* Initial value of the bus clock frequency in Hz */
    #define CPU_CORE_CLK_HZ                 0x05B8D800U /* Initial value of the core/system clock frequency in Hz.  */
#endif

#define CPU_CLOCK_CONFIG_NUMBER         0x03U       /* Specifies number of defined clock configurations. */

#define CPU_XTAL_CLK_HZ                 0x02FAF080U /* Value of the external crystal or oscillator clock frequency in Hz */
#define CPU_XTAL32k_CLK_HZ              0x8000U     /* Value of the external 32k crystal or oscillator clock frequency in Hz */
#define CPU_INT_SLOW_CLK_HZ             0x8000U     /* Value of the slow internal oscillator clock frequency in Hz  */
#define CPU_INT_FAST_CLK_HZ             0x003D0900U /* Value of the fast internal oscillator clock frequency in Hz  */

#define CPU_FAMILY_Kinetis             /* Specification of the core type of the selected cpu */
#define CPU_DERIVATIVE_MK60DN512MD10   /* Name of the selected cpu derivative */
#define CPU_PARTNUM_MK60DN512VMD10     /* Part number of the selected cpu */

/* CPU frequencies in clock configuration 0 */
#define CPU_CLOCK_CONFIG_0              0x00U           /* Clock configuration 0 identifier */
#define CPU_PLL_FLL_CLK_HZ_CONFIG_0     WHISTLE_CONFIG0_CLOCK          /* PLL/FLL clock frequency in clock configuration 0 */
#define CPU_CORE_CLK_HZ_CONFIG_0        WHISTLE_CONFIG0_CORE_CLOCK     /* Core clock frequency in clock configuration 0 */
#define CPU_BUS_CLK_HZ_CONFIG_0         WHISTLE_CONFIG0_BUS_CLOCK      /* Bus clock frequency in clock configuration 0 */
#define CPU_FLEXBUS_CLK_HZ_CONFIG_0     WHISTLE_CONFIG0_FLEXBUS_CLOCK  /* Flexbus clock frequency in clock configuration 0 */
#define CPU_FLASH_CLK_HZ_CONFIG_0       WHISTLE_CONFIG0_FLASH_CLOCK     /* FLASH clock frequency in clock configuration 0 */
#define CPU_USB_CLK_HZ_CONFIG_0         WHISTLE_CONFIG0_USB_CLOCK      /* USB clock frequency in clock configuration 0 */
#define CPU_MCGIR_CLK_HZ_CONFIG_0       0x00UL          /* MCG internal reference clock frequency in clock configuration 0 */
#define CPU_OSCER_CLK_HZ_CONFIG_0       0x00UL          /* System OSC external reference clock frequency in clock configuration 0 */
#define CPU_ERCLK32K_CLK_HZ_CONFIG_0    0x8000UL        /* External reference clock 32k frequency in clock configuration 0 */
#define CPU_MCGFF_CLK_HZ_CONFIG_0       0x5F5EUL        /* MCG fixed frequency clock */

/* CPU frequencies in clock configuration 1 */
#define CPU_CLOCK_CONFIG_1              0x01U           /* Clock configuration 1 identifier */
#define CPU_PLL_FLL_CLK_HZ_CONFIG_1     WHISTLE_CONFIG1_CLOCK          /* PLL/FLL clock frequency in clock configuration 1 */
#define CPU_CORE_CLK_HZ_CONFIG_1        WHISTLE_CONFIG1_CORE_CLOCK     /* Core clock frequency in clock configuration 1 */
#define CPU_BUS_CLK_HZ_CONFIG_1         WHISTLE_CONFIG1_BUS_CLOCK      /* Bus clock frequency in clock configuration 1 */
#define CPU_FLEXBUS_CLK_HZ_CONFIG_1     WHISTLE_CONFIG1_FLEXBUS_CLOCK  /* Flexbus clock frequency in clock configuration 1 */
#define CPU_FLASH_CLK_HZ_CONFIG_1       WHISTLE_CONFIG1_FLASH_CLOCK    /* FLASH clock frequency in clock configuration 1 */
#define CPU_USB_CLK_HZ_CONFIG_1         WHISTLE_CONFIG1_USB_CLOCK      /* USB clock frequency in clock configuration 1 */
#define CPU_MCGIR_CLK_HZ_CONFIG_1       0x00UL          /* MCG internal reference clock frequency in clock configuration 1 */
#define CPU_OSCER_CLK_HZ_CONFIG_1       0x00UL          /* System OSC external reference clock frequency in clock configuration 1 */
#define CPU_ERCLK32K_CLK_HZ_CONFIG_1    0x8000UL        /* External reference clock 32k frequency in clock configuration 1 */
#define CPU_MCGFF_CLK_HZ_CONFIG_1       0x5F5EUL        /* MCG fixed frequency clock */

/* CPU frequencies in clock configuration 2 */
#define CPU_CLOCK_CONFIG_2              0x02U           /* Clock configuration 2 identifier */
#define CPU_CORE_CLK_HZ_CONFIG_2        VLPR_CORE_CLOCK /* Core clock frequency in clock configuration 2 */
#define CPU_BUS_CLK_HZ_CONFIG_2         VLPR_BUS_CLOCK  /* Bus clock frequency in clock configuration 2 */
#define CPU_FLEXBUS_CLK_HZ_CONFIG_2     VLPR_FLEXBUS_CLOCK  /* Flexbus clock frequency in clock configuration 2 */
#define CPU_FLASH_CLK_HZ_CONFIG_2       VLPR_FLASH_CLOCK /* FLASH clock frequency in clock configuration 2 */
#define CPU_USB_CLK_HZ_CONFIG_2         0x00UL          /* USB clock frequency in clock configuration 2 */
#define CPU_PLL_FLL_CLK_HZ_CONFIG_2     0x00UL          /* PLL/FLL clock frequency in clock configuration 2 */
#define CPU_MCGIR_CLK_HZ_CONFIG_2       VLPR_CORE_CLOCK /* MCG internal reference clock frequency in clock configuration 2 */
#define CPU_OSCER_CLK_HZ_CONFIG_2       0x00UL          /* System OSC external reference clock frequency in clock configuration 2 */
#define CPU_ERCLK32K_CLK_HZ_CONFIG_2    0x8000UL        /* External reference clock 32k frequency in clock configuration 2 */
#define CPU_MCGFF_CLK_HZ_CONFIG_2       0x4000UL        /* MCG fixed frequency clock */

#endif /* PE_LDD_VERSION */


typedef enum {
    BSP_CLOCK_CONFIGURATION_0 = 0,      /* 24MHZ PEE  BJM: changed CONFIG_0 from 96MHz to 24MHz */
    BSP_CLOCK_CONFIGURATION_1,          /* 96MHz PEE  BJM: changed CONFIG_1 from 12MHz to 96MHz */
    BSP_CLOCK_CONFIGURATION_2,          /* 4 MHz BLPI  BJM: changed CONFIG_2 from 2MHz to 4MHz */
    BSP_CLOCK_CONFIGURATIONS,           /* Number of available clock configurations */
    
    // TODO: get CLOCK CONFIG 1 working so that it can be used for auto-trim. important thing
    // is for bus clock to be 8-16MHz when the autotrim is called. 
  //  BSP_CLOCK_CONFIGURATION_AUTOTRIM    = BSP_CLOCK_CONFIGURATION_1,
    BSP_CLOCK_CONFIGURATION_AUTOTRIM    = BSP_CLOCK_CONFIGURATION_0,
    
    
    BSP_CLOCK_CONFIGURATION_DEFAULT     = BSP_CLOCK_CONFIGURATION_0,
    BSP_CLOCK_CONFIGURATION_24MHZ       = BSP_CLOCK_CONFIGURATION_0, /* BJM: changed to 24MHz */
    BSP_CLOCK_CONFIGURATION_96MHZ       = BSP_CLOCK_CONFIGURATION_1, /* BJM: changed to 96MHz */
    BSP_CLOCK_CONFIGURATION_4MHZ        = BSP_CLOCK_CONFIGURATION_2  /* BJM: changed to 4MHz */
} BSP_CLOCK_CONFIGURATION;

#ifdef __cplusplus
extern "C" {
#endif


void _bsp_initialize_hardware
(
    void
);

BSP_CLOCK_CONFIGURATION _bsp_get_clock_configuration
(
    void
);

uint16_t _bsp_set_clock_configuration
(
    /* [IN] runtime clock configuration */
    const BSP_CLOCK_CONFIGURATION clock_configuration
);


uint32_t _bsp_get_clock
(
    /* [IN] clock configuration */
    const BSP_CLOCK_CONFIGURATION   clock_configuration,
    /* [IN] clock source index */
    const CM_CLOCK_SOURCE           clock_source
);

uint16_t _bsp_osc_autotrim
(
    void
);

#ifdef __cplusplus
}
#endif


#endif /* __bsp_cm_h__ */

