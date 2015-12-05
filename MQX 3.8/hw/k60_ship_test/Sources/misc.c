/*
 * misc.c
 *
 *  Created on: Feb 28, 2013
 *      Author: SpottedLabs
 */

#include "IO_Map.h"
#include "PE_Types.h"

/***********************************************************************/
/*
 * Configures the ARM system control register for STOP (deep sleep) mode
 * and then executes the WFI instruction to enter the mode.
 *
 * Parameters:
 * none
 *
 * Note: Might want to change this later to allow for passing in a parameter
 *       to optionally set the sleep on exit bit.
 */

void stop (void)
{
	/* Set the SLEEPDEEP bit to enable deep sleep mode (STOP) */
	SCB_SCR |= SCB_SCR_SLEEPDEEP_MASK;	

	/* WFI instruction will start entry into STOP mode */
	asm("WFI");
}

/********************************************************************/
/* VLPR mode exit routine. Puts the processor into normal run mode
 * from VLPR mode. You can transition from VLPR to normal run using
 * this function or an interrupt with LPWUI set. The enable_lpwui() 
 * and disable_lpwui() functions can be used to set LPWUI to the 
 * desired option prior to calling enter_vlpr().
 *
 * Mode transitions:
 * VLPR -> RUN
 *
 * Parameters:
 * none
 */
void exit_vlpr(void)
{
    /* check to make sure in VLPR before exiting    */
    if  ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4) {
      
       /* Clear RUNM */
       SMC_PMCTRL &= ~(SMC_PMCTRL_RUNM(0x3));
                      
       /* Wait for normal RUN regulation mode to be confirmed */                   
       // 0 MCU is not in run regulation mode
       // 1 MCU is in run regulation mode
       while(!(PMC_REGSC & PMC_REGSC_REGONS_MASK)){
    	 printf(" \n[exit_vlpr] Waiting on REGONS bit to set to indicate Run regulation mode ! ");
         } 
       
    }  //if in VLPR mode
     // else if not in VLPR ignore call
}

