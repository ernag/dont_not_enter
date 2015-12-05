/**HEADER**********************************************************************
 *
 * Copyright (c) 2011 Freescale Semiconductor;
 * All Rights Reserved
 *
 ******************************************************************************
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
 ******************************************************************************
 *
 * $FileName: init_bsp.c$
 * $Version : 3.8.0.3$
 * $Date    : Mar-6-2012$
 *
 * Comments:
 *   This file contains the source functions for functions required to
 *   specifically initialize the card.
 *
 *END*************************************************************************/

#include "mqx_inc.h"
#include "bsp.h"
#include "bsp_prv.h"
#include "io_rev.h"
#include "bsp_rev.h"

const char _PTR_ _mqx_bsp_revision = REAL_NUM_TO_STR(BSP_REVISION);
const char _PTR_ _mqx_io_revision  = REAL_NUM_TO_STR(IO_REVISION);

void _bsp_systick(pointer dummy);

/*FUNCTION*--------------------------------------------------------------------
 *
 * Function Name    : systick_config
 * Returned Value   : void
 * Comments         :
 *   Initialize system timer
 *
 *END*-----------------------------------------------------------------------*/

void systick_config
(
        uint32_t bsp_system_clock,
        uint32_t bsp_alarm_frequency
)
{
    SYST_RVR = bsp_system_clock / bsp_alarm_frequency - 1;
}

/*FUNCTION*--------------------------------------------------------------------
 *
 * Function Name    : systick_init
 * Returned Value   : void
 * Comments         :
 *   Initialize system timer
 *
 *END*-----------------------------------------------------------------------*/

static void systick_init
(
        void
)
{

    /* Get system clock for active clock configuration */
    uint32_t system_clock = _cm_get_clock(_cm_get_clock_configuration(), CM_CLOCK_SOURCE_CORE);

#if BSP_ALARM_FREQUENCY == 0
#error Wrong definition of BSP_ALARM_FREQUENCY
#endif

    systick_config(system_clock, BSP_ALARM_FREQUENCY);

    SYST_CSR = 7;

    /* SVCall priority*/
    SCB_SHPR2 |= 0x10000000;
    /* SysTick priority*/
    SCB_SHPR3 |= SCB_SHPR3_PRI_15(CORTEX_PRIOR(BSP_TIMER_INTERRUPT_PRIORITY));
}


/*FUNCTION*--------------------------------------------------------------------
 *
 * Function Name    : _bsp_enable_card
 * Returned Value   : uint_32 result
 * Comments         :
 *   This function sets up operation of the card.
 *
 *END*-----------------------------------------------------------------------*/
