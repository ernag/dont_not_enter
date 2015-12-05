/*****< hcitrans.c >***********************************************************/
/*      Copyright 2012 - 2013 Stonestreet One.                                */
/*      All Rights Reserved.                                                  */
/*                                                                            */
/*  HCITRANS - HCI Transport Layer for use with Bluetopia.                    */
/*                                                                            */
/*  Author:  Marcus Funk                                                      */
/*                                                                            */
/*** MODIFICATION HISTORY *****************************************************/
/*                                                                            */
/*   mm/dd/yy  F. Lastname    Description of Modification                     */
/*   --------  -----------    ------------------------------------------------*/
/*   11/08/12  M. Funk        Initial creation.                               */
/******************************************************************************/
#include <mqx.h>
#include <bsp.h>
#include <fio.h>

#include "BTPSKRNL.h"       /* Bluetooth Kernel Prototypes/Constants.         */
#include "HCITRANS.h"       /* HCI Transport Prototypes/Constants.            */

#define INPUT_DMA_BUFFER_SIZE        64
#define INPUT_BUFFER_SIZE            512
#define OUTPUT_BUFFER_SIZE           2048
#define MAXIMUM_DMA_TXD_COUNT        64

#define UART_BASE                    UART1_BASE_PTR
#define UART_PORT_MUX                3
#define UART_TX_FIFO_SIZE            8
#define UART_TX_FIFO_THRESHOLD       7
#define UART_RX_FIFO_SIZE            8
#define UART_RX_FIFO_THRESHOLD       1

#define UART_TXD_DMA_SOURCE          5
#define UART_RXD_DMA_SOURCE          4
#define UART_TXD_DMA_CHANNEL         0
#define UART_RXD_DMA_CHANNEL         1
#define UART_TXD_DMA_IRQ             0
#define UART_RXD_DMA_IRQ             1
#define UART_TXD_DMA_VECTOR          16
#define UART_RXD_DMA_VECTOR          17

   /* The following definition is the bit mask of the PORT clock gates  */
   /* that need to be enabled for the HCI interface.                    */
#define UART_PORT_SCG                SIM_SCGC5_PORTE_MASK

   /* The following definitions represent the ports and pins associated */
   /* with the signals used by the HCI interface.                       */
#define TXD_PORT                     PORTE_BASE_PTR
#define TXD_PIN                      0

#define RXD_PORT                     PORTE_BASE_PTR
#define RXD_PIN                      1

#define RTS_PORT                     PORTE_BASE_PTR
#define RTS_PIN                      3

#define CTS_PORT                     PORTE_BASE_PTR
#define CTS_PIN                      2

#define RESET_PORT                   PORTE_BASE_PTR
#define RESET_GPIO                   PTE_BASE_PTR
#define RESET_PIN                    4

#define EnableUartPeriphClock()      { SIM_SCGC4 |=  SIM_SCGC4_UART1_MASK; }
#define DisableUartPeriphClock()     { SIM_SCGC4 &= ~SIM_SCGC4_UART1_MASK; }

#define DisableInterrupts()          _int_disable()
#define EnableInterrupts()           _int_enable()

#define INTERRUPT_PRIORITY           6

#define TRANSPORT_ID                 1

typedef struct _tagUartContext_t
{
   ThreadHandle_t          ReceiveThreadHandle;
   LWEVENT_STRUCT          DataReceivedEvent;

   HCITR_COMDataCallback_t COMDataCallbackFunction;
   unsigned long           COMDataCallbackParameter;

   unsigned char           RxDMABuffer[INPUT_DMA_BUFFER_SIZE];

   unsigned short          RxInIndex;
   unsigned short          RxOutIndex;
   volatile unsigned short RxBytesFree;
   unsigned char           RxBuffer[INPUT_BUFFER_SIZE];

   unsigned short          TxDMARequestSize;
   unsigned short          TxInIndex;
   unsigned short          TxOutIndex;
   volatile unsigned short TxBytesFree;
   unsigned char           TxBuffer[OUTPUT_BUFFER_SIZE];
} UartContext_t;

   /* Internal Variables to this Module (Remember that all variables    */
   /* declared static are initialized to 0 automatically by the         */
   /* compiler as part of standard C/C++).                              */

