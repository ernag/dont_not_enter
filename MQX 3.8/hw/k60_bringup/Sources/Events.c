
/** ###################################################################
**     Filename    : Events.c
**     Project     : ProcessorExpert
**     Processor   : MK60DN512VMC10
**     Component   : Events
**     Version     : Driver 01.00
**     Compiler    : CodeWarrior ARM C Compiler
**     Date/Time   : 2013-01-30, 14:02, # CodeGen: 0
**     Abstract    :
**         This is user's event module.
**         Put your event handler code here.
**     Settings    :
**     Contents    :
**         Cpu_OnNMIINT - void Cpu_OnNMIINT(void);
**
** ###################################################################*/
/* MODULE Events */

#include "Cpu.h"
#include "Events.h"

/* User includes (#include below this line is not maintained by Processor Expert) */
#include "sleep_timer.h"
#include "corona_isr_util.h"
#include <stdio.h>
#include "corona_console.h"
#include "bu26507_display.h"
extern cc_handle_t g_cc_handle;


#define ACC_INT1_MASK		0x1000
#define ACC_INT2_MASK		0x2000
#define CHG_B_INT_MASK		0x4000
#define WIFI_INT_MASK		0x20
#define BUTTON_INT_MASK		( (unsigned long)0x00000002 )
#define PGOOD_B_INT_MASK    ( (unsigned long)0x00000004 )

extern volatile bool  g_UART2_DataReceivedFlg;
extern volatile bool  g_UART2_DataTransmittedFlg;
extern volatile bool  g_UART5_DataReceivedFlg;
extern volatile bool  g_UART5_DataTransmittedFlg;
extern volatile bool  g_UART0_DataReceivedFlg;
extern volatile bool  g_UART0_DataTransmittedFlg;
extern volatile bool  g_I2C1_DataReceivedFlg;
extern volatile bool  g_I2C1_DataTransmittedFlg;
extern volatile uint8 g_SPI0_Wifi_Wait_for_Send;
extern volatile uint8 g_SPI0_Wifi_Wait_for_Receive;
extern volatile uint8 g_SPI2_Flash_Wait_for_Send;
extern volatile uint8 g_SPI2_Flash_Wait_for_Receive;
extern volatile uint8 g_SPI1_Accel_Wait_for_Send;
extern volatile uint8 g_SPI1_Accel_Wait_for_Receive;

uint8 g_pgood = 1;


