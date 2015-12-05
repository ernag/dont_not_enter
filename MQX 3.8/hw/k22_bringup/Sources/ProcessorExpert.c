/** ###################################################################
**     Filename    : ProcessorExpert.c
**     Project     : ProcessorExpert
**     Processor   : MK20DN512VMC10
**     Version     : Driver 01.00
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2013-01-10, 15:34, # CodeGen: 0
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
#include "PORTD_GPIO.h"
#include "PORTC_GPIO.h"
#include "PORTE_GPIO.h"
#include "PORTA_GPIO.h"
#include "SPI1_Accel.h"
#include "UART5_DBG.h"
#include "SPI0_WIFI.h"
#include "I2C_Display.h"
#include "Sleep_Timer_LDD.h"
#include "TU1.h"
#include "UART2_BT_UART.h"
#include "BT_PWDN_B.h"
#include "WIFI_PD_B.h"
#include "ADC0_Vsense.h"
#include "SPI2_Flash.h"
#include "PORTB_GPIO.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"

/* User includes (#include below this line is not maintained by Processor Expert) */
#include "dbg_led.h"
#include "k2x_errors.h"
#include "lis3dh_accel.h"
#include "button.h"
#include "sleep_timer.h"
#include "dbg_uart.h"
#include "a4100_wifi.h"
#include "bu26507_display.h"
#include "adc_sense.h"
#include "wson_flash.h"
#include "k22_irq.h"

void main(void)
{
  /* Write your local variable definition here */

  printf("K2x Board Bring Up\n");
  printf("Performing low level init\n");

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  /* Write your code here */
  /* For example: for(;;) { } */
  
  k22_enable_irq(IRQ_PORT_C, VECTOR_PORT_C, PORTC_GPIO_ISR);
  button_init();
  bu26507_display_test();
  bu26507_init();
  //dbg_led_test();
  //lis3dh_accel_test();
  //sleep_timer_test(5);
  //sleep_timer_test(10);
  //corona_gpio_test();
  //a4100_wifi_test();
  //bt_uart_test();
  //apple_mfi_test();
  //adc_sense_test();
  //wson_flash_test();
  //wson_flash_torture(); // warning this test takes a LONG time, because it writes to the entire SPI Flash memory and checks for errors.
  //dbg_uart_test();
  //main_k60_usb(); // this will fire up the USB CDC demo...

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
**     This file was created by Processor Expert 5.3 [05.01]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
