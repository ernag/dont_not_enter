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
* $FileName: twrk60n512.h$
* $Version : 3.8.60.3$
* $Date    : Nov-16-2011$
*
* Comments:
*
*   This include file is used to provide information needed by
*   an application program using the kernel running on the
*   Freescale TWR-K60N512 Evaluation board.
*
*END************************************************************************/

#ifndef _twrk60n512_h_
    #define _twrk60n512_h_  1


/*----------------------------------------------------------------------
**                  HARDWARE INITIALIZATION DEFINITIONS
*/

/*
** Define the board type
*/
#define BSP_TWR_K60N512                     1

/*
** The revision of the silicon.
** This should be specified in the user_config.h,
** otherwise the following default setting is applied.
*/
#ifndef MK60_REV_1_0
    #define MK60_REV_1_0         1  /* MQX 3.7.0 */
#endif

/*
** PROCESSOR MEMORY BOUNDS
*/
#define BSP_PERIPH_BASE                     (CORTEX_PERIPH_BASE)
extern uchar __EXTERNAL_MRAM_ROM_BASE[],    __EXTERNAL_MRAM_ROM_SIZE[];
extern uchar __EXTERNAL_MRAM_RAM_BASE[],    __EXTERNAL_MRAM_RAM_SIZE[];
extern uchar __IPSBAR[];
extern uchar __EXTERNAL_LCD_BASE[],         __EXTERNAL_LCD_SIZE[];
extern uchar __EXTERNAL_LCD_DC_BASE[];

#define BSP_INTERNAL_FLASH_BASE    0x00000000
#define BSP_INTERNAL_FLASH_SIZE    0x00080000
#define BSP_INTERNAL_FLASH_SECTOR_SIZE  0x800

#define BSP_EXTERNAL_MRAM_ROM_BASE  		((pointer)__EXTERNAL_MRAM_ROM_BASE)
#define BSP_EXTERNAL_MRAM_ROM_SIZE  		((uint_32)__EXTERNAL_MRAM_ROM_SIZE)
#define BSP_EXTERNAL_MRAM_RAM_BASE  		((pointer)__EXTERNAL_MRAM_RAM_BASE)

extern uchar __EXTERNAL_MRAM_ROM_BASE[],    __EXTERNAL_MRAM_ROM_SIZE[];
extern uchar __EXTERNAL_MRAM_RAM_BASE[],    __EXTERNAL_MRAM_RAM_SIZE[];
extern uchar __EXTERNAL_LCD_BASE[],         __EXTERNAL_LCD_SIZE[];
extern uchar __EXTERNAL_LCD_DC_BASE[];
extern const uchar __FLASHX_START_ADDR[];
extern const uchar __FLASHX_END_ADDR[];
extern const uchar __FLASHX_SECT_SIZE[];
extern uchar __INTERNAL_FLEXNVM_BASE[];
extern uchar __INTERNAL_FLEXNVM_SIZE[];

/*
 * MRAM cannot be reliably used on TWR_K60N512 due to a hardware conflict unless a hw modification is done:
 * disconnecting IrDA receiver e.g. by removing R14 makes MRAM usage possible.
 */
//#define BSP_EXTERNAL_MRAM_RAM_SIZE  		((uint_32)__EXTERNAL_MRAM_RAM_SIZE)

#define BSP_EXTERNAL_LCD_BASE               ((pointer)__EXTERNAL_LCD_BASE)
#define BSP_EXTERNAL_LCD_SIZE               ((uint_32)__EXTERNAL_LCD_SIZE)
#define BSP_EXTERNAL_LCD_DC                 ((pointer)__EXTERNAL_LCD_DC_BASE)

/* Compact Flash card base address */
#define BSP_CFCARD_BASE                     ((pointer)0xA0010000)

