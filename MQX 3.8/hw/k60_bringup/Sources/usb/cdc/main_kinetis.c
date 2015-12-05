/******************************************************************************
 *
 * Freescale Semiconductor Inc.
 * (c) Copyright 2004-2010 Freescale Semiconductor, Inc.
 * ALL RIGHTS RESERVED.
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
 **************************************************************************//*!
 *
 * @file main_kinetis.c
 *
 * @author
 *
 * @version
 *
 * @date
 *
 * @brief   This software is the USB driver stack for S08 family
 *****************************************************************************/
#include "types.h"
#include "derivative.h" /* include peripheral declarations */
#include "user_config.h"
#include "RealTimerCounter.h"
#include "Wdt_kinetis.h"
#include "hidef.h"
#ifndef _SERIAL_AGENT_
#include "usb_dci_kinetis.h"
#include "usb_dciapi.h"
#endif

#include "corona_console.h"
extern cc_handle_t g_cc_handle;
extern uint_8 USB_Class_CDC_Send_Data (
	    uint_8 controller_ID,   /* [IN] Controller ID */
	    uint_8 ep_num,          /* [IN] Endpoint Number */
	    uint_8_ptr app_buff,    /* Pointer to Application Buffer */
	    USB_PACKET_SIZE size    /* Size of Application Buffer */
	);

#define PE_32KHz // Try 32KHz external ref clock from Silego.

/*****************************************************************************
 * Global Functions Prototypes
 *****************************************************************************/
static void Init_32k_clk();

#if MAX_TIMER_OBJECTS
	extern uint_8 TimerQInitialize(uint_8 ControllerId);
#endif
extern void TestApp_Init(void);
extern void TestApp_Task(void);
#ifdef MCU_MK70F12
extern void sci2_init();
#endif /* MCU_MK70F12 */
#if (defined MCU_MK20D7) || (defined MCU_MK40D7)
    #define MCGOUTCLK_72_MHZ
#endif

#if (defined MCU_MK60N512VMD100) || (defined MCU_MK53N512CMD100)
	#define BSP_CLOCK_SRC   (50000000ul)    /* crystal, oscillator freq. */
#elif (defined MCU_MK40N512VMD100)
   #if (defined KWIKSTIK)
    #define BSP_CLOCK_SRC   (4000000ul)     /* crystal, oscillator freq. */
   #else
	#define BSP_CLOCK_SRC   (8000000ul)     /* crystal, oscillator freq. */
   #endif
#else
    #define BSP_CLOCK_SRC   (8000000ul)     /* crystal, oscillator freq. */
#endif
#define BSP_REF_CLOCK_SRC   (2000000ul)     /* must be 2-4MHz */


#ifndef MCGOUTCLK_72_MHZ
#define PLL_48              (1)
#define PLL_96              (0)
#else
#define PLL_48              (0)
#define PLL_96              (0)
#endif

#ifdef MCGOUTCLK_72_MHZ
	#define BSP_CORE_DIV    (1)
	#define BSP_BUS_DIV     (2)
	#define BSP_FLEXBUS_DIV (2)
	#define BSP_FLASH_DIV   (3)

	// BSP_CLOCK_MUL from interval 24 - 55
	#define BSP_CLOCK_MUL   (36)    // 72MHz
#elif PLL_48
	#define BSP_CORE_DIV    (1)
	#define BSP_BUS_DIV     (1)
	#define BSP_FLEXBUS_DIV (1)
	#define BSP_FLASH_DIV   (2)

	// BSP_CLOCK_MUL from interval 24 - 55
	#define BSP_CLOCK_MUL   (24)    // 48MHz
#elif PLL_96
    #define BSP_CORE_DIV    (1)
    #define BSP_BUS_DIV     (2)
    #define BSP_FLEXBUS_DIV (2)
    #define BSP_FLASH_DIV   (4)
    // BSP_CLOCK_MUL from interval 24 - 55
    #define BSP_CLOCK_MUL   (48)    // 96MHz
#endif

#define BSP_REF_CLOCK_DIV   (BSP_CLOCK_SRC / BSP_REF_CLOCK_SRC)

#define BSP_CLOCK           (BSP_REF_CLOCK_SRC * BSP_CLOCK_MUL)
#define BSP_CORE_CLOCK      (BSP_CLOCK / BSP_CORE_DIV)      /* CORE CLK, max 100MHz */
#define BSP_SYSTEM_CLOCK    (BSP_CORE_CLOCK)                /* SYSTEM CLK, max 100MHz */
#define BSP_BUS_CLOCK       (BSP_CLOCK / BSP_BUS_DIV)       /* max 50MHz */
#define BSP_FLEXBUS_CLOCK   (BSP_CLOCK / BSP_FLEXBUS_DIV)
#define BSP_FLASH_CLOCK     (BSP_CLOCK / BSP_FLASH_DIV)     /* max 25MHz */

#ifdef MCU_MK70F12
enum usbhs_clock
{
  MCGPLL0,
  MCGPLL1,
  MCGFLL,
  PLL1,
  CLKIN
};

// Constants for use in pll_init
#define NO_OSCINIT          0
#define OSCINIT             1

#define OSC_0               0
#define OSC_1               1

#define LOW_POWER           0
#define HIGH_GAIN           1

#define CANNED_OSC          0
#define CRYSTAL             1

#define PLL_0               0
#define PLL_1               1

#define PLL_ONLY            0
#define MCGOUT              1

#define BLPI                1
#define FBI                 2
#define FEI                 3
#define FEE                 4
#define FBE                 5
#define BLPE                6
#define PBE                 7
#define PEE                 8

// IRC defines
#define SLOW_IRC            0
#define FAST_IRC            1

/*
 * Input Clock Info
 */
#define CLK0_FREQ_HZ        50000000
#define CLK0_TYPE           CANNED_OSC

#define CLK1_FREQ_HZ        12000000
#define CLK1_TYPE           CRYSTAL

/* Select Clock source */
/* USBHS Fractional Divider value for 120MHz input */
/* USBHS Clock = PLL0 x (USBHSFRAC+1) / (USBHSDIV+1)       */
#define USBHS_FRAC          0
#define USBHS_DIV           SIM_CLKDIV2_USBHSDIV(1)
#define USBHS_CLOCK         MCGPLL0


/* USB Fractional Divider value for 120MHz input */
/** USB Clock = PLL0 x (FRAC +1) / (DIV+1)       */
/** USB Clock = 120MHz x (1+1) / (4+1) = 48 MHz    */
#define USB_FRAC            SIM_CLKDIV2_USBFSFRAC_MASK
#define USB_DIV             SIM_CLKDIV2_USBFSDIV(4)


/* Select Clock source */
#define USB_CLOCK           MCGPLL0
//#define USB_CLOCK         MCGPLL1
//#define USB_CLOCK         MCGFLL
//#define USB_CLOCK         PLL1
//#define USB_CLOCK         CLKIN

/* The expected PLL output frequency is:
 * PLL out = (((CLKIN/PRDIV) x VDIV) / 2)
 * where the CLKIN can be either CLK0_FREQ_HZ or CLK1_FREQ_HZ.
 *
 * For more info on PLL initialization refer to the mcg driver files.
 */

#define PLL0_PRDIV          5
#define PLL0_VDIV           24

#define PLL1_PRDIV          5
#define PLL1_VDIV           30
#endif

/*****************************************************************************
 * Local Variables
 *****************************************************************************/
#ifdef MCU_MK70F12
	int mcg_clk_hz;
	int mcg_clk_khz;
	int core_clk_khz;
	int periph_clk_khz;
	int pll_0_clk_khz;
	int pll_1_clk_khz;
#endif

/*****************************************************************************
 * Local Functions Prototypes
 *****************************************************************************/
static void SYS_Init(void);
static void GPIO_Init();
static void USB_Init(uint_8 controller_ID);
static int pll_init(
	#ifdef MCU_MK70F12
		unsigned char init_osc,
		unsigned char osc_select,
		int crystal_val,
		unsigned char hgo_val,
		unsigned char erefs_val,
		unsigned char pll_select,
		signed char prdiv_val,
		signed char vdiv_val,
		unsigned char mcgout_select
	#endif
		);
#ifdef MCU_MK70F12
	void trace_clk_init(void);
	void fb_clk_init(void);
#endif

#if WATCHDOG_DISABLE
static void wdog_disable(void);
#endif /* WATCHDOG_DISABLE */

/****************************************************************************
 * Global Variables
 ****************************************************************************/
#if ((defined __CWCC__) || (defined __IAR_SYSTEMS_ICC__) || (defined __CC_ARM)|| (defined __arm__))
//	extern uint_32 ___VECTOR_RAM[];      // ERNIE_HACK      // Get vector table that was copied to RAM
#elif defined(__GNUC__)
	extern uint_32 __cs3_interrupt_vector[];
#endif
volatile uint_8 kbi_stat;	   /* Status of the Key Pressed */
#ifdef USE_FEEDBACK_ENDPOINT
	extern uint_32 feedback_data;
#endif

extern cc_handle_t g_cc_handle;


/*****************************************************************************
 * Global Functions
 *****************************************************************************/
/******************************************************************************
 * @name        main
 *
 * @brief       This routine is the starting point of the application
 *
 * @param       None
 *
 * @return      None
 *
 *****************************************************************************
 * This function initializes the system, enables the interrupts and calls the
 * application
 *****************************************************************************/
#ifdef __GNUC__
 int main(void)
#else
 void main_k60_usb(void)
#endif
{

	//char *pIntro = "\n\r\n*** Corona Console (ChEOS)***\n\r\n]";
	//uint32_t usb_retries = 5000;
	//uint8_t status;

	/* Initialize the system */
    //SYS_Init();

#ifndef _SERIAL_AGENT_
    /* Initialize USB module */
#if HIGH_SPEED_DEVICE
    USB_Init(ULPI);
#else
    USB_Init(MAX3353);
#endif
#else
    SIM_SOPT2  |= (SIM_SOPT2_USBSRC_MASK | SIM_SOPT2_PLLFLLSEL_MASK);
#endif
    /* Initialize GPIO pins */
#ifdef ERNIE_HACK_SHOULD_NOT_NEED_THESE_GPIOS
    GPIO_Init();
#endif

#if MAX_TIMER_OBJECTS
    (void)TimerQInitialize(0);
#endif
    
    /* Enable USB Interrupts */
#ifdef CORONA_PROTO1_INTERRUPT_HACK
    NVICICER1 |= (1<<21);    /* Clear any pending interrupts on USB */
    NVICISER1 |= (1<<21);    /* Enable interrupts from USB module */
#else
    NVICICPR2 = (1 << 9);   /* Clear any pending interrupts on USB */
    NVICISER2 = (1 << 9);   /* Enable interrupts from USB module */
    
    /*
     *   Ernie:  The following is needed so button freaking works!
     */
    NVICISER2 |= NVIC_ISER_SETENA(0x08000000);    
#endif
    
    cc_init(&g_cc_handle, NULL, CC_TYPE_USB);

    /* Initialize the USB Test Application */
    TestApp_Init();

#if 0
	do
	{
		status = USB_Class_CDC_Send_Data (
				0/*CONTROLLER_ID*/,   /* [IN] Controller ID */
				1/*DIC_BULK_IN_ENDPOINT*/,          /* [IN] Endpoint Number */
				pIntro,    /* Pointer to Application Buffer */
				strlen(pIntro)    /* Size of Application Buffer */);
	} while( (status == USBERR_DEVICE_BUSY) && (usb_retries-- != 0) ); // 0xC1 is USBERR_DEVICE_BUSY

	if( status != USB_OK )
	{
		/* Send Data Error Handling Code goes here */
		// printf("USB Intro CDC Send Error!\n");
	}
#endif
	
    while(TRUE)
    {
    	//Watchdog_Reset();  // no need, watchdog is disabled.
        
    	/* Call the application task */
    	TestApp_Task();
    }

#ifdef __GNUC__
    return 0;
#endif
}