static UartContext_t UartContext;
static int           HCITransportOpen = 0;

   /* Local Function Prototypes.                                        */
static void  SetBaudRate(unsigned long BaudRate);
static void  HCI_TXD_IRQHandler(void *Param);
static void  HCI_RXD_IRQHandler(void *Param);
static void *RxThread(void *Param);

   /* The following function will reconfigure the BAUD rate without     */
   /* reconfiguring the entire port.  This function is also potentially */
   /* more accurate than the method used in the ST standard peripheral  */
   /* libraries.                                                        */
static void SetBaudRate(unsigned long BaudRate)
{
   unsigned long SourceClock;
   unsigned long Divider;

   if((UART_BASE == UART0_BASE_PTR) || (UART_BASE == UART1_BASE_PTR))
      SourceClock = _cm_get_clock(_cm_get_clock_configuration(), CM_CLOCK_SOURCE_CORE);
   else
      SourceClock = _cm_get_clock(_cm_get_clock_configuration(), CM_CLOCK_SOURCE_BUS);

   /* The following calculates the baud rate divisor and fractional     */
   /* adjustment rounded to the nearest value. The result is formated as*/
   /* follows:                                                          */
   /*    [|     BDRH     |         BDRL          |     BRFA     |]      */
   /*    [.17.16.15.14.13.12.11.10.09.08.07.06.05.04.03.02.01.00.]      */
   Divider = (((4 * SourceClock) / BaudRate) + 1) / 2;

   UART_BDH_REG(UART_BASE) = ((Divider >> 13) & 0x1F);
   UART_BDL_REG(UART_BASE) = ((Divider >>  5) & 0xFF);
   UART_C4_REG(UART_BASE)  = (Divider         & 0x1F);
}

   /* The following function is the FIFO Primer and Interrupt Service   */
   /* Routine for the UART TX interrupt.                                */
static void HCI_TXD_IRQHandler(void* Param)
{
   unsigned int  Length;

   /* First clear the interrupt. This must be done here so that if the  */
   /* interrupt flag is set again while in the ISR, it will be handled. */
   DMA_CINT = UART_TXD_DMA_CHANNEL;

   /* Only configure a DMA request if one is not already configured.    */
   /* This is determined by checking if the DMA request is currently    */
   /* enabled.                                                          */
   if(!(DMA_ERQ & (1 << UART_TXD_DMA_CHANNEL)))
   {
      /* Wait to make sure that no transfers are active. As the DMA only*/
      /* moves one byte at a time, this shouldn't take very long.       */
      while(DMA_CSR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) & (DMA_CSR_ACTIVE_MASK)) {};

      /* If a DMA request has just been completed, adjust the buffer    */
      /* paramters for the data that was transmitted.                   */
      if(UartContext.TxDMARequestSize)
      {
         UartContext.TxBytesFree += UartContext.TxDMARequestSize;

         UartContext.TxOutIndex += UartContext.TxDMARequestSize;
         if(UartContext.TxOutIndex >= OUTPUT_BUFFER_SIZE)
            UartContext.TxOutIndex -= OUTPUT_BUFFER_SIZE;
      }

      /* If more data is waiting in the transmit buffer, setup a new DMA*/
      /* request.                                                       */
      Length = OUTPUT_BUFFER_SIZE - UartContext.TxBytesFree;
      if(Length)
      {
         /* Determine the length of the transfer.                       */
         if(Length > (OUTPUT_BUFFER_SIZE - UartContext.TxOutIndex))
            Length = (OUTPUT_BUFFER_SIZE - UartContext.TxOutIndex);
         if(Length > MAXIMUM_DMA_TXD_COUNT)
            Length = MAXIMUM_DMA_TXD_COUNT;

         /* Setup and enable the DMA request.                           */
         UartContext.TxDMARequestSize = Length;

         DMA_SADDR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)         = (unsigned int)(&UartContext.TxBuffer[UartContext.TxOutIndex]);
         DMA_CITER_ELINKNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) = Length;
         DMA_BITER_ELINKNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) = Length;

         DMA_SERQ = UART_TXD_DMA_CHANNEL;
      }
      else
      {
         UartContext.TxDMARequestSize = 0;

         /* Since the transmit buffer is empty, disable the DMA         */
         /* interrupt.                                                  */
         NVIC_ICER(UART_TXD_DMA_IRQ >> 5) = (1 << (UART_TXD_DMA_IRQ & 0x1F));
      }
   }
}

   /* The following function is the Interrupt Service Routine for the   */
   /* UART RX interrupt.                                                */