/*
 * The clock configuration settings
 * Remove old definitions of "BSP_CLOCKS" in drivers and replace
 * by runtime clock checking. Its assumed that BSP_CLOCK_CONFIGURATION_1
 * sets PLL to full speed 96MHz to be compatible with old drivers.
 */
#ifndef BSP_CLOCK_CONFIGURATION_STARTUP
    #define BSP_CLOCK_CONFIGURATION_STARTUP (BSP_CLOCK_CONFIGURATION_96MHZ)
#endif /* BSP_CLOCK_CONFIGURATION_STARTUP */

#if PE_LDD_VERSION
	#define BSP_CLOCK_SRC                   (CPU_XTAL_CLK_HZ)	
	#define BSP_CORE_CLOCK                  (CPU_CORE_CLK_HZ_CONFIG_0)
	#define BSP_SYSTEM_CLOCK                (CPU_CORE_CLK_HZ_CONFIG_0)
	#define BSP_CLOCK                       (CPU_BUS_CLK_HZ_CONFIG_0)
	#define BSP_BUS_CLOCK                   (CPU_BUS_CLK_HZ_CONFIG_0)
	#define BSP_FLEXBUS_CLOCK               (CPU_FLEXBUS_CLK_HZ_CONFIG_0)
	#define BSP_FLASH_CLOCK    				(CPU_FLASH_CLK_HZ_CONFIG_0)
#else
    /* crystal, oscillator frequency */
    #define BSP_CLOCK_SRC                   (50000000UL)
    /* reference clock frequency - must be 2-4MHz */
    #define BSP_REF_CLOCK_SRC               (2000000UL)
    
    #define BSP_CORE_DIV                    (1)
    #define BSP_BUS_DIV                     (2)
    #define BSP_FLEXBUS_DIV                 (2)
    #define BSP_FLASH_DIV                   (4)
    #define BSP_USB_DIV                     (1)
    #define BSP_USB_FRAC                    (0)    
    
    /* BSP_CLOCK_MUL from interval 24 - 55               */
    /* 48MHz = 24, 50MHz  = 25, 96MHz = 48, 100MHz = 50 */    
    #define BSP_CLOCK_MUL                   (48)    
                                                     
    #define BSP_REF_CLOCK_DIV               (BSP_CLOCK_SRC / BSP_REF_CLOCK_SRC)
    #define BSP_CLOCK                       (BSP_REF_CLOCK_SRC * BSP_CLOCK_MUL)
    #define BSP_CORE_CLOCK                  (BSP_CLOCK / BSP_CORE_DIV)          /* CORE CLK      , max 100MHz     */
    #define BSP_SYSTEM_CLOCK                (BSP_CORE_CLOCK)                    /* SYSTEM CLK    , max 100MHz     */
    #define BSP_BUS_CLOCK                   (BSP_CLOCK / BSP_BUS_DIV)           /* BUS CLK       , max 50MHz      */
    #define BSP_FLEXBUS_CLOCK               (BSP_CLOCK / BSP_FLEXBUS_DIV)       /* FLEXBUS CLK                    */
    #define BSP_FLASH_CLOCK                 (BSP_CLOCK / BSP_FLASH_DIV)         /* FLASH CLK     , max 25MHz      */
#endif

/*
** The clock tick rate
*/
#ifndef BSP_ALARM_FREQUENCY
    #define BSP_ALARM_FREQUENCY             (100)
#endif

/*
** Old clock rate definition in MS per tick, kept for compatibility
*/
#define BSP_ALARM_RESOLUTION                (1000 / BSP_ALARM_FREQUENCY)

/*
** Define the location of the hardware interrupt vector table
*/
typedef void (*vector_entry)(void);

#if MQX_ROM_VECTORS
    extern vector_entry __VECTOR_TABLE_ROM_START[]; // defined in linker command file
    #define BSP_INTERRUPT_VECTOR_TABLE              ((uint_32)__VECTOR_TABLE_ROM_START)
