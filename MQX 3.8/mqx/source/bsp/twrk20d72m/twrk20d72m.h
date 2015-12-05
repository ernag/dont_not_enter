/**HEADER********************************************************************
*
* Copyright (c) 2011 Freescale Semiconductor;
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
* $FileName: twrk20d72m.h$
* $Version : 3.8.0.3$
* $Date    : Mar-9-2012$
*
* Comments:
*
*   This include file is used to provide information needed by
*   an application program using the kernel running on the
*   Freescale TWR-K20D72M Evaluation board.
*
*END************************************************************************/

#ifndef _twrk20d72m_h_
    #define _twrk20d72m_h_  1


/*----------------------------------------------------------------------
**                  HARDWARE INITIALIZATION DEFINITIONS
*/

/*
** Define the board type
*/
#define BSP_TWR_K20D72M                     1

/*
** PROCESSOR MEMORY BOUNDS
*/
#define BSP_PERIPH_BASE                 (CORTEX_PERIPH_BASE)

typedef void (*vector_entry)(void);

#define BSP_INTERNAL_FLASH_BASE    0x00000000
#define BSP_INTERNAL_FLASH_SIZE    0x00040000
#define BSP_INTERNAL_FLASH_SECTOR_SIZE  0x800

#ifdef __CC_ARM
/* Keil compiler */
#define __EXTERNAL_MRAM_ROM_BASE 0x70000000
#define __EXTERNAL_MRAM_ROM_SIZE 0x00000000
#define __EXTERNAL_MRAM_RAM_BASE 0x70000000
#define __EXTERNAL_MRAM_RAM_SIZE 0x00080000
#define __EXTERNAL_LCD_BASE      0x60010000
#define __EXTERNAL_LCD_SIZE      0x0001FFFF
#define __EXTERNAL_LCD_DC_BASE   0x60000000
#define __INTERNAL_FLEXNVM_BASE  0x10000000
#define __INTERNAL_FLEXNVM_SIZE  0x00008000
#define __INTERNAL_FLEXRAM_BASE  0x14000000
#define __INTERNAL_FLEXRAM_SIZE  0x00000800
// This define MUST match the limit set in the linker file.
// The internal flash starts at 0x00000000, and ends at 0x00040000
// 1/4 of the internal flash is set aside for user flash.  Change as needed.
// MAKE SURE THE START ADDRESS IS ALIGNED (is a multiple of) __FLASHX_SECT_SIZE
#define __FLASHX_START_ADDR     0x00030000
#define __FLASHX_END_ADDR       0x00040000
#define __DEFAULT_PROCESSOR_NUMBER 1
#define __DEFAULT_INTERRUPT_STACK_SIZE 1024

#define __VECTOR_TABLE_ROM_START 0x00000000
#define __VECTOR_TABLE_RAM_START 0x1FFF8000

#else /* __CC_ARM */

extern uchar __EXTERNAL_MRAM_ROM_BASE[],    __EXTERNAL_MRAM_ROM_SIZE[];
extern uchar __EXTERNAL_MRAM_RAM_BASE[],    __EXTERNAL_MRAM_RAM_SIZE[];
extern uchar __EXTERNAL_LCD_BASE[],         __EXTERNAL_LCD_SIZE[];
extern uchar __EXTERNAL_LCD_DC_BASE[];
extern const uchar __FLASHX_START_ADDR[];
extern const uchar __FLASHX_END_ADDR[];
extern const uchar __FLASHX_SECT_SIZE[];
extern uchar __INTERNAL_FLEXNVM_BASE[];
extern uchar __INTERNAL_FLEXNVM_SIZE[];
extern uchar __INTERNAL_FLEXRAM_BASE[];
extern uchar __INTERNAL_FLEXRAM_SIZE[];

extern vector_entry __VECTOR_TABLE_RAM_START[]; // defined in linker command file
extern vector_entry __VECTOR_TABLE_ROM_START[]; // defined in linker command file

extern uchar __DEFAULT_PROCESSOR_NUMBER[];
extern uchar __DEFAULT_INTERRUPT_STACK_SIZE[];

#endif /* __CC_ARM */

