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
* $FileName: coronak60n512.h$
* $Version : 3.8.60.3$
* $Date    : Nov-16-2011$
*
* Comments:
*
*   This include file is used to provide information needed by
*   an application program using the kernel running on corona.
*
*END************************************************************************/

#ifndef _coronak60n512_h_
    #define _coronak60n512_h_  1


/*----------------------------------------------------------------------
**                  HARDWARE INITIALIZATION DEFINITIONS
*/

/*
** Define the board type
*/
#define BSP_TWR_K60D100M                     1
//#define PROTO2                               1

/*
 *   COR-174 Workaround
 *     We are trying to bypass the CONMGR issues of COR-174 for Alpha, so we are doing things
 *     in the firmware to do our best to workaround the issue.  Root cause is still not known,
 *     but these debug steps could be helpful:
 *     *  Single out which DMA client could be causing the issue, by remove them 1 by 1.
 *         ** SPI Flash
 *         ** Bluetooth
 *         ** Accelerometer
 *     *  Hook up the Atheros UART, so we can see if it is printing anything out when the problem occurs.
 *     *  See if the problem is happening because we are sending something bogus to Atheros.
 *     *  Ben M. said he hasn't seen any DMA hardware errors present when the problem is reproduced.
 */
#define COR_174_WORKAROUND 1

/*
 *   Ernie
 *   LLS support when doing WIFI stuff.
 *   This does not fully work, but WIFI_INT on PTD2 was confirmed to be able to actually wake us up.
 */
#define LLS_DURING_WIFI_USAGE       (0)

/*
** The revision of the silicon.
** This should be specified in the user_config.h,
** otherwise the following default setting is applied.
*/

/*
** PROCESSOR MEMORY BOUNDS
*/
#define BSP_PERIPH_BASE                     (CORTEX_PERIPH_BASE)
extern uchar __IPSBAR[];

#define BSP_INTERNAL_FLASH_BASE    0x00000000
#define BSP_INTERNAL_FLASH_SIZE    0x00080000
#define BSP_INTERNAL_FLASH_SECTOR_SIZE  0x800

/*
 * The clock configuration settings
 * Remove old definitions of "BSP_CLOCKS" in drivers and replace
 * by runtime clock checking. Its assumed that BSP_CLOCK_CONFIGURATION_1
 * sets PLL to full speed 96MHz to be compatible with old drivers.
 */
#ifndef BSP_CLOCK_CONFIGURATION_STARTUP
    #define BSP_CLOCK_CONFIGURATION_STARTUP (BSP_CLOCK_CONFIGURATION_24MHZ)
#endif /* BSP_CLOCK_CONFIGURATION_STARTUP */

#if PE_LDD_VERSION
	#define BSP_CLOCK_SRC           (CPU_XTAL_CLK_HZ)	
	#define BSP_CORE_CLOCK          (CPU_CORE_CLK_HZ_CONFIG_0)
	#define BSP_SYSTEM_CLOCK        (CPU_CORE_CLK_HZ_CONFIG_0)
	#define BSP_CLOCK               (CPU_BUS_CLK_HZ_CONFIG_0)
	#define BSP_BUS_CLOCK           (CPU_BUS_CLK_HZ_CONFIG_0)
	#define BSP_FLASH_CLOCK    		(CPU_FLASH_CLK_HZ_CONFIG_0)
#else
    /* crystal, oscillator frequency */
    #define BSP_CLOCK_SRC                   (32768UL)

/*
 *   Whistle uses FLL, and therefore does NOT use the PLL.
 *   The main control register is MCG_C4, in particular the DMX32 table.
 *   NOTE:  The BSP defines below are for CONFIG0 within MQX's frequency config table and 
 *          the VLPR defines are for CONFIG2. 
 */

#ifdef USB_ENABLED
    /*
     *    EA:  When USB is enabled, we need 48MHz, ain't no way around it, other than cranking it up dynamically upon connection. 
     *         NOTE:  USB_ENABLED is obviously going to consume more idle current.
     *         NOTE:  Use 96MHz, then divide bus clock and USB clock down.
     */
    #define WHISTLE_CONFIG0_96MHz             (1)
#else
    /*
     *   For most cases, define the desired FLL frequency here.
     *      NOTE:  Define only one of the "WHISTLE_CONFIG0_zzMHz".
     */
    #define WHISTLE_CONFIG0_24MHz             (1)
#endif

/*
 *   Static clock configurations for CONFIG0 
 */
