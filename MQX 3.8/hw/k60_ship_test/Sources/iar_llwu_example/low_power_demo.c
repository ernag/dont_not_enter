/**
  \mainpage 
  \n Copyright (c) 2011 Freescale
  \n Freescale Confidential Proprietary 
  \brief 
  \author   	Freescale Semiconductor
  \author    
  \version      m.n
  \date         Sep 14, 2011
  
  Put here all the information needed of the Project. Basic configuration as well as information on
  the project definition will be very useful 
*/
/****************************************************************************************************/
/*                                                                                                  */
/* All software, source code, included documentation, and any implied know-how are property of      */
/* Freescale Semiconductor and therefore considered CONFIDENTIAL INFORMATION.                       */
/* This confidential information is disclosed FOR DEMONSTRATION PURPOSES ONLY.                      */
/*                                                                                                  */
/* All Confidential Information remains the property of Freescale Semiconductor and will not be     */
/* copied or reproduced without the express written permission of the Discloser, except for copies  */
/* that are absolutely necessary in order to fulfill the Purpose.                                   */
/*                                                                                                  */
/* Services performed by FREESCALE in this matter are performed AS IS and without any warranty.     */
/* CUSTOMER retains the final decision relative to the total design and functionality of the end    */
/* product.                                                                                         */
/* FREESCALE neither guarantees nor will be held liable by CUSTOMER for the success of this project.*/
/*                                                                                                  */
/* FREESCALE disclaims all warranties, express, implied or statutory including, but not limited to, */
/* implied warranty of merchantability or fitness for a particular purpose on any hardware,         */
/* software ore advise supplied to the project by FREESCALE, and or any product resulting from      */
/* FREESCALE services.                                                                              */
/* In no event shall FREESCALE be liable for incidental or consequential damages arising out of     */
/* this agreement. CUSTOMER agrees to hold FREESCALE harmless against any and all claims demands or */
/* actions by anyone on account of any damage,or injury, whether commercial, contractual, or        */
/* tortuous, rising directly or indirectly as a result of the advise or assistance supplied CUSTOMER*/ 
/* in connectionwith product, services or goods supplied under this Agreement.                      */
/*                                                                                                  */
/****************************************************************************************************/

/****************************************************************************************************/
/****************************************************************************************************/
/****************************************NOTES*******************************************************/
/****************************************************************************************************/
/****************************************************************************************************/
/* This project was desing to work on TWR-K20D72M board, configured with PEE at 72Mhz               */
/*      Jumper J6 may be used to measured to current consumption                                                                                             */
/****************************************************************************************************/

/*****************************************************************************************************
* Include files
*****************************************************************************************************/
#include "common.h"
#include "llwu.h"
#include "smc.h"
#include "mcg.h"
#include "pmc.h"
#include "uart.h"

/*****************************************************************************************************
* Declaration of module wide FUNCTIONs - NOT for use in other modules
*****************************************************************************************************/
void LowPowerModes_test(void);
void LLWU_Init(void);
void SW_LED_Init(void);
void port_a_isr(void);

void uart(unsigned char state);
void uart_configure(int mcg_clock_hz);
int PEE_to_BLPE(void);
int BLPE_to_PEE(void);
void prepareToGetLowestPower(void);
void clockMonitor(unsigned char state);
void JTAG_TDO_PullUp_Enable(void);

/*****************************************************************************************************
* Definition of module wide MACROs / #DEFINE-CONSTANTs - NOT for use in other modules
*****************************************************************************************************/
#define UNDEF_VALUE  0xFF
/*****************************************************************************************************
* Declaration of module wide TYPEs - NOT for use in other modules
*****************************************************************************************************/

/*****************************************************************************************************
* Definition of module wide VARIABLEs - NOT for use in other modules
*****************************************************************************************************/
unsigned char measureFlag = 0;
unsigned char enterVLLSOflag = 0;
unsigned long ram_data[216];

extern int mcg_clk_hz;
extern int mcg_clk_khz;
extern int core_clk_khz;

/*****************************************************************************************************
* Definition of module wide (CONST-) CONSTANTs - NOT for use in other modules
*****************************************************************************************************/