static void HCI_RXD_IRQHandler(void *Param)
{
   unsigned int BytesFree;
   unsigned int BytesInDMABuffer;
   unsigned int FirstPassLength;
   unsigned int SecondPassLength;

   /* First clear the interrupt. This must be done here so that if the  */
   /* interrupt flag is set again while in the ISR, it will be handled. */
   DMA_CINT = UART_RXD_DMA_CHANNEL;

   /* Momentarily disable the DMA requests for the receive channel and  */
   /* wait for any outstanding transfers to complete. As the DMA only   */
   /* moves one byte at a time, this shouldn't take very long.          */
   DMA_CERQ = UART_RXD_DMA_CHANNEL;
   while(DMA_CSR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) & (DMA_CSR_ACTIVE_MASK)) {};

   /* Determine how much data needs to be copied from the DMA buffer to */
   /* the receive buffer.                                               */
   BytesFree = UartContext.RxBytesFree;
   BytesInDMABuffer = DMA_DADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) - (unsigned int)(UartContext.RxDMABuffer);

   /* Only copy data from the DMA buffer if there is enough space in the*/
   /* receive buffer.                                                   */
   if(BytesFree >= BytesInDMABuffer)
   {
      /* Determine if the data will wrap the receive buffer. If it does,*/
      /* the data will need to be copied as two segments.               */
      FirstPassLength = INPUT_BUFFER_SIZE - UartContext.RxInIndex;
      if(BytesInDMABuffer > FirstPassLength)
      {
         /* Copy the data in two segments.                              */
         SecondPassLength = BytesInDMABuffer - FirstPassLength;
         BTPS_MemCopy(&UartContext.RxBuffer[UartContext.RxInIndex], UartContext.RxDMABuffer, FirstPassLength);
         BTPS_MemCopy(UartContext.RxBuffer, &UartContext.RxDMABuffer[FirstPassLength], SecondPassLength);
         UartContext.RxInIndex = SecondPassLength;
      }
      else
      {
         if(BytesInDMABuffer)
         {
            /* Copy the data as one segment.                            */
            BTPS_MemCopy(&UartContext.RxBuffer[UartContext.RxInIndex], UartContext.RxDMABuffer, BytesInDMABuffer);
            UartContext.RxInIndex    += BytesInDMABuffer;
            if(UartContext.RxInIndex == INPUT_BUFFER_SIZE)
               UartContext.RxInIndex  = 0;
         }
      }

      UartContext.RxBytesFree = BytesFree - BytesInDMABuffer;

      /* Since the data was read form the DMA buffer, reset the         */
      /* destination address to the beginning of the buffer.            */
      BytesInDMABuffer = 0;
      DMA_DADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = (unsigned int)(UartContext.RxDMABuffer);
      DMA_CITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = INPUT_DMA_BUFFER_SIZE;
      DMA_BITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = INPUT_DMA_BUFFER_SIZE;

      /* Enable the DMA request.                                        */
      DMA_SERQ = UART_RXD_DMA_CHANNEL;

      /* Signal the reception of some data.                             */
      _lwevent_set(&UartContext.DataReceivedEvent, 1);
   }
   else
   {
      /* Since the receive buffer is full, disable the interrupt.       */
      NVIC_ICER(UART_RXD_DMA_IRQ >> 5) = (1 << (UART_RXD_DMA_IRQ & 0x1F));

      /* If there is still space available at the end of the DMA buffer,*/
      /* re-enable the DMA requests.                                    */
      if(DMA_DADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) < (((unsigned int)(UartContext.RxDMABuffer)) + INPUT_DMA_BUFFER_SIZE))
      {
         /* Reset the CITER and BITER fields to the current space left  */
         /* in the DMA buffer.                                          */
         DMA_CITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = INPUT_DMA_BUFFER_SIZE - BytesInDMABuffer;
         DMA_BITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = INPUT_DMA_BUFFER_SIZE - BytesInDMABuffer;

         /* Enable the DMA request.                                     */
         DMA_SERQ = UART_RXD_DMA_CHANNEL;
      }
   }
}

   /* The following thread is used to process the data that has been    */
   /* received from the UART and placed in the receive buffer.          */