uint_32 _bsp_enable_card(void)
{
    uint_32 result;
    KERNEL_DATA_STRUCT_PTR         kernel_data;

    /* Set the CPU type */
    _mqx_set_cpu_type(MQX_CPU);

#if MQX_EXIT_ENABLED
    /* Set the bsp exit handler, called by _mqx_exit */
    _mqx_set_exit_handler(_bsp_exit_handler);
#endif

    /* Memory splitter - prevent accessing both ram banks in one instruction */
    _mem_alloc_at(0, (void*)0x20000000);

    result = _psp_int_init(BSP_FIRST_INTERRUPT_VECTOR_USED, BSP_LAST_INTERRUPT_VECTOR_USED);
    if (result != MQX_OK) {
        return result;
    }

    /* set possible new interrupt vector table - if MQX_ROM_VECTORS = 0 switch to
    ram interrupt table which was initialized in _psp_int_init) */
    _int_set_vector_table(BSP_INTERRUPT_VECTOR_TABLE);

    /* Store timer interrupt vector for debugger */
    _time_set_timer_vector(BSP_TIMER_INTERRUPT_VECTOR);

    /* Install Timer ISR. */
    if (_int_install_isr(BSP_TIMER_INTERRUPT_VECTOR, (void (_CODE_PTR_)(pointer))_bsp_systick, NULL) == NULL)
    {
        return MQX_TIMER_ISR_INSTALL_FAIL;
    }
   
#if PE_LDD_VERSION
    /** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
    PE_low_level_init();
#endif

    /*  System timer initialization */
    systick_init();

    /* MCG initialization and internal oscillators trimming */
    if (CM_ERR_OK != _bsp_set_clock_configuration(BSP_CLOCK_CONFIGURATION_AUTOTRIM))
    {
        return MQX_TIMER_ISR_INSTALL_FAIL;
    }

    if (CM_ERR_OK != _bsp_osc_autotrim())
    {
        return MQX_TIMER_ISR_INSTALL_FAIL;
    }

    /* Switch to startup clock configuration */
    if (CM_ERR_OK != _bsp_set_clock_configuration(BSP_CLOCK_CONFIGURATION_STARTUP))
    {
        return MQX_TIMER_ISR_INSTALL_FAIL;
    }

    /* Initialize the system ticks */
    _GET_KERNEL_DATA(kernel_data);
    kernel_data->TIMER_HW_REFERENCE = (BSP_SYSTEM_CLOCK / BSP_ALARM_FREQUENCY);
    _time_set_ticks_per_sec(BSP_ALARM_FREQUENCY);
    _time_set_hwticks_per_tick(kernel_data->TIMER_HW_REFERENCE);
    _time_set_hwtick_function(_bsp_get_hwticks, (pointer)NULL);
    
#if BSPCFG_ENABLE_IO_SUBSYSTEM
    /* Initialize the I/O Sub-system */
    result = _io_init();
    if (result != MQX_OK) {
        return result;
    }

    /* Install low power support */
#if MQX_ENABLE_LOW_POWER
    SMC_PMPROT = SMC_PMPROT_AVLP_MASK | SMC_PMPROT_ALLS_MASK; // allow VLPx, LLS, disallow VLLSx
    _lpm_install (LPM_CPU_OPERATION_MODES, LPM_OPERATION_MODE_RUN);
#endif

    /* Initialize RTC and MQX time */
#if BSPCFG_ENABLE_RTCDEV
    if (MQX_OK == _bsp_rtc_io_init())   {
        _rtc_init (RTC_INIT_FLAG_ENABLE);
        _rtc_sync_with_mqx (TRUE);
    }
#endif

    /* Install device drivers */
#if BSPCFG_ENABLE_TTYA
    _kuart_polled_install("ttya:", &_bsp_sci0_init, _bsp_sci0_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_ITTYA
    _kuart_int_install("ittya:", &_bsp_sci0_init, _bsp_sci0_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_TTYB
    _kuart_polled_install("ttyb:", &_bsp_sci1_init, _bsp_sci1_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_ITTYB
    _kuart_int_install("ittyb:", &_bsp_sci1_init, _bsp_sci1_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_TTYC
    _kuart_polled_install("ttyc:", &_bsp_sci2_init, _bsp_sci2_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_ITTYC
    _kuart_int_install("ittyc:", &_bsp_sci2_init, _bsp_sci2_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_TTYD
    _kuart_polled_install("ttyd:", &_bsp_sci3_init, _bsp_sci3_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_ITTYD
    _kuart_int_install("ittyd:", &_bsp_sci3_init, _bsp_sci3_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_TTYE
    _kuart_polled_install("ttye:", &_bsp_sci4_init, _bsp_sci4_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_ITTYE
    _kuart_int_install("ittye:", &_bsp_sci4_init, _bsp_sci4_init.QUEUE_SIZE);
#endif

#if BSPCFG_ENABLE_I2C0
    _ki2c_polled_install("i2c0:", &_bsp_i2c0_init);
#endif
#if BSPCFG_ENABLE_I2C1
    _ki2c_polled_install("i2c1:", &_bsp_i2c1_init);
#endif
#if BSPCFG_ENABLE_II2C0
    _ki2c_int_install("ii2c0:", &_bsp_i2c0_init);
#endif
#if BSPCFG_ENABLE_II2C1
    _ki2c_int_install("ii2c1:", &_bsp_i2c1_init);
#endif

#if BSPCFG_ENABLE_SPI0
    _dspi_polled_install("spi0:", &_bsp_dspi0_init);
#endif

#if BSPCFG_ENABLE_ISPI0
    _dspi_dma_install("ispi0:", &_bsp_dspi0_init);
#endif

#if BSPCFG_ENABLE_SPI1
    _dspi_polled_install("spi1:", &_bsp_dspi1_init);
#endif

#if BSPCFG_ENABLE_ISPI1
    _dspi_dma_install("ispi1:", &_bsp_dspi1_init);
#endif

#if BSPCFG_ENABLE_SPI2
    _dspi_polled_install("spi2:", &_bsp_dspi2_init);
#endif

#if BSPCFG_ENABLE_ISPI2
    _dspi_dma_install("ispi2:", &_bsp_dspi2_init);
#endif

    /* Install the GPIO driver */
#if BSPCFG_ENABLE_GPIODEV
    _io_gpio_install("gpio:");
#endif

#if BSPCFG_ENABLE_ADC0
    _io_adc_install("adc0:", (pointer) &_bsp_adc0_init);
#endif
#if BSPCFG_ENABLE_ADC1
    _io_adc_install("adc1:", (pointer) &_bsp_adc1_init);
#endif

    /* Install the LCD driver */
#if BSPCFG_ENABLE_LCD
    _bsp_lcd_io_init();
#endif

    /* Install the PCCard Flash drivers */
#if BSPCFG_ENABLE_PCFLASH
    _io_pccardflexbus_install("pccarda:", (PCCARDFLEXBUS_INIT_STRUCT _PTR_) &_bsp_cfcard_init);
    _io_apcflash_install("pcflasha:");
#endif

#if BSPCFG_ENABLE_FLASHX
    _io_flashx_install("flashx:", &_bsp_flashx_init);
#endif

#if BSPCFG_ENABLE_IODEBUG
    _io_debug_install("iodebug:", &_bsp_iodebug_init);
#endif

   
    /* Initialize the default serial I/O */
    _io_serial_default_init();

#endif // BSPCFG_ENABLE_IO_SUBSYSTEM

    return MQX_OK;
}


/*FUNCTION*--------------------------------------------------------------------
 *
 * Function Name    : _bsp_exit_handler
 * Returned Value   : none
 * Comments         :
 *    This function is called when MQX exits
 *
 *END*-----------------------------------------------------------------------*/

void _bsp_exit_handler(void) {
}

/*ISR*********************************************************************
 *
 * Function Name    : _bsp_timer_isr
 * Returned Value   : void
 * Comments         :
 *    The timer ISR is the interrupt handler for the clock tick.
 *
 *END**********************************************************************/

void _bsp_systick(pointer dummy) {
    _time_notify_kernel();
}


/*FUNCTION*--------------------------------------------------------------------
 *
 * Function Name    : _bsp_get_hwticks
 * Returned Value   : none
 * Comments         :
 *    This function returns the number of hw ticks that have elapsed
 * since the last interrupt
 *
 *END*-----------------------------------------------------------------------*/

uint_32 _bsp_get_hwticks(pointer param) {
    uint_32 modulo;
    uint_32 count;
    uint_32 overflow;

    overflow = 0;
    modulo = (SYST_RVR & SysTick_RVR_RELOAD_MASK) + 1;
    count = SYST_CVR & SysTick_CVR_CURRENT_MASK;

    if (SCB_ICSR & SCB_ICSR_PENDSTSET_MASK) {
        /* Another full TICK period has expired... */
        count = SYST_CVR & SysTick_CVR_CURRENT_MASK;
        overflow = modulo;
    }

    /* interrupt flag is set upon 1->0 transition, not upon reload - wrap around */
    if (count == 0)
        count = modulo;

    return (modulo - count + overflow);
}

/* EOF */