#define BSP_EXTERNAL_MRAM_ROM_BASE          ((pointer)__EXTERNAL_MRAM_ROM_BASE)
#define BSP_EXTERNAL_MRAM_ROM_SIZE          ((uint_32)__EXTERNAL_MRAM_ROM_SIZE)
#define BSP_EXTERNAL_MRAM_RAM_BASE          ((pointer)__EXTERNAL_MRAM_RAM_BASE)
#define BSP_EXTERNAL_MRAM_RAM_SIZE          ((uint_32)__EXTERNAL_MRAM_RAM_SIZE)

#define BSP_EXTERNAL_LCD_BASE               ((pointer)__EXTERNAL_LCD_BASE)
#define BSP_EXTERNAL_LCD_SIZE               ((uint_32)__EXTERNAL_LCD_SIZE)
#define BSP_EXTERNAL_LCD_DC                 ((pointer)__EXTERNAL_LCD_DC_BASE)

#define BSP_INTERNAL_FLEXRAM_BASE           ((pointer)__INTERNAL_FLEXRAM_BASE)
#define BSP_INTERNAL_FLEXRAM_SIZE           ((_mem_size)__INTERNAL_FLEXRAM_SIZE)
#define INTERNAL_FLEXRAM_BASE                0x14000000

/* Compact Flash card base address */
#define BSP_CFCARD_BASE                     ((pointer)0xA0010000)

/* Enable modification of flash configuration bytes during loading for flash targets */
#ifndef BSPCFG_ENABLE_CFMPROTECT
    #define BSPCFG_ENABLE_CFMPROTECT        1
#endif
#if !BSPCFG_ENABLE_CFMPROTECT && defined(__ICCARM__)
    #error Cannot disable CFMPROTECT field on IAR compiler. Please define BSPCFG_ENABLE_CFMPROTECT to 1.
#endif

/*
 * The clock configuration settings
 * Remove old definitions of "BSP_CLOCKS" in drivers and replace
 * by runtime clock checking. Its assumed that BSP_CLOCK_CONFIGURATION_1
 * sets PLL to full speed 72MHz to be compatible with old drivers.
 */

#ifndef BSP_CLOCK_CONFIGURATION_STARTUP
    #define BSP_CLOCK_CONFIGURATION_STARTUP (BSP_CLOCK_CONFIGURATION_72MHZ)
#endif /* BSP_CLOCK_CONFIGURATION_STARTUP */

/* Init startup clock configuration is CPU_CLOCK_CONFIG_0 */
#define BSP_CLOCK_SRC                   (CPU_XTAL_CLK_HZ)
#define BSP_CORE_CLOCK                  (CPU_CORE_CLK_HZ_CONFIG_0)
#define BSP_SYSTEM_CLOCK                (CPU_CORE_CLK_HZ_CONFIG_0)
#define BSP_CLOCK                       (CPU_BUS_CLK_HZ_CONFIG_0)
#define BSP_BUS_CLOCK                   (CPU_BUS_CLK_HZ_CONFIG_0)
#define BSP_FLEXBUS_CLOCK               (CPU_FLEXBUS_CLK_HZ_CONFIG_0)
#define BSP_FLASH_CLOCK                 (CPU_FLASH_CLK_HZ_CONFIG_0)

/*
** The clock tick rate
*/
#ifndef BSP_ALARM_FREQUENCY
    #define BSP_ALARM_FREQUENCY             (200)
#endif

/*
** Old clock rate definition in MS per tick, kept for compatibility
*/
#define BSP_ALARM_RESOLUTION                (1000 / BSP_ALARM_FREQUENCY)

/*
** Define the location of the hardware interrupt vector table
*/
#if MQX_ROM_VECTORS
    #define BSP_INTERRUPT_VECTOR_TABLE              ((uint_32)__VECTOR_TABLE_ROM_START)
#else
    #define BSP_INTERRUPT_VECTOR_TABLE              ((uint_32)__VECTOR_TABLE_RAM_START)
#endif

#ifndef BSP_FIRST_INTERRUPT_VECTOR_USED
    #define BSP_FIRST_INTERRUPT_VECTOR_USED     (0)
#endif

#ifndef BSP_LAST_INTERRUPT_VECTOR_USED
    #define BSP_LAST_INTERRUPT_VECTOR_USED      (250)