static void *RxThread(void *Param)
{
   int             MaxRead;
   int             Count;
   Boolean_t       Done;
   TIME_STRUCT     WaitTime;
   MQX_TICK_STRUCT WaitTicks;

   /* Setup the structure for a timed wait on the data received event.  */
   WaitTime.SECONDS      = 0;
   WaitTime.MILLISECONDS = 1;
   _time_to_ticks(&WaitTime, &WaitTicks);

   /* This thread will loop forever.                                    */
   Done = FALSE;
   while(!Done)
   {
      /* Wait until there is data available in the receive buffer.      */
      while((Count = (INPUT_BUFFER_SIZE - UartContext.RxBytesFree)) == 0)
      {
         /* Wait for an event to be received indicating that data is    */
         /* available in the receive buffer. Also, periodically check if*/
         /* data is availabe in the DMA buffer as the DMA interrupt will*/
         /* not occur until the buffer is half full.                    */
         if(_lwevent_wait_for(&UartContext.DataReceivedEvent, 1, FALSE, &WaitTicks) != MQX_OK)
         {
            DisableInterrupts();
            /* If there is space in the receive buffer and data in the  */
            /* DMA buffer, call the DMA interrupt handler to retreieve  */
            /* the data.                                                */
            if((UartContext.RxBytesFree) && (DMA_DADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) > ((unsigned int)(UartContext.RxDMABuffer))))
               HCI_RXD_IRQHandler(NULL);
            EnableInterrupts();
         }
      }

      /* Determine the maximum number of characters that we can send    */
      /* before we reach the end of the buffer.  We need to process the */
      /* smaller of the max characters or the number of characters that */
      /* are in the buffer.                                             */
      MaxRead = (INPUT_BUFFER_SIZE - UartContext.RxOutIndex);
      Count   = (MaxRead < Count) ? MaxRead : Count;

      /* Call the upper layer back with the data.                       */
      if(UartContext.COMDataCallbackFunction)
         (*UartContext.COMDataCallbackFunction)(TRANSPORT_ID, Count, &(UartContext.RxBuffer[UartContext.RxOutIndex]), UartContext.COMDataCallbackParameter);

      /* Adjust the Out Index and handle any wrapping.                  */
      UartContext.RxOutIndex += Count;
      if(UartContext.RxOutIndex == INPUT_BUFFER_SIZE)
         UartContext.RxOutIndex = 0;

      /* Credit the amount that was processed and make sure the receive */
      /* interrupt is enabled.                                          */
      DisableInterrupts();
      UartContext.RxBytesFree += Count;
      NVIC_ISER(UART_RXD_DMA_IRQ >> 5) = (1 << (UART_RXD_DMA_IRQ & 0x1F));
      EnableInterrupts();
   }

   return(NULL);
}

   /* The following function is responsible for opening the HCI         */
   /* Transport layer that will be used by Bluetopia to send and receive*/
   /* COM (Serial) data.  This function must be successfully issued in  */
   /* order for Bluetopia to function.  This function accepts as its    */
   /* parameter the HCI COM Transport COM Information that is to be used*/
   /* to open the port.  The final two parameters specify the HCI       */
   /* Transport Data Callback and Callback Parameter (respectively) that*/
   /* is to be called when data is received from the UART.  A successful*/
   /* call to this function will return a non-zero, positive value which*/
   /* specifies the HCITransportID that is used with the remaining      */
   /* transport functions in this module.  This function returns a      */
   /* negative return value to signify an error.                        */