#else
    extern vector_entry __VECTOR_TABLE_RAM_START[]; // defined in linker command file
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
** GPIO board specifications
*/
#define BSP_LED1                    (GPIO_PORT_A | GPIO_PIN11)
#define BSP_LED2                    (GPIO_PORT_A | GPIO_PIN28)
#define BSP_LED3                    (GPIO_PORT_A | GPIO_PIN29)
#define BSP_LED4                    (GPIO_PORT_A | GPIO_PIN10)

#define BSP_SW1                     (GPIO_PORT_A | GPIO_PIN19)
#define BSP_SW2                     (GPIO_PORT_E | GPIO_PIN26)
#define BSP_ACCEL_IRQ               (GPIO_PORT_D | GPIO_PIN10) /* onboard accelerometer IRQ pin */

#define BSP_LED1_MUX_GPIO           (LWGPIO_MUX_A11_GPIO)
#define BSP_LED2_MUX_GPIO           (LWGPIO_MUX_A28_GPIO)
#define BSP_LED3_MUX_GPIO           (LWGPIO_MUX_A29_GPIO)
#define BSP_LED4_MUX_GPIO           (LWGPIO_MUX_A10_GPIO)
#define BSP_SW1_MUX_GPIO            (LWGPIO_MUX_A19_GPIO)
#define BSP_SW2_MUX_GPIO            (LWGPIO_MUX_E26_GPIO)
#define BSP_ACCEL_MUX_GPIO          (LWGPIO_MUX_D10_GPIO)

/* definitions for user applications */
#define BSP_BUTTON1                 BSP_SW1
#define BSP_BUTTON2                 BSP_SW2

#define BSP_BUTTON1_MUX_GPIO  BSP_SW1_MUX_GPIO
#define BSP_BUTTON2_MUX_GPIO  BSP_SW2_MUX_GPIO

/* Port IRQ levels */
#define BSP_PORTA_INT_LEVEL                    (3)
#define BSP_PORTB_INT_LEVEL                    (3)
#define BSP_PORTC_INT_LEVEL                    (3)
#define BSP_PORTD_INT_LEVEL                    (3)
#define BSP_PORTE_INT_LEVEL                    (3)
    
/* TWR_LCD board settings */
#define BSP_LCD_DC               (GPIO_PORT_B | GPIO_PIN17)    
#define BSP_LCD_TCHRES_X_PLUS    (GPIO_PORT_B | GPIO_PIN4) 
#define BSP_LCD_TCHRES_X_MINUS   (GPIO_PORT_B | GPIO_PIN6)
#define BSP_LCD_TCHRES_Y_PLUS    (GPIO_PORT_B | GPIO_PIN7)
#define BSP_LCD_TCHRES_Y_MINUS   (GPIO_PORT_B | GPIO_PIN5)

#define BSP_LCD_SPI_CHANNEL         "spi2:"

#define BSP_LCD_X_PLUS_ADC_CHANNEL  (ADC1_SOURCE_AD10)
#define BSP_LCD_Y_PLUS_ADC_CHANNEL  (ADC1_SOURCE_AD13)

/* ADC tchres device init struct */
#define BSP_TCHRES_ADC_DEVICE "adc1:"
#define BSP_TCHRES_X_TRIGGER ADC_PDB_TRIGGER
#define BSP_TCHRES_Y_TRIGGER ADC_PDB_TRIGGER

/*-----------------------------------------------------------------------------
**                      Ethernet Info
*/
#define BSP_ENET_DEVICE_COUNT               (BSPCFG_ENABLE_ATHEROS_WIFI?1:0)

#define ATHEROS_WIFI_DEFAULT_ENET_DEVICE    0
#define BSP_DEFAULT_ENET_OUI                { 0x00, 0x00, 0x5E, 0, 0, 0 }

/*-----------------------------------------------------------------------------
**                      DSPI
*/