#ifdef WHISTLE_CONFIG0_96MHz
    #define WHISTLE_CONFIG0_CLOCK                       96000000
    #define WHISTLE_CONFIG0_DRST_DRS                    0x3

    #define WHISTLE_CONFIG0_CORE_DIV                    (0x0)
    #define WHISTLE_CONFIG0_BUS_DIV                     (0x1)
    #define WHISTLE_CONFIG0_FLASH_DIV                   (0x3)
    #define WHISTLE_CONFIG0_USB_DIV                     (1)
    #define WHISTLE_CONFIG0_USB_FRAC                    (0)
#endif

#ifdef WHISTLE_CONFIG0_72MHz
    #define WHISTLE_CONFIG0_CLOCK                       72000000
    #define WHISTLE_CONFIG0_DRST_DRS                    0x2

    #define WHISTLE_CONFIG0_CORE_DIV                    (0)
    #define WHISTLE_CONFIG0_BUS_DIV                     (0)
    #define WHISTLE_CONFIG0_FLASH_DIV                   (3)
    #define WHISTLE_CONFIG0_USB_DIV                     (2)  // WARNING - USB (and 72MHz in general) has not been tested !
    #define WHISTLE_CONFIG0_USB_FRAC                    (1)
#endif

#ifdef WHISTLE_CONFIG0_48MHz
    #define WHISTLE_CONFIG0_CLOCK                       48000000
    #define WHISTLE_CONFIG0_DRST_DRS                    0x1
    #define WHISTLE_CONFIG0_CORE_DIV                    (0)
    #define WHISTLE_CONFIG0_BUS_DIV                     (0)
    #define WHISTLE_CONFIG0_FLASH_DIV                   (3)
    #define WHISTLE_CONFIG0_USB_DIV                     (0)
    #define WHISTLE_CONFIG0_USB_FRAC                    (0)
#endif

#ifdef WHISTLE_CONFIG0_24MHz
    #define WHISTLE_CONFIG0_CLOCK                       24000000
    #define WHISTLE_CONFIG0_DRST_DRS                    0x0

    #define WHISTLE_CONFIG0_CORE_DIV                    (0)    // OUTDIV1  --  EA:  Setting to 24MHz for Alpha idle current effort.

    // BM: 12MHz bus clock causes I2C accesses to MFI check to hang during BT pairing. Changed bus clock to 8MHz
    #define WHISTLE_CONFIG0_BUS_DIV                     (2)    // OUTDIV2  --  EA:  Setting to 8MHz for Alpha idle current effort.

    #define WHISTLE_CONFIG0_FLASH_DIV                   (3)    // OUTDIV4  --  EA:  Setting to 6MHz for Alpha idle current effort.
    #define WHISTLE_CONFIG0_USB_DIV                     (0)    // NOTE:  USB cannot work at 24MHz.
    #define WHISTLE_CONFIG0_USB_FRAC                    (0)
#endif

/*
 *   Static clock configurations for CONFIG1
 */
    #define WHISTLE_CONFIG1_CLOCK               96000000
    #define WHISTLE_CONFIG1_DRST_DRS            0x3
    #define WHISTLE_CONFIG1_CORE_DIV            (0x0)
    #define WHISTLE_CONFIG1_BUS_DIV             (0x1)
    #define WHISTLE_CONFIG1_FLASH_DIV           (0x3)
    #define WHISTLE_CONFIG1_USB_DIV             (1)
    #define WHISTLE_CONFIG1_USB_FRAC            (0)


/*
 *   Whistle does NOT use FLEXBUS, so just set it to max divider, doesn't matter either way.
 */
	#define BSP_FLEXBUS_DIV                 (0x3) // OUTDIV3  --  EA:  Not used for Whistle.

/*
 * VLPR Mode Clocks:
 *   Core:    4MHz
 *   Bus:     2MHz
 *   Flash:   800KHz
 *   Flexbus: 1MHz 
 *   
 */ 
	#define VLPR_CLOCK						(4000000)
	#define VLPR_CORE_DIV					(0)
	#define VLPR_BUS_DIV					(1)
    // NOTE: Max. Flash Clock in VLPR mode is 800KHz
	#define VLPR_FLASH_DIV					(4)
	#define VLPR_FLEXBUS_DIV				(3)