/* function: llwu_configure

   description: Set up the LLWU for wakeup the MCU from LLS and VLLSx modes 
   from the selected pin or module.
   
   inputs:
   pin_en - unsigned integer, bit position indicates the pin is enabled.  
            More than one bit can be set to enable more than one pin at a time.  
   
   rise_fall - 0x00 = External input disabled as wakeup
               0x01 - External input enabled as rising edge detection
               0x02 - External input enabled as falling edge detection
               0x03 - External input enablge as any edge detection
   module_en - unsigned char, bit position indicates the module is enabled.  
               More than one bit can be set to enabled more than one module                   
   
   for example:  if bit 0 and 1 need to be enabled as rising edge detect call this  routine with
   pin_en = 0x0003 and rise_fall = 0x02
   
   Note: to set up one set of pins for rising and another for falling, 2 calls to this 
         function are required, 1st for rising then the second for falling.
   
*/
void llwu_configure(unsigned int pin_en, unsigned char rise_fall, unsigned char module_en ) {
    uint8_t temp;
    
    SIM_SCGC4 |= SIM_SCGC4_LLWU_MASK;
//   used on rev 1.4 of P2
    temp = LLWU_PE1;
    if( pin_en & 0x0001)
    {
        temp |= LLWU_PE1_WUPE0(rise_fall);
        printf(" LLWU configured pins PTE1/UART1_RX/I2C1_SCL /SPI1_SIN is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF0_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0002)
    {
        temp |= LLWU_PE1_WUPE1(rise_fall);
        printf(" LLWU configured pins PTE2/SPI1_SCK/SDHC0_DCLK is wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF1_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0004)
    {
        temp |= LLWU_PE1_WUPE2(rise_fall);
        printf(" LLWU configured pins PTE4/SPI1_PCS0/SDHC0_D3 is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF2_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0008)
    {
        temp |= LLWU_PE1_WUPE3(rise_fall);
        printf(" LLWU configured pins PTA4/FTM0_CH1/NMI/EZP_CSis LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF3_MASK;   // write one to clear the flag
    }
    LLWU_PE1 = temp;

    temp = LLWU_PE2;
    if( pin_en & 0x0010)
    {
        temp |= LLWU_PE2_WUPE4(rise_fall);
        printf(" LLWU configured pins PTA13/FTM1_CH1 /FTM1_QD_PHB is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF4_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0020)
    {
        temp |= LLWU_PE2_WUPE5(rise_fall);
        printf(" LLWU configured pins PTB0/I2C0_SCL/FTM1_CH0/FTM1_QD_PHA is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF5_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0040)
    {
        temp |= LLWU_PE2_WUPE6(rise_fall);
        printf(" LLWU configured pins PTC1/UART1_RTS/FTM0_CH0 is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF6_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0080)
    {
        temp |= LLWU_PE2_WUPE7(rise_fall);
        printf(" LLWU configured pins PTC3/UART1_RX/FTM0_CH2 is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF7_MASK;   // write one to clear the flag
    }
    LLWU_PE2 = temp;

    temp = LLWU_PE3;
    if( pin_en & 0x0100)
    {
        temp |= LLWU_PE3_WUPE8(rise_fall);
        printf(" LLWU configured pins PTC4/SPI0_PCS0/FTM0_CH3 is LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF8_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0200)
    {
        temp |= LLWU_PE3_WUPE9(rise_fall);
        printf(" LLWU configured pins PTC5/SPI0_SCK/I2S0_RXD0 is LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF9_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0400)
    {
        temp |= LLWU_PE3_WUPE10(rise_fall);
        printf(" LLWU configured pins PTC6/PDB0_EXTRG to be an LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF10_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0800)
    {
        temp |= LLWU_PE3_WUPE11(rise_fall);
        printf(" LLWU configured pins PTC11/I2S0_RXD1 to be an LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF11_MASK;   // write one to clear the flag
    }
    LLWU_PE3 = temp;

    temp = LLWU_PE4;
    if( pin_en & 0x1000)
    {
        temp |= LLWU_PE4_WUPE12(rise_fall);
        printf(" LLWU configured pins PTD0/SPI0_PCS0/UART2_RTS to be an LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF12_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x2000)
    {
        temp |= LLWU_PE4_WUPE13(rise_fall);
        printf(" LLWU configured pins PTD2/UART2_RX to be an LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF13_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x4000)
    {
        temp |= LLWU_PE4_WUPE14(rise_fall);
        printf(" LLWU configured pins PTD4/UART0_RTS/FTM0_CH4/EWM_IN is LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF14_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x8000)
    {
        temp |= LLWU_PE4_WUPE15(rise_fall);
        printf(" LLWU configured pins PTD6/UART0_RX/FTM0_CH6/FTM0_FLT0 is LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF15_MASK;   // write one to clear the flag
    }
    LLWU_PE4 = temp;
    if (module_en == 0){
      LLWU_ME = 0;
    }else  {
    LLWU_ME |= module_en;  //Set up more modules to wakeup up
    }
    
   // printf("LLWU PE1   = 0x%02X,    ",   (LLWU_PE1)) ;
//    printf("LLWU PE2   = 0x%02X\n",      (LLWU_PE2)) ;
   // printf("LLWU PE3   = 0x%02X,    ",   (LLWU_PE3)) ;
    //printf("LLWU PE4   = 0x%02X\n",      (LLWU_PE4)) ;
    //printf("LLWU ME    = 0x%02X,    ",    (LLWU_ME)) ;
    //printf("LLWU F1    = 0x%02X\n",       (LLWU_F1)) ;
    //printf("LLWU F2    = 0x%02X,    ",    (LLWU_F2)) ;
    //printf("LLWU F3    = 0x%02X\n",       (LLWU_F3)) ;
    //printf("LLWU FILT1 = 0x%02X,    ", (LLWU_FILT1)) ;
//    printf("LLWU FILT2 = 0x%02X\n",    (LLWU_FILT2)) ;
  //  printf("LLWU RST   = 0x%02X\n",      (LLWU_RST)) ;
      //function ends
}

#if 0
void prepareToGetLowestPower(void)
{
     //uart(OFF);
     clockMonitor(0);
     JTAG_TDO_PullUp_Enable();          
}

/*******************************************************************************************************/
void JTAG_TDO_PullUp_Enable(void)
{
    PORTA_PCR2 = PORT_PCR_PE_MASK | PORT_PCR_PS_MASK;//JTAG_TDO      
}


/*******************************************************************************************************/
void clockMonitor(unsigned char state)
{
    if(state)
      MCG_C6 |= MCG_C6_CME0_MASK;
    else
      MCG_C6 &= ~MCG_C6_CME0_MASK;
}
#endif


/***************************************************************/
/*
 * Configures the ARM system control register for WAIT(sleep)mode
 * and then executes the WFI instruction to enter the mode.
 *
 * Parameters:
 * none
 *
 */

void sleep (void)
{
/* Clear the SLEEPDEEP bit to make sure we go into WAIT (sleep) 
 * mode instead of deep sleep.
 */
SCB_SCR &= ~SCB_SCR_SLEEPDEEP_MASK;

/* WFI instruction will start entry into WAIT mode */
 asm("WFI");
}
/***************************************************************/
/*
 * Configures the ARM system control register for STOP  
 * (deepsleep) mode and then executes the WFI instruction 
 * to enter the mode.
 *
 * Parameters:
 * none
 *
 */

void deepsleep (void)
{
  /* Set the SLEEPDEEP bit to enable deep sleep mode (STOP) */
  SCB_SCR |= SCB_SCR_SLEEPDEEP_MASK;

  /* WFI instruction will start entry into STOP mode */
  asm("WFI");
}
/********************************************************************/
/* WAIT mode entry routine. Puts the processor into wait mode.
 * In this mode the core clock is disabled (no code executing), but 
 * bus clocks are enabled (peripheral modules are operational). 
 *
 * Mode transitions:
 * RUN -> WAIT
 * VLPR -> VLPW
 *
 * This function can be used to enter normal wait mode or VLPW
 * mode. If you are executing in normal run mode when calling this
 * function, then you will enter normal wait mode. If you are in VLPR
 * mode when calling this function, then you will enter VLPW mode instead.
 *
 * NOTE: Some modules include a programmable option to disable them in 
 * wait mode. If those modules are programmed to disable in wait mode, 
 * they will not be able to generate interrupts to wake up the core.
 *
 * WAIT mode is exited using any enabled interrupt or RESET, so no
 * exit_wait routine is needed. 
 *
* If in VLPW mode, the statue of the SMC_PMCTRL[LPWUI] bit determines if 
 * the processor exits to VLPR (LPWUI cleared) or normal run mode (LPWUI 
 * set). The enable_lpwui() and disable_lpwui()functions can be used to set
 * this bit to the desired option prior to calling enter_wait().
 * 
 * 
 * Parameters:
 * none
 */
void enter_wait(void)
{
    wait();
}
/********************************************************************/
/* STOP mode entry routine. Puts the processor into normal stop mode.
 * In this mode core, bus and peripheral clocks are disabled.
 *
 * Mode transitions:
 * RUN -> STOP
 *
 * This function can be used to enter normal stop mode. 
 * If you are executing in normal run mode when calling this
 * function and AVLP = 0, then you will enter normal stop mode. 
 * If AVLP = 1 with previous write to PMPROT
 * then you will enter VLPS mode instead.
 *
 * STOP mode is exited using any enabled interrupt or RESET, so no
 * exit_stop routine is needed.
 *
 * Parameters:
 * none
 */
void enter_stop(void)
{
  volatile unsigned int dummyread;
  /* The PMPROT register may have already been written by init code
     If so then this next write is not done since  
     PMPROT is write once after RESET  
     this write-once bit allows the MCU to enter the
     normal STOP mode.
     If AVLP is already a 1, VLPS mode is entered instead of normal STOP*/
  SMC_PMPROT = 0;  

  /* Set the STOPM field to 0b000 for normal STOP mode */
  SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK;
  SMC_PMCTRL |=  SMC_PMCTRL_STOPM(0); 
  /*wait for write to complete to SMC before stopping core */  
  dummyread = SMC_PMCTRL;
  stop();
}
/****************************************************************/
/* VLPR mode entry routine.Puts the processor into very low power
 * run mode. In this mode all clocks are enabled, but the core, bus,
 * and peripheral clocks are limited to 2MHz or less. The flash 
 * clock is limited to 1MHz or less. 
 *
 * Mode transitions:
 * RUN -> VLPR
 *
 * exit_vlpr() function or an interrupt with LPWUI set can be used 
 * to switch from VLPR back to RUN. 
 * 
 * while in VLPR,VLPW or VLPS the exit to VLPR is determined by 
 * the value passed in from the calling program.  
 *
 * LPWUI is static during VLPR mode and 
 * should not be written to while in VLPR mode.
 *
 * Parameters:
 * lpwui_value - The input determines what is written to the LPWUI
 *               bit in the PMCTRL register
 *               Clear lpwui and interrupts keep you in VLPR
 *               Set LPWUI and interrupts return you to RUN mode
 * Return value : PMSTAT value or error code
 *                PMSTAT = return_value = PMSTAT
 *                         000_0001 Current power mode is RUN
 *                         000_0100 Current power mode is VLPR
 *                ERROR Code =  0x14 - already in VLPR mode
 *                           =  0x24 - REGONS never clear indicating stop regulation
 */
int enter_vlpr(char lpwui_value)
{
  int i; 
  unsigned int return_value;
  if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK)== 4){
         return_value = 0x14;
         }
  /* The PMPROT register may have already been written by init code
     If so then this next write is not done.  
     PMPROT is write once after RESET  
     this write-once bit allows the MCU to enter the
     very low power modes: VLPR, VLPW, and VLPS   */
  SMC_PMPROT = SMC_PMPROT_AVLP_MASK;  
    
  /* Set the (for MC1)LPLLSM or (for MC2)STOPM field 
     to 0b010 for VLPS mode -
     and RUNM bits to 0b010 for VLPR mode 
     Need to set state of LPWUI bit */
  lpwui_value &= 1;
  SMC_PMCTRL &= ~SMC_PMCTRL_RUNM_MASK; 
  SMC_PMCTRL  |= SMC_PMCTRL_RUNM(0x2) | 
                (lpwui_value<<SMC_PMCTRL_LPWUI_SHIFT);
  /* Wait for VLPS regulator mode to be confirmed */
  for (i = 0 ; i < 10000 ; i++)
    {     /* check that the value of REGONS bit is not 0
             once it is a zero we can stop checking */
      if ((PMC_REGSC & PMC_REGSC_REGONS_MASK) ==0x04){
       /* 0 Regulator is in stop regulation or in transition 
            to/from it
          1 MCU is in Run regulation mode */
      }  
      else  break;
    }  
  if ((PMC_REGSC & PMC_REGSC_REGONS_MASK) ==0x04) 
    {  
      return_value = 0x24; 
    }
  /* SMC_PMSTAT register only exist in Mode Controller 2 MCU versions */
  if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK) == 4)  
    {
      return_value = SMC_PMSTAT;
    }
  return (return_value);
}

/***************************************************************/
/* VLPS mode entry routine. Puts the processor into VLPS mode 
 * directly from run or VLPR modes. 
 *
 * Mode transitions:
 * RUN  -> VLPS
 * VLPR -> VLPS
 *
 * Note, when VLPS is entered directly from RUN mode, 
 * exit to VLPR is disabled by hardware and the system will
 * always exit back to RUN.
 *
 * If however VLPS mode is entered from VLPR the state of 
 * the LPWUI bit determines the state the MCU will return 
 * to upon exit from VLPS.If LPWUI is 1 and an interrupt 
 * occurs you will exit to normal run mode instead of VLPR. 
 * If LPWUI is 0 and an interrupt occurs you will exit to VLPR.
 *
 * Parameters:  
 * none
 */
 /****************************************************************/

void enter_vlps(void)
{
  volatile unsigned int dummyread;
  /* The PMPROT register may have already been written by init code
     If so then this next write is not done since  
     PMPROT is write once after RESET 
     allows the MCU to enter the VLPR, VLPW, and VLPS modes.
     If AVLP is already writen to 0 
     Stop is entered instead of VLPS*/
  SMC_PMPROT = SMC_PMPROT_AVLP_MASK;           
  /* Set the STOPM field to 0b010 for VLPS mode */
  SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK; 
  SMC_PMCTRL |=  SMC_PMCTRL_STOPM(0x2); 
  /*wait for write to complete to SMC before stopping core */  
  dummyread = SMC_PMCTRL;
  /* Now execute the stop instruction to go into VLPS */
  stop();
}
/****************************************************************/
/* LLS mode entry routine. Puts the processor into LLS mode from
 * normal run mode or VLPR. 
 *
 * Mode transitions:
 * RUN -> LLS
 * VLPR -> LLS
 *
 * NOTE: LLS mode will always exit to RUN mode even if you were 
 * in VLPR mode before entering LLS.
 *
 * Wakeup from LLS mode is controlled by the LLWU module. Most
 * modules cannot issue a wakeup interrupt in LLS mode, so make
 * sure to setup the desired wakeup sources in the LLWU before 
 * calling this function.
 *
 * Parameters:
 * none
 */
 /********************************************************************/

void enter_lls(void)
{
  volatile unsigned int dummyread;
  /* Write to PMPROT to allow LLS power modes this write-once 
     bit allows the MCU to enter the LLS low power mode*/
  SMC_PMPROT = SMC_PMPROT_ALLS_MASK;   
  /* Set the STOPM field to 0b011 for LLS mode  */
  SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK; 
  SMC_PMCTRL |=  SMC_PMCTRL_STOPM(0x3); 
  /*wait for write to complete to SMC before stopping core */  
  dummyread = SMC_PMCTRL;
  /* Now execute the stop instruction to go into LLS */
  stop();
}
/***************************************************************/
/* VLLS3 mode entry routine. Puts the processor into 
 * VLLS3 mode from normal run mode or VLPR. 
 *
 * Mode transitions:
 * RUN -> VLLS3
 * VLPR -> VLLS3
 *
 * NOTE: VLLSx modes will always exit to RUN mode even if you were 
 * in VLPR mode before entering VLLSx.
 *
 * Wakeup from VLLSx mode is controlled by the LLWU module. Most
 * modules cannot issue a wakeup interrupt in VLLSx mode, so make
 * sure to setup the desired wakeup sources in the LLWU before 
 * calling this function.
 *
 * Parameters:
 * none  
 */
 /********************************************************************/

void enter_vlls3(void)
{
  volatile unsigned int dummyread;
  /* Write to PMPROT to allow VLLS3 power modes */
  SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;      
  /* Set the STOPM field to 0b100 for VLLS3 mode */
  SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK; 
  SMC_PMCTRL |=  SMC_PMCTRL_STOPM(0x4); 
  /* set VLLSM = 0b11 */
  SMC_VLLSCTRL =  SMC_VLLSCTRL_VLLSM(3);          
  /*wait for write to complete to SMC before stopping core */  
  dummyread = SMC_VLLSCTRL;
  /* Now execute the stop instruction to go into VLLS3 */
  stop();
}
/***************************************************************/
/* VLLS2 mode entry routine. Puts the processor into 
 * VLLS2 mode from normal run mode or VLPR. 
 *
 * Mode transitions:
 * RUN -> VLLS2
 * VLPR -> VLLS2
 *
 * NOTE: VLLSx modes will always exit to RUN mode even  
 *       if you werein VLPR mode before entering VLLSx.
 *
 * Wakeup from VLLSx mode is controlled by the LLWU module. Most
 * modules cannot issue a wakeup interrupt in VLLSx mode, so make
 * sure to setup the desired wakeup sources in the LLWU before 
 * calling this function.
 *
 * Parameters:
 * none  
 */
 /********************************************************************/

void enter_vlls2(void)
{
  volatile unsigned int dummyread;
  /* Write to PMPROT to allow VLLS2 power modes */
  SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;      
  /* Set the STOPM field to 0b100 for VLLS2 mode */
  SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK; 
  SMC_PMCTRL |=  SMC_PMCTRL_STOPM(0x4); 
  /* set VLLSM = 0b10 */
  SMC_VLLSCTRL =  SMC_VLLSCTRL_VLLSM(2);                
  /*wait for write to complete to SMC before stopping core */  
  dummyread = SMC_VLLSCTRL;
  /* Now execute the stop instruction to go into VLLS2 */
  stop();
}
/***************************************************************/
/* VLLS1 mode entry routine. Puts the processor into 
 * VLLS1 mode from normal run mode or VLPR. 
 *
 * Mode transitions:
 * RUN -> VLLS1
 * VLPR -> VLLS1
 *
 * NOTE: VLLSx modes will always exit to RUN mode even if you were 
 * in VLPR mode before entering VLLSx.
 *
 * Wakeup from VLLSx mode is controlled by the LLWU module. Most
 * modules cannot issue a wakeup interrupt in VLLSx mode, so make
 * sure to setup the desired wakeup sources in the LLWU before 
 * calling this function.
 *
 * Parameters:
 * none  
 */
 /********************************************************************/

void enter_vlls1(void)
{
  volatile unsigned int dummyread;
  SMC_PMPROT = SMC_PMPROT_AVLLS_MASK;
    
  /* Write to PMPROT to allow all possible power modes */
  /* Set the STOPM field to 0b100 for VLLS1 mode */
  SMC_PMCTRL &= ~SMC_PMCTRL_STOPM_MASK; 
  SMC_PMCTRL |=  SMC_PMCTRL_STOPM(0x4); 
  /* set VLLSM = 0b01 */
  SMC_VLLSCTRL =  SMC_VLLSCTRL_VLLSM(1);          
  /*wait for write to complete to SMC before stopping core */  
  dummyread = SMC_VLLSCTRL;
  /* Now execute the stop instruction to go into VLLS1 */
  stop();
}

void enter_vlls1_old(void)
{
    /* Write to PMPROT to allow all possible power modes */
    /* Set the VLLSM field to 0b100 for VLLS1 mode - Need to retain state of LPWUI bit 8 */
    SMC_PMCTRL = (SMC_PMCTRL & (SMC_PMCTRL_RUNM_MASK |SMC_PMCTRL_LPWUI_MASK)) |
                  SMC_PMCTRL_STOPM(0x4) ; // retain LPWUI

    SMC_VLLSCTRL =  SMC_VLLSCTRL_VLLSM(1);           // set VLLSM = 0b01
       
    /* Now execute the stop instruction to go into VLLS1 */
    stop();
}



/********************************************************************/
/* Enable low power wake up on interrupt. This function can be used
 * to set the LPWUI bit. When this bit is set VLPx modes will exit
 * to normal run mode. When this bit is cleared VLPx modes will exit
 * to VLPR mode.
 * LPWUI should be static during VLPR mode, it should not be changed
 * while in VLPR
 * The disable_lpwui() function can be used to clear the LPWUI bit.
 *
 * Parameters:
 * none
 */
/********************************************************************/

void enable_lpwui(void)
{
  if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK) != 4)
  {
    SMC_PMCTRL |= SMC_PMCTRL_LPWUI_MASK;
    printf("[enable_lpwui]SMC_PMCTRL   = %#02X \r\n", (SMC_PMCTRL))  ;
  } else 
  {
    printf("[enable_lpwui]LPWUI should not be changed in VLPR mode SMC_PMCTRL   = %#02X \r\n", (SMC_PMCTRL))  ;
  }
}
/********************************************************************/
/* Disable low power wake up on interrupt. This function can be used
 * to clear the LPWUI bit. When this bit is set VLPx modes will exit
 * to normal run mode. When this bit is cleared VLPx modes will exit
 * to VLPR mode.
 *
 * The enable_lpwui() function can be used to set the LPWUI bit.
 *
 * Parameters:
 * none
 */
 /********************************************************************/

void disable_lpwui(void)
{
  if ((SMC_PMSTAT & SMC_PMSTAT_PMSTAT_MASK) != 4)
  {
    SMC_PMCTRL &= ~SMC_PMCTRL_LPWUI_MASK;
    printf("[disable_lpwui]SMC_PMCTRL   = %#02X \r\n", (SMC_PMCTRL))  ;
  } else 
  {
    printf("[disable_lpwui]LPWUI should not be changed in VLPR mode SMC_PMCTRL   = %#02X \r\n", (SMC_PMCTRL))  ;
  }
}
/********************************************************************/
/********************End of Functions *******************************/
/********************************************************************/