/*****************************************************************************
 * Local Functions
 *****************************************************************************/
/*****************************************************************************
 *
 *    @name     GPIO_Init
 *
 *    @brief    This function Initializes LED GPIO
 *
 *    @param    None
 *
 *    @return   None
 *
 ****************************************************************************
 * Intializes GPIO pins
 ***************************************************************************/
static void GPIO_Init()
{
#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
	#if(!(defined MCU_MK20D5)) || (!(defined _USB_BATT_CHG_APP_H_))
		/* Enable clock gating to PORTC */
		SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;

		/* LEDs settings */
		PORTC_PCR9|= PORT_PCR_SRE_MASK    /* Slow slew rate */
				  |  PORT_PCR_ODE_MASK    /* Open Drain Enable */
				  |  PORT_PCR_DSE_MASK    /* High drive strength */
				  ;
		PORTC_PCR9 = PORT_PCR_MUX(1);
		PORTC_PCR10|= PORT_PCR_SRE_MASK    /* Slow slew rate */
				  |  PORT_PCR_ODE_MASK    /* Open Drain Enable */
				  |  PORT_PCR_DSE_MASK    /* High drive strength */
				  ;
		PORTC_PCR10 = PORT_PCR_MUX(1);
		GPIOC_PSOR |= 1 << 9 | 1 << 10;
		GPIOC_PDDR |= 1 << 9 | 1 << 10;

		/* Switch buttons settings */
		/* Set input PORTC1 */
		PORTC_PCR1 = PORT_PCR_MUX(1);
		GPIOC_PDDR &= ~((uint_32)1 << 1);
		/* Pull up */
		PORTC_PCR1 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
		/* GPIO_INT_EDGE_FALLING */
		PORTC_PCR1 |= PORT_PCR_IRQC(0xA);
		/* Set input PORTC2 */
		PORTC_PCR2 =  PORT_PCR_MUX(1);
		GPIOC_PDDR &= ~((uint_32)1 << 2);
		/* Pull up */
		PORTC_PCR2 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
		/* GPIO_INT_EDGE_FALLING */
		PORTC_PCR2 |= PORT_PCR_IRQC(0xA);
		/* Enable interrupt */
		PORTC_ISFR |= (1<<1);
		PORTC_ISFR |= (1<<2);
		#ifdef MCU_MK20D5
			NVICICPR1 = 1 << ((42)%32);
			NVICISER1 = 1 << ((42)%32);
		#else
			NVICICPR2 |= (uint32_t)(1 << ((89)%32));		/* Clear any pending interrupts on PORTC */
			NVICISER2 |= (uint32_t)(1 << ((89)%32));		/* Enable interrupts from PORTC module */
		#endif
	#endif
#endif
#if (defined MCU_MK40N512VMD100) ||  (defined MCU_MK53N512CMD100)
    /* Enable clock gating to PORTC */
    SIM_SCGC5 |= SIM_SCGC5_PORTC_MASK;

    /* LEDs settings */
    PORTC_PCR7|= PORT_PCR_SRE_MASK    /* Slow slew rate */
              |  PORT_PCR_ODE_MASK    /* Open Drain Enable */
              |  PORT_PCR_DSE_MASK    /* High drive strength */
              ;
    PORTC_PCR7 = PORT_PCR_MUX(1);
    PORTC_PCR8|= PORT_PCR_SRE_MASK    /* Slow slew rate */
              |  PORT_PCR_ODE_MASK    /* Open Drain Enable */
              |  PORT_PCR_DSE_MASK    /* High drive strength */
              ;
    PORTC_PCR8 = PORT_PCR_MUX(1);
    GPIOC_PSOR |= 1 << 7 | 1 << 8;
    GPIOC_PDDR |= 1 << 7 | 1 << 8;

	/* Switch buttons settings */
	/* Set in put PORTC5 */
	PORTC_PCR5 =  PORT_PCR_MUX(1);
	GPIOC_PDDR &= ~((uint_32)1 << 5);
	/* Pull up */
	PORTC_PCR5 |= PORT_PCR_PE_MASK|PORT_PCR_PS_MASK;
	/* GPIO_INT_EDGE_HIGH */
	PORTC_PCR5 |= PORT_PCR_IRQC(9);
	/* Set in put PORTC13*/
	PORTC_PCR13 =  PORT_PCR_MUX(1);
	GPIOC_PDDR &= ~((uint_32)1 << 13);
	/* Pull up */
	PORTC_PCR13 |= PORT_PCR_PE_MASK|PORT_PCR_PS_MASK;
	/* GPIO_INT_EDGE_HIGH */
	PORTC_PCR13 |= PORT_PCR_IRQC(9);
	/* Enable interrupt */
	PORTC_ISFR |=(1<<5);
	PORTC_ISFR |=(1<<13);
	NVICICPR2 = 1 << ((89)%32);
	NVICISER2 = 1 << ((89)%32);

	PORTC_PCR8 =  PORT_PCR_MUX(1);
	GPIOC_PDDR |= 1<<8;
#endif
#if defined(MCU_MK60N512VMD100)
	/* Enable clock gating to PORTA and PORTE */
	SIM_SCGC5 |= (SIM_SCGC5_PORTA_MASK | SIM_SCGC5_PORTE_MASK);

	/* LEDs settings */
	PORTA_PCR10 =  PORT_PCR_MUX(1);
	PORTA_PCR11 =  PORT_PCR_MUX(1);
	PORTA_PCR28 =  PORT_PCR_MUX(1);
	PORTA_PCR29 =  PORT_PCR_MUX(1);

	GPIOA_PDDR |= (1<<10);
	GPIOA_PDDR |= (1<<11);
	GPIOA_PDDR |= (1<<28);
	GPIOA_PDDR |= (1<<29);

	/* Switch buttons settings */
	/* Set input on PORTA pin 19 */
#ifndef _USB_BATT_CHG_APP_H_
	PORTA_PCR19 =  PORT_PCR_MUX(1);
	GPIOA_PDDR &= ~((uint_32)1 << 19);
	/* Pull up enabled */
	PORTA_PCR19 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
	/* GPIO_INT_EDGE_HIGH */
	PORTA_PCR19 |= PORT_PCR_IRQC(9);

	/* Set input on PORTE pin 26 */
	PORTE_PCR26 =  PORT_PCR_MUX(1);
	GPIOE_PDDR &= ~((uint_32)1 << 26);
	/* Pull up */
	PORTE_PCR26 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
	/* GPIO_INT_EDGE_HIGH */
	PORTE_PCR26 |= PORT_PCR_IRQC(9);
#endif

	/* Clear interrupt flag */
#ifndef _USB_BATT_CHG_APP_H_
	PORTA_ISFR |= (1<<19);
#endif
	PORTE_ISFR |= (1<<26);

	/* Enable interrupt port A */
#ifndef _USB_BATT_CHG_APP_H_
	NVICICPR2 = 1 << ((87)%32);
	NVICISER2 = 1 << ((87)%32);
#endif

	/* Enable interrupt port E */
	NVICICPR2 = 1 << ((91)%32);
	NVICISER2 = 1 << ((91)%32);
#endif
#ifdef MCU_MK70F12
	/* Enable clock gating to PORTA, PORTD and PORTE */
	SIM_SCGC5 |= 0 |
			//SIM_SCGC5_PORTA_MASK |
			SIM_SCGC5_PORTD_MASK |
			SIM_SCGC5_PORTE_MASK;

	/* Set function to GPIO */
	//PORTA_PCR10 = PORT_PCR_MUX(1);
	PORTD_PCR0 = PORT_PCR_MUX(1);
	PORTE_PCR26 = PORT_PCR_MUX(1);

	/* Set pin direction to in */
	GPIOD_PDDR &= ~(1<<0);
	GPIOE_PDDR &= ~(1<<26);

	/* Set pin direction to out */
	//GPIOA_PDDR |= (1<<10);

	/* Enable pull up */
	PORTD_PCR0 |= PORT_PCR_PE_MASK|PORT_PCR_PS_MASK;
	PORTE_PCR26 |= PORT_PCR_PE_MASK|PORT_PCR_PS_MASK;

	/* GPIO_INT_EDGE_HIGH */
	PORTD_PCR0 |= PORT_PCR_IRQC(9);
	PORTE_PCR26 |= PORT_PCR_IRQC(9);

	/* Clear interrupt flag */
	PORTD_ISFR |= (1<<0);
	PORTE_ISFR |= (1<<26);

	/* enable interrupt port D */
	NVICICPR2 |= 1 << ((90)%32);
	NVICISER2 |= 1 << ((90)%32);

	/* enable interrupt port E */
	NVICICPR2 |= 1 << ((91)%32);
	NVICISER2 |= 1 << ((91)%32);
#endif	// MCU_MK70F12
#if defined(MCU_MK21D5)
    /* Enable clock gating to PORTC and PORTD */
    SIM_SCGC5 |= (SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK | SIM_SCGC5_PORTE_MASK);

    /* LEDs settings */
    PORTD_PCR4 =  PORT_PCR_MUX(1);
    PORTD_PCR5 =  PORT_PCR_MUX(1);
    PORTD_PCR6 =  PORT_PCR_MUX(1);
    PORTD_PCR7 =  PORT_PCR_MUX(1);

    GPIOD_PDDR |= (1<<4) | (1<<5) | (1<<6) | (1<<7);

    /* Switch buttons settings */
    /* Set input on PORTC pin 6 */
#ifndef _USB_BATT_CHG_APP_H_
    PORTC_PCR6 =  PORT_PCR_MUX(1);
    GPIOC_PDDR &= ~((uint_32)1 << 6);
    /* Pull up enabled */
    PORTC_PCR6 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* GPIO_INT_EDGE_HIGH */
    PORTC_PCR6 |= PORT_PCR_IRQC(9);

    /* Set input on PORTC pin 7 */
    PORTC_PCR7 =  PORT_PCR_MUX(1);
    GPIOC_PDDR &= ~((uint_32)1 << 7);
    /* Pull up */
    PORTC_PCR7 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* GPIO_INT_EDGE_HIGH */
    PORTC_PCR7 |= PORT_PCR_IRQC(9);

    /* Clear interrupt flag */
    PORTC_ISFR |= (1<<6);
    PORTC_ISFR |= (1<<7);

    /* Enable interrupt port C */
    NVICICPR1 = 1 << ((61)%32);
    NVICISER1 = 1 << ((61)%32);
#endif
#endif
#if defined(MCU_MKL25Z4)
    /* Enable clock gating to PORTA, PORTB, PORTC, PORTD and PORTE */
    SIM_SCGC5 |= (SIM_SCGC5_PORTA_MASK
              | SIM_SCGC5_PORTB_MASK
              | SIM_SCGC5_PORTC_MASK
              | SIM_SCGC5_PORTD_MASK
              | SIM_SCGC5_PORTE_MASK);

    /* LEDs settings */
    PORTA_PCR5  =  PORT_PCR_MUX(1);
    PORTA_PCR16 =  PORT_PCR_MUX(1);
    PORTA_PCR17 =  PORT_PCR_MUX(1);
    PORTB_PCR8  =  PORT_PCR_MUX(1);

    GPIOA_PDDR |= (1<<5) | (1<<16) | (1<<17);
    GPIOB_PDDR |= (1<<8);

    /* Switch buttons settings */
    /* Set input on PORTC pin 3 */
#ifndef _USB_BATT_CHG_APP_H_
    PORTC_PCR3 =  PORT_PCR_MUX(1);
    GPIOC_PDDR &= ~((uint_32)1 << 3);
    /* Pull up enabled */
    PORTC_PCR3 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* GPIO_INT_EDGE_HIGH */
    PORTC_PCR3 |= PORT_PCR_IRQC(9);

    /* Set input on PORTA pin 4 */
    PORTA_PCR4 =  PORT_PCR_MUX(1);
    GPIOA_PDDR &= ~((uint_32)1 << 4);
    /* Pull up */
    PORTA_PCR4 |= PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;
    /* GPIO_INT_EDGE_HIGH */
    PORTA_PCR4 |= PORT_PCR_IRQC(9);

    /* Clear interrupt flag */
    PORTC_ISFR |= (1<<3);
    PORTA_ISFR |= (1<<4);

    /* Enable interrupt port A */
    NVIC_ICPR = 1 << 30;
    NVIC_ISER = 1 << 30;
#endif
#endif
}