/*
 *    Ernie:
 *      The dividers above are K60 dividers and not mathematically correct.
 *      Here are the defines that are actually mathematically correct.
 *      NOTE:  Do not modify these values!  Modify above K60 dividers instead.
 */
    #define WHISTLE_CONFIG0_CORE_REAL_DIV          (WHISTLE_CONFIG0_CORE_DIV    + 1)  // OUTDIV1 translated to a real divisor.
    #define WHISTLE_CONFIG0_BUS_REAL_DIV           (WHISTLE_CONFIG0_BUS_DIV     + 1)  // OUTDIV2 translated to a real divisor.
    #define WHISTLE_CONFIG0_FLASH_REAL_DIV         (WHISTLE_CONFIG0_FLASH_DIV   + 1)  // OUTDIV3 translated to a real divisor.
    #define WHISTLE_CONFIG0_FLEXBUS_REAL_DIV       (BSP_FLEXBUS_DIV + 1)  // OUTDIV4 translated to a real divisor.
    #define WHISTLE_CONFIG0_USB_REAL_DIV           (WHISTLE_CONFIG0_USB_DIV + 1)
    #define WHISTLE_CONFIG1_CORE_REAL_DIV          (WHISTLE_CONFIG1_CORE_DIV    + 1)  // OUTDIV1 translated to a real divisor.
    #define WHISTLE_CONFIG1_BUS_REAL_DIV           (WHISTLE_CONFIG1_BUS_DIV     + 1)  // OUTDIV2 translated to a real divisor.
    #define WHISTLE_CONFIG1_FLASH_REAL_DIV         (WHISTLE_CONFIG1_FLASH_DIV   + 1)  // OUTDIV3 translated to a real divisor.
    #define WHISTLE_CONFIG1_FLEXBUS_REAL_DIV       (BSP_FLEXBUS_DIV + 1)  // OUTDIV4 translated to a real divisor.
    #define WHISTLE_CONFIG1_USB_REAL_DIV           (WHISTLE_CONFIG1_USB_DIV + 1)
    #define VLPR_CORE_REAL_DIV                     (VLPR_CORE_DIV + 1)
    #define VLPR_BUS_REAL_DIV                      (VLPR_BUS_DIV + 1)
    #define VLPR_FLASH_REAL_DIV                    (VLPR_FLASH_DIV + 1)
    #define VLPR_FLEXBUS_REAL_DIV                  (VLPR_FLEXBUS_DIV + 1)

    /*
     *   All the clocks are functions of WHISTLE_CONFIGx_CLOCK or VLPR_CLOCK and their dividers.
     */
    #define WHISTLE_CONFIG0_CORE_CLOCK             (WHISTLE_CONFIG0_CLOCK / WHISTLE_CONFIG0_CORE_REAL_DIV)
    #define WHISTLE_CONFIG0_BUS_CLOCK              (WHISTLE_CONFIG0_CLOCK / WHISTLE_CONFIG0_BUS_REAL_DIV)
    #define WHISTLE_CONFIG0_FLASH_CLOCK            (WHISTLE_CONFIG0_CLOCK / WHISTLE_CONFIG0_FLASH_REAL_DIV)
    #define WHISTLE_CONFIG0_FLEXBUS_CLOCK          (WHISTLE_CONFIG0_CLOCK / WHISTLE_CONFIG0_FLEXBUS_REAL_DIV)
    #define WHISTLE_CONFIG0_USB_CLOCK              (WHISTLE_CONFIG0_CLOCK / WHISTLE_CONFIG0_USB_REAL_DIV)
    #define WHISTLE_CONFIG1_CORE_CLOCK             (WHISTLE_CONFIG1_CLOCK / WHISTLE_CONFIG1_CORE_REAL_DIV)
    #define WHISTLE_CONFIG1_BUS_CLOCK              (WHISTLE_CONFIG1_CLOCK / WHISTLE_CONFIG1_BUS_REAL_DIV)
    #define WHISTLE_CONFIG1_FLASH_CLOCK            (WHISTLE_CONFIG1_CLOCK / WHISTLE_CONFIG1_FLASH_REAL_DIV)
    #define WHISTLE_CONFIG1_FLEXBUS_CLOCK          (WHISTLE_CONFIG1_CLOCK / WHISTLE_CONFIG1_FLEXBUS_REAL_DIV)
    #define WHISTLE_CONFIG1_USB_CLOCK              (WHISTLE_CONFIG1_CLOCK / WHISTLE_CONFIG1_USB_REAL_DIV)

    #define VLPR_CORE_CLOCK                        (VLPR_CLOCK / VLPR_CORE_REAL_DIV)
	#define VLPR_BUS_CLOCK                         (VLPR_CLOCK / VLPR_BUS_REAL_DIV)
    #define VLPR_FLASH_CLOCK                       (VLPR_CLOCK / VLPR_FLASH_REAL_DIV)
	#define VLPR_FLEXBUS_CLOCK                     (VLPR_CLOCK / VLPR_FLEXBUS_REAL_DIV)

    /*
     * Default BSP Clock Configuration
     */
    #define BSP_BUS_CLOCK       WHISTLE_CONFIG0_BUS_CLOCK

    /*
     *   ATHEROS CLOCK RATE
     */
    #define AR4XXX_NORMAL_CLOCK_RATE        (WHISTLE_CONFIG0_BUS_CLOCK / 2)  // Best SPI can do is system clock divided by 2.

    /*
     *   Speed for Spansion/WSON SPI Flash
     *   (Spansion capable of 50MHz)
     *   
     *   It is possible that 1MHz made COR-174 worse, since SPI Flash will hold
     *     the DMA bus captive while it completes its reads and writes.
     *     See "COR_174_WORKAROUND" flag for the related bug/flag/todo's.
     */
    #define SPANSION_RUNM_BAUD_RATE              (4000000) // Best SPI can do is system clock divided by 2.
    #define SPANSION_VLPR_BAUD_RATE              (VLPR_BUS_CLOCK / 2) 

    /*
     *   ST Micro LIS3DH Accelerometer speed (10MHz max for this part)
     */
    #define ACCEL_BAUD_RATE              (VLPR_BUS_CLOCK / 2)  // Best SPI can do is system clock divided by 2.

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
#ifdef PROTO2
#define BSP_GPIO_BUTTON_PIN                    (GPIO_PORT_A | GPIO_PIN29)
#define BSP_GPIO_BUTTON_MUX_GPIO               (LWGPIO_MUX_A29_GPIO)
#else
#define BSP_GPIO_BUTTON_PIN                    (GPIO_PORT_E | GPIO_PIN1)
#define BSP_GPIO_BUTTON_MUX_GPIO               (LWGPIO_MUX_E1_GPIO)        
#endif
    