int BTPSAPI HCITR_COMOpen(HCI_COMMDriverInformation_t *COMMDriverInformation, HCITR_COMDataCallback_t COMDataCallback, unsigned long CallbackParameter)
{
   int ret_val;

   /* First, make sure that the port is not already open and make sure  */
   /* that valid COMM Driver Information was specified.                 */
   if((!HCITransportOpen) && (COMMDriverInformation) && (COMDataCallback))
   {
      /* Initialize the return value for success.                       */
      ret_val               = TRANSPORT_ID;

      /* Initialize the context structure.                              */
      BTPS_MemInitialize(&UartContext, 0, sizeof(UartContext_t));

      UartContext.COMDataCallbackFunction  = COMDataCallback;
      UartContext.COMDataCallbackParameter = CallbackParameter;
      UartContext.RxBytesFree              = INPUT_BUFFER_SIZE;
      UartContext.TxBytesFree              = OUTPUT_BUFFER_SIZE;

      /* Create the event that will be used to signal data has arrived. */
      if(_lwevent_create(&UartContext.DataReceivedEvent, LWEVENT_AUTO_CLEAR) == MQX_OK)
      {
         /* Make sure that the event is in the reset state.             */
         _lwevent_clear(&UartContext.DataReceivedEvent, 1);

         /* Create a thread that will process the received data.        */
         if(!UartContext.ReceiveThreadHandle)
            UartContext.ReceiveThreadHandle = BTPS_CreateThread(RxThread, 768, NULL);

         if(!UartContext.ReceiveThreadHandle)
         {
            _lwevent_destroy(&UartContext.DataReceivedEvent);
            ret_val = HCITR_ERROR_UNABLE_TO_OPEN_TRANSPORT;
         }
      }
      else
         ret_val = HCITR_ERROR_UNABLE_TO_OPEN_TRANSPORT;

      /* If there was no error, then continue to setup the port.        */
      if(ret_val != HCITR_ERROR_UNABLE_TO_OPEN_TRANSPORT)
      {
         /* Configure the GPIO.                                         */
         SIM_SCGC5 |= UART_PORT_SCG;
         PORT_PCR_REG(TXD_PORT, TXD_PIN)     = PORT_PCR_MUX(UART_PORT_MUX) | PORT_PCR_DSE_MASK;
         PORT_PCR_REG(RXD_PORT, RXD_PIN)     = PORT_PCR_MUX(UART_PORT_MUX) | PORT_PCR_DSE_MASK;
         PORT_PCR_REG(RTS_PORT, RTS_PIN)     = PORT_PCR_MUX(UART_PORT_MUX) | PORT_PCR_DSE_MASK;
         PORT_PCR_REG(CTS_PORT, CTS_PIN)     = PORT_PCR_MUX(UART_PORT_MUX) | PORT_PCR_DSE_MASK;
         PORT_PCR_REG(RESET_PORT, RESET_PIN) = PORT_PCR_MUX(1);
         GPIO_PDDR_REG(RESET_GPIO) |= (1 << RESET_PIN);

         /* Place the Bluetooth device in reset.                        */
         GPIO_PCOR_REG(RESET_GPIO) |= (1 << RESET_PIN);

         /* Enable the UART clock.                                      */
         EnableUartPeriphClock();
         SIM_SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
         SIM_SCGC7 |= SIM_SCGC7_DMA_MASK;

         /* Initialize the UART.                                        */
         UART_C1_REG(UART_BASE) = 0;
         UART_C2_REG(UART_BASE) = 0;
         UART_C3_REG(UART_BASE) = 0;
         UART_C5_REG(UART_BASE) = UART_C5_TDMAS_MASK | UART_C5_RDMAS_MASK;
         UART_MODEM_REG(UART_BASE) = UART_MODEM_RXRTSE_MASK | UART_MODEM_TXCTSE_MASK;
         SetBaudRate(COMMDriverInformation->BaudRate);
         UART_PFIFO_REG(UART_BASE) = (UART_PFIFO_REG(UART_BASE) & (UART_PFIFO_TXFIFOSIZE_MASK | UART_PFIFO_RXFIFOSIZE_MASK)) | UART_PFIFO_TXFE_MASK | UART_PFIFO_RXFE_MASK;
         UART_TWFIFO_REG(UART_BASE) = UART_TX_FIFO_THRESHOLD;
         UART_RWFIFO_REG(UART_BASE) = UART_RX_FIFO_THRESHOLD;
         UART_CFIFO_REG(UART_BASE) = UART_CFIFO_TXFLUSH_MASK | UART_CFIFO_RXFLUSH_MASK;
         UART_C2_REG(UART_BASE) = UART_C2_RIE_MASK | UART_C2_TIE_MASK | UART_C2_TE_MASK | UART_C2_RE_MASK;

         /* Configure the DMAs.                                         */

         /* Configure the receive DMA channel.                          */
         DMAMUX_CHCFG_REG(DMAMUX_BASE_PTR, UART_RXD_DMA_CHANNEL)   = DMAMUX_CHCFG_ENBL_MASK | DMAMUX_CHCFG_SOURCE(UART_RXD_DMA_SOURCE);
         DMA_SADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)         = (unsigned int)(&UART_D_REG(UART_BASE));
         DMA_DADDR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)         = (unsigned int)(UartContext.RxDMABuffer);
         DMA_SOFF_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)          = 0;
         DMA_DOFF_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)          = 1;
         DMA_ATTR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)          = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0);
         DMA_SLAST_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)         = 0;
         DMA_DLAST_SGA_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)     = 0;
         DMA_NBYTES_MLNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)   = 1;
         DMA_CITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = INPUT_DMA_BUFFER_SIZE;
         DMA_BITER_ELINKNO_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL) = INPUT_DMA_BUFFER_SIZE;
         DMA_CSR_REG(DMA_BASE_PTR, UART_RXD_DMA_CHANNEL)           = DMA_CSR_DREQ_MASK | DMA_CSR_INTHALF_MASK | DMA_CSR_INTMAJOR_MASK;

         /* Configure the transmit DMA channel.                         */
         DMAMUX_CHCFG_REG(DMAMUX_BASE_PTR, UART_TXD_DMA_CHANNEL)   = DMAMUX_CHCFG_ENBL_MASK | DMAMUX_CHCFG_SOURCE(UART_TXD_DMA_SOURCE);
         DMA_SADDR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)         = (unsigned int)(UartContext.TxBuffer);
         DMA_DADDR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)         = (unsigned int)(&UART_D_REG(UART_BASE));
         DMA_SOFF_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)          = 1;
         DMA_DOFF_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)          = 0;
         DMA_ATTR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)          = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0);
         DMA_SLAST_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)         = 0;
         DMA_DLAST_SGA_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)     = 0;
         DMA_NBYTES_MLNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)   = 1;
         DMA_CITER_ELINKNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) = 0;
         DMA_BITER_ELINKNO_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL) = 0;
         DMA_CSR_REG(DMA_BASE_PTR, UART_TXD_DMA_CHANNEL)           = DMA_CSR_DREQ_MASK | DMA_CSR_INTMAJOR_MASK;

         /* Enable the DMA channels.                                    */
         DMA_SERQ = UART_RXD_DMA_CHANNEL;

         _int_install_isr(UART_TXD_DMA_VECTOR, HCI_TXD_IRQHandler, NULL);
         _int_install_isr(UART_RXD_DMA_VECTOR, HCI_RXD_IRQHandler, NULL);
         NVIC_ICPR(UART_TXD_DMA_IRQ >> 5) = (1 << (UART_TXD_DMA_IRQ & 0x1F));
         NVIC_ICPR(UART_RXD_DMA_IRQ >> 5) = (1 << (UART_RXD_DMA_IRQ & 0x1F));
         NVIC_IP(UART_TXD_DMA_IRQ)        = (INTERRUPT_PRIORITY << 4);
         NVIC_IP(UART_RXD_DMA_IRQ)        = (INTERRUPT_PRIORITY << 4);
         NVIC_ICER(UART_TXD_DMA_IRQ >> 5) = (1 << (UART_TXD_DMA_IRQ & 0x1F));
         NVIC_ISER(UART_RXD_DMA_IRQ >> 5) = (1 << (UART_RXD_DMA_IRQ & 0x1F));

         /* Clear the reset.                                            */
         BTPS_Delay(10);
         GPIO_PSOR_REG(RESET_GPIO) |= (1 << RESET_PIN);

         /* Check to see if we need to delay after opening the COM Port.*/
         if(COMMDriverInformation->InitializationDelay)
            BTPS_Delay(COMMDriverInformation->InitializationDelay);

         /* Flag that the HCI Transport is open.                        */
         HCITransportOpen = 1;
      }
   }
   else
      ret_val = HCITR_ERROR_UNABLE_TO_OPEN_TRANSPORT;

   return(ret_val);
}

   /* The following function is responsible for closing the specific HCI*/
   /* Transport layer that was opened via a successful call to the      */
   /* HCITR_COMOpen() function (specified by the first parameter).      */
   /* Bluetopia makes a call to this function whenever an either        */
   /* Bluetopia is closed, or an error occurs during initialization and */
   /* the driver has been opened (and ONLY in this case).  Once this    */
   /* function completes, the transport layer that was closed will no   */
   /* longer process received data until the transport layer is         */
   /* Re-Opened by calling the HCITR_COMOpen() function.                */
   /* * NOTE * This function *MUST* close the specified COM Port.  This */
   /*          module will then call the registered COM Data Callback   */
   /*          function with zero as the data length and NULL as the    */
   /*          data pointer.  This will signify to the HCI Driver that  */
   /*          this module is completely finished with the port and     */
   /*          information and (more importantly) that NO further data  */
   /*          callbacks will be issued.  In other words the very last  */
   /*          data callback that is issued from this module *MUST* be a*/
   /*          data callback specifying zero and NULL for the data      */
   /*          length and data buffer (respectively).                   */
