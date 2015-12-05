/*
 * pwr_mgr.c
 *
 *  Created on: Feb 14, 2013
 *      Author: Ernie Aguilar
 */

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "pwr_mgr.h"
#include "IO_Map.h"
#include "corona_console.h"
#include "PE_Types.h"

extern cc_handle_t g_cc_handle;


////////////////////////////////////////////////////////////////////////////////
// External Functions
////////////////////////////////////////////////////////////////////////////////
extern corona_err_t pwr_adc(pwr_state_t state);
extern corona_err_t pwr_mfi(pwr_state_t state);
extern corona_err_t pwr_bt(pwr_state_t state);
extern corona_err_t pwr_wifi(pwr_state_t state);
extern corona_err_t pwr_bu26507(pwr_state_t state);
extern corona_err_t pwr_led(pwr_state_t state);
extern corona_err_t pwr_uart(pwr_state_t state);
extern corona_err_t pwr_wson(pwr_state_t state);
extern corona_err_t pwr_usb(pwr_state_t state);
extern corona_err_t pwr_accel(pwr_state_t state);
extern corona_err_t pwr_gpio(pwr_state_t state);
extern corona_err_t pwr_timer(pwr_state_t state);
extern corona_err_t pwr_silego(pwr_state_t state);

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
static corona_err_t pwr_mcu(pwr_state_t state);

////////////////////////////////////////////////////////////////////////////////
// Globals
////////////////////////////////////////////////////////////////////////////////
pwr_mgr_cbk_t g_power_callbacks[] = \
{
	pwr_wifi,
	pwr_adc,
	pwr_mfi,
	pwr_bt,
	pwr_bu26507,
	pwr_led,
	pwr_uart,
	pwr_wson,
	pwr_usb,
	pwr_accel,
	pwr_silego,
	pwr_timer,
	pwr_gpio,
	pwr_mcu
};

////////////////////////////////////////////////////////////////////////////////
// Defines
////////////////////////////////////////////////////////////////////////////////
#define NUM_CALLBACKS	sizeof(g_power_callbacks) / sizeof(pwr_mgr_cbk_t)

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

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
    	 cc_print(&g_cc_handle, " \n[exit_vlpr] Waiting on REGONS bit to set to indicate Run regulation mode ! ");
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