#define BSP_DSPI_RX_BUFFER_SIZE          (32)
#define BSP_DSPI_TX_BUFFER_SIZE          (32)
#define BSP_DSPI_BAUDRATE                (20000000)
#define BSP_DSPI_INT_LEVEL               (4)

/*-----------------------------------------------------------------------------
**                      ESDHC
*/

#define BSP_SDCARD_ESDHC_CHANNEL         "esdhc:"
#define BSP_SDCARD_GPIO_DETECT           (GPIO_PORT_E | GPIO_PIN28)
#define BSP_SDCARD_GPIO_PROTECT          (GPIO_PORT_E | GPIO_PIN27)

/*-----------------------------------------------------------------------------
**                      I2C
*/
#define BSP_I2C_CLOCK                    (BSP_BUS_CLOCK)
#define BSP_I2C0_ADDRESS                 (0x6E)
#define BSP_I2C1_ADDRESS                 (0x6F)
#define BSP_I2C0_BAUD_RATE               (100000)
#define BSP_I2C1_BAUD_RATE               (100000)
#define BSP_I2C0_MODE                    (I2C_MODE_MASTER)
#define BSP_I2C1_MODE                    (I2C_MODE_MASTER)
#define BSP_I2C0_INT_LEVEL               (1)
#define BSP_I2C0_INT_SUBLEVEL            (0)
#define BSP_I2C1_INT_LEVEL               (1)
#define BSP_I2C1_INT_SUBLEVEL            (1)
#define BSP_I2C0_RX_BUFFER_SIZE          (64)
#define BSP_I2C0_TX_BUFFER_SIZE          (64)
#define BSP_I2C1_RX_BUFFER_SIZE          (64)
#define BSP_I2C1_TX_BUFFER_SIZE          (64)

    
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

#define BSP_ADC_CH_POT                  (ADC1_SOURCE_AD1)

#define BSP_ADC0_VECTOR_PRIORITY        (3)
#define BSP_ADC1_VECTOR_PRIORITY        (3)

#define BSP_PDB_VECTOR_PRIORITY         (3)

/*-----------------------------------------------------------------------------
**                      USB
*/
#define BSP_USB_INT_LEVEL                (4)
#define BSP_USB_TWR_SER2                 (0) //set to 1 if TWR-SER2 (2 eth) board used (only host)

/* USB host controller for K60x processors */
#define USBCFG_KHCI                       1

/*
** The Atheros Wifi settings.
*/
#ifdef ATH_SPI_DMA    
#define BSP_ATHEROS_WIFI_SPI_DEVICE                 "dspi2:"    
#else
#define BSP_ATHEROS_WIFI_SPI_DEVICE                 "ispi2:"
#endif
#define BSP_ATHEROS_WIFI_GPIO_INT_DEVICE            "gpio:input"
#define BSP_ATHEROS_WIFI_GPIO_INT_PIN               (GPIO_PORT_A|GPIO_PIN_IRQ_FALLING|GPIO_PIN27) 
#define BSP_ATHEROS_WIFI_GPIO_PWD_DEVICE			"gpio:write"
// corresponds to B23 on elevator
#define BSP_ATHEROS_WIFI_GPIO_PWD_PIN				(GPIO_PORT_E|GPIO_PIN28)
/*-----------------------------------------------------------------------------
**                  IO DEVICE DRIVERS CONFIGURATION
*/

#ifndef BSPCFG_ENABLE_IO_SUBSYSTEM
    #define BSPCFG_ENABLE_IO_SUBSYSTEM      1
#endif

/* polled TTY device (UART0) */
#ifndef BSPCFG_ENABLE_TTYA
    #define BSPCFG_ENABLE_TTYA              0
#endif

/* polled TTY device (UART1) */
#ifndef BSPCFG_ENABLE_TTYB
    #define BSPCFG_ENABLE_TTYB              0
#endif

/* polled TTY device (UART2) */
#ifndef BSPCFG_ENABLE_TTYC
    #define BSPCFG_ENABLE_TTYC  0