#define BSP_GPIO_UID_ENABLE_PIN                (GPIO_PORT_A | GPIO_PIN12)
#define BSP_GPIO_UID_ENABLE_MUX_GPIO           (LWGPIO_MUX_A12_GPIO)
#define BSP_GPIO_BT_PWDN_B_PIN                 (GPIO_PORT_B | GPIO_PIN0)
#define BSP_GPIO_BT_PWDN_B_MUX_GPIO            (LWGPIO_MUX_B0_GPIO)
#define BSP_GPIO_MFI_RST_B_PIN                 (GPIO_PORT_B | GPIO_PIN1)
#define BSP_GPIO_MFI_RST_B_MUX_GPIO            (LWGPIO_MUX_B1_GPIO)
#define BSP_GPIO_RF_SW_V1_PIN                  (GPIO_PORT_B | GPIO_PIN2)
#define BSP_GPIO_RF_SW_V1_MUX_GPIO             (LWGPIO_MUX_B2_GPIO)
#define BSP_GPIO_RF_SW_V2_PIN                  (GPIO_PORT_B | GPIO_PIN3)
#define BSP_GPIO_RF_SW_V2_MUX_GPIO             (LWGPIO_MUX_B3_GPIO)
#define BSP_GPIO_DATA_SPI_WP_B_PIN             (GPIO_PORT_B | GPIO_PIN18)
#define BSP_GPIO_DATA_SPI_WP_B_MUX_GPIO        (LWGPIO_MUX_B18_GPIO)
#define BSP_GPIO_DATA_SPI_HOLD_B_PIN           (GPIO_PORT_B | GPIO_PIN19)
#define BSP_GPIO_DATA_SPI_HOLD_B_MUX_GPIO      (LWGPIO_MUX_B19_GPIO)
#ifdef PROTO2
#define BSP_GPIO_ACC_INT1_PIN                  (GPIO_PORT_C | GPIO_PIN12)
#define BSP_GPIO_ACC_INT1_MUX_GPIO             (LWGPIO_MUX_C12_GPIO)
#define BSP_GPIO_ACC_INT1_DMA_SOURCE           (51)
#else
#define BSP_GPIO_ACC_INT1_PIN                  (GPIO_PORT_A | GPIO_PIN13)
#define BSP_GPIO_ACC_INT1_MUX_GPIO             (LWGPIO_MUX_A13_GPIO)
#define BSP_GPIO_ACC_INT1_DMA_SOURCE           (49)
#endif
#ifdef PROTO2
#define BSP_GPIO_ACC_INT2_PIN                  (GPIO_PORT_C | GPIO_PIN13)
#define BSP_GPIO_ACC_INT2_MUX_GPIO             (LWGPIO_MUX_C13_GPIO)
#else
#define BSP_GPIO_ACC_INT2_PIN                  (GPIO_PORT_C | GPIO_PIN3)
#define BSP_GPIO_ACC_INT2_MUX_GPIO             (LWGPIO_MUX_C3_GPIO)
#endif
#ifdef PROTO2
#define BSP_GPIO_CHG_B_PIN                     (GPIO_PORT_C | GPIO_PIN14)
#define BSP_GPIO_CHG_B_MUX_GPIO                (LWGPIO_MUX_C14_GPIO)
#else
#define BSP_GPIO_CHG_B_PIN                     (GPIO_PORT_D | GPIO_PIN0)
#define BSP_GPIO_CHG_B_MUX_GPIO                (LWGPIO_MUX_D0_GPIO)
#endif    
#ifdef PROTO2
#define BSP_GPIO_PGOOD_B_PIN                   (GPIO_PORT_C | GPIO_PIN15)
#define BSP_GPIO_PGOOD_B_MUX_GPIO              (LWGPIO_MUX_C15_GPIO)
#else
#define BSP_GPIO_PGOOD_B_PIN                   (GPIO_PORT_E | GPIO_PIN2)
#define BSP_GPIO_PGOOD_B_MUX_GPIO              (LWGPIO_MUX_E2_GPIO)
#endif
#define BSP_GPIO_MCU_26MHz_EN_PIN              (GPIO_PORT_C | GPIO_PIN19)
#define BSP_GPIO_MCU_26MHz_EN_MUX_GPIO         (LWGPIO_MUX_C19_GPIO)
#define BSP_GPIO_LED0_PIN                      (GPIO_PORT_D | GPIO_PIN12)
#define BSP_GPIO_LED0_MUX_GPIO                 (LWGPIO_MUX_D12_GPIO)
#define BSP_GPIO_LED1_PIN                      (GPIO_PORT_D | GPIO_PIN13)
#define BSP_GPIO_LED1_MUX_GPIO                 (LWGPIO_MUX_D13_GPIO)
#define BSP_GPIO_LED2_PIN                      (GPIO_PORT_D | GPIO_PIN14)
#define BSP_GPIO_LED2_MUX_GPIO                 (LWGPIO_MUX_D14_GPIO)
#define BSP_GPIO_LED3_PIN                      (GPIO_PORT_D | GPIO_PIN15)
#define BSP_GPIO_LED3_MUX_GPIO                 (LWGPIO_MUX_D15_GPIO)
#define BSP_GPIO_WIFI_PWDN_B_PIN               (GPIO_PORT_E | GPIO_PIN4)
#define BSP_GPIO_WIFI_PWDN_B_MUX_GPIO          (LWGPIO_MUX_E4_GPIO)
    