#ifndef _SERIAL_AGENT_
/*****************************************************************************
 *
 *    @name     USB_Init
 *
 *    @brief    This function Initializes USB module
 *
 *    @param    None
 *
 *    @return   None
 *
 ***************************************************************************/
static void USB_Init(uint_8 controller_ID)
{
	if(controller_ID == MAX3353)
	{
#ifdef MCU_MK70F12
		/* MPU is disabled. All accesses from all bus masters are allowed */
		MPU_CESR=0;

		SIM_SOPT2 |= SIM_SOPT2_PLLFLLSEL(1) 	/** PLL0 reference */
						| SIM_SOPT2_USBFSRC(0)			/** MCGPLLCLK as CLKC source */
						|  SIM_SOPT2_USBF_CLKSEL_MASK;	/** USB fractional divider like USB reference clock */
		SIM_CLKDIV2 = USB_FRAC | USB_DIV;		/** Divide reference clock to obtain 48MHz */

		/* Enable USB-OTG IP clocking */
		SIM_SCGC4 |= SIM_SCGC4_USBFS_MASK;
#else
    #ifndef MCU_MKL25Z4
		SIM_CLKDIV2 &= (uint32_t)(~(SIM_CLKDIV2_USBFRAC_MASK | SIM_CLKDIV2_USBDIV_MASK));
    #endif /* MCU_MKL25Z4 */
    #ifdef MCGOUTCLK_72_MHZ
		/* Configure USBFRAC = 0, USBDIV = 0 => frq(USBout) = 2 / 3 * frq(PLLin) */
		SIM_CLKDIV2 = SIM_CLKDIV2_USBDIV(2) | SIM_CLKDIV2_USBFRAC_MASK;
    #else
        #ifndef MCU_MKL25Z4
		/* Configure USBFRAC = 0, USBDIV = 0 => frq(USBout) = 1 / 1 * frq(PLLin) */
		SIM_CLKDIV2 = SIM_CLKDIV2_USBDIV(0);
        #endif /* MCU_MKL25Z4 */
    #endif /* MCGOUTCLK_72_MHZ */

		/* 1. Configure USB to be clocked from PLL */
#ifdef PE_32KHz
		SIM_SOPT2 |= SIM_SOPT2_USBSRC_MASK;
		//SIM_SOPT2 &= ~SIM_SOPT2_PLLFLLSEL_MASK; // we need FLL for 32KHz case.
#else
		SIM_SOPT2 |= SIM_SOPT2_USBSRC_MASK | SIM_SOPT2_PLLFLLSEL_MASK;  // default USB Stack is to use PLL, which is what we need for 26MHz source.
#endif

#if PLL_96
		/* 2. USB freq divider */
		SIM_CLKDIV2 = 0x02;
#endif /* PLL_96 */

		/* 3. Enable USB-OTG IP clocking */
		SIM_SCGC4 |= (SIM_SCGC4_USBOTG_MASK);

#if 1 //ERNIE_HACK
		/* old documentation writes setting this bit is mandatory */
		USB0_USBTRC0 = 0x40;
#endif
		/* Configure enable USB regulator for device */
		SIM_SOPT1 |= SIM_SOPT1_USBREGEN_MASK;

#endif /* MCU_MK70F12 */

#if defined(MCU_MK20D5)
		NVICICPR1 |= (1 << 3);	/* Clear any pending interrupts on USB */
		NVICISER1 |= (1 << 3);	/* Enable interrupts from USB module */
#elif (defined MCU_MK21D5)

	#if ERNIE_HACK_INTERRUPTS_CAUSING_MEMORY_BADZ
		/* K21 on Corona Proto1 */
        NVICICER1 |= (1<<21);    /* Clear any pending interrupts on USB */
        NVICISER1 |= (1<<21);    /* Enable interrupts from USB module */
	#endif
#elif (defined MCU_MKL25Z4)
        NVIC_ICER |= (1<<24);	 /* Clear any pending interrupts on USB */
        NVIC_ISER |= (1<<24);	 /* Enable interrupts from USB module */
#else
    //printf("K60 postponing enabling of USB interrupt...\n");
	#if ERNIE_HACK_INTERRUPTS_CAUSING_MEMORY_BADZ
    /* K21 on Corona Proto2 */
		NVICICER2 |= (1 << 9);	/* Clear any pending interrupts on USB */
		NVICISER2 |= (1 << 9);	/* Enable interrupts from USB module */
	#endif
#endif

	} // MAX3353
	else if(controller_ID == ULPI)
	{
#ifdef MCU_MK70F12
		/* disable cache */
		LMEM_PCCCR &= ~LMEM_PCCCR_ENCACHE_MASK;
		/* increase priority for usb module
		 * AXBS_PRS1:
		 * ??=0,M7=7,??=0,M6=0,??=0,M5=5,??=0,M4=4,??=0,M3=3,??=0,M2=2,??=0,M1=1,??=0,M0=6 */
		AXBS_PRS1 = (uint32_t)0x70543216UL;

		/* Disable the MPU so that USB can access RAM */
		MPU_CESR &= ~MPU_CESR_VLD_MASK;

		/* clock init */
		SIM_CLKDIV2 |= USBHS_FRAC |
				USBHS_DIV;			/* Divide reference clock to obtain 60MHz */

		/* MCGPLLCLK for the USB 60MHz CLKC source */
		SIM_SOPT2 |= SIM_SOPT2_USBHSRC(1);

		/* External 60MHz UPLI Clock */
		SIM_SOPT2 |= SIM_SOPT2_USBH_CLKSEL_MASK;

		/* enable USBHS clock */
		SIM_SCGC6 |= SIM_SCGC6_USB2OTG_MASK;

		/* use external 60MHz ULPI clock */
		SIM_SOPT2 |= (SIM_SOPT2_USBH_CLKSEL_MASK);

		/* select alternate function 2 for ULPI pins */
		SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK;
		PORTA_PCR10 = PORT_PCR_MUX(2);  // Data0
		PORTA_PCR6 = PORT_PCR_MUX(2);	// CLK
		PORTA_PCR11 = PORT_PCR_MUX(2);  // Data1
		PORTA_PCR24 = PORT_PCR_MUX(2);	// Data2
		PORTA_PCR29 = PORT_PCR_MUX(2);	// Data7
		PORTA_PCR25 = PORT_PCR_MUX(2);	// Data3
		PORTA_PCR26 = PORT_PCR_MUX(2);	// Data4
		PORTA_PCR27 = PORT_PCR_MUX(2);	// Data5
		PORTA_PCR28 = PORT_PCR_MUX(2);	// Data6
		PORTA_PCR8 = PORT_PCR_MUX(2);	// NXT
		PORTA_PCR7 = PORT_PCR_MUX(2);	// DIR
		PORTA_PCR9 = PORT_PCR_MUX(2);	// STP

		/* reset ULPI module */
		USBHS_USBCMD |= USBHS_USBCMD_RST_MASK;

		/* check if USBHS module is ok */
		USBHS_ULPI_VIEWPORT = 0x40000000;

		/* wait for ULPI to initialize */
		while(USBHS_ULPI_VIEWPORT & (0x40000000));
#ifdef SERIAL_DEBUG
		printf("OK\n");
#endif

		NVICICER3 |= 1;	//Clear any pending interrupts on USBHS
		NVICISER3 |= 1;	//Enable interrupts on USBHS
#endif /* MCU_MK70F12 */
	} /* ULPI */
}
#endif

/******************************************************************************
 * @name       all_led_off
 *
 * @brief      Switch OFF all LEDs on Board
 *
 * @param	   None
 *
 * @return     None
 *
 *****************************************************************************
 * This function switch OFF all LEDs on Board
 *****************************************************************************/