#endif

/* polled TTY device (UART3) */
#ifndef BSPCFG_ENABLE_TTYD
    #define BSPCFG_ENABLE_TTYD              1
#endif

/* polled TTY device (UART4) */
#ifndef BSPCFG_ENABLE_TTYE
    #define BSPCFG_ENABLE_TTYE              0
#endif

/* polled TTY device (UART5) */
#ifndef BSPCFG_ENABLE_TTYF
    #define BSPCFG_ENABLE_TTYF              1
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
    #define BSPCFG_ENABLE_ITTYC 0
#endif

/* interrupt-driven TTY device (UART3) */
#ifndef BSPCFG_ENABLE_ITTYD
    #define BSPCFG_ENABLE_ITTYD             0
#endif

/* interrupt-driven TTY device (UART4) */
#ifndef BSPCFG_ENABLE_ITTYE
    #define BSPCFG_ENABLE_ITTYE             0
#endif

/* interrupt-driven TTY device (UART5) */
#ifndef BSPCFG_ENABLE_ITTYF
    #define BSPCFG_ENABLE_ITTYF             0
#endif

/* TTY device HW signals (UART3) */
/*#ifndef BSPCFG_ENABLE_TTYD_HW_SIGNALS
    #define BSPCFG_ENABLE_TTYD_HW_SIGNALS   0
#endif
*/
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

/* TTYF and ITTYF baud rate */
#ifndef BSPCFG_SCI5_BAUD_RATE
    #define BSPCFG_SCI5_BAUD_RATE             115200
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

/* TTYF and ITTYF buffer size */
#ifndef BSPCFG_SCI5_QUEUE_SIZE
    #define BSPCFG_SCI5_QUEUE_SIZE             64
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
    #define BSPCFG_ENABLE_GPIODEV   1
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

/*----------------------------------------------------------------------
**                  DEFAULT MQX INITIALIZATION DEFINITIONS
*/

/* Defined in link.xxx */
extern uchar __KERNEL_DATA_START[];
extern uchar __KERNEL_DATA_END[];
extern const uchar __FLASHX_START_ADDR[];
extern uchar __DEFAULT_PROCESSOR_NUMBER[];
extern uchar __DEFAULT_INTERRUPT_STACK_SIZE[];

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
 *      "ittyf:"     OSJTAG-COM  interrupt mode
 *      "ttyd:"      TWR-SER     polled mode
 *      "ittyd:"     TWR-SER     interrupt mode
 *      "iodebug:"   IDE debug console
 */
#ifndef BSP_DEFAULT_IO_CHANNEL
    #if BSPCFG_ENABLE_TTYF
        #define BSP_DEFAULT_IO_CHANNEL                    "ttyd:" /* TWR-SER */
//        #define BSP_DEFAULT_IO_CHANNEL                    "ttyf:" /* OSJTAG-COM */
        #define BSP_DEFAULT_IO_CHANNEL_DEFINED
    #else
        #define BSP_DEFAULT_IO_CHANNEL                      NULL
    #endif
#endif

#ifndef BSP_DEFAULT_IO_OPEN_MODE
    #define BSP_DEFAULT_IO_OPEN_MODE                      (pointer) (IO_SERIAL_XON_XOFF | IO_SERIAL_TRANSLATION | IO_SERIAL_ECHO)
#endif

/* FLASHX flash memory pool minimum size */
#define USBCFG_KHCI             1
#if BSPCFG_ENABLE_FLASHX
    #ifndef BSPCFG_FLASHX_SIZE
        #define BSPCFG_FLASHX_SIZE      0x4000 //(FLASHX_END_ADDR - FLASHX_START_ADDR) /* defines maximum possible space */
    #endif
#else
    #undef  BSPCFG_FLASHX_SIZE
    #define BSPCFG_FLASHX_SIZE      0x0
#endif

#endif /* _twrk60n512_h_  */