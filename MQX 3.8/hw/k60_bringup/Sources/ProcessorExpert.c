/** ###################################################################
**     Filename    : ProcessorExpert.c
**     Project     : ProcessorExpert
**     Processor   : MK60DN512VMC10
**     Version     : Driver 01.01
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2013-01-30, 14:02, # CodeGen: 0
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/* MODULE ProcessorExpert */


/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "Events.h"
#include "SPI0_WIFI.h"
#include "PORTA_GPIO.h"
#include "PORTB_GPIO.h"
#include "SPI1_Accel.h"
#include "DISPLAY_SYNC_PWM0.h"
#include "TU1.h"
#include "I2C1.h"
#include "SPI2_Flash.h"
#include "UART3_WIFI.h"
#include "PORTD_GPIO.h"
#include "PORTE_GPIO.h"
#include "UART0_BT.h"
#include "UART5_DBG.h"
#include "Sleep_Timer_LDD.h"
#include "TU2.h"
#include "ADC0_Vsense.h"
#include "PORTB_GPIO_LDD.h"
#include "PORTC_GPIO_LDD.h"
#include "PORTD_GPIO_LDD.h"
#include "PORTE_GPIO_LDD.h"
#include "USB_CORONA.h"
#include "RNG1.h"
#include "PORTC_GPIO.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"

/* User includes (#include below this line is not maintained by Processor Expert) */
#include "button.h"
#include "lis3dh_accel.h"
#include "bu26507_display.h"
#include "wson_flash.h"
#include "apple_mfi.h"
#include "bt_uart.h"
#include "dbg_uart.h"
#include "corona_errors.h"
#include "corona_console.h"
#include "corona_gpio.h"
#include "sleep_timer.h"
#include "jtag_console.h"
#include "pwr_mgr.h"
#include "RNG1.h"
#include <stdio.h>

#define PRINTF_BUFSIZ	128
char g_printf_buffer[PRINTF_BUFSIZ];
extern void main_k60_usb(void);
extern unsigned long g_32khz_test_flags;
extern uint8 g_pgood;

uint8_t apph[];

// App Header
// Pragma to place the App header field on correct location defined in linker file.
#pragma define_section appheader ".appheader" far_abs R

__declspec(appheader) uint8_t apph[0x20] = {
    // Name [0,1,2]
    'A', 'P', 'P',      // 0x41, 0x50, 0x50
    // Version [3,4,5]
    0x00, 0x01, 0x00,
    // Checksum of Name and Version [6]
    0x1EU,
    // Success, Update, 3 Attempt flags, BAD flag [7,8,9,A,B,C]
    0x55U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
    // SAVD [D], Reserved [E-F]
    0xFFU, 0xFFU, 0xFFU,
    // Version tag [10-1F]
    'A','L','P','H','A','_','1',0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,0xFFU, 0xFFU, 0xFFU
};


/*
 *   This is the "production" console, with will choose either USB console or JTAG.
 *     The preference is USB, but if the PGOOD signal is not present, it will use JTAG instead.
 *     USB is typically used, JTAG is really only used for the "bed of nails" fixture.
 *
 *     Since this is the "production" console, we lazily don't handle the UART (FTDI) case.
 */
void production_console_init(void)
{
    int i;
	const uint32_t PGOOD_B_MASK = (uint32_t)0x00000004;
	
	if( PGOOD_B_MASK == (GPIOE_PDIR & PGOOD_B_MASK) )
	{
	    while( g_pgood /*PGOOD_B_MASK == (GPIOE_PDIR & PGOOD_B_MASK)*/ )
	    {
	        /*
	         *   In this case, USB is not plugged in, so poll until we see it.
	         */
	        
	        if( g_cstate != BU_NUM_COLORS )
	        {
	            bu26507_color(g_cstate);
	            g_cstate = BU_NUM_COLORS;   // set it back to the bogus state, so we don't keep setting colors...
	        }
	        
	        sleep(5);
	    }    
	}
	
	// PGOOD indicator to user.
    //for(i=0; i < 2; i++)
    {
          bu26507_color(BU_COLOR_YELLOW);
          sleep(50);
          bu26507_color(BU_COLOR_OFF);
          //sleep(10);
    }
	
	//sleep(50); // give some PGOOD "debouncing" delay.

    /*
     *   In this case, USB is plugged in, so try the USB CDC console.
     */
    main_k60_usb();
}

/*
 *   Test the random number generator built into hardware on the K60.
 */