static void all_led_off(void)
{
	#if defined (MCU_MK40N512VMD100)
		GPIOC_PSOR |= 1 << 7 | 1 << 8 | 1 << 9;
	#elif defined (MCU_MK53N512CMD100)
		GPIOC_PSOR |= 1 << 7 | 1 << 8;
	#elif defined (MCU_MK60N512VMD100)
		GPIOA_PSOR |= 1 << 10 | 1 << 11 | 1 << 28 | 1 << 29;
	#elif (defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		GPIOC_PSOR |= 1 << 9 | 1 << 10;
    #elif (defined MCU_MK21D5)
        GPIOD_PSOR |= 1 << 4 | 1 << 5 | 1 << 6 | 1 << 7;
    #endif
}

/******************************************************************************
 * @name       display_led
 *
 * @brief      Displays 8bit value on Board LEDs
 *
 * @param	   val
 *
 * @return     None
 *
 *****************************************************************************
 * This function displays 8 bit value on Board LED
 *****************************************************************************/
void display_led(uint_8 val)
{
    uint_8 i = 0;
	UNUSED(i);
    all_led_off();

	#if defined (MCU_MK40N512VMD100)
    	val &= 0x07;
        if(val & 0x1)
    		GPIOC_PCOR |= 1 << 7;
    	if(val & 0x2)
    		GPIOC_PCOR |= 1 << 8;
    	if(val & 0x4)
    		GPIOC_PCOR |= 1 << 9;
	#elif defined (MCU_MK53N512CMD100)
		val &= 0x03;
	    if(val & 0x1)
			GPIOC_PCOR |= 1 << 7;
		if(val & 0x2)
			GPIOC_PCOR |= 1 << 8;
	#elif defined (MCU_MK60N512VMD100)
		val &= 0x0F;
	    if(val & 0x1)
			GPIOA_PCOR |= 1 << 11;
		if(val & 0x2)
			GPIOA_PCOR |= 1 << 28;
	    if(val & 0x4)
			GPIOA_PCOR |= 1 << 29;
		if(val & 0x8)
			GPIOA_PCOR |= 1 << 10;
	#elif (defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		val &= 0x03;
	    if(val & 0x1)
			GPIOC_PCOR |= 1 << 9;
		if(val & 0x2)
			GPIOC_PCOR |= 1 << 10;
    #elif (defined MCU_MK21D5)
        val &= 0x0F;
        if(val & 0x1)
            GPIOD_PCOR |= 1 << 4;
        if(val & 0x2)
            GPIOD_PCOR |= 1 << 5;
        if(val & 0x4)
            GPIOD_PCOR |= 1 << 6;
        if(val & 0x8)
            GPIOD_PCOR |= 1 << 7;
    #endif
}
/*****************************************************************************
 *
 *    @name     Init_Sys
 *
 *    @brief    This function Initializes the system
 *
 *    @param    None
 *
 *    @return   None
 *
 ****************************************************************************
 * Initializes the MCU, MCG, KBI, RTC modules
 ***************************************************************************/
static void SYS_Init(void)
{
	/* Point the VTOR to the new copy of the vector table */
#if (defined(__CWCC__) || defined(__IAR_SYSTEMS_ICC__) || defined(__CC_ARM)|| (defined __arm__))
//	SCB_VTOR = (uint_32)___VECTOR_RAM;   // ERNIE_HACK
#elif defined(__GNUC__)
	SCB_VTOR = (uint_32)__cs3_interrupt_vector;
#endif
#ifndef MCU_MK70F12
    /* SIM Configuration */
    /* SIM Configuration */

#ifdef PE_32KHz
	// Do Nothing.  PE will set up the clocks for us.
#elif JIM_32KHz
	Init_32k_clk(); // Jim's function
#else
	pll_init();     // Default USB Stack init, modified by Ernie to use 26MHz external clock (requires Solego 26MHz enable), verify this one works.
#endif
    #if !(defined MCU_MK20D5) && !(defined MCU_MK20D7) && !(defined MCU_MK40D7) && !(defined MCU_MK21D5) && !(defined MCU_MKL25Z4)
		MPU_CESR=0x00;
	#endif
#else
    // K70 initialization
    /*
     * Enable all of the port clocks. These have to be enabled to configure
     * pin muxing options, so most code will need all of these on anyway.
     */
    SIM_SCGC5 |= (SIM_SCGC5_PORTA_MASK
    		| SIM_SCGC5_PORTB_MASK
    		| SIM_SCGC5_PORTC_MASK
    		| SIM_SCGC5_PORTD_MASK
    		| SIM_SCGC5_PORTE_MASK
    		| SIM_SCGC5_PORTF_MASK );

    // releases hold with ACKISO:  Only has an effect if recovering from VLLS1, VLLS2, or VLLS3
    // if ACKISO is set you must clear ackiso before calling pll_init
    //    or pll init hangs waiting for OSC to initialize
    // if osc enabled in low power modes - enable it first before ack
    // if I/O needs to be maintained without glitches enable outputs and modules first before ack.
    if (PMC_REGSC &  PMC_REGSC_ACKISO_MASK)
    	PMC_REGSC |= PMC_REGSC_ACKISO_MASK;

#if defined(NO_PLL_INIT)
    mcg_clk_hz = 21000000; //FEI mode
#elif defined (ASYNCH_MODE)
    /* Set the system dividers */
    /* NOTE: The PLL init will not configure the system clock dividers,
     * so they must be configured appropriately before calling the PLL
     * init function to ensure that clocks remain in valid ranges.
     */
    SIM_CLKDIV1 = ( 0
    		| SIM_CLKDIV1_OUTDIV1(0)
    		| SIM_CLKDIV1_OUTDIV2(1)
    		| SIM_CLKDIV1_OUTDIV3(1)
    		| SIM_CLKDIV1_OUTDIV4(5) );

    /* Initialize PLL0 */
    /* PLL0 will be the source for MCG CLKOUT so the core, system, FlexBus, and flash clocks are derived from it */
    mcg_clk_hz = pll_init(OSCINIT,   /* Initialize the oscillator circuit */
    		OSC_0,     /* Use CLKIN0 as the input clock */
    		CLK0_FREQ_HZ,  /* CLKIN0 frequency */
    		LOW_POWER,     /* Set the oscillator for low power mode */
    		CLK0_TYPE,     /* Crystal or canned oscillator clock input */
    		PLL_0,         /* PLL to initialize, in this case PLL0 */
    		PLL0_PRDIV,    /* PLL predivider value */
    		PLL0_VDIV,     /* PLL multiplier */
    		MCGOUT);       /* Use the output from this PLL as the MCGOUT */

    /* Check the value returned from pll_init() to make sure there wasn't an error */
    if (mcg_clk_hz < 0x100)
    	while(1);

    /* Initialize PLL1 */
    /* PLL1 will be the source for the DDR controller, but NOT the MCGOUT */
    pll_1_clk_khz = (pll_init(NO_OSCINIT, /* Don't init the osc circuit, already done */
    		OSC_0,      /* Use CLKIN0 as the input clock */
    		CLK0_FREQ_HZ,  /* CLKIN0 frequency */
    		LOW_POWER,     /* Set the oscillator for low power mode */
    		CLK0_TYPE,     /* Crystal or canned oscillator clock input */
    		PLL_1,         /* PLL to initialize, in this case PLL1 */
    		PLL1_PRDIV,    /* PLL predivider value */
    		PLL1_VDIV,     /* PLL multiplier */
    		PLL_ONLY) / 1000);   /* Don't use the output from this PLL as the MCGOUT */

    /* Check the value returned from pll_init() to make sure there wasn't an error */
    if ((pll_1_clk_khz * 1000) < 0x100)
    	while(1);

    pll_0_clk_khz = mcg_clk_hz / 1000;

#elif defined (SYNCH_MODE)
    /* Set the system dividers */
    /* NOTE: The PLL init will not configure the system clock dividers,
     * so they must be configured appropriately before calling the PLL
     * init function to ensure that clocks remain in valid ranges.
     */
    SIM_CLKDIV1 = ( 0
    		| SIM_CLKDIV1_OUTDIV1(0)
    		| SIM_CLKDIV1_OUTDIV2(2)
    		| SIM_CLKDIV1_OUTDIV3(2)
    		| SIM_CLKDIV1_OUTDIV4(5) );

    /* Initialize PLL1 */
    /* PLL1 will be the source MCGOUT and the DDR controller */
    mcg_clk_hz = pll_init(OSCINIT, /* Don't init the osc circuit, already done */
    		OSC_0,      /* Use CLKIN0 as the input clock */
    		CLK0_FREQ_HZ,  /* CLKIN0 frequency */
    		LOW_POWER,     /* Set the oscillator for low power mode */
    		CLK0_TYPE,     /* Crystal or canned oscillator clock input */
    		PLL_1,         /* PLL to initialize, in this case PLL1 */
    		PLL1_PRDIV,    /* PLL predivider value */
    		PLL1_VDIV,     /* PLL multiplier */
    		MCGOUT);   /* Don't use the output from this PLL as the MCGOUT */

    /* Check the value returned from pll_init() to make sure there wasn't an error */
    if (mcg_clk_hz < 0x100)
    	while(1);

    /* Initialize PLL0 */
    /* PLL0 is initialized, but not used as the MCGOUT */
    pll_0_clk_khz = (pll_init(NO_OSCINIT,   /* Initialize the oscillator circuit */
    		OSC_0,     /* Use CLKIN0 as the input clock */
    		CLK0_FREQ_HZ,  /* CLKIN0 frequency */
    		LOW_POWER,     /* Set the oscillator for low power mode */
    		CLK0_TYPE,     /* Crystal or canned oscillator clock input */
    		PLL_0,         /* PLL to initialize, in this case PLL0 */
    		PLL0_PRDIV,    /* PLL predivider value */
    		PLL0_VDIV,     /* PLL multiplier */
    		PLL_ONLY) / 1000);       /* Use the output from this PLL as the MCGOUT */

    /* Check the value returned from pll_init() to make sure there wasn't an error */
    if ((pll_0_clk_khz * 1000) < 0x100)
    	while(1);

    pll_1_clk_khz = mcg_clk_hz / 1000;

#else
#error "A PLL configuration for this platform is NOT defined"
#endif


    /*
     * Use the value obtained from the pll_init function to define variables
     * for the core clock in kHz and also the peripheral clock. These
     * variables can be used by other functions that need awareness of the
     * system frequency.
     */
    mcg_clk_khz = mcg_clk_hz / 1000;
    core_clk_khz = mcg_clk_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV1_MASK) >> 28)+ 1);
    periph_clk_khz = mcg_clk_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV2_MASK) >> 24)+ 1);

    /* For debugging purposes, enable the trace clock and/or FB_CLK so that
     * we'll be able to monitor clocks and know the PLL is at the frequency
     * that we expect.
     */
    trace_clk_init();
    fb_clk_init();

    /* Initialize the DDR if the project option if defined */
#ifdef DDR_INIT
    twr_ddr2_script_init();
#endif
#endif
}