/*****************************************************************************************************
* Code of project wide FUNCTIONS
*****************************************************************************************************/

void main (void)
{
 
#ifdef KEIL
	start();
#endif
        /*Disable Clock Out*/        
        PORTC_PCR3 = ( PORT_PCR_MUX(0));
        
        /*Enable all operation modes because this is a write once register*/  
        SMC_PMPROT =  SMC_PMPROT_AVLLS_MASK |
                      SMC_PMPROT_ALLS_MASK  |    
                      SMC_PMPROT_AVLP_MASK;
        
        /*PTC1(SW1) is configured to wake up MCU from VLLSx and LLS modes, Interrup is ne*/
        //LLWU_Init();
        /*Configure Tower hardware for the application*/
        SW_LED_Init();
        /*Start test*/
        LowPowerModes_test();
}

/*******************************************************************************************************/
#define IRC_FREQ 4*1000*1000
static void pee_blpi(void)
{
    int crystal_val = 0;
    atc(1, IRC_FREQ, 100*1000*1000);
    pee_pbe(crystal_val);
    pbe_fbe(crystal_val);
    fbe_fbi(IRC_FREQ, 1);
    fbi_blpi(IRC_FREQ, 1);

    MCG_SC &= 0xf1;
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0)|SIM_CLKDIV1_OUTDIV2(0)|SIM_CLKDIV1_OUTDIV4(4);
    uart_init(TERM_PORT, 4000, 19200);
    printf("pee_blpi finished, core clk: 2M, bus clk 1M, flash clock 800kHz\n");
}
static void blpi_pee(void)
{
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0)|SIM_CLKDIV1_OUTDIV2(1)|SIM_CLKDIV1_OUTDIV4(3);
    blpi_fbi(IRC_FREQ, 1);
    fbi_fbe(IRC_FREQ, 1, 0);
    fbe_pbe(CLK0_FREQ_HZ, PLL0_PRDIV,PLL0_VDIV);
    pbe_pee(CLK0_FREQ_HZ);

    uart_configure(100*1000*1000);
    
    printf("blpi_pee finished, core clk: 100M, bus clk 50M, flash clock 25M\n");
}
static void fei_blpi(void)
{
    atc(1, IRC_FREQ, 100*1000*1000);
  
    fei_fbi(IRC_FREQ, 1);
    fbi_blpi(IRC_FREQ, 1);

    MCG_SC &= 0xf1;
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0)|SIM_CLKDIV1_OUTDIV2(0)|SIM_CLKDIV1_OUTDIV4(4);
    uart_init(TERM_PORT, 4000, 19200);
    printf("fei_blpi finished, core clk: 2M, bus clk 1M, flash clock 800kHz \n");
}
static void blpi_fei(void)
{
    SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0)|SIM_CLKDIV1_OUTDIV2(1)|SIM_CLKDIV1_OUTDIV4(4);
    blpi_fbi(IRC_FREQ, 1);
    fbi_fei(IRC_FREQ);

    uart_configure(100*1000*1000);
    
    printf("blpi_fei finished, core clk: 100M, bus clk 50M, flash clock 25M\n");
}
#undef IRC_FREQ
void LowPowerModes_test(void)
{
  unsigned char op_mode;  
  unsigned char test_num = UNDEF_VALUE;
  char test_val;
   
  
        printf("*----------------------------------------*\n");
        printf("*        Low Power Mode Test             *\n");
        printf("*----------------------------------------*\n\n");

        SIM_SCGC4 = 0x2000;
        SIM_SCGC5 = 0x0a00;
        SIM_SCGC7 = 0;
        printf("SIM_SCGC1:%x\n", SIM_SCGC1);
        printf("SIM_SCGC2:%x\n", SIM_SCGC2);
        printf("SIM_SCGC3:%x\n", SIM_SCGC3);
        printf("SIM_SCGC4:%x\n", SIM_SCGC4);
        printf("SIM_SCGC5:%x\n", SIM_SCGC5);
        printf("SIM_SCGC6:%x\n", SIM_SCGC6);
        printf("SIM_SCGC7:%x\n", SIM_SCGC7);
        printf("SIM_CLKDIV1:%x\n", SIM_CLKDIV1);
        
        
	while(1)
	{
	    
            while (test_num > 11){
                printf("\nSelect the mode to enter \n");
                printf("1 to enter VLLS1\n");
                printf("2 to enter VLLS2\n");
                printf("3 to enter VLLS3\n"); 
                printf("4 to enter LLS\n");
                printf("5 to enter VLPS\n");
                printf("6 to enter VLPR\n");
                printf("7 exit VLPR\n");
                printf("8 to enter VLPW\n");
                printf("9 to enter WAIT\n");
                printf("A to enter STOP\n");
                test_val = in_char();
				out_char(test_val);
                
                if((test_val>=0x30) && (test_val<=0x39))
                      test_num = test_val - 0x30;
                if((test_val>=0x41) && (test_val<=0x47))
                      test_num = test_val - 0x37;
                if((test_val>=0x61) && (test_val<=0x67))
                      test_num = test_val - 0x57;
              }
         
          
            switch(test_num){
              case 1://VLLS1
                  printf("Press any key to enter VLLS1\n ");
                in_char();
                  printf("Press SW1 to wake up from VLLS1\n ");
                prepareToGetLowestPower();          
                enter_vlls1();
                break;
              
              case 2://VLLS2
                  printf("Press any key to enter VLLS2\n ");
                in_char();
                  printf("Press SW1 to wake up from VLLS2\n ");
                prepareToGetLowestPower();
                enter_vlls2();
                break;
                
              case 3://VLLS3
                  printf("Press any key to enter VLLS3\n ");
                in_char();
                  printf("Press SW1 to wake up from VLLS3\n ");
                prepareToGetLowestPower();
                enter_vlls3();
                break;
                
              case 4://LLS
                  printf("Press any key to enter LLS\n ");
                in_char();
                  printf("Press SW1 to wake up from LLS\n ");
                prepareToGetLowestPower(); 
                enter_lls();
                /*After LLS wake up that was enter from PEE the exit will be on PBE */ 
                /*  for this reason after wake up make sure to get back to desired  */
                /*  clock mode                                                      */
                op_mode = what_mcg_mode();
                if(op_mode==PBE)
                {
                  mcg_clk_hz = pbe_pee(CLK0_FREQ_HZ);
                }
                clockMonitor(ON);
                uart(ON);
                uart_configure(mcg_clk_hz);
                break;
                
              case 5://VLPS
                // PEE-->PBE-->PEE
                printf("Press any key to enter VLPS\n "); in_char();
                printf("Press SW1 to wake up from VLPS\n ");  
                clockMonitor(OFF);
                prepareToGetLowestPower();
                enter_vlps(); 
                uart(ON);
                BLPE_to_PEE();
                printf("Now in Run mode\n");
                clockMonitor(ON);
                break;

              case 6://VLPR
                /*Maximum clock frequency for this mode is core 4MHz and Flash 1Mhz*/
                printf("Press any key to enter VLPR\n ");in_char();
                //pee_blpi();
                fei_blpi();
                clockMonitor(OFF);
                enter_vlpr(0);   // 0: Interruption does not wake up from VLPR
                break;

              case 7:// Exit VLPR
                printf("Press any key to exit VLPR\n "); in_char();
                exit_vlpr();
                //blpi_pee();
                blpi_fei();
                clockMonitor(ON);
                printf("Now in Run mode\n");
                break;

             case 8:// VLPW
                printf("Press any key to enter VLPW\n "); in_char();
                printf("Press SW1 to wake up from VLPW\n ");  
                pee_blpi();
                clockMonitor(OFF);
                enter_vlpr(1); //1 Means: Any interrupt could waku up from VLPR
                enter_wait();
                blpi_pee();
                printf("Now in Run mode\n");
                clockMonitor(ON);
                break;  
                
              case 9://WAIT
                printf("Press any key to enter Wait\n ");
                in_char();
                printf("Press SW1 to wake up from Wait\n "); 
                prepareToGetLowestPower(); 
                enter_wait();
                clockMonitor(ON);
                uart(ON);
                printf("Back to Run mode\n");
                break;
              
              case 10://STOP
                /*Wake up in PBE, need to go to PEE */ 
                printf("Press any key to enter Stop\n ");
                in_char();
                printf("Press SW1 to wake up from Stop\n "); 
                prepareToGetLowestPower();
                enter_stop();
                op_mode = what_mcg_mode();
                if(op_mode==PBE)
                    mcg_clk_hz = pbe_pee(CLK0_FREQ_HZ);
                clockMonitor(ON);
                uart(ON);
                uart_configure(mcg_clk_hz);               
                printf("\nBack to Run mode, %d\n", op_mode);
                break; 
            }
            test_num = UNDEF_VALUE ;    

	} 
}