#endif

#define BSP_TIMER_INTERRUPT_VECTOR              INT_SysTick
#define BSP_TIMER_INTERRUPT_PRIORITY            2

/*
** RTC interrupt level
*/
#define BSP_RTC_INT_LEVEL                      (4)

/*
** LPM related
*/
#define BSP_LPM_DEPENDENCY_LEVEL_SERIAL_POLLED (30)
#define BSP_LPM_DEPENDENCY_LEVEL_SERIAL_INT    (31)

/*
** GPIO board specifications
*/
#define BSP_LED1                    (GPIO_PORT_C | GPIO_PIN9)
#define BSP_LED2                    (GPIO_PORT_C | GPIO_PIN10)

#define BSP_SW1                     (GPIO_PORT_C | GPIO_PIN1)
#define BSP_SW2                     (GPIO_PORT_C | GPIO_PIN2)
#define BSP_ACCEL_IRQ               (GPIO_PORT_B | GPIO_PIN0) /* onboard accelerometer IRQ pin */

#define BSP_LED1_MUX_GPIO           (LWGPIO_MUX_C9_GPIO)
#define BSP_LED2_MUX_GPIO           (LWGPIO_MUX_C10_GPIO)
#define BSP_SW1_MUX_GPIO            (LWGPIO_MUX_C1_GPIO)
#define BSP_SW2_MUX_GPIO            (LWGPIO_MUX_C2_GPIO)
#define BSP_ACCEL_MUX_GPIO          (LWGPIO_MUX_B0_GPIO)

/* definitions for user applications */
#define BSP_BUTTON1                 BSP_SW1
#define BSP_BUTTON2                 BSP_SW2

#define BSP_BUTTON1_MUX_GPIO        BSP_SW1_MUX_GPIO
#define BSP_BUTTON2_MUX_GPIO        BSP_SW2_MUX_GPIO

/* Port IRQ levels */
#define BSP_PORTA_INT_LEVEL         (3)
#define BSP_PORTB_INT_LEVEL         (3)
#define BSP_PORTC_INT_LEVEL         (3)
#define BSP_PORTD_INT_LEVEL         (3)
#define BSP_PORTE_INT_LEVEL         (3)

/* GPIO settings for resistive touch screen on LCD board */
#define BSP_LCD_TCHRES_X_PLUS       (GPIO_PORT_B | GPIO_PIN3) /* open J7 7-8 */
#define BSP_LCD_TCHRES_X_MINUS      (GPIO_PORT_B | GPIO_PIN1) /* open J7 5-6 */
#define BSP_LCD_TCHRES_Y_PLUS       (GPIO_PORT_B | GPIO_PIN0) /* open J7 3-4 */
#define BSP_LCD_TCHRES_Y_MINUS      (GPIO_PORT_B | GPIO_PIN2) /* open J7 7-8 */

#define BSP_LCD_SPI_CHANNEL         "spi0:"

#define BSP_LCD_X_PLUS_ADC_CHANNEL  (ADC0_SOURCE_AD13)
#define BSP_LCD_Y_PLUS_ADC_CHANNEL  (ADC0_SOURCE_AD8)

/* ADC tchres device init struct */
#define BSP_TCHRES_ADC_DEVICE "adc0:"
#define BSP_TCHRES_X_TRIGGER ADC_PDB_TRIGGER
#define BSP_TCHRES_Y_TRIGGER ADC_PDB_TRIGGER


/*-----------------------------------------------------------------------------
**                      DSPI
*/

#define BSP_DSPI_RX_BUFFER_SIZE          (32)
#define BSP_DSPI_TX_BUFFER_SIZE          (32)
#define BSP_DSPI_BAUDRATE                (1000000)
#define BSP_DSPI_INT_LEVEL               (4)
#define BSP_SPI_MEMORY_CHANNEL           (1)