void BTPSAPI HCITR_COMClose(unsigned int HCITransportID)
{
   HCITR_COMDataCallback_t COMDataCallback;

   /* Check to make sure that the specified Transport ID is valid.      */
   if((HCITransportID == TRANSPORT_ID) && (HCITransportOpen))
   {
      /* Appears to be valid, go ahead and close the port.              */
      UART_C2_REG(UART_BASE) = 0;

      /* Place the Bluetooth Device in Reset.                           */
      GPIO_PCOR_REG(RESET_GPIO) |= (1 << RESET_PIN);

      /* Disable the peripheral clock for the UART.                     */
      DisableUartPeriphClock();

      /* Note the Callback information.                                 */
      COMDataCallback   = UartContext.COMDataCallbackFunction;

      /* Flag that the HCI Transport is no longer open.                 */
      HCITransportOpen = 0;
      UartContext.COMDataCallbackFunction = NULL;

      /* All finished, perform the callback to let the upper layer know */
      /* that this module will no longer issue data callbacks and is    */
      /* completely cleaned up.                                         */
      if(COMDataCallback)
         (*COMDataCallback)(HCITransportID, 0, NULL, UartContext.COMDataCallbackParameter);

      UartContext.COMDataCallbackParameter = 0;
   }
}

   /* The following function is responsible for instructing the         */
   /* specified HCI Transport layer (first parameter) that was opened   */
   /* via a successful call to the HCITR_COMOpen() function to          */
   /* reconfigure itself with the specified information.  This          */
   /* information is completely opaque to the upper layers and is passed*/
   /* through the HCI Driver layer to the transport untouched.  It is   */
   /* the responsibility of the HCI Transport driver writer to define   */
   /* the contents of this member (or completely ignore it).            */
   /* * NOTE * This function does not close the HCI Transport specified */
   /*          by HCI Transport ID, it merely reconfigures the          */
   /*          transport.  This means that the HCI Transport specified  */
   /*          by HCI Transport ID is still valid until it is closed    */
   /*          via the HCI_COMClose() function.                         */