/*******************************************************************************************************/
void LLWU_Init(void)
{
    enable_irq(INT_LLW-16);
    llwu_configure(0x0040/*PTC1*/, LLWU_PIN_FALLING, 0x0);
}

/*******************************************************************************************************/
void SW_LED_Init(void)
{
    enable_irq(INT_PORTA-16);
       
    /*Configure SW1 = A19*/
    PORTA_PCR19  = PORT_PCR_PS_MASK|PORT_PCR_PE_MASK|PORT_PCR_PFE_MASK|PORT_PCR_IRQC(10)|PORT_PCR_MUX(1); /* IRQ Falling edge */   
    PORTA_DFER  |= (1<<19);
    PORTA_DFWR  = 0x1F;
}

/*******************************************************************************************************/
void port_a_isr(void)
{
    
  if(PORTA_ISFR == (1<<19))
  {
   PORTA_PCR19 |= PORT_PCR_ISF_MASK;    
  }

   PORTA_ISFR = 0xFFFFFFFF;
}

/*******************************************************************************************************/
void uart(unsigned char state)
{
    if(state)
    {
       PORTC_PCR16 = PORT_PCR_MUX(3); // UART is alt3 function for this pin
       PORTC_PCR17 = PORT_PCR_MUX(3); // UART is alt3 function for this pin
    }else
    {
       PORTC_PCR16 = PORT_PCR_MUX(0); // Analog is alt0 function for this pin
       PORTC_PCR17 = PORT_PCR_MUX(0); // Analog is alt0 function for this pin
    }
  
 
}