/*
** ===================================================================
**     Event       :  Cpu_OnNMIINT (module Events)
**
**     Component   :  Cpu [MK60DN512LQ10]
**     Description :
**         This event is called when the Non maskable interrupt had
**         occurred. This event is automatically enabled when the <NMI
**         interrrupt> property is set to 'Enabled'.
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void Cpu_OnNMIINT(void)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Event       :  Cpu_OnNMIINT0 (module Events)
**
**     Component   :  Cpu [MK60DN512MC10]
**     Description :
**         This event is called when the Non maskable interrupt had
**         occurred. This event is automatically enabled when the <NMI
**         interrrupt> property is set to 'Enabled'.
**     Parameters  : None
**     Returns     : Nothing
** ===================================================================
*/
void Cpu_OnNMIINT0(void)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Event       :  SPI0_WIFI_OnBlockSent (module Events)
**
**     Component   :  SPI0_WIFI [SPIMaster_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter. This event is
**         available only if the SendBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void SPI0_WIFI_OnBlockSent(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_SPI0_Wifi_Wait_for_Send = 0;
}

/*
** ===================================================================
**     Event       :  SPI0_WIFI_OnBlockReceived (module Events)
**
**     Component   :  SPI0_WIFI [SPIMaster_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer. This method is available only if
**         the ReceiveBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void SPI0_WIFI_OnBlockReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_SPI0_Wifi_Wait_for_Receive = 0;
}

/*
** ===================================================================
**     Event       :  SPI0_WIFI_OnError (module Events)
**
**     Component   :  SPI0_WIFI [SPIMaster_LDD]
**     Description :
**         This event is called when a channel error (not the error
**         returned by a given method) occurs. The errors can be read
**         using <GetError> method. This event is enabled when SPI
**         device supports error detect.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void SPI0_WIFI_OnError(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	cc_print(&g_cc_handle, "SPI0 WIFI gave an error!\n");
}

/*
** ===================================================================
**     Event       :  SPI1_Accel_OnBlockSent (module Events)
**
**     Component   :  SPI1_Accel [SPIMaster_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter. This event is
**         available only if the SendBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void SPI1_Accel_OnBlockSent(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_SPI1_Accel_Wait_for_Send = FALSE;
}

/*
** ===================================================================
**     Event       :  SPI1_Accel_OnBlockReceived (module Events)
**
**     Component   :  SPI1_Accel [SPIMaster_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer. This method is available only if
**         the ReceiveBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void SPI1_Accel_OnBlockReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_SPI1_Accel_Wait_for_Receive = FALSE;

}

/*
** ===================================================================
**     Event       :  SPI1_Accel_OnError (module Events)
**
**     Component   :  SPI1_Accel [SPIMaster_LDD]
**     Description :
**         This event is called when a channel error (not the error
**         returned by a given method) occurs. The errors can be read
**         using <GetError> method. This event is enabled when SPI
**         device supports error detect.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void SPI1_Accel_OnError(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	cc_print(&g_cc_handle, "SPI1 Accel gave an error!\n");
}

/*
** ===================================================================
**     Event       :  I2C1_OnMasterBlockSent (module Events)
**
**     Component   :  I2C1 [I2C_LDD]
**     Description :
**         This event is called when I2C in master mode finishes the
**         transmission of the data successfully. This event is not
**         available for the SLAVE mode and if MasterSendBlock is
**         disabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void I2C1_OnMasterBlockSent(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_I2C1_DataTransmittedFlg = TRUE;
}

/*
** ===================================================================
**     Event       :  I2C1_OnMasterBlockReceived (module Events)
**
**     Component   :  I2C1 [I2C_LDD]
**     Description :
**         This event is called when I2C is in master mode and finishes
**         the reception of the data successfully. This event is not
**         available for the SLAVE mode and if MasterReceiveBlock is
**         disabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void I2C1_OnMasterBlockReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_I2C1_DataReceivedFlg = TRUE;
}

/*
** ===================================================================
**     Event       :  I2C1_OnMasterByteReceived (module Events)
**
**     Component   :  I2C1 [I2C_LDD]
**     Description :
**         This event is called when I2C is in master mode and finishes
**         the reception of the byte successfully. This event is not
**         available for the SLAVE mode and if MasterReceiveBlock is
**         disabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void I2C1_OnMasterByteReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Event       :  I2C1_OnError (module Events)
**
**     Component   :  I2C1 [I2C_LDD]
**     Description :
**         This event is called when an error (e.g. Arbitration lost)
**         occurs. The errors can be read with GetError method.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void I2C1_OnError(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	//cc_print(&g_cc_handle, "I2C1 gave an error!\n");
}

/*
** ===================================================================
**     Event       :  SPI2_Flash_OnBlockSent (module Events)
**
**     Component   :  SPI2_Flash [SPIMaster_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter. This event is
**         available only if the SendBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void SPI2_Flash_OnBlockSent(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_SPI2_Flash_Wait_for_Send = 0;
}

/*
** ===================================================================
**     Event       :  SPI2_Flash_OnBlockReceived (module Events)
**
**     Component   :  SPI2_Flash [SPIMaster_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer. This method is available only if
**         the ReceiveBlock method is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void SPI2_Flash_OnBlockReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_SPI2_Flash_Wait_for_Receive = 0;
}

/*
** ===================================================================
**     Event       :  SPI2_Flash_OnError (module Events)
**
**     Component   :  SPI2_Flash [SPIMaster_LDD]
**     Description :
**         This event is called when a channel error (not the error
**         returned by a given method) occurs. The errors can be read
**         using <GetError> method. This event is enabled when SPI
**         device supports error detect.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void SPI2_Flash_OnError(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	cc_print(&g_cc_handle, "SPI2 Flash gave an error!\n");
}

/*
** ===================================================================
**     Event       :  UART3_WIFI_OnBlockReceived (module Events)
**
**     Component   :  UART3_WIFI [Serial_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void UART3_WIFI_OnBlockReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
//! \todo Need a "block received" flag setting here.
}

/*
** ===================================================================
**     Event       :  UART3_WIFI_OnBlockSent (module Events)
**
**     Component   :  UART3_WIFI [Serial_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void UART3_WIFI_OnBlockSent(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
//! \todo Need a "block sent" flag setting here.
}

/*
** ###################################################################
**
**  The interrupt service routine(s) must be implemented
**  by user in one of the following user modules.
**
**  If the "Generate ISR" option is enabled, Processor Expert generates
**  ISR templates in the CPU event module.
**
**  User modules:
**      ProcessorExpert.c
**      Events.c
**
** ###################################################################
*/
PE_ISR(PortC_GPIO_ISR)
{
	//cc_print(&g_cc_handle, "PORTC Interrupt Entered.\n\n");
	//cc_print(&g_cc_handle, "TODO - ISFR register not working properly!\n");
    
#if 0
	if(PORTC_ISFR & ACC_INT1_MASK)
	{
		PORTC_ISFR |= ACC_INT1_MASK;
		ciu_inc(INT_PORTC, 12);
	}

	if(PORTC_ISFR & ACC_INT2_MASK)
	{
		PORTC_ISFR |= ACC_INT2_MASK;
		ciu_inc(INT_PORTC, 13);
	}

	if(PORTC_ISFR & CHG_B_INT_MASK)
	{
		PORTC_ISFR |= CHG_B_INT_MASK;
		ciu_inc(INT_PORTC, 14);
	}

	if(PORTC_ISFR & PGOOD_B_INT_MASK)
	{
		PORTC_ISFR |= PGOOD_B_INT_MASK;
		ciu_inc(INT_PORTC, 15);
	}
#endif
	
	PORTC_ISFR |= 0xFFFFFFFF;
}


/*
** ###################################################################
**
**  The interrupt service routine(s) must be implemented
**  by user in one of the following user modules.
**
**  If the "Generate ISR" option is enabled, Processor Expert generates
**  ISR templates in the CPU event module.
**
**  User modules:
**      ProcessorExpert.c
**      Events.c
**
** ###################################################################
*/
PE_ISR(PORTE_ISR)
{
    static uint32_t toggle = 0;
    static bu_color_t local_color = 0;
#if 0
	if(PORTE_ISFR & WIFI_INT_MASK)
	{
		PORTE_ISFR |= WIFI_INT_MASK;
		//ciu_inc(INT_PORTE, 5);
	}
#endif
	
	if(PORTE_ISFR & PGOOD_B_INT_MASK)
	{
	    PORTE_ISFR |= PGOOD_B_INT_MASK;
	    g_pgood = 0;
	}

	if(PORTE_ISFR & BUTTON_INT_MASK)
	{
	    if(local_color == BU_NUM_COLORS)
	    {
	        local_color = 0;
	    }
	    
	    g_cstate = local_color;
	    
	    local_color++;
	    
	    PORTE_ISFR |= BUTTON_INT_MASK;
	}
}


/*
** ===================================================================
**     Event       :  UART0_BT_OnBlockReceived (module Events)
**
**     Component   :  UART0_BT [Serial_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void UART0_BT_OnBlockReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_UART0_DataReceivedFlg = TRUE;
}

/*
** ===================================================================
**     Event       :  UART0_BT_OnBlockSent (module Events)
**
**     Component   :  UART0_BT [Serial_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void UART0_BT_OnBlockSent(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_UART0_DataTransmittedFlg = TRUE;
}

/*
** ===================================================================
**     Event       :  UART5_DBG_OnBlockReceived (module Events)
**
**     Component   :  UART5_DBG [Serial_LDD]
**     Description :
**         This event is called when the requested number of data is
**         moved to the input buffer.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void UART5_DBG_OnBlockReceived(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_UART5_DataReceivedFlg = TRUE;
}

/*
** ===================================================================
**     Event       :  UART5_DBG_OnBlockSent (module Events)
**
**     Component   :  UART5_DBG [Serial_LDD]
**     Description :
**         This event is called after the last character from the
**         output buffer is moved to the transmitter.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void UART5_DBG_OnBlockSent(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	g_UART5_DataTransmittedFlg = TRUE;
}

/*
** ===================================================================
**     Event       :  UART5_DBG_OnTxComplete (module Events)
**
**     Component   :  UART5_DBG [Serial_LDD]
**     Description :
**         This event indicates that the transmitter is finished
**         transmitting all data, preamble, and break characters and is
**         idle. It can be used to determine when it is safe to switch
**         a line driver (e.g. in RS-485 applications).
**         The event is available only when both <Interrupt
**         service/event> and <Transmitter> properties are enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void UART5_DBG_OnTxComplete(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

/*
** ===================================================================
**     Event       :  Sleep_Timer_LDD_OnInterrupt (module Events)
**
**     Component   :  Sleep_Timer_LDD [TimerInt_LDD]
**     Description :
**         Called if periodic event occur. Component and OnInterrupt
**         event must be enabled. See <SetEventMask> and <GetEventMask>
**         methods. This event is available only if a <Interrupt
**         service/event> is enabled.
**     Parameters  :
**         NAME            - DESCRIPTION
**       * UserDataPtr     - Pointer to the user or
**                           RTOS specific data. The pointer passed as
**                           the parameter of Init method.
**     Returns     : Nothing
** ===================================================================
*/
void Sleep_Timer_LDD_OnInterrupt(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
	sleep_timer_tick();
}

/*
** ===================================================================
**     Event       :  RNG1_OnError (module Events)
**
**     Component   :  RNG1 [RNG_LDD]
*/
/*!
**     @brief
**         Called when empty FIFO is read (interrupt must be enabled
**         and no mask applied). See SetEventMask() and GetEventMask()
**         methods.
**     @param
**         UserDataPtr     - Pointer to the user or
**                           RTOS specific data. This pointer is passed
**                           as the parameter of Init method.
*/
/* ===================================================================*/
void RNG1_OnError(LDD_TUserData *UserDataPtr)
{
  /* Write your code here ... */
}

/* END Events */

/*
** ###################################################################
**
**     This file was created by Processor Expert 10.0 [05.03]
**     for the Freescale Kinetis series of microcontrollers.
**
** ###################################################################
*/
