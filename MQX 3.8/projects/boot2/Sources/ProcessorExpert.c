/** ###################################################################
**     Filename    : ProcessorExpert.c
**     Project     : ProcessorExpert
**     Processor   : MK60DN512VMC10
**     Version     : Driver 01.01
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2013-02-15, 13:20, # CodeGen: 0
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
#include "USB_OTG1.h"
#include "FLASH1.h"
#include "SPI2_Flash.h"
#include "Sleep_Timer_LDD.h"
#include "TU2.h"
#include "ADC0_Vsense.h"
#include "PORTA_GPIO.h"
#include "PORTE_GPIO.h"
#include "PORTC_GPIO.h"
#include "PORTD_GPIO.h"
#include "CRC1.h"
#include "whistle_random.h"
#include "I2C2.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"
// #include <stdio.h>
#include "wson_flash.h"
#include "adc_sense.h"
#include "pmem.h"
#include "corona_gpio.h"

#define NUM_ADC_SAMPLES		8

/* User includes (#include below this line is not maintained by Processor Expert) */
extern int boot2_main( cable_t cable );
extern void goto_app();

static void _init_pgood_gpio(void)
{
	int i = 25;
	
	/* PORTE_PCR2: ISF=0,IRQC=0x0A,MUX=1,PE=1,PS=1 */
	PORTE_PCR2 = (uint32_t)((PORTE_PCR2 & (uint32_t)~(uint32_t)(
				PORT_PCR_ISF_MASK |
				PORT_PCR_IRQC(0x05) |
				PORT_PCR_MUX(0x06)
			   )) | (uint32_t)(
				PORT_PCR_IRQC(0x0A) |
				PORT_PCR_MUX(0x01) |
				PORT_PCR_PE_MASK |
				PORT_PCR_PS_MASK
			   ));     
	  
	  while(i--){} // stupid delay to give some time for pull-up.
}

/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
	uint16_t raw = 0, natural = 0;
	uint32_t sum = 0, avg, num_samples = NUM_ADC_SAMPLES, pgood;
	cable_t cable = CABLE_CUSTOMER;
	
    /*
     *   We need to read PGOOD every time we go through boot2, so bring it on up.
     */
    _init_pgood_gpio();
    
    /*
     *   PORT A GPIO Init so that we can get UID_EN support.
     */
    PORTA_GPIO_Init();
	
	/*
	 *   If pmem is not valid, clear it explicitly,
	 *     to avoid validating a totally bogus pmem in boot2.
	 *     
	 *   Validating a cleared pmem in boot2 is allowed by design.
	 */
	if( 0 == pmem_is_data_valid() )
	{
	    pmem_clear();
	}
	
	/*
	 *   Make sure USB is connected before we try to get the cable type.
	 */
	pgood = GPIO_READ(GPIOE_PDIR, 2)
	if( 0 == pgood )
	{
		/*
		 *   Sample measurement and take an average.
		 */
		while(num_samples--)
		{
			adc_sense(ADC0_Vsense_Corona_UID, &raw, &natural);
			sum += raw;
		}
		
		avg = sum / NUM_ADC_SAMPLES;
		
		/*
		 *   If we are on a factory debug cable, stay in boot2 no matter what.
		 */
		
		cable = adc_get_cable(avg);
	}

	/*
	 *    NOTE:  We do not want to do any peripheral / GPIO initialization if this was
	 *           a graceful reboot initiated by the app image.  Therefore, if the CRC
	 *           looks good, just gto to the app directly.
	 */
	if( (CABLE_FACTORY_DBG != cable ) && 
		(pmem_is_data_valid())        &&
		(PV_REBOOT_REASON_FWU_COMPLETED != __SAVED_ACROSS_BOOT.reboot_reason) &&
		(!pmem_flag_get(PV_FLAG_FWU)))  // If FWU flag is set, allow going through the boot2 process,
		                                //    so that we can set the success flag, & finish updating FW.
	{
		goto_app();
	}
	
	/*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
	PE_low_level_init();
	/*** End of Processor Expert internal initialization.                    ***/
	
	return boot2_main( cable );
	
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