/*-----------------------------------------------------------------------------
**                      SDCARD
*/
#define BSP_SDCARD_SPI_CHANNEL           "spi1:"
#define BSP_SDCARD_GPIO_DETECT           (GPIO_PORT_C | GPIO_PIN5)
#define BSP_SDCARD_GPIO_PROTECT          (GPIO_PORT_B | GPIO_PIN21)
#define BSP_SDCARD_DETECT_MUX_GPIO       (LWGPIO_MUX_C5_GPIO)
#define BSP_SDCARD_PROTECT_MUX_GPIO      (LWGPIO_MUX_B21_GPIO)
#define BSP_SDCARD_SPI_CS                (SPI_PUSHR_PCS(1))

/*-----------------------------------------------------------------------------
**                      I2C
*/
#define BSP_I2C_CLOCK                       (BSP_BUS_CLOCK)
#define BSP_I2C0_ADDRESS                    (0x6E)
#define BSP_I2C1_ADDRESS                    (0x6F)
#define BSP_I2C0_BAUD_RATE                  (100000)
#define BSP_I2C1_BAUD_RATE                  (100000)
#define BSP_I2C0_MODE                       (I2C_MODE_MASTER)
#define BSP_I2C1_MODE                       (I2C_MODE_MASTER)
#define BSP_I2C0_INT_LEVEL                  (5)
#define BSP_I2C1_INT_LEVEL                  (5)
#define BSP_I2C0_RX_BUFFER_SIZE             (64)
#define BSP_I2C0_TX_BUFFER_SIZE             (64)
#define BSP_I2C1_RX_BUFFER_SIZE             (64)
#define BSP_I2C1_TX_BUFFER_SIZE             (64)

/*-----------------------------------------------------------------------------
**                      I2S
*/

#define BSP_I2S0_MODE                   (I2S_MODE_MASTER)
#define BSP_I2S0_DATA_BITS              (16)
#define BSP_I2S0_CLOCK_SOURCE           (I2S_CLK_INT)
#define BSP_I2S0_INT_LEVEL              (5)
#define BSP_I2S0_BUFFER_SIZE            (128)

/*-----------------------------------------------------------------------------
**                      ADC
*/
#define ADC_MAX_MODULES                 (2)

#define BSP_ADC_CH_POT                  (ADC0_SOURCE_AD1)

#define BSP_ADC0_VECTOR_PRIORITY        (3)
#define BSP_ADC1_VECTOR_PRIORITY        (3)

#define BSP_PDB_VECTOR_PRIORITY         (3)

/*-----------------------------------------------------------------------------
**                      USB
*/
#define BSP_USB_INT_LEVEL                (4)
#define BSP_USB_TWR_SER2                 (0) /*set to 1 if TWR-SER2 (2 eth) board used (only host)*/

/* USB host controler for K20D processors */
#define USBCFG_KHCI                       1

/*----------------------------------------------------------------------
**                  IO DEVICE DRIVERS CONFIGURATION
*/

#ifndef BSPCFG_ENABLE_IO_SUBSYSTEM
    #define BSPCFG_ENABLE_IO_SUBSYSTEM      1
#endif

/* polled TTY device (UART0) */
#ifndef BSPCFG_ENABLE_TTYA
    #define BSPCFG_ENABLE_TTYA              1
#endif

/* polled TTY device (UART1) */
#ifndef BSPCFG_ENABLE_TTYB
    #define BSPCFG_ENABLE_TTYB              0
#endif

/* polled TTY device (UART2) */
#ifndef BSPCFG_ENABLE_TTYC
    #define BSPCFG_ENABLE_TTYC              0
#endif

/* polled TTY device (UART3) */
#ifndef BSPCFG_ENABLE_TTYD
    #define BSPCFG_ENABLE_TTYD              0
#endif

/* polled TTY device (UART4) */
#ifndef BSPCFG_ENABLE_TTYE
    #define BSPCFG_ENABLE_TTYE              1
#endif

/* interrupt-driven TTY device (UART0) */
#ifndef BSPCFG_ENABLE_ITTYA
    #define BSPCFG_ENABLE_ITTYA             0
#endif

/* interrupt-driven TTY device (UART1) */
#ifndef BSPCFG_ENABLE_ITTYB
    #define BSPCFG_ENABLE_ITTYB             0
#endif

/* interrupt-driven TTY device (UART2) */
#ifndef BSPCFG_ENABLE_ITTYC
    #define BSPCFG_ENABLE_ITTYC             0