/////////////////
//
// Init_32k_clk() - Config to use the external 32k source for 48MHz USB clk
//
////////////////
static void Init_32k_clk()
{
#if 0
  int i;

  printf ("Now changing to the xternal 32KHz osc & 48MHz USB clock...\n");

  for (i = 0; i < 1000000; i++);

  // Change to the external 32.768 KHz osc...
  /* SIM_SCGC6: RTC=1 */
  SIM_SCGC6 |= SIM_SCGC6_RTC_MASK;
  if ((RTC_CR & RTC_CR_OSCE_MASK) == 0u) { /* Only if the OSCILLATOR is not already enabled */
    /* RTC_CR: SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    RTC_CR &= (uint32_t)~(uint32_t)(
               RTC_CR_SC2P_MASK |
               RTC_CR_SC4P_MASK |
               RTC_CR_SC8P_MASK |
               RTC_CR_SC16P_MASK
              );
    /* RTC_CR: OSCE=1 */
    RTC_CR |= RTC_CR_OSCE_MASK;
    /* RTC_CR: CLKO=0 */
    RTC_CR &= (uint32_t)~(uint32_t)(RTC_CR_CLKO_MASK);
  }

  /* System clock initialization */
  /* SIM_SCGC5: PORTA=1 */
  SIM_SCGC5 |= SIM_SCGC5_PORTA_MASK;   /* Enable clock gate for ports to enable pin routing */
  /* SIM_CLKDIV1: OUTDIV1=1,OUTDIV2=1,??=0,??=0,??=1,??=1,OUTDIV4=3,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0 */
  SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0x01) |
                SIM_CLKDIV1_OUTDIV2(0x01) |
                SIM_CLKDIV1_OUTDIV4(0x03) |
                0x00300000U;           /* Update system prescalers */
  /* SIM_SOPT2: PLLFLLSEL=0 */
  SIM_SOPT2 &= (uint32_t)~(uint32_t)(SIM_SOPT2_PLLFLLSEL_MASK); /* Select FLL as a clock source for various peripherals */
  /* SIM_SOPT1: OSC32KSEL=0 */
  SIM_SOPT1 &= (uint32_t)~(uint32_t)(SIM_SOPT1_OSC32KSEL(0x03)); /* System oscillator drives 32 kHz clock for various peripherals */
  /* Switch to FEE Mode */
  /* MCG_C2: LOCRE0=0,??=0,RANGE0=0,HGO0=0,EREFS0=0,LP=0,IRCS=0 */
  MCG_C2 = 0x00U;
  /* OSC_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
  OSC_CR = OSC_CR_ERCLKEN_MASK;
  /* MCG_C7: OSCSEL=1 */
#define MCG_C7_OSCSEL_MASK                       0x1u
  MCG_C7 |= MCG_C7_OSCSEL_MASK;   // Selects 32 kHz RTC Osc
  /* MCG_C1: CLKS=0,FRDIV=0,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
  MCG_C1 = MCG_C1_IRCLKEN_MASK;
  /* MCG_C4: DMX32=1,DRST_DRS=1 */
  MCG_C4 = (uint8_t)((MCG_C4 & (uint8_t)~(uint8_t)(
            MCG_C4_DRST_DRS(0x02)
           )) | (uint8_t)(
            MCG_C4_DMX32_MASK |
            MCG_C4_DRST_DRS(0x01)
           ));
  /* MCG_C5: ??=0,PLLCLKEN0=0,PLLSTEN0=0,PRDIV0=0 */
  MCG_C5 = 0x00U;
  /* MCG_C6: LOLIE0=0,PLLS=0,CME0=0,VDIV0=0 */
  MCG_C6 = 0x00U;
  while((MCG_S & MCG_S_IREFST_MASK) != 0x00U) { /* Check that the source of the FLL reference clock is the external reference clock. */
  }
  while((MCG_S & 0x0CU) != 0x00U) {    /* Wait until output of the FLL is selected */
  }

  for (i = 0; i < 1000000; i++);

  printf ("Clock re-config successful!!\n");  // or this wouldn't print...
#endif
}

#ifdef MCU_MK70F12
/********************************************************************/
void trace_clk_init(void)
{
	/* Set the trace clock to the core clock frequency */
	SIM_SOPT2 |= SIM_SOPT2_TRACECLKSEL_MASK;

	/* Enable the TRACE_CLKOUT pin function on PTF23 (alt6 function) */
	PORTF_PCR23 = ( PORT_PCR_MUX(0x6));
}

/********************************************************************/
void fb_clk_init(void)
{
	/* Enable the clock to the FlexBus module */
	SIM_SCGC7 |= SIM_SCGC7_FLEXBUS_MASK;

	/* Enable the FB_CLKOUT function on PTC3 (alt5 function) */
	PORTC_PCR3 = ( PORT_PCR_MUX(0x5));
}
#endif // MCU_MK70F12

#if(!(defined MCU_MK21D5))
/******************************************************************************
*   @name        IRQ_ISR_PORTA
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment
*
*******************************************************************************/
void IRQ_ISR_PORTA(void)
{
#if defined(MCU_MK20D5)
    NVICICPR1 |= 1 << ((40)%32);
    NVICISER1 |= 1 << ((40)%32);
#elif defined (MCU_MKL25Z4)
    NVIC_ICPR |= 1 << 30;
    NVIC_ISER |= 1 << 30;
#else
    NVICICPR2 |= 1 << ((87)%32);
    NVICISER2 |= 1 << ((87)%32);
#endif
	DisableInterrupts;
#if defined MCU_MKL25Z4
    if(PORTA_ISFR & (1<<4))
    {
        kbi_stat |= 0x02;                 /* Update the kbi state */
        PORTA_ISFR |=(1<<4);            /* Clear the bit by writing a 1 to it */
    }
#else
	if(PORTA_ISFR & (1<<19))
	{
		kbi_stat |= 0x02; 				/* Update the kbi state */
		PORTA_ISFR |=(1<<19);			/* Clear the bit by writing a 1 to it */
	}
#endif
	EnableInterrupts;
}
#endif
#if (!defined MCU_MKL25Z4)&&((!(defined MCU_MK20D5)) || (!(defined _USB_BATT_CHG_APP_H_)))
/******************************************************************************
*   @name        IRQ_ISR
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment
*
*******************************************************************************/
void IRQ_ISR_PORTC(void)
{
		#if defined(MCU_MK20D5)
			NVICICPR1 |= (uint32_t)(1 << ((42)%32));
			NVICISER1 |= (uint32_t)(1 << ((42)%32));
        #elif defined(MCU_MK21D5)
            NVICICPR1 = 1 << ((61)%32);
            NVICISER1 = 1 << ((61)%32);
        #else
			NVICICPR2 |= (uint32_t)(1 << ((89)%32));		/* Clear any pending interrupt on PORTC */
			NVICISER2 |= (uint32_t)(1 << ((89)%32));		/* Set interrupt on PORTC */
		#endif

		DisableInterrupts;
	#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		if(PORTC_ISFR & (1<<1))
    #elif defined(MCU_MK21D5)
        if(PORTC_ISFR & (1<<6))
	#else
		if(PORTC_ISFR & (1<<5))
	#endif
		{
			kbi_stat |= 0x02; 				/* Update the kbi state */

			/* Clear the bit by writing a 1 to it */
		#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
			PORTC_ISFR |=(1<<1);
        #elif defined(MCU_MK21D5)
            PORTC_ISFR |=(1<<6);
		#else
			PORTC_ISFR |=(1<<5);
		#endif
		}

	#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		if(PORTC_ISFR & (1<<2))
    #elif defined(MCU_MK21D5)
        if(PORTC_ISFR & (1<<7))
	#else
		if(PORTC_ISFR & (1<<13))
	#endif
		{
			kbi_stat |= 0x08;				/* Update the kbi state */

			/* Clear the bit by writing a 1 to it */
			#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
				PORTC_ISFR |=(1<<2);
            #elif defined(MCU_MK21D5)
                PORTC_ISFR |=(1<<7);
			#else
				PORTC_ISFR |=(1<<13);
			#endif
		}
		EnableInterrupts;
	}
	#endif
//#endif

#if((!defined MCU_MK21D5)&&(!defined MCU_MKL25Z4))
/******************************************************************************
*   @name        IRQ_ISR
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment
*
*******************************************************************************/
void IRQ_ISR_PORTD(void)
{
	#ifdef MCU_MK20D5
		NVICICPR1 |= 1 << ((43)%32);
		NVICISER1 |= 1 << ((43)%32);
	#else
		NVICICPR2 |= 1 << ((90)%32);
		NVICISER2 |= 1 << ((90)%32);
	#endif

	DisableInterrupts;
	if(PORTD_ISFR & (1<<0))
	{
		/* Update the kbi state */
		kbi_stat |= 0x02;	// right click
		PORTD_ISFR |= (1<<0);			/* Clear the bit by writing a 1 to it */
	}
	EnableInterrupts;
}
#endif

/******************************************************************************
*   @name        IRQ_ISR_PORTE
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment
*
*******************************************************************************/
#if((!defined MCU_MK21D5)&&(!defined MCU_MKL25Z4))
void IRQ_ISR_PORTE(void)
{
	#ifdef MCU_MK20D5
		NVICICPR1 |= 1 << ((44)%32);
		NVICISER1 |= 1 << ((44)%32);
	#else
		NVICICPR2 |= 1 << ((91)%32);
		NVICISER2 |= 1 << ((91)%32);
	#endif
	DisableInterrupts;
	if(PORTE_ISFR & (1<<26))
	{
		/* Update the kbi state */
#ifdef MCU_MK70F12
		kbi_stat |= 0x01;	// left click
#else
		kbi_stat |= 0x08;	// move pointer down
#endif
		PORTE_ISFR |= (1<<26);			/* Clear the bit by writing a 1 to it */
	}
	EnableInterrupts;
}
#endif
/******************************************************************************
*   @name        IRQ_ISR_PORTF
*
*   @brief       Service interrupt routine of IRQ
*
*   @return      None
*
*   @comment
*
*******************************************************************************/
#ifdef MCU_MK70F12
void IRQ_ISR_PORTF(void)
{
}
#endif

#if WATCHDOG_DISABLE
/*****************************************************************************
 * @name     wdog_disable
 *
 * @brief:   Disable watchdog.
 *
 * @param  : None
 *
 * @return : None
 *****************************************************************************
 * It will disable watchdog.
  ****************************************************************************/
static void wdog_disable(void)
{
#ifdef MCU_MKL25Z4
    SIM_COPC = 0x00;
#else
	/* Write 0xC520 to the unlock register */
	WDOG_UNLOCK = 0xC520;

	/* Followed by 0xD928 to complete the unlock */
	WDOG_UNLOCK = 0xD928;

	/* Clear the WDOGEN bit to disable the watchdog */
	WDOG_STCTRLH &= ~WDOG_STCTRLH_WDOGEN_MASK;
#endif
}
#endif /* WATCHDOG_DISABLE */