/*******************************************************************************************************/
void uart_configure(int mcg_clock_hz)
{
     int mcg_clock_khz;
     int bus_clock_khz;
  
     mcg_clock_khz = mcg_clock_hz / 1000;
     bus_clock_khz = mcg_clock_khz / (((SIM_CLKDIV1 & SIM_CLKDIV1_OUTDIV2_MASK) >> 24)+ 1);
     uart_init (TERM_PORT, bus_clock_khz, TERMINAL_BAUD);
}

/*******************************************************************************************************/
int PEE_to_BLPE(void)
{
     int mcg_clock_hz;    
  
      mcg_clock_hz = pee_pbe(CLK0_FREQ_HZ);
      mcg_clock_hz = pbe_blpe(CLK0_FREQ_HZ);
      /*Reduce Flash clok to 1MHz*/
      //SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(1)|SIM_CLKDIV1_OUTDIV2(1)|SIM_CLKDIV1_OUTDIV4(0x7);
      SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0xf)|SIM_CLKDIV1_OUTDIV2(0xf)|SIM_CLKDIV1_OUTDIV4(0xf);

      return mcg_clock_hz; 
}

/*******************************************************************************************************/
int BLPE_to_PEE(void)
{
      int mcg_clock_hz;    
  
      SIM_CLKDIV1 = SIM_CLKDIV1_OUTDIV1(0)|SIM_CLKDIV1_OUTDIV2(0)|SIM_CLKDIV1_OUTDIV4(0x1);
      /*After wake up back to the original clock frequency*/            
      mcg_clock_hz = blpe_pbe(CLK0_FREQ_HZ, PLL0_PRDIV,PLL0_VDIV);
      mcg_clock_hz = pbe_pee(CLK0_FREQ_HZ);
      
      return mcg_clock_hz;
}

/*******************************************************************************************************/
void prepareToGetLowestPower(void)
{
     uart(OFF);
     clockMonitor(OFF);
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
  