#if LLS_DURING_WIFI_USAGE  // Rev B Interrupt Signal on LLWU pin.
    #define BSP_GPIO_WIFI_INT_PIN                  (GPIO_PORT_D | GPIO_PIN2) 
    #define BSP_GPIO_WIFI_INT_MUX_GPIO             (LWGPIO_MUX_D2_GPIO) 
#else
    #define BSP_GPIO_WIFI_INT_PIN                  (GPIO_PORT_E | GPIO_PIN5) 
    #define BSP_GPIO_WIFI_INT_MUX_GPIO             (LWGPIO_MUX_E5_GPIO) 
#endif
    
#define BSP_GPIO_BT_32KHz_EN_PIN               (GPIO_PORT_E | GPIO_PIN6)
#define BSP_GPIO_BT_32KHz_EN_MUX_GPIO          (LWGPIO_MUX_E6_GPIO)

/* Port IRQ levels */
#define BSP_PORTA_INT_LEVEL                    (3)
#define BSP_PORTB_INT_LEVEL                    (3)
#define BSP_PORTC_INT_LEVEL                    (3)
#define BSP_PORTD_INT_LEVEL                    (3)
#define BSP_PORTE_INT_LEVEL                    (3)
    
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
**                      I2C
*/
#define BSP_I2C_CLOCK                    (BSP_BUS_CLOCK)
#define BSP_I2C0_ADDRESS                 (0x00) // I2C0 not used
#define BSP_I2C1_ADDRESS                 (0x00) // Rohm, MFi
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