void BTPSAPI HCITR_COMReconfigure(unsigned int HCITransportID, HCI_Driver_Reconfigure_Data_t *DriverReconfigureData)
{

   /* Check to make sure that the specified Transport ID is valid.      */
   if((HCITransportID == TRANSPORT_ID) && (HCITransportOpen) && (DriverReconfigureData))
   {
      if((DriverReconfigureData->ReconfigureCommand == HCI_COMM_DRIVER_RECONFIGURE_DATA_COMMAND_CHANGE_PARAMETERS) && (DriverReconfigureData->ReconfigureData))
      {
         DisableInterrupts();

         SetBaudRate(((HCI_COMMDriverInformation_t *)(DriverReconfigureData->ReconfigureData))->BaudRate);

         EnableInterrupts();
      }
   }
}

   /* The following function is responsible for actually sending data   */
   /* through the opened HCI Transport layer (specified by the first    */
   /* parameter).  Bluetopia uses this function to send formatted HCI   */
   /* packets to the attached Bluetooth Device.  The second parameter to*/
   /* this function specifies the number of bytes pointed to by the     */
   /* third parameter that are to be sent to the Bluetooth Device.  This*/
   /* function returns a zero if the all data was transferred           */
   /* successfully or a negative value if an error occurred.  This      */
   /* function MUST NOT return until all of the data is sent (or an     */
   /* error condition occurs).  Bluetopia WILL NOT attempt to call this */
   /* function repeatedly if data fails to be delivered.  This function */
   /* will block until it has either buffered the specified data or sent*/
   /* all of the specified data to the Bluetooth Device.                */
   /* * NOTE * The type of data (Command, ACL, SCO, etc.) is NOT passed */
   /*          to this function because it is assumed that this         */
   /*          information is contained in the Data Stream being passed */
   /*          to this function.                                        */