#if 0
void llwu_configure(unsigned int pin_en, unsigned char rise_fall, unsigned char module_en ) {
    uint8_t temp;

    SIM_SCGC4 |= SIM_SCGC4_LLWU_MASK;
//   used on rev 1.4 of P2
    temp = LLWU_PE1;
    if( pin_en & 0x0001)
    {
        temp |= LLWU_PE1_WUPE0(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTE1/UART1_RX/I2C1_SCL /SPI1_SIN is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF0_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0002)
    {
        temp |= LLWU_PE1_WUPE1(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTE2/SPI1_SCK/SDHC0_DCLK is wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF1_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0004)
    {
        temp |= LLWU_PE1_WUPE2(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTE4/SPI1_PCS0/SDHC0_D3 is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF2_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0008)
    {
        temp |= LLWU_PE1_WUPE3(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTA4/FTM0_CH1/NMI/EZP_CSis LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF3_MASK;   // write one to clear the flag
    }
    LLWU_PE1 = temp;

    temp = LLWU_PE2;
    if( pin_en & 0x0010)
    {
        temp |= LLWU_PE2_WUPE4(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTA13/FTM1_CH1 /FTM1_QD_PHB is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF4_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0020)
    {
        temp |= LLWU_PE2_WUPE5(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTB0/I2C0_SCL/FTM1_CH0/FTM1_QD_PHA is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF5_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0040)
    {
        temp |= LLWU_PE2_WUPE6(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTC1/UART1_RTS/FTM0_CH0 is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF6_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0080)
    {
        temp |= LLWU_PE2_WUPE7(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTC3/UART1_RX/FTM0_CH2 is LLWU wakeup source \n");
        LLWU_F1 |= LLWU_F1_WUF7_MASK;   // write one to clear the flag
    }
    LLWU_PE2 = temp;

    temp = LLWU_PE3;
    if( pin_en & 0x0100)
    {
        temp |= LLWU_PE3_WUPE8(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTC4/SPI0_PCS0/FTM0_CH3 is LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF8_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0200)
    {
        temp |= LLWU_PE3_WUPE9(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTC5/SPI0_SCK/I2S0_RXD0 is LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF9_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0400)
    {
        temp |= LLWU_PE3_WUPE10(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTC6/PDB0_EXTRG to be an LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF10_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x0800)
    {
        temp |= LLWU_PE3_WUPE11(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTC11/I2S0_RXD1 to be an LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF11_MASK;   // write one to clear the flag
    }
    LLWU_PE3 = temp;

    temp = LLWU_PE4;
    if( pin_en & 0x1000)
    {
        temp |= LLWU_PE4_WUPE12(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTD0/SPI0_PCS0/UART2_RTS to be an LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF12_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x2000)
    {
        temp |= LLWU_PE4_WUPE13(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTD2/UART2_RX to be an LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF13_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x4000)
    {
        temp |= LLWU_PE4_WUPE14(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTD4/UART0_RTS/FTM0_CH4/EWM_IN is LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF14_MASK;   // write one to clear the flag
    }
    if( pin_en & 0x8000)
    {
        temp |= LLWU_PE4_WUPE15(rise_fall);
        cc_print(&g_cc_handle, " LLWU configured pins PTD6/UART0_RX/FTM0_CH6/FTM0_FLT0 is LLWU wakeup source \n");
        LLWU_F2 |= LLWU_F2_WUF15_MASK;   // write one to clear the flag
    }
    LLWU_PE4 = temp;
    if (module_en == 0){
      LLWU_ME = 0;
    }else  {
    LLWU_ME |= module_en;  //Set up more modules to wakeup up
    }

   // cc_print(&g_cc_handle, "LLWU PE1   = 0x%02X,    ",   (LLWU_PE1)) ;
//    cc_print(&g_cc_handle, "LLWU PE2   = 0x%02X\n",      (LLWU_PE2)) ;
   // cc_print(&g_cc_handle, "LLWU PE3   = 0x%02X,    ",   (LLWU_PE3)) ;
    //cc_print(&g_cc_handle, "LLWU PE4   = 0x%02X\n",      (LLWU_PE4)) ;
    //cc_print(&g_cc_handle, "LLWU ME    = 0x%02X,    ",    (LLWU_ME)) ;
    //cc_print(&g_cc_handle, "LLWU F1    = 0x%02X\n",       (LLWU_F1)) ;
    //cc_print(&g_cc_handle, "LLWU F2    = 0x%02X,    ",    (LLWU_F2)) ;
    //cc_print(&g_cc_handle, "LLWU F3    = 0x%02X\n",       (LLWU_F3)) ;
    //cc_print(&g_cc_handle, "LLWU FILT1 = 0x%02X,    ", (LLWU_FILT1)) ;
//    cc_print(&g_cc_handle, "LLWU FILT2 = 0x%02X\n",    (LLWU_FILT2)) ;
  //  cc_print(&g_cc_handle, "LLWU RST   = 0x%02X\n",      (LLWU_RST)) ;
      //function ends
}
#endif


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

static void pwr_sleep (void)
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
  
  /*
   *   Remove the 26MHz clock here as a last step.
   */
  GPIOC_PDOR &= ~0x00080000;
  
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
    cc_print(&g_cc_handle, "[enable_lpwui]SMC_PMCTRL   = %#02X \r\n", (SMC_PMCTRL))  ;
  } else
  {
    cc_print(&g_cc_handle, "[enable_lpwui]LPWUI should not be changed in VLPR mode SMC_PMCTRL   = %#02X \r\n", (SMC_PMCTRL))  ;
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
    cc_print(&g_cc_handle, "[disable_lpwui]SMC_PMCTRL   = %#02X \r\n", (SMC_PMCTRL))  ;
  } else
  {
    cc_print(&g_cc_handle, "[disable_lpwui]LPWUI should not be changed in VLPR mode SMC_PMCTRL   = %#02X \r\n", (SMC_PMCTRL))  ;
  }
}

static void __switch_to_32khz(void)
{
    int i = 16000;
    
    SIM_SOPT2 &= (uint32_t)~(uint32_t)(SIM_SOPT2_PLLFLLSEL_MASK); /* Select FLL as a clock source for various peripherals */
    /* SIM_SOPT1: OSC32KSEL=0 */
    SIM_SOPT1 &= (uint32_t)~(uint32_t)(SIM_SOPT1_OSC32KSEL(0x03)); /* System oscillator drives 32 kHz clock for various peripherals */
    /* Switch to FEE Mode */
    /* MCG_C2: LOCRE0=0,??=0,RANGE0=0,HGO0=0,EREFS0=0,LP=0,IRCS=0 */
    MCG_C2 = MCG_C2_RANGE0(0x00);                                   
    /* OSC_CR: ERCLKEN=1,??=0,EREFSTEN=0,??=0,SC2P=0,SC4P=0,SC8P=0,SC16P=0 */
    OSC_CR = OSC_CR_ERCLKEN_MASK;                                   
    /* MCG_C7: OSCSEL=1 */
    //MCG_C7 |= MCG_C7_OSCSEL_MASK;                                   
    /* MCG_C1: CLKS=0,FRDIV=0,IREFS=0,IRCLKEN=1,IREFSTEN=0 */
    MCG_C1 = (MCG_C1_CLKS(0x00) | MCG_C1_FRDIV(0x00) | MCG_C1_IRCLKEN_MASK);                                   
    /* MCG_C4: DMX32=1,DRST_DRS=1 */
    MCG_C4 = (uint8_t)((MCG_C4 & (uint8_t)~(uint8_t)(
              MCG_C4_DRST_DRS(0x02)
             )) | (uint8_t)(
              MCG_C4_DMX32_MASK |
              MCG_C4_DRST_DRS(0x01)
             ));                                  
    /* MCG_C5: ??=0,PLLCLKEN0=0,PLLSTEN0=0,PRDIV0=0 */
    MCG_C5 = MCG_C5_PRDIV0(0x00);                                   
    /* MCG_C6: LOLIE0=0,PLLS=0,CME0=0,VDIV0=0 */
    MCG_C6 = MCG_C6_VDIV0(0x00);                                   
    while((MCG_S & MCG_S_IREFST_MASK) != 0x00U) { /* Check that the source of the FLL reference clock is the external reference clock. */
    }
    while((MCG_S & 0x0CU) != 0x00U) {    /* Wait until output of the FLL is selected */
    }

    /*
     *   Remove the 26MHz clock here as a last step.
     */
    GPIOC_PDOR &= ~0x00080000;
    
    while(i--){}  // just add some delay to assure 26mhz is indeed off.
}

/*
 *   Handle power management for MCU.
 */
corona_err_t pwr_mcu(pwr_state_t state)
{
    uint8_t regval;
    
	switch(state)
	{
		case PWR_STATE_SHIP:

			  //cc_print(&g_cc_handle, "Put Corona in _SHIP_ mode...\n");
			  //sleep(500);

			  // Disable interrupts and pins, that don't need it, (FYI PORTC Stuff made ZERO difference in current)
		    __DI();
		    
		    __switch_to_32khz();

			  //PORTA_PCR29 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);

#if 0
			  //PORTA_PCR0  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  //PORTA_PCR1  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  //PORTA_PCR2  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  //PORTA_PCR3  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTA_PCR4  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  //PORTA_PCR6  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTA_PCR14 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTA_PCR15 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTA_PCR16 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTA_PCR17 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);

			  //PORTB_PCR0 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR1 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR2 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR3 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR10 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR11 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR16 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR17 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR18 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR19 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR20 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR21 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR22 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTB_PCR23 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);

			  PORTC_PCR2 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // Display PWM
			  PORTC_PCR4  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // Wifi CS
			  PORTC_PCR5  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // Wifi Clk
			  PORTC_PCR6  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // Wifi MOSI
			  PORTC_PCR7  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // Wifi MISO
			  PORTC_PCR10 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // I2C SCL
			  PORTC_PCR11 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // I2C SDA
			  PORTC_PCR12 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // ACC INT1
			  PORTC_PCR13 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // ACC INT2
			  PORTC_PCR14 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // CHG
			  PORTC_PCR15 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // PGOOD
			  PORTC_PCR16 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // WIFI UART RX
			  PORTC_PCR17 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // WIFI UART TX
			  PORTC_PCR19 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);  // 26MHz EN

			  // PORT D
			  PORTD_PCR4  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR5  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR6  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR7  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR8  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR9  &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR10 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR11 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR12 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR13 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR14 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTD_PCR15 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);

			  // PORT E
			  PORTE_PCR0 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTE_PCR1 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTE_PCR2 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTE_PCR3 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTE_PCR4 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTE_PCR5 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
			  PORTE_PCR6 &= ~(PORT_PCR_IRQC_MASK | PORT_PCR_MUX_MASK);
#endif

			  //cc_print(&g_cc_handle, "SIM_SCGC1: 0x%x\n", SIM_SCGC1);
			  //cc_print(&g_cc_handle, "SIM_SCGC2: 0x%x\n", SIM_SCGC2);
			  //cc_print(&g_cc_handle, "SIM_SCGC3: 0x%x\n", SIM_SCGC3);

			  SIM_SCGC4 &= ~(SIM_SCGC4_USBOTG_MASK | SIM_SCGC4_VREF_MASK | SIM_SCGC4_UART0_MASK | SIM_SCGC4_UART1_MASK | SIM_SCGC4_UART2_MASK | SIM_SCGC4_UART3_MASK | SIM_SCGC4_I2C1_MASK);
			  //cc_print(&g_cc_handle, "SIM_SCGC4: 0x%x\n", SIM_SCGC4);

			  SIM_SCGC5 &= ~(/*SIM_SCGC5_PORTA_MASK |*/ SIM_SCGC5_PORTB_MASK | SIM_SCGC5_PORTC_MASK | SIM_SCGC5_PORTD_MASK | SIM_SCGC5_PORTE_MASK | SIM_SCGC5_LPTIMER_MASK);
			  //cc_print(&g_cc_handle, "SIM_SCGC5: 0x%x\n", SIM_SCGC5); // We do not want to clock gate the PORTS in LLWU mode.


			  SIM_SCGC6 &= ~(SIM_SCGC6_RTC_MASK | SIM_SCGC6_ADC0_MASK | SIM_SCGC6_FTM0_MASK | SIM_SCGC6_FTM1_MASK |\
					  	    SIM_SCGC6_SPI0_MASK | SIM_SCGC6_SPI1_MASK | SIM_SCGC6_I2S_MASK /*| SIM_SCGC6_FTFL_MASK*/);

			  //cc_print(&g_cc_handle, "SIM_SCGC6: 0x%x\n", SIM_SCGC6); // fully optimized.

			  SIM_SCGC7 = 0;
			  //cc_print(&g_cc_handle, "SIM_SCGC7: 0x%x\n", SIM_SCGC7);  // fully optimized.

			  SIM_CLKDIV1 |=  (0xFFFF0000); // maximum divides for all clocks.
			 // SIM_CLKDIV1 = 0;
			  //cc_print(&g_cc_handle, "SIM_CLKDIV1: 0x%x\n", SIM_CLKDIV1); // fully optimized now.

			  //cc_print(&g_cc_handle, "MDM-AP Control Register: 0x%x\n", *pMDM_AP);

			  //llwu_configure(0x1, 0x2, 0x1);  // PTE1 (button) can wake up out of VLLS1 state, this "0xFF" probably isn't quite right...
			  //while(i-- != 0){} // a completely idiotic form of a (potentially unnecessary) delay.

			  /*
			   *   Make sure we can get out of ship mode via button and PGOOD.
			   */
			  
                regval = LLWU_PE1; // get current settings
                //if ( wakeup_source & PWR_LLWU_BUTTON )
                {
                    // button (PTE1) maps to wakeup source LLWU_P0
                    regval |= LLWU_PE1_WUPE0( 0x02 ); // falling edge
                    LLWU_F1 |= LLWU_F1_WUF0_MASK;   // clear the flag for LLWU_P0
                }
                
                //if ( wakeup_source & PWR_LLWU_PGOOD_USB )
                {
                    // PGOOD (PTE2) maps to wakeup source LLWU_P1
                    regval |= LLWU_PE1_WUPE1( 0x02 ); // (insertion only, not an ejection).
                    LLWU_F1 |= LLWU_F1_WUF1_MASK;   // clear the flag for LLWU_P1
                }
                LLWU_PE1 = regval;
            
			  
			  MCG_C6 &= ~MCG_C6_CME0_MASK; // disables the clock monitor
			  PORTA_PCR2 = PORT_PCR_PE_MASK | PORT_PCR_PS_MASK; // enables pull-up on TDO (they do this in prepapreForLowestPower() in demo).

			  //enter_vlls1();
			  enter_vlls3();

			break;

		case PWR_STATE_STANDBY:
			break;

		case PWR_STATE_NORMAL:
			break;

		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}


/*
 *   Process all the power management callbacks.
 */
corona_err_t pwr_mgr_state(pwr_state_t state)
{
	int i;

	for(i = 0; i < NUM_CALLBACKS; i++)
	{
		corona_err_t err;
		err = g_power_callbacks[i](state);
		if (err != SUCCESS)
		{
			cc_print(&g_cc_handle, "\n\nPower Callback (%d) raised error (%d)!!!\n\n", i, err);
			return err;
		}
	}
}