/* USB host controller for K60x processors */
#define USBCFG_KHCI                       1

/*
** The Atheros Wifi settings.
*/
#ifdef ATH_SPI_DMA    
#define BSP_ATHEROS_WIFI_SPI_DEVICE                 "dspi0:"    
#else
#define BSP_ATHEROS_WIFI_SPI_DEVICE                 "ispi0:"
#endif
#if NOT_WHISTLE
#define BSP_ATHEROS_WIFI_GPIO_INT_DEVICE            "gpio:input"
#define BSP_ATHEROS_WIFI_GPIO_INT_PIN               (BSP_GPIO_WIFI_INT_PIN|GPIO_PIN_IRQ_FALLING) 
#define BSP_ATHEROS_WIFI_GPIO_PWD_DEVICE            "gpio:write"
#define BSP_ATHEROS_WIFI_GPIO_PWD_PIN               (BSP_GPIO_WIFI_PWDN_B_PIN)
#else
#define BSP_ATHEROS_WIFI_LWGPIO_INT_PIN             (BSP_GPIO_WIFI_INT_PIN)
#define BSP_ATHEROS_WIFI_LWGPIO_INT_MUX             (BSP_GPIO_WIFI_INT_MUX_GPIO)
#define BSP_ATHEROS_WIFI_LWGPIO_PWD_PIN             (BSP_GPIO_WIFI_PWDN_B_PIN)
#define BSP_ATHEROS_WIFI_LWGPIO_PWD_MUX             (BSP_GPIO_WIFI_PWDN_B_MUX_GPIO)
#endif /* NOT_WHISTLE */
    
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

/* TTY device HW signals (UART0) */
#ifndef BSPCFG_ENABLE_TTYA_HW_SIGNALS
    #define BSPCFG_ENABLE_TTYA_HW_SIGNALS   1
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
    #define BSPCFG_ENABLE_I2C0                  0
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
    #define BSPCFG_ENABLE_GPIODEV   0 /* We use lwgpio, so can't use this */
#endif

/* RTC device */
#ifndef BSPCFG_ENABLE_RTCDEV
    #define BSPCFG_ENABLE_RTCDEV                1
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

#ifndef BSPCFG_ENABLE_IODEBUG
    #define BSPCFG_ENABLE_IODEBUG               1
#endif
/*----------------------------------------------------------------------
**                  DEFAULT MQX INITIALIZATION DEFINITIONS
*/

/* Defined in link.xxx */
extern uchar __KERNEL_DATA_START[];
extern uchar __KERNEL_DATA_END[];
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
 *      "ttyd:"      Debug UART     polled mode
 *      "ittyd:"     Debug UART     interrupt mode
 *      "iodebug:"   IDE debug console
 */
/*
 * If USB is disabled then console and logging all goes to the serial port.
 * If USB is enabled then console and logging goes to JTAG until USB is plugged in,
 *   then it is switched to USB. 
 */
#if 1 // For WHISTLE:  Always use the FTDI UART (ittyf) rather than JTAG (iodebug).
    #ifndef BSP_DEFAULT_IO_CHANNEL
        #if BSPCFG_ENABLE_ITTYF
            #define BSP_DEFAULT_IO_CHANNEL                    "ittyf:"
            #define BSP_DEFAULT_IO_CHANNEL_DEFINED
        #elif BSPCFG_ENABLE_TTYF
            #define BSP_DEFAULT_IO_CHANNEL                     "ttyf:"  // UTF_AGENT needs polled driver.
            #define BSP_DEFAULT_IO_CHANNEL_DEFINED
        #else
            #define BSP_DEFAULT_IO_CHANNEL                      NULL
        #endif
    #endif
#else
    #define BSP_DEFAULT_IO_CHANNEL                    "iodebug:"
    #define BSP_DEFAULT_IO_CHANNEL_DEFINED
#endif
    
#ifndef BSP_DEFAULT_IO_OPEN_MODE
    #define BSP_DEFAULT_IO_OPEN_MODE                      (pointer) (IO_SERIAL_XON_XOFF | IO_SERIAL_TRANSLATION | IO_SERIAL_ECHO)
#endif

#endif /* _coronak60n512_h_  */
