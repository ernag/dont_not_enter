/** ###################################################################
**     Filename    : ProcessorExpert.c
**     Project     : ProcessorExpert
**     Processor   : MK22DN512VMC5
**     Version     : Driver 01.01
**     Compiler    : GNU C Compiler
**     Date/Time   : 2013-01-23, 13:20, # CodeGen: 0
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
#include "GPIO1.h"
/* Including shared modules, which are used for whole project */
#include "PE_Types.h"
#include "PE_Error.h"
#include "PE_Const.h"
#include "IO_Map.h"

/* User includes (#include below this line is not maintained by Processor Expert) */
#include <stdio.h>
#include "hidef.h"
#include "Wdt_kinetis.h"
#include "hidef.h"
#include "usb_dci_kinetis.h"
#include "usb_dciapi.h"

extern uint_8 TimerQInitialize(uint_8 ControllerId);
extern void TestApp_Init(void);
extern void TestApp_Task(void);

static void USB_Init(uint_8 controller_ID);

LDD_TDeviceData *ptc16_osc_enable;

/*lint -save  -e970 Disable MISRA rule (6.3) checking. */
int main(void)
/*lint -restore Enable MISRA rule (6.3) checking. */
{
  /* Write your local variable definition here */
  void Cpu_SetBASEPRI(uint32_t Level);
  int i;

  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  PE_low_level_init();
  /*** End of Processor Expert internal initialization.                    ***/

  
  printf ("Hello World from usb_cdc_wpt!!!\n");
  
  // GPIO1_SetFieldBits(ptc16_osc_enable, ptc16, 1);
  
  for (i = 0; i < 1000000; i++);
  
  printf ("Now changing to the xternal 32KHz osc & 48MHz USB clock...\n");
  
  DisableInterrupts;
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
  MCG_C7 |= MCG_C7_OSCSEL_MASK;                                                   
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
 
  // Enable interrupts
  // Cpu_SetBASEPRI(0U);
  EnableInterrupts;

  printf ("Clock re-config successful!!\n");  // or this wouldn't print...
  printf ("Init USB...\n");
  USB_Init(MAX3353);

  (void)TimerQInitialize(0);
  
  /* Initialize the USB Test Application */
  printf ("Init the USB test app...\n");
  TestApp_Init();

  while(TRUE)
  {
    Watchdog_Reset();
    /* Call the application task */
    TestApp_Task();
  }
  
  
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
    SIM_CLKDIV2 &= (uint32_t)(~(SIM_CLKDIV2_USBFRAC_MASK | SIM_CLKDIV2_USBDIV_MASK));
    /* Configure USBFRAC = 0, USBDIV = 0 => frq(USBout) = 1 / 1 * frq(PLLin) */
    SIM_CLKDIV2 = SIM_CLKDIV2_USBDIV(0);

    /* 1. Configure USB to be clocked from PLL */
    SIM_SOPT2 |= SIM_SOPT2_USBSRC_MASK | SIM_SOPT2_PLLFLLSEL_MASK;

    /* 3. Enable USB-OTG IP clocking */
    SIM_SCGC4 |= (SIM_SCGC4_USBOTG_MASK);      

    /* old documentation writes setting this bit is mandatory */
    USB0_USBTRC0 = 0x40;
    
    /* Configure enable USB regulator for device */
    SIM_SOPT1 |= SIM_SOPT1_USBREGEN_MASK;

    NVICICPR2 = (1 << 9);  /* Clear any pending interrupts on USB */
    NVICISER2 |= (1 << 9);  /* Enable interrupts from USB module */             
  } // MAX3353
}