#endif

/* interrupt-driven TTY device (UART3) */
#ifndef BSPCFG_ENABLE_ITTYD
    #define BSPCFG_ENABLE_ITTYD             0
#endif

/* interrupt-driven TTY device (UART4) */
#ifndef BSPCFG_ENABLE_ITTYE
    #define BSPCFG_ENABLE_ITTYE             0
#endif

/* TTY device HW signals (UART3) */
#ifndef BSPCFG_ENABLE_TTYD_HW_SIGNALS
    #define BSPCFG_ENABLE_TTYD_HW_SIGNALS   0
#endif

/* TTYA and ITTYA baud rate */
#ifndef BSPCFG_SCI0_BAUD_RATE
    #define BSPCFG_SCI0_BAUD_RATE             115200
#endif

/* TTYB and ITTYB baud rate */
#ifndef BSPCFG_SCI1_BAUD_RATE
    #define BSPCFG_SCI1_BAUD_RATE             115200
#endif

/* TTYC and ITTYC baud rate */
#ifndef BSPCFG_SCI2_BAUD_RATE
    #define BSPCFG_SCI2_BAUD_RATE             115200
#endif

/* TTYD and ITTYD baud rate */
#ifndef BSPCFG_SCI3_BAUD_RATE
    #define BSPCFG_SCI3_BAUD_RATE             115200
#endif

/* TTYE and ITTYE baud rate */
#ifndef BSPCFG_SCI4_BAUD_RATE
    #define BSPCFG_SCI4_BAUD_RATE             115200
#endif

/* TTYA and ITTYA buffer size */
#ifndef BSPCFG_SCI0_QUEUE_SIZE
    #define BSPCFG_SCI0_QUEUE_SIZE             64
#endif

/* TTYB and ITTYB buffer size */
#ifndef BSPCFG_SCI1_QUEUE_SIZE
    #define BSPCFG_SCI1_QUEUE_SIZE             64
#endif

/* TTYC and ITTYC buffer size */
#ifndef BSPCFG_SCI2_QUEUE_SIZE
    #define BSPCFG_SCI2_QUEUE_SIZE             64
#endif

/* TTYD and ITTYD buffer size */
#ifndef BSPCFG_SCI3_QUEUE_SIZE
    #define BSPCFG_SCI3_QUEUE_SIZE             64
#endif

/* TTYE and ITTYE buffer size */
#ifndef BSPCFG_SCI4_QUEUE_SIZE
    #define BSPCFG_SCI4_QUEUE_SIZE             64
#endif

/* polled I2C0 device */
#ifndef BSPCFG_ENABLE_I2C0
    #define BSPCFG_ENABLE_I2C0                  1
#endif

/* polled I2C1 device */
#ifndef BSPCFG_ENABLE_I2C1
    #define BSPCFG_ENABLE_I2C1                  0
#endif

/* int I2C0 device */
#ifndef BSPCFG_ENABLE_II2C0
    #define BSPCFG_ENABLE_II2C0                 0
#endif

/* int I2C1 device */
#ifndef BSPCFG_ENABLE_II2C1
    #define BSPCFG_ENABLE_II2C1                 0
#endif

/* GPIO device */
#ifndef BSPCFG_ENABLE_GPIODEV
    #define BSPCFG_ENABLE_GPIODEV               0
#endif

/* RTC device */
#ifndef BSPCFG_ENABLE_RTCDEV
    #define BSPCFG_ENABLE_RTCDEV                0
#endif

/* PCFLASH device */
#ifndef BSPCFG_ENABLE_PCFLASH
    #define BSPCFG_ENABLE_PCFLASH               0
#endif

/* polled SPI0 */
#ifndef BSPCFG_ENABLE_SPI0
    #define BSPCFG_ENABLE_SPI0                  0
#endif

/* int SPI0 */
#ifndef BSPCFG_ENABLE_ISPI0
    #define BSPCFG_ENABLE_ISPI0                 0
#endif

/* polled SPI1 */
#ifndef BSPCFG_ENABLE_SPI1
    #define BSPCFG_ENABLE_SPI1                  0
#endif

/* int SPI1 */
#ifndef BSPCFG_ENABLE_ISPI1
    #define BSPCFG_ENABLE_ISPI1                 0