int BTPSAPI HCITR_COMWrite(unsigned int HCITransportID, unsigned int Length, unsigned char *Buffer)
{
   int ret_val;
   int Count;

   /* Check to make sure that the specified Transport ID is valid and   */
   /* the output buffer appears to be valid as well.                    */
   if((HCITransportID == TRANSPORT_ID) && (HCITransportOpen) && (Length) && (Buffer))
   {
      /* Process all of the data.                                       */
      while(Length)
      {
         /* Wait for space in the transmit buffer.                      */
         while(!UartContext.TxBytesFree)
            BTPS_Delay(1);

         /* The data may have to be copied in 2 or more phases.         */
         /* Calculate the number of character that can be placed in the */
         /* the buffer on this pass.                                    */
         Count = UartContext.TxBytesFree;
         if(Count > Length)
            Count = Length;
         if(Count > (OUTPUT_BUFFER_SIZE - UartContext.TxInIndex))
            Count = (OUTPUT_BUFFER_SIZE - UartContext.TxInIndex);

         BTPS_MemCopy(&(UartContext.TxBuffer[UartContext.TxInIndex]), Buffer, Count);

         /* Adjust the index values.                                    */
         Buffer                  += Count;
         Length                  -= Count;
         UartContext.TxInIndex   += Count;
         if(UartContext.TxInIndex == OUTPUT_BUFFER_SIZE)
            UartContext.TxInIndex = 0;

         /* Update the bytes free and make sure the transmit interrupt  */
         /* is enabled.                                                 */
         DisableInterrupts();
         UartContext.TxBytesFree -= Count;
         NVIC_ISER(UART_TXD_DMA_IRQ >> 5) = (1 << (UART_TXD_DMA_IRQ & 0x1F));

         /* If no transmit DMA request is currently enabled, call the   */
         /* interrupt handler to setup a new one.                       */
         if(!UartContext.TxDMARequestSize)
            HCI_TXD_IRQHandler(NULL);
         EnableInterrupts();
      }

      ret_val = 0;
   }
   else
      ret_val = HCITR_ERROR_WRITING_TO_PORT;

   return(ret_val);
}