/*****************************************************************************
 * @name     pll_init
 *
 * @brief:   Initialization of the MCU.
 *
 * @param  : None
 *
 * @return : None
 *****************************************************************************
 * It will configure the MCU to disable STOP and COP Modules.
 * It also set the MCG configuration and bus clock frequency.
 ****************************************************************************/
static int pll_init(
#ifdef MCU_MK70F12
	unsigned char init_osc,
	unsigned char osc_select,
	int crystal_val,
	unsigned char hgo_val,
	unsigned char erefs_val,
	unsigned char pll_select,
	signed char prdiv_val,
	signed char vdiv_val,
	unsigned char mcgout_select
#endif
		)
{
#ifdef MCU_MK21D5
    /* System clock initialization */
    if ( *((uint8_t*) 0x03FFU) != 0xFFU) {
        MCG_C3 = *((uint8_t*) 0x03FFU);   // \TODO - Do we really want this?  This could just be some bogus TRIM VALUE !!!
        MCG_C4 = (MCG_C4 & 0xE0U) | ((*((uint8_t*) 0x03FEU)) & 0x1FU);
    }
    /* SIM_CLKDIV1: OUTDIV1=0,OUTDIV2=1,OUTDIV3=0,OUTDIV4=2,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0,??=0 */
    SIM_CLKDIV1 = (uint32_t)0x00010000UL; /* Update system prescalers */
    /* OSC_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC_CR = (uint8_t)0x80U;
    /* MCG_C7: OSCSEL=0 */
    MCG_C7 &= (uint8_t)~(uint8_t)0x00U;
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=0,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = (uint8_t)0x25U; // high gain and ext ref clock.
    /* MCG_C1: CLKS=2,FRDIV=3,IREFS=0,IRCLKEN=0,IREFSTEN=0 */
    MCG_C1 = (uint8_t)0x98U;
    /* MCG_C4: DMX32=0,DRST_DRS=0 */
    MCG_C4 &= (uint8_t)~(uint8_t)0xE0U;
    /* MCG_C5: ??=0,PLLCLKEN0=0,PLLSTEN0=0,PRDIV0=3 */
    MCG_C5 = (uint8_t)0x0CU;
    /* MCG_C6: LOLIE0=0,PLLS=0,CME0=0,VDIV0=0 */
    MCG_C6 = (uint8_t)0x00U;
    while((MCG_S & MCG_S_OSCINIT0_MASK) == 0x00U) { /* Check that the oscillator is running */
    }
    while((MCG_S & MCG_S_IREFST_MASK) != 0x00U) { /* Check that the source of the FLL reference clock is the external reference clock. */
    }
    while((MCG_S & 0x0CU) != 0x08U) {    /* Wait until external reference clock is selected as MCG output */
    }
    /* Switch to PBE Mode */
    /* OSC_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC_CR = (uint8_t)0x80U;
    /* MCG_C7: OSCSEL=0 */
    MCG_C7 &= (uint8_t)~(uint8_t)0x00U;
    /* MCG_C1: CLKS=2,FRDIV=3,IREFS=0,IRCLKEN=0,IREFSTEN=0 */
    MCG_C1 = (uint8_t)0x98U;
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=0,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = (uint8_t)0x29U;
    /* MCG_C5: ??=0,PLLCLKEN0=0,PLLSTEN0=0,PRDIV0=3 */
    MCG_C5 = (uint8_t)0x0CU;
    /* MCG_C6: LOLIE0=0,PLLS=1,CME0=0,VDIV0=0 */
    MCG_C6 = (uint8_t)0x40U;
    while((MCG_S & 0x0CU) != 0x08U) {    /* Wait until external reference clock is selected as MCG output */
    }
    while((MCG_S & MCG_S_LOCK0_MASK) == 0x00U) { /* Wait until locked */
    }
    /* Switch to PEE Mode */
    /* OSC_CR: ERCLKEN=0,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC_CR = (uint8_t)0x80U;
    /* MCG_C7: OSCSEL=0 */
    MCG_C7 &= (uint8_t)~(uint8_t)0x0U;
    /* MCG_C1: CLKS=0,FRDIV=3,IREFS=0,IRCLKEN=0,IREFSTEN=0 */
    MCG_C1 = (uint8_t)0x18U;
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=2,HGO0=0,EREFS0=1,LP=0,IRCS=1 */
    MCG_C2 = (uint8_t)0x29U;
    /* MCG_C5: ??=0,PLLCLKEN0=0,PLLSTEN0=0,PRDIV0=3 */
    MCG_C5 = (uint8_t)0x0CU;
    /* MCG_C6: LOLIE0=0,PLLS=1,CME0=0,VDIV0=0 */
    MCG_C6 = (uint8_t)0x40U;
    while((MCG_S & 0x0CU) != 0x0CU) {    /* Wait until output of the PLL is selected */
    }
    while((MCG_S & MCG_S_LOCK0_MASK) == 0x00U) { /* Wait until locked */
    }
    /* MCG_C6: CME0=1 */
    MCG_C6 |= (uint8_t)0x20U;            /* Enable the clock monitor */

    return 0;
    /*end MCU_MK21D5 */
#elif (defined MCU_MKL25Z4)
    /* Update system prescalers */
    SIM_CLKDIV1  = SIM_CLKDIV1_OUTDIV4(1);
    SIM_CLKDIV1 |= SIM_CLKDIV1_OUTDIV1(1);
    /* First FEI must transition to FBE mode
    Enable external oscillator, RANGE=02, HGO=, EREFS=, LP=, IRCS= */
    MCG_C2 = MCG_C2_RANGE0(2) | MCG_C2_HGO0_MASK | MCG_C2_EREFS0_MASK | MCG_C2_IRCS_MASK;

    /* Select external oscillator and Reference Divider and clear IREFS
     * to start external oscillator
     * CLKS = 2, FRDIV = 3, IREFS = 0, IRCLKEN = 0, IREFSTEN = 0
     */
    MCG_C1 = MCG_C1_CLKS(2) | MCG_C1_FRDIV(3);

    /* MCG_C4: DMX32=0,DRST_DRS=0 */
    MCG_C4 &= (uint8_t)~(uint8_t)0xE0U;

    /* MCG_C5: ??=0,PLLCLKEN=0,PLLSTEN=1,PRDIV=0x3, external clock reference = 8/4 = 2MHz */
    MCG_C5 = (uint8_t)0x23U;

    /* MCG_C6: LOLIE=0,PLLS=0,CME=0,VDIV=0x18 */
    MCG_C6 = (uint8_t)0x18U;

    while((MCG_S & MCG_S_IREFST_MASK) != 0x00U) { /* Check that the source of the FLL reference clock is the external reference clock. */
    }

    while((MCG_S & 0x0CU) != 0x08U) {    /* Wait until external reference clock is selected as MCG output */
    }

    /* Switch to PBE Mode */
    /* MCG_C1: CLKS=2,FRDIV=3,IREFS=0,IRCLKEN=0,IREFSTEN=0 */
    MCG_C1 = MCG_C1_CLKS(2) | MCG_C1_FRDIV(3);

    /* MCG_C2: ??=0,??=0,RANGE=2,HGO=0,EREFS=0,LP=0,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(2) | MCG_C2_HGO0_MASK | MCG_C2_EREFS0_MASK | MCG_C2_IRCS_MASK;

    /* MCG_C5: ??=0,PLLCLKEN=0,PLLSTEN=1,PRDIV=0x03 */
    MCG_C5 = (uint8_t)0x23U;

    /* MCG_C6: LOLIE=0,PLLS=1,CME=0,VDIV=0x18 */
    MCG_C6 = (uint8_t)0x58U;
    while((MCG_S & 0x0CU) != 0x08U) {    /* Wait until external reference clock is selected as MCG output */
    }
    while((MCG_S & MCG_S_LOCK0_MASK) == 0x00U) { /* Wait until locked */
    }
    /* Switch to PEE Mode */
    /* MCG_C1: CLKS=0,FRDIV=3,IREFS=0,IRCLKEN=0,IREFSTEN=0 */
    MCG_C1 = (uint8_t)0x18U;

    /* MCG_C2: ??=0,??=0,RANGE=2,HGO=0,EREFS=0,LP=0,IRCS=1 */
    MCG_C2 = MCG_C2_RANGE0(2) | MCG_C2_HGO0_MASK | MCG_C2_EREFS0_MASK | MCG_C2_IRCS_MASK;

    /* MCG_C5: ??=0,PLLCLKEN=0,PLLSTEN=1,PRDIV=0x03 */
    MCG_C5 = (uint8_t)0x23U;

    /* MCG_C6: LOLIE=0,PLLS=1,CME=0,VDIV=0x18 */
    MCG_C6 = (uint8_t)0x58U;
    while((MCG_S & 0x0CU) != 0x0CU) {    /* Wait until output of the PLL is selected */
    }
    /* MCG_C6: CME=1 */
    MCG_C6 |= (uint8_t)0x20U;            /* Enable the clock monitor */
    /*** End of PE initialization code after reset ***/

    return 0;
    /* end MCU_MKL25Z4 */
#elif (defined MCU_MK70F12)
    unsigned char frdiv_val;
    unsigned char temp_reg;
    unsigned char prdiv, vdiv;
    short i;
    int ref_freq;
    int pll_freq;

    // If using the PLL as MCG_OUT must check if the MCG is in FEI mode first
    if (mcgout_select)
    {
    	// check if in FEI mode
    	if (!((((MCG_S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) == 0x0) && // check CLKS mux has selcted FLL output
    			(MCG_S & MCG_S_IREFST_MASK) &&                                  // check FLL ref is internal ref clk
    			(!(MCG_S & MCG_S_PLLST_MASK))))                                 // check PLLS mux has selected FLL
    	{
    		return 0x1;                                                     // return error code
    	}
    } // if (mcgout_select)

    // Check if OSC1 is being used as a reference for the MCGOUT PLL
    // This requires a more complicated MCG configuration.
    // At this time (Sept 8th 2011) this driver does not support this option
    if (osc_select && mcgout_select)
    {
    	return 0x80; // Driver does not support using OSC1 as the PLL reference for the system clock on MCGOUT
    }

    // check external frequency is less than the maximum frequency
    if  (crystal_val > 60000000) {return 0x21;}

    // check crystal frequency is within spec. if crystal osc is being used as PLL ref
    if (erefs_val)
    {
    	if ((crystal_val < 8000000) || (crystal_val > 32000000)) {return 0x22;} // return 1 if one of the available crystal options is not available
    }

    // make sure HGO will never be greater than 1. Could return an error instead if desired.
    if (hgo_val > 0)
    {
    	hgo_val = 1; // force hgo_val to 1 if > 0
    }

    // Check PLL divider settings are within spec.
    if ((prdiv_val < 1) || (prdiv_val > 8)) {return 0x41;}
    if ((vdiv_val < 16) || (vdiv_val > 47)) {return 0x42;}

    // Check PLL reference clock frequency is within spec.
    ref_freq = crystal_val / prdiv_val;
    if ((ref_freq < 8000000) || (ref_freq > 32000000)) {return 0x43;}

    // Check PLL output frequency is within spec.
    pll_freq = (crystal_val / prdiv_val) * vdiv_val;
    if ((pll_freq < 180000000) || (pll_freq > 360000000)) {return 0x45;}

    // Determine if oscillator needs to be set up
    if (init_osc)
    {
    	// Check if the oscillator needs to be configured
    	if (!osc_select)
    	{
    		// configure the MCG_C2 register
    		// the RANGE value is determined by the external frequency. Since the RANGE parameter affects the FRDIV divide value
    		// it still needs to be set correctly even if the oscillator is not being used

    		temp_reg = MCG_C2;
    		temp_reg &= ~(MCG_C2_RANGE_MASK | MCG_C2_HGO_MASK | MCG_C2_EREFS_MASK); // clear fields before writing new values

    		if (crystal_val <= 8000000)
    		{
    			temp_reg |= (MCG_C2_RANGE(1) | (hgo_val << MCG_C2_HGO_SHIFT) | (erefs_val << MCG_C2_EREFS_SHIFT));
    		}
    		else
    		{
    			// On rev. 1.0 of silicon there is an issue where the the input bufferd are enabled when JTAG is connected.
    			// This has the affect of sometimes preventing the oscillator from running. To keep the oscillator amplitude
    			// low, RANGE = 2 should not be used. This should be removed when fixed silicon is available.
    			//temp_reg |= (MCG_C2_RANGE(2) | (hgo_val << MCG_C2_HGO_SHIFT) | (erefs_val << MCG_C2_EREFS_SHIFT));
    			temp_reg |= (MCG_C2_RANGE(1) | (hgo_val << MCG_C2_HGO_SHIFT) | (erefs_val << MCG_C2_EREFS_SHIFT));
    		}
    		MCG_C2 = temp_reg;
    	}
    	else
    	{
    		// configure the MCG_C10 register
    		// the RANGE value is determined by the external frequency. Since the RANGE parameter affects the FRDIV divide value
    		// it still needs to be set correctly even if the oscillator is not being used
    		temp_reg = MCG_C10;
    		temp_reg &= ~(MCG_C10_RANGE2_MASK | MCG_C10_HGO2_MASK | MCG_C10_EREFS2_MASK); // clear fields before writing new values
    		if (crystal_val <= 8000000)
    		{
    			temp_reg |= MCG_C10_RANGE2(1) | (hgo_val << MCG_C10_HGO2_SHIFT) | (erefs_val << MCG_C10_EREFS2_SHIFT);
    		}
    		else
    		{
    			// On rev. 1.0 of silicon there is an issue where the the input bufferd are enabled when JTAG is connected.
    			// This has the affect of sometimes preventing the oscillator from running. To keep the oscillator amplitude
    			// low, RANGE = 2 should not be used. This should be removed when fixed silicon is available.
    			//temp_reg |= MCG_C10_RANGE2(2) | (hgo_val << MCG_C10_HGO2_SHIFT) | (erefs_val << MCG_C10_EREFS2_SHIFT);
    			temp_reg |= MCG_C10_RANGE2(1) | (hgo_val << MCG_C10_HGO2_SHIFT) | (erefs_val << MCG_C10_EREFS2_SHIFT);
    		}
    		MCG_C10 = temp_reg;
    	} // if (!osc_select)
    } // if (init_osc)

    if (mcgout_select)
    {
    	// determine FRDIV based on reference clock frequency
    	// since the external frequency has already been checked only the maximum frequency for each FRDIV value needs to be compared here.
    	if (crystal_val <= 1250000) {frdiv_val = 0;}
    	else if (crystal_val <= 2500000) {frdiv_val = 1;}
    	else if (crystal_val <= 5000000) {frdiv_val = 2;}
    	else if (crystal_val <= 10000000) {frdiv_val = 3;}
    	else if (crystal_val <= 20000000) {frdiv_val = 4;}
    	else {frdiv_val = 5;}

    	// Select external oscillator and Reference Divider and clear IREFS to start ext osc
    	// If IRCLK is required it must be enabled outside of this driver, existing state will be maintained
    	// CLKS=2, FRDIV=frdiv_val, IREFS=0, IRCLKEN=0, IREFSTEN=0
    	temp_reg = MCG_C1;
    	temp_reg &= ~(MCG_C1_CLKS_MASK | MCG_C1_FRDIV_MASK | MCG_C1_IREFS_MASK); // Clear values in these fields
    	temp_reg = MCG_C1_CLKS(2) | MCG_C1_FRDIV(frdiv_val); // Set the required CLKS and FRDIV values
    	MCG_C1 = temp_reg;

    	// if the external oscillator is used need to wait for OSCINIT to set
    	if (erefs_val)
    	{
    		for (i = 0 ; i < 10000 ; i++)
    		{
    			if (MCG_S & MCG_S_OSCINIT_MASK) break; // jump out early if OSCINIT sets before loop finishes
    		}
    		if (!(MCG_S & MCG_S_OSCINIT_MASK)) return 0x23; // check bit is really set and return with error if not set
    	}

    	// wait for Reference clock Status bit to clear
    	for (i = 0 ; i < 2000 ; i++)
    	{
    		if (!(MCG_S & MCG_S_IREFST_MASK)) break; // jump out early if IREFST clears before loop finishes
    	}
    	if (MCG_S & MCG_S_IREFST_MASK) return 0x11; // check bit is really clear and return with error if not set

    	// Wait for clock status bits to show clock source is ext ref clk
    	for (i = 0 ; i < 2000 ; i++)
    	{
    		if (((MCG_S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) == 0x2) break; // jump out early if CLKST shows EXT CLK slected before loop finishes
    	}
    	if (((MCG_S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) != 0x2) return 0x1A; // check EXT CLK is really selected and return with error if not

    	// Now in FBE
    	// It is recommended that the clock monitor is enabled when using an external clock as the clock source/reference.
    	// It is enabled here but can be removed if this is not required.
    	MCG_C6 |= MCG_C6_CME_MASK;

    	// Select which PLL to enable
    	if (!pll_select)
    	{
    		// Configure PLL0
    		// Ensure OSC0 is selected as the reference clock
    		MCG_C5 &= ~MCG_C5_PLLREFSEL_MASK;
    		//Select PLL0 as the source of the PLLS mux
    		MCG_C11 &= ~MCG_C11_PLLCS_MASK;
    		// Configure MCG_C5
    		// If the PLL is to run in STOP mode then the PLLSTEN bit needs to be OR'ed in here or in user code.
    		temp_reg = MCG_C5;
    		temp_reg &= ~MCG_C5_PRDIV_MASK;
    		temp_reg |= MCG_C5_PRDIV(prdiv_val - 1);    //set PLL ref divider
    		MCG_C5 = temp_reg;

    		// Configure MCG_C6
    		// The PLLS bit is set to enable the PLL, MCGOUT still sourced from ext ref clk
    		// The loss of lock interrupt can be enabled by seperately OR'ing in the LOLIE bit in MCG_C6
    		temp_reg = MCG_C6; // store present C6 value
    		temp_reg &= ~MCG_C6_VDIV_MASK; // clear VDIV settings
    		temp_reg |= MCG_C6_PLLS_MASK | MCG_C6_VDIV(vdiv_val - 16); // write new VDIV and enable PLL
    		MCG_C6 = temp_reg; // update MCG_C6

    		// wait for PLLST status bit to set
    		for (i = 0 ; i < 2000 ; i++)
    		{
    			if (MCG_S & MCG_S_PLLST_MASK) break; // jump out early if PLLST sets before loop finishes
    		}
    		if (!(MCG_S & MCG_S_PLLST_MASK)) return 0x16; // check bit is really set and return with error if not set

    		// Wait for LOCK bit to set
    		for (i = 0 ; i < 2000 ; i++)
    		{
    			if (MCG_S & MCG_S_LOCK_MASK) break; // jump out early if LOCK sets before loop finishes
    		}
    		if (!(MCG_S & MCG_S_LOCK_MASK)) return 0x44; // check bit is really set and return with error if not set

    		// Use actual PLL settings to calculate PLL frequency
    		prdiv = ((MCG_C5 & MCG_C5_PRDIV_MASK) + 1);
    		vdiv = ((MCG_C6 & MCG_C6_VDIV_MASK) + 16);
    	}
    	else
    	{
    		// Configure PLL1
    		// Ensure OSC0 is selected as the reference clock
    		MCG_C11 &= ~MCG_C11_PLLREFSEL2_MASK;
    		//Select PLL1 as the source of the PLLS mux
    		MCG_C11 |= MCG_C11_PLLCS_MASK;
    		// Configure MCG_C11
    		// If the PLL is to run in STOP mode then the PLLSTEN2 bit needs to be OR'ed in here or in user code.
    		temp_reg = MCG_C11;
    		temp_reg &= ~MCG_C11_PRDIV2_MASK;
    		temp_reg |= MCG_C11_PRDIV2(prdiv_val - 1);    //set PLL ref divider
    		MCG_C11 = temp_reg;

    		// Configure MCG_C12
    		// The PLLS bit is set to enable the PLL, MCGOUT still sourced from ext ref clk
    		// The loss of lock interrupt can be enabled by seperately OR'ing in the LOLIE2 bit in MCG_C12
    		temp_reg = MCG_C12; // store present C12 value
    		temp_reg &= ~MCG_C12_VDIV2_MASK; // clear VDIV settings
    		temp_reg |=  MCG_C12_VDIV2(vdiv_val - 16); // write new VDIV and enable PLL
    		MCG_C12 = temp_reg; // update MCG_C12
    		// Enable PLL by setting PLLS bit
    		MCG_C6 |= MCG_C6_PLLS_MASK;

    		// wait for PLLCST status bit to set
    		for (i = 0 ; i < 2000 ; i++)
    		{
    			if (MCG_S2 & MCG_S2_PLLCST_MASK) break; // jump out early if PLLST sets before loop finishes
    		}
    		if (!(MCG_S2 & MCG_S2_PLLCST_MASK)) return 0x17; // check bit is really set and return with error if not set

    		// wait for PLLST status bit to set
    		for (i = 0 ; i < 2000 ; i++)
    		{
    			if (MCG_S & MCG_S_PLLST_MASK) break; // jump out early if PLLST sets before loop finishes
    		}
    		if (!(MCG_S & MCG_S_PLLST_MASK)) return 0x16; // check bit is really set and return with error if not set

    		// Wait for LOCK bit to set
    		for (i = 0 ; i < 2000 ; i++)
    		{
    			if (MCG_S2 & MCG_S2_LOCK2_MASK) break; // jump out early if LOCK sets before loop finishes
    		}
    		if (!(MCG_S2 & MCG_S2_LOCK2_MASK)) return 0x44; // check bit is really set and return with error if not set

    		// Use actual PLL settings to calculate PLL frequency
    		prdiv = ((MCG_C11 & MCG_C11_PRDIV2_MASK) + 1);
    		vdiv = ((MCG_C12 & MCG_C12_VDIV2_MASK) + 16);
    	} // if (!pll_select)

    	// now in PBE

    	MCG_C1 &= ~MCG_C1_CLKS_MASK; // clear CLKS to switch CLKS mux to select PLL as MCG_OUT

    	// Wait for clock status bits to update
    	for (i = 0 ; i < 2000 ; i++)
    	{
    		if (((MCG_S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) == 0x3) break; // jump out early if CLKST = 3 before loop finishes
    	}
    	if (((MCG_S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) != 0x3) return 0x1B; // check CLKST is set correctly and return with error if not

    	// Now in PEE
    }
    else
    {
    	// Setup PLL for peripheral only use
    	if (pll_select)
    	{
    		// Setup and enable PLL1
    		// Select ref source
    		if (osc_select)
    		{
    			MCG_C11 |= MCG_C11_PLLREFSEL2_MASK; // Set select bit to choose OSC1
    		}
    		else
    		{
    			MCG_C11 &= ~MCG_C11_PLLREFSEL2_MASK; // Clear select bit to choose OSC0
    		}
    		// Configure MCG_C11
    		// If the PLL is to run in STOP mode then the PLLSTEN2 bit needs to be OR'ed in here or in user code.
    		temp_reg = MCG_C11;
    		temp_reg &= ~MCG_C11_PRDIV2_MASK;
    		temp_reg |= MCG_C11_PRDIV2(prdiv_val - 1);    //set PLL ref divider
    		MCG_C11 = temp_reg;

    		// Configure MCG_C12
    		// The loss of lock interrupt can be enabled by seperately OR'ing in the LOLIE2 bit in MCG_C12
    		temp_reg = MCG_C12; // store present C12 value
    		temp_reg &= ~MCG_C12_VDIV2_MASK; // clear VDIV settings
    		temp_reg |=  MCG_C12_VDIV2(vdiv_val - 16); // write new VDIV and enable PLL
    		MCG_C12 = temp_reg; // update MCG_C12
    		// Now enable the PLL
    		MCG_C11 |= MCG_C11_PLLCLKEN2_MASK; // Set PLLCLKEN2 to enable PLL1

    		// Wait for LOCK bit to set
    		for (i = 0 ; i < 2000 ; i++)
    		{
    			if (MCG_S2 & MCG_S2_LOCK2_MASK) break; // jump out early if LOCK sets before loop finishes
    		}
    		if (!(MCG_S2 & MCG_S2_LOCK2_MASK)) return 0x44; // check bit is really set and return with error if not set

    		// Use actual PLL settings to calculate PLL frequency
    		prdiv = ((MCG_C11 & MCG_C11_PRDIV2_MASK) + 1);
    		vdiv = ((MCG_C12 & MCG_C12_VDIV2_MASK) + 16);
    	}
    	else
    	{
    		// Setup and enable PLL0
    		// Select ref source
    		if (osc_select)
    		{
    			MCG_C5 |= MCG_C5_PLLREFSEL_MASK; // Set select bit to choose OSC1
    		}
    		else
    		{
    			MCG_C5 &= ~MCG_C5_PLLREFSEL_MASK; // Clear select bit to choose OSC0
    		}
    		// Configure MCG_C5
    		// If the PLL is to run in STOP mode then the PLLSTEN bit needs to be OR'ed in here or in user code.
    		temp_reg = MCG_C5;
    		temp_reg &= ~MCG_C5_PRDIV_MASK;
    		temp_reg |= MCG_C5_PRDIV(prdiv_val - 1);    //set PLL ref divider
    		MCG_C5 = temp_reg;

    		// Configure MCG_C6
    		// The loss of lock interrupt can be enabled by seperately OR'ing in the LOLIE bit in MCG_C6
    		temp_reg = MCG_C6; // store present C6 value
    		temp_reg &= ~MCG_C6_VDIV_MASK; // clear VDIV settings
    		temp_reg |=  MCG_C6_VDIV(vdiv_val - 16); // write new VDIV and enable PLL
    		MCG_C6 = temp_reg; // update MCG_C6
    		// Now enable the PLL
    		MCG_C5 |= MCG_C5_PLLCLKEN_MASK; // Set PLLCLKEN to enable PLL0

    		// Wait for LOCK bit to set
    		for (i = 0 ; i < 2000 ; i++)
    		{
    			if (MCG_S & MCG_S_LOCK_MASK) break; // jump out early if LOCK sets before loop finishes
    		}
    		if (!(MCG_S & MCG_S_LOCK_MASK)) return 0x44; // check bit is really set and return with error if not set

    		// Use actual PLL settings to calculate PLL frequency
    		prdiv = ((MCG_C5 & MCG_C5_PRDIV_MASK) + 1);
    		vdiv = ((MCG_C6 & MCG_C6_VDIV_MASK) + 16);
    	} // if (pll_select)

    } // if (mcgout_select)

    return (((crystal_val / prdiv) * vdiv) / 2); //MCGOUT equals PLL output frequency/2
    /* MCU_MK70F12 */
#else
    /*
     * First move to FBE mode
     * Enable external oscillator, RANGE=0, HGO=, EREFS=, LP=, IRCS=
    */
	#if ((defined MCU_MK60N512VMD100) || (defined MCU_MK53N512CMD100))
		MCG_C2 = 0;
	#else
		#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
			MCG_C2 = MCG_C2_RANGE0(2) | MCG_C2_HGO0_MASK | MCG_C2_EREFS0_MASK | MCG_C2_IRCS_MASK;
		#else
			MCG_C2 = MCG_C2_RANGE(2) | MCG_C2_HGO_MASK | MCG_C2_EREFS_MASK|MCG_C2_IRCS_MASK;
		#endif
	#endif

    /* Select external oscillator and Reference Divider and clear IREFS
     * to start external oscillator
     * CLKS = 2, FRDIV = 3, IREFS = 0, IRCLKEN = 0, IREFSTEN = 0
     */
    MCG_C1 = MCG_C1_CLKS(2) | MCG_C1_FRDIV(3);

	/* Wait for oscillator to initialize */
	#if((defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7))
		while (!(MCG_S & MCG_S_OSCINIT0_MASK)){};
	#elif defined (MCU_MK40N512VMD100)
		while (!(MCG_S & MCG_S_OSCINIT_MASK)){};
	#endif

	#ifndef MCU_MK20D5
    	/* Wait for Reference Clock Status bit to clear */
    	while (MCG_S & MCG_S_IREFST_MASK) {};
	#endif

    /* Wait for clock status bits to show clock source
     * is external reference clock */
    while (((MCG_S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) != 0x2) {};

    /* Now in FBE
     * Configure PLL Reference Divider, PLLCLKEN = 0, PLLSTEN = 0, PRDIV = 0x18
     * The crystal frequency is used to select the PRDIV value.
     * Only even frequency crystals are supported
     * that will produce a 2MHz reference clock to the PLL.
     */
	#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
    	MCG_C5 = MCG_C5_PRDIV0(BSP_REF_CLOCK_DIV - 1) | MCG_C5_PLLCLKEN0_MASK;
	#else
    	MCG_C5 = MCG_C5_PRDIV(BSP_REF_CLOCK_DIV - 1);
	#endif

    /* Ensure MCG_C6 is at the reset default of 0. LOLIE disabled,
     * PLL disabled, clock monitor disabled, PLL VCO divider is clear
     */
    MCG_C6 = 0;


    /* Calculate mask for System Clock Divider Register 1 SIM_CLKDIV1 */
	#if (defined MCU_MK20D5) || (defined MCU_MK40D7)
		SIM_CLKDIV1 =   SIM_CLKDIV1_OUTDIV1(BSP_CORE_DIV - 1) | 	/* core/system clock */
						SIM_CLKDIV1_OUTDIV2(BSP_BUS_DIV - 1)  | 	/* peripheral clock; */
						SIM_CLKDIV1_OUTDIV4(BSP_FLASH_DIV - 1);     /* flash clock */
	#else
		SIM_CLKDIV1 =  	SIM_CLKDIV1_OUTDIV1(BSP_CORE_DIV    - 1) |
						SIM_CLKDIV1_OUTDIV2(BSP_BUS_DIV     - 1) |
						SIM_CLKDIV1_OUTDIV3(BSP_FLEXBUS_DIV - 1) |
						SIM_CLKDIV1_OUTDIV4(BSP_FLASH_DIV   - 1);
	#endif

   /* Set the VCO divider and enable the PLL,
     * LOLIE = 0, PLLS = 1, CME = 0, VDIV = 2MHz * BSP_CLOCK_MUL
     */
	#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		MCG_C6 = MCG_C6_PLLS_MASK | MCG_C6_VDIV0(BSP_CLOCK_MUL - 24);
	#else
		MCG_C6 = MCG_C6_PLLS_MASK | MCG_C6_VDIV(BSP_CLOCK_MUL - 24);
	#endif

    /* Wait for PLL status bit to set */
    while (!(MCG_S & MCG_S_PLLST_MASK)) {};

    /* Wait for LOCK bit to set */
	#if(defined MCU_MK20D5) || (defined MCU_MK20D7) || (defined MCU_MK40D7)
		while (!(MCG_S & MCG_S_LOCK0_MASK)){};
	#else
		while (!(MCG_S & MCG_S_LOCK_MASK)) {};
	#endif

    /* Now running PBE Mode */

    /* Transition into PEE by setting CLKS to 0
     * CLKS=0, FRDIV=3, IREFS=0, IRCLKEN=0, IREFSTEN=0
     */
    MCG_C1 &= ~MCG_C1_CLKS_MASK;

    /* Wait for clock status bits to update */
    while (((MCG_S & MCG_S_CLKST_MASK) >> MCG_S_CLKST_SHIFT) != 0x3) {};

	#if(defined MCU_MK20D5)
    	/* Enable the ER clock of oscillators */
    	OSC0_CR = OSC_CR_ERCLKEN_MASK | OSC_CR_EREFSTEN_MASK;
	#else
    	/* Enable the ER clock of oscillators */
    	OSC_CR = OSC_CR_ERCLKEN_MASK | OSC_CR_EREFSTEN_MASK;
    #endif

    /* Now running PEE Mode */
    return 0;
#endif
}

/* EOF */