#endif

/* polled SPI2 */
#ifndef BSPCFG_ENABLE_SPI2
    #define BSPCFG_ENABLE_SPI2                  0
#endif

/* int SPI2 */
#ifndef BSPCFG_ENABLE_ISPI2
    #define BSPCFG_ENABLE_ISPI2                 0
#endif

/* ADC */
#ifndef BSPCFG_ENABLE_ADC
    #define BSPCFG_ENABLE_ADC                   0
#endif

/* FTFL */
#ifndef BSPCFG_ENABLE_FLASHX
    #define BSPCFG_ENABLE_FLASHX                0
#endif

/* ESDHC device */
#ifndef BSPCFG_ENABLE_ESDHC
    #define BSPCFG_ENABLE_ESDHC                 0
#endif

/* LCD device */
#ifndef BSPCFG_ENABLE_LCD
    #define BSPCFG_ENABLE_LCD               0
#endif

/* IODEBUG device */
#ifndef BSPCFG_ENABLE_IODEBUG
    #define BSPCFG_ENABLE_IODEBUG               1
#endif

/*----------------------------------------------------------------------
**                  DEFAULT MQX INITIALIZATION DEFINITIONS
*/

/* Defined in link.xxx */
extern uchar __KERNEL_DATA_START[];
extern uchar __KERNEL_DATA_END[];

#define BSP_DEFAULT_START_OF_KERNEL_MEMORY                    ((pointer)__KERNEL_DATA_START)
#define BSP_DEFAULT_END_OF_KERNEL_MEMORY                      ((pointer)__KERNEL_DATA_END)
#define BSP_DEFAULT_PROCESSOR_NUMBER                          ((uint_32)__DEFAULT_PROCESSOR_NUMBER)

#ifndef BSP_DEFAULT_INTERRUPT_STACK_SIZE
    #define BSP_DEFAULT_INTERRUPT_STACK_SIZE                  ((uint_32)__DEFAULT_INTERRUPT_STACK_SIZE)
#endif

#ifndef BSP_DEFAULT_MQX_HARDWARE_INTERRUPT_LEVEL_MAX
    #define BSP_DEFAULT_MQX_HARDWARE_INTERRUPT_LEVEL_MAX      (2L)
#endif

#ifndef BSP_DEFAULT_MAX_MSGPOOLS
    #define BSP_DEFAULT_MAX_MSGPOOLS                          (2L)
#endif

#ifndef BSP_DEFAULT_MAX_MSGQS
    #define BSP_DEFAULT_MAX_MSGQS                             (16L)
#endif

/*
 * Other Serial console options:(do not forget to enable BSPCFG_ENABLE_TTY define if changed)
 *      "ttyb:"      TWR-SER / OSJTAG-COM     polled mode
 *      "ittyb:"     TWR-SER / OSJTAG-COM  interrupt mode
 *      "iodebug:"   IDE debug console
 */
#ifndef BSP_DEFAULT_IO_CHANNEL
    #if BSPCFG_ENABLE_TTYB
        #define BSP_DEFAULT_IO_CHANNEL                      "ttyb:"    /* OSJTAG-COM   polled mode   */
        #define BSP_DEFAULT_IO_CHANNEL_DEFINED
    #else
        #define BSP_DEFAULT_IO_CHANNEL                      NULL
    #endif
#endif

#ifndef BSP_DEFAULT_IO_OPEN_MODE
    #define BSP_DEFAULT_IO_OPEN_MODE                      (pointer) (IO_SERIAL_XON_XOFF | IO_SERIAL_TRANSLATION | IO_SERIAL_ECHO)
#endif

/* FLASHX flash memory pool minimum size */
#if BSPCFG_ENABLE_FLASHX
    #ifndef BSPCFG_FLASHX_SIZE
        #define BSPCFG_FLASHX_SIZE      (FLASHX_END_ADDR - FLASHX_START_ADDR) /* defines maximum possible space */
    #endif
#else
    #undef  BSPCFG_FLASHX_SIZE
    #define BSPCFG_FLASHX_SIZE      0x0
#endif

#endif /* _twrk20d72m_h_  */