static void _WHISTLE_RANDOM_NUMBER_TEST(void)
{
  LDD_TDeviceData *DeviceData;
  LDD_TError Error;
  uint32_t number;
  uint32_t status;
  uint32_t i;

  DeviceData = RNG1_Init(NULL);                                 /* Initialization of RNG component */
  sleep( 50 );

#if 0
  /* Wait for self-test and initial seed */
  do 
  {
    status = RNG1_GetStatus(DeviceData);
  } 
  while ( (!(status & LDD_RNG_STATUS_NEW_SEED_DONE)) &&
          (!(status & LDD_RNG_STATUS_SELF_TEST_DONE)) );
#endif
  
#if 0
  // Function not supported.
  if (RNG1_GetError != LDD_RNG_NO_ERROR) 
  {
    /* RNG failed, reset module and try re-initialization. */
      printf("RNG Error.\n");
  }
#endif
  
  /* Use RNG to get random numbers as needed */
  for( i=0; i < 30; i++ ) 
  {
    Error = RNG1_GetRandomNumber(DeviceData, &number);          /* Read random data from the FIFO */
    
    /*
     *   TODO:  Check the status register for real errors in the final implementation to assure truly random values.
     */
    
    if (Error == ERR_FAULT) 
    {
      /* RNG failed, reset module and try re-initialization. */
        printf("ERR 0x%x getting random number\n", Error);
    }
    else
    {
        printf("Random number (%i) --> 0x%x\n", i, number);
    }
    sleep( 26 );  // we really only need to wait 256 clock cycles, so this is big time overkill.
  }
}

/*
 *   Tests to make sure 32KHz clock is good with all the peripherals, prior to
 *      booting up to 26MHz.
 */
static void _test_32khz_pre_usb(void)
{
    int i;
    
    sleep(100);  // extra time for BT 32KHz clock to stabilize and hold BT chip in reset.
    PORTB_GPIO_LDD_SetFieldValue(NULL, BT_PWDN_B, 1);
    sleep(100);  // time for BT chip to boot up.
    
    for(i = 0; i < 3; i++)   // Run through essentially testbrd, this many times at 32KHz.
    {
        if(bt_uart_comm_ok(BT_BAUD_DEFAULT_32KHz_Src))
        {
            g_32khz_test_flags |= TEST_32KHz_FLAG_BT;
        }

        if(a4100_wifi_comm_ok())
        {
            g_32khz_test_flags |= TEST_32KHz_FLAG_WIFI;
        }

        if(lis3dh_comm_ok())
        {
            g_32khz_test_flags |= TEST_32KHz_FLAG_ACC;
        }

        if(mfi_comm_ok())
        {
            g_32khz_test_flags |= TEST_32KHz_FLAG_MFI;
        }

        if(wson_comm_ok())
        {
            g_32khz_test_flags |= TEST_32KHz_FLAG_FLASH;
        }
    }
    
    if(bt_stress())
    {
        g_32khz_test_flags |= TEST_32KHz_FLAG_BT_STRESS;
    }
}


/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */
    int i;

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
    
    /*
     *   Default init uses external 32KHz clock so we can stress test before USB enumeration.
     *     Perform a BT stress test to make sure 32KHz clock is OK.
     */
    _test_32khz_pre_usb();
    
    /*
     *   Now switch to using 26MHz for maximum USB performance.
     */
  __init_hardware_26MHz();
  PE_low_level_init_26MHz();
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  /* For example: for(;;) { } */
  
  // This is required so that the button will be able to go through the colors.
  //for(i=0; i < 15; i++)
  {
      bu26507_display_test();
      sleep(40);
      bu26507_color(BU_COLOR_OFF);
      //sleep(40);
  }

#if 0
  if( setvbuf(stdout, g_printf_buffer, _IOFBF, PRINTF_BUFSIZ) )
  {
	  //printf("There was a problem setting setvbuf!\n");
  }
  setbuf(stdout, g_printf_buffer);
#endif

#if ENABLE_TRACE_CLK_PORTE0
  printf("Enabling TRACE_CLKOUT via PTE0...\n");
  PORTE_PCR0 = (uint32_t)((PORTE_PCR0 & (uint32_t)~(uint32_t)(
                 PORT_PCR_ISF_MASK |
                 PORT_PCR_MUX(0x06)
                )) | (uint32_t)(
                 PORT_PCR_MUX(0x05)
                ));
  SIM_SOPT2 &= ~(SIM_SOPT2_TRACECLKSEL_MASK); // clear this so we see MCGOUTCLK instead of core/system clock.
#endif

  //button_init();
//  bu26507_display_test();
  //lis3dh_accel_test();
  //sleep_timer_test(5);
  //sleep_timer_test(10);
  //corona_gpio_test();
  //a4100_wifi_test();
  //bt_uart_test();
  //apple_mfi_test();
//  adc_sense_test();
//  wson_flash_test();
//  wson_flash_torture(); // warning this test takes a LONG time, because it writes to the entire SPI Flash memory and checks for errors.

  //pwr_mgr_state( PWR_STATE_SHIP ); // just so we don't have to muck around with the UART to put it into ship mode.

  //_WHISTLE_RANDOM_NUMBER_TEST();   // this will run the random number example test.
  
#define PRODUCTION_CONSOLE
#ifdef PRODUCTION_CONSOLE
  production_console_init();
#else
  dbg_uart_test();
#endif

  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;){}
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/



/* END ProcessorExpert */
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.0 [05.03]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
