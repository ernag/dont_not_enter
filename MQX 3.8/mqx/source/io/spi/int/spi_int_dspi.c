/**HEADER********************************************************************
* 
* Copyright (c) 2009 Freescale Semiconductor;
* All Rights Reserved                       
*
*************************************************************************** 
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
**************************************************************************
*
* $FileName: spi_int_dspi.c$
* $Version : 3.8.11.1$
* $Date    : Oct-19-2011$
*
* Comments:
*
*   The file contains low level DSPI interrupt driver functions.
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>
#include <io_prv.h>
#include <fio_prv.h>
#include "spi.h"
#include "spi_int_prv.h"
#include "spi_dspi_prv.h"

extern uint_32 _dspi_polled_init(DSPI_INIT_STRUCT_PTR, pointer _PTR_, char_ptr);
extern uint_32 _dspi_polled_ioctl(DSPI_INFO_STRUCT_PTR, uint_32, uint_32_ptr, uint_32);

static uint_32 _dspi_int_init(IO_SPI_INT_DEVICE_STRUCT_PTR, char_ptr);
static uint_32 _dspi_int_deinit(IO_SPI_INT_DEVICE_STRUCT_PTR, DSPI_INFO_STRUCT_PTR);
static uint_32 _dspi_int_rx(IO_SPI_INT_DEVICE_STRUCT_PTR, uchar_ptr, int_32);
static uint_32 _dspi_int_tx(IO_SPI_INT_DEVICE_STRUCT_PTR, uchar_ptr, int_32);
static uint_32 _dspi_int_enable(DSPI_INFO_STRUCT_PTR io_info_ptr);
static void _dspi_int_isr(pointer parameter);
                       
/*FUNCTION****************************************************************
* 
* Function Name    : _dspi_int_install
* Returned Value   : MQX error code
* Comments         :
*    Install an SPI device.
*
*END*********************************************************************/
uint_32 _dspi_int_install
   (
      /* [IN] A string that identifies the device for fopen */
      char_ptr                         identifier,
  
      /* [IN] The I/O init data pointer */
      DSPI_INIT_STRUCT_CPTR            init_data_ptr
   )
{
   return _io_spi_int_install(identifier,
      (uint_32 (_CODE_PTR_)(pointer,char_ptr))_dspi_int_init,
      (uint_32 (_CODE_PTR_)(pointer))_dspi_int_enable,
      (uint_32 (_CODE_PTR_)(pointer, pointer))_dspi_int_deinit,
      (_mqx_int (_CODE_PTR_)(pointer, char_ptr, int_32))_dspi_int_rx,
      (_mqx_int (_CODE_PTR_)(pointer, char_ptr, int_32))_dspi_int_tx,
      (_mqx_int (_CODE_PTR_)(pointer, uint_32, _mqx_uint_ptr, _mqx_uint))_dspi_polled_ioctl, 
      (pointer)init_data_ptr);
}

/*FUNCTION****************************************************************
* 
* Function Name    : _dspi_int_init  
* Returned Value   : MQX error code
* Comments         :
*    This function initializes the SPI module 
*
*END*********************************************************************/
static uint_32 _dspi_int_init
  (
     /* [IN] The address of the device specific information */
     IO_SPI_INT_DEVICE_STRUCT_PTR int_io_dev_ptr,

     /* [IN] The rest of the name of the device opened */
     char_ptr                     open_name_ptr
  )
{
   VDSPI_REG_STRUCT_PTR           dspi_ptr;
   DSPI_INFO_STRUCT_PTR           io_info_ptr;
   DSPI_INIT_STRUCT_PTR           spi_init_ptr;
   uint_32                        result = SPI_OK;
   const uint_32 _PTR_            vectors;
   int                            num_vectors;
   int                            i;

   /* Initialization as in polled mode */
   spi_init_ptr = (DSPI_INIT_STRUCT_PTR)(int_io_dev_ptr->DEV_INIT_DATA_PTR);
   result = _dspi_polled_init(spi_init_ptr,
                                     &(int_io_dev_ptr->DEV_INFO_PTR),
                                     open_name_ptr);
   if (result)
   {
      return result;
   }
   
   num_vectors = _bsp_get_dspi_vectors(spi_init_ptr->CHANNEL, &vectors);
   if (0 == num_vectors)
   {
      _mem_free (int_io_dev_ptr->DEV_INFO_PTR);
      int_io_dev_ptr->DEV_INFO_PTR = NULL;
      return MQX_INVALID_VECTORED_INTERRUPT;
   }
   
   io_info_ptr = int_io_dev_ptr->DEV_INFO_PTR;
   dspi_ptr = io_info_ptr->DSPI_PTR;

   /* Disable TX FIFO */   
   dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
   dspi_ptr->MCR |= DSPI_MCR_DIS_TXF_MASK | DSPI_MCR_DIS_RXF_MASK;
   dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);
   
   /* Allocate buffers rounding size up to even number */
   spi_init_ptr->RX_BUFFER_SIZE = (spi_init_ptr->RX_BUFFER_SIZE + 1) & ~((uint_32)1);
   io_info_ptr->RX_BUFFER = (pointer)_mem_alloc_system(spi_init_ptr->RX_BUFFER_SIZE);
   if (NULL == io_info_ptr->RX_BUFFER) 
   {
      _mem_free (int_io_dev_ptr->DEV_INFO_PTR);
      int_io_dev_ptr->DEV_INFO_PTR = NULL;
      return MQX_OUT_OF_MEMORY;
   }

   spi_init_ptr->TX_BUFFER_SIZE = (spi_init_ptr->TX_BUFFER_SIZE + 1) & ~((uint_32)1);
   io_info_ptr->TX_BUFFER = (pointer)_mem_alloc_system(spi_init_ptr->TX_BUFFER_SIZE);
   if (NULL == io_info_ptr->TX_BUFFER) 
   {
      _mem_free (int_io_dev_ptr->DEV_INFO_PTR);
      int_io_dev_ptr->DEV_INFO_PTR = NULL;
      _mem_free (io_info_ptr->RX_BUFFER);
      io_info_ptr->RX_BUFFER = NULL;
      return MQX_OUT_OF_MEMORY;
   }
   
   _mem_set_type(io_info_ptr->RX_BUFFER,MEM_TYPE_IO_SPI_IN_BUFFER);       
   _mem_set_type(io_info_ptr->TX_BUFFER,MEM_TYPE_IO_SPI_OUT_BUFFER);       
   
   /* Install ISRs */
   for (i=0; i<num_vectors; i++) {
      io_info_ptr->OLD_ISR_DATA[i] = _int_get_isr_data(vectors[i]);
      io_info_ptr->OLD_ISR[i] = _int_install_isr(vectors[i], _dspi_int_isr, int_io_dev_ptr);
   }
   
   return SPI_OK; 
}

/*FUNCTION****************************************************************
* 
* Function Name    : _dspi_int_deinit  
* Returned Value   : MQX error code
* Comments         :
*    This function de-initializes the SPI module 
*
*END*********************************************************************/
static uint_32 _dspi_int_deinit
   (
      /* [IN] The address of the device specific information */
      IO_SPI_INT_DEVICE_STRUCT_PTR int_io_dev_ptr,

      /* [IN] The address of the channel specific information */
      DSPI_INFO_STRUCT_PTR         io_info_ptr
   )
{
   VDSPI_REG_STRUCT_PTR            dspi_ptr;
   DSPI_INIT_STRUCT_PTR            dspi_init_ptr;
   uint_32                         index;
   const uint_32 _PTR_             vectors;
   int                             num_vectors;
   int                             i;
   
   if ((NULL == io_info_ptr) || (NULL == int_io_dev_ptr)) 
   {
      return SPI_ERROR_DEINIT_FAILED;
   }
   
   /* Disable the SPI */
   dspi_ptr = io_info_ptr->DSPI_PTR;
   index = dspi_ptr->MCR & DSPI_MCR_MSTR_MASK;
   dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
   dspi_ptr->RSER = 0;

   /* Disable all chip selects */
   if (0 != index) 
   {
      for (index = 0; index < DSPI_CS_COUNT; index++)
      {
         if ((NULL != io_info_ptr->CS_CALLBACK[index]) && (0 != (io_info_ptr->CS_ACTIVE & (1 << index))))
         {
            io_info_ptr->CS_CALLBACK[index] (DSPI_PUSHR_PCS(1 << index), 1, io_info_ptr->CS_USERDATA[index]);
         }                
      }
      io_info_ptr->CS_ACTIVE = 0;
   }
   
   /* Return original interrupt vectors */
   dspi_init_ptr = (DSPI_INIT_STRUCT_PTR)(int_io_dev_ptr->DEV_INIT_DATA_PTR);
   num_vectors = _bsp_get_dspi_vectors(dspi_init_ptr->CHANNEL, &vectors);
   for (i=0; i<num_vectors; i++) {
      _int_install_isr(vectors[i], io_info_ptr->OLD_ISR[i], io_info_ptr->OLD_ISR_DATA[i]);
   }

   /* Release buffers */
   _mem_free(int_io_dev_ptr->DEV_INFO_PTR);
   int_io_dev_ptr->DEV_INFO_PTR = NULL;
   _mem_free(io_info_ptr->RX_BUFFER);
   io_info_ptr->RX_BUFFER = NULL;
   _mem_free(io_info_ptr->TX_BUFFER);
   io_info_ptr->TX_BUFFER = NULL;
      
   return SPI_OK;
}

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_int_enable
* Returned Value   : MQX error code
* Comments         :
*    This function enables receive and error interrupt.
*
*END*********************************************************************/
static uint_32 _dspi_int_enable
   ( 
   /* [IN] The address of the channel specific information */
      DSPI_INFO_STRUCT_PTR          io_info_ptr
   )
{
   VDSPI_REG_STRUCT_PTR             dspi_ptr;
   const uint_32 _PTR_              vectors;
   int                              num_vectors;
   int                              i;

   if (NULL == io_info_ptr)   
   {
      return SPI_ERROR_INVALID_PARAMETER;   
   }

   dspi_ptr = io_info_ptr->DSPI_PTR;
   dspi_ptr->RSER |= DSPI_RSER_RFDF_RE_MASK;
   
   num_vectors = _bsp_get_dspi_vectors(io_info_ptr->INIT.CHANNEL, &vectors);
   for (i=0; i<num_vectors; i++) {
      _bsp_int_init((PSP_INTERRUPT_TABLE_INDEX)vectors[i], BSP_DSPI_INT_LEVEL, 0, TRUE);
   }

   return SPI_OK;
}

/*FUNCTION****************************************************************
* 
* Function Name    : _dspi_int_rx
* Returned Value   : Returns the number of bytes received
* Comments         : 
*   Reads the data into provided array.
*
*END*********************************************************************/
static uint_32 _dspi_int_rx
   (
      /* [IN] The address of the device specific information */
      IO_SPI_INT_DEVICE_STRUCT_PTR int_io_dev_ptr,

      /* [IN] The array characters are to be read from */
      uchar_ptr                    buffer,
      
      /* [IN] Number of char's to transmit */
      int_32                       size
   )
{
   DSPI_INFO_STRUCT_PTR            io_info_ptr;
   VDSPI_REG_STRUCT_PTR            dspi_ptr;
   uint_32                         num, index;
   uchar_ptr                       rx_ptr;

   io_info_ptr  = int_io_dev_ptr->DEV_INFO_PTR;
   dspi_ptr = io_info_ptr->DSPI_PTR;

   /* Check whether new data in rx buffer */
   rx_ptr = (uchar_ptr)(io_info_ptr->RX_BUFFER);
   for (num = 0; num < size; num++) 
   {
      index = io_info_ptr->RX_OUT;
      if (index == io_info_ptr->RX_IN) break;
      *buffer++ = rx_ptr[index++];
      if (index >= io_info_ptr->INIT.RX_BUFFER_SIZE) index = 0;
      io_info_ptr->RX_OUT = index;
   }
   index = size - num;

   if (0 != index)
   {
      /* Not enough data, assert chip selects and enable further transfer */
      io_info_ptr->RX_REQUEST = index;
      
      if (dspi_ptr->MCR & DSPI_MCR_MSTR_MASK) 
      {
          if (io_info_ptr->CS ^ io_info_ptr->CS_ACTIVE)
          {
             for (index = 0; index < DSPI_CS_COUNT; index++)
             {
                if ((0 != ((io_info_ptr->CS ^ io_info_ptr->CS_ACTIVE) & (1 << index))) && (NULL != io_info_ptr->CS_CALLBACK[index]))
                {
                   io_info_ptr->CS_CALLBACK[index] (DSPI_PUSHR_PCS(1 << index), (io_info_ptr->CS_ACTIVE >> index) & 1, io_info_ptr->CS_USERDATA[index]);
                }
             }
             io_info_ptr->CS_ACTIVE = io_info_ptr->CS;
          }
      }
      io_info_ptr->DMA_FLAGS |= DSPI_DMA_FLAG_RX;
      dspi_ptr->RSER |= DSPI_RSER_TFFF_RE_MASK;
   }
   return num;
}

/*FUNCTION****************************************************************
* 
* Function Name    : _dspi_int_tx
* Returned Value   : return number of byte transmitted
* Comments         : 
*   Writes the provided data into trasmit buffer
*
*END*********************************************************************/
static uint_32 _dspi_int_tx
   (
      /* [IN] The address of the device specific information */
      IO_SPI_INT_DEVICE_STRUCT_PTR int_io_dev_ptr,

      /* [IN] The array to store data */
      uchar_ptr                    buffer,
      
      /* [IN] Number of char's to transmit */
      int_32                       size
   )
{
   DSPI_INFO_STRUCT_PTR            io_info_ptr;
   VDSPI_REG_STRUCT_PTR            dspi_ptr;
   uint_32                         num, index, tmp;
   uchar_ptr                       tx_ptr;
    
   io_info_ptr = int_io_dev_ptr->DEV_INFO_PTR;
   dspi_ptr = io_info_ptr->DSPI_PTR;

   /* Fill new data into transmit buffer */
   tx_ptr = (uchar_ptr)(io_info_ptr->TX_BUFFER);
   index = io_info_ptr->TX_IN;
   for (num = 0; num < size; num++) 
   {
      tmp = index + 1;
      if (tmp >= io_info_ptr->INIT.TX_BUFFER_SIZE) tmp = 0;
      if (tmp == io_info_ptr->TX_OUT) break;
      tx_ptr[index] = *buffer++;
      index = tmp;
   }
   io_info_ptr->TX_IN = index;
   
   if (0 != num) 
   {
      /* At least one byte to transmit, assert chip selects and enable transfer */
      if (dspi_ptr->MCR & DSPI_MCR_MSTR_MASK) 
      {
          if (io_info_ptr->CS ^ io_info_ptr->CS_ACTIVE)
          {
             for (index = 0; index < DSPI_CS_COUNT; index++)
             {
                if ((0 != ((io_info_ptr->CS ^ io_info_ptr->CS_ACTIVE) & (1 << index))) && (NULL != io_info_ptr->CS_CALLBACK[index]))
                {
                   io_info_ptr->CS_CALLBACK[index] (DSPI_PUSHR_PCS(1 << index), (io_info_ptr->CS_ACTIVE >> index) & 1, io_info_ptr->CS_USERDATA[index]);
                }
             }
             io_info_ptr->CS_ACTIVE = io_info_ptr->CS;
          }
      }
      io_info_ptr->DMA_FLAGS |= DSPI_DMA_FLAG_TX;
      dspi_ptr->RSER |= DSPI_RSER_TFFF_RE_MASK;
   }
   return num;
}

/*FUNCTION****************************************************************
* 
* Function Name    : _dspi_int_isr
* Returned Value   : SPI interrupt routine
* Comments         : 
*   Reads/writes bytes transmitted from/to buffers.
*
*END*********************************************************************/
static void _dspi_int_isr
   (
      /* [IN] The address of the device specific information */
      pointer                    parameter
   )
{
   DSPI_INFO_STRUCT_PTR          io_info_ptr;
   VDSPI_REG_STRUCT_PTR          dspi_ptr;
   uint_32                       state;
   uint_16                       index, tmp;
   boolean                       tx_and_rx;
   
   tx_and_rx = (((IO_SPI_INT_DEVICE_STRUCT_PTR)parameter)->FLAGS & SPI_FLAG_FULL_DUPLEX) != 0;
   io_info_ptr = ((IO_SPI_INT_DEVICE_STRUCT_PTR)parameter)->DEV_INFO_PTR;
   dspi_ptr = (VDSPI_REG_STRUCT_PTR)(io_info_ptr->DSPI_PTR);
   
   /* Read interrupt state */
   state = dspi_ptr->SR;
   io_info_ptr->STATS.INTERRUPTS++;

   /* Receive always */
   if (state & DSPI_SR_RFDF_MASK)
   {
      /* Always clear receive int flag */
      tmp = DSPI_POPR_RXDATA_GET(dspi_ptr->POPR);
      dspi_ptr->SR |= DSPI_SR_RFDF_MASK;
      if (io_info_ptr->TX_COUNT > 0) io_info_ptr->TX_COUNT--;

      /* If request for receive and receive allowed (full duplex or first byte skipped already) */
      if ((tx_and_rx) || ((1 == io_info_ptr->RX_COUNT) && (io_info_ptr->RX_REQUEST > 0)))
      {
         /* Actual receive into buffer, if overflow, incoming bytes are thrown away */
         index = io_info_ptr->RX_IN + 1;
         if (index >= io_info_ptr->INIT.RX_BUFFER_SIZE) index = 0;
         if (index != io_info_ptr->RX_OUT) 
         {
            if (DSPI_CTAR_FMSZ_GET(dspi_ptr->CTAR[0]) > 7)
            {
               ((uchar_ptr)(io_info_ptr->RX_BUFFER))[io_info_ptr->RX_IN] = (uint_8)(tmp >> 8);
               io_info_ptr->RX_IN = index++;
               if (index >= io_info_ptr->INIT.RX_BUFFER_SIZE) index = 0;
            }
            ((uchar_ptr)(io_info_ptr->RX_BUFFER))[io_info_ptr->RX_IN] = (uint_8)tmp;
            io_info_ptr->RX_IN = index;
            io_info_ptr->STATS.RX_PACKETS++;
         } else {
            io_info_ptr->STATS.RX_OVERFLOWS++;
         }
         if (io_info_ptr->RX_REQUEST)
         {
            if (--io_info_ptr->RX_REQUEST == 0) 
            {
               io_info_ptr->RX_COUNT = 0;
            }
         }
      } 

      /* Skipping bytes sent before receive */
      if (io_info_ptr->RX_COUNT > 1) io_info_ptr->RX_COUNT--;
   }

   /* Transmit is disabled when idle */
   if ((dspi_ptr->RSER & DSPI_RSER_TFFF_RE_MASK) && (state & DSPI_SR_TFFF_MASK))
   {
      /* Transmit flag is fuzzy, so decide according to TX fifo fill counter */ 
      dspi_ptr->SR |= DSPI_SR_TFFF_MASK;
      if (0 == DSPI_SR_TXCTR_GET(dspi_ptr->SR))
      {
         index = io_info_ptr->RX_IN + 1;
         if (index >= io_info_ptr->INIT.RX_BUFFER_SIZE) index = 0;
         if ((tx_and_rx) || (io_info_ptr->RX_COUNT == 0)) 
         {
            if (io_info_ptr->TX_OUT != io_info_ptr->TX_IN)
            {
               // Actual transmit
               tmp = ((uchar_ptr)(io_info_ptr->TX_BUFFER))[io_info_ptr->TX_OUT++];
               if (io_info_ptr->TX_OUT >= io_info_ptr->INIT.TX_BUFFER_SIZE) io_info_ptr->TX_OUT = 0;
               if (DSPI_CTAR_FMSZ_GET(dspi_ptr->CTAR[0]) > 7)
               {
                 tmp = (tmp << 8) | ((uchar_ptr)(io_info_ptr->TX_BUFFER))[io_info_ptr->TX_OUT++];
                  if (io_info_ptr->TX_OUT >= io_info_ptr->INIT.TX_BUFFER_SIZE) io_info_ptr->TX_OUT = 0;
               }
               dspi_ptr->PUSHR = DSPI_PUSHR_CONT_MASK | DSPI_PUSHR_PCS(io_info_ptr->CS) | DSPI_PUSHR_CTAS(0) | DSPI_PUSHR_TXDATA(tmp);
               io_info_ptr->STATS.TX_PACKETS++;
               io_info_ptr->TX_COUNT++;            
            } 
            else 
            {
               if ((index != io_info_ptr->RX_OUT) && (io_info_ptr->RX_REQUEST > 0))
               {
                  // Dummy transmit, setup receive
                  dspi_ptr->PUSHR = DSPI_PUSHR_CONT_MASK | DSPI_PUSHR_PCS(io_info_ptr->CS) | DSPI_PUSHR_CTAS(0) | DSPI_PUSHR_TXDATA(0xFF);
                  io_info_ptr->TX_COUNT++;
                  io_info_ptr->RX_COUNT = io_info_ptr->TX_COUNT;
               } 
               else
               {
                  // No reason for transmit, block transmit interrupt
                  dspi_ptr->RSER &= (~ DSPI_RSER_TFFF_RE_MASK);
                  io_info_ptr->DMA_FLAGS = 0;
               }
            }
         } 
         else 
         {
            if ((index != io_info_ptr->RX_OUT) && (io_info_ptr->RX_REQUEST > io_info_ptr->TX_COUNT - (io_info_ptr->RX_COUNT - 1)))
            {
               // Dummy transmit for receiving
               dspi_ptr->PUSHR = DSPI_PUSHR_CONT_MASK | DSPI_PUSHR_PCS(io_info_ptr->CS) | DSPI_PUSHR_CTAS(0) | DSPI_PUSHR_TXDATA(0xFF);
               io_info_ptr->TX_COUNT++;
            } 
            else
            {
               // No reason for transmit, block transmit interrupt
               dspi_ptr->RSER &= (~ DSPI_RSER_TFFF_RE_MASK);
               io_info_ptr->DMA_FLAGS = 0;
            }
         }
      }
   }
   
   if (state & DSPI_SR_TFUF_MASK)
   {
      dspi_ptr->SR |= DSPI_SR_TFUF_MASK;
      io_info_ptr->STATS.TX_UNDERFLOWS++;
   }
   if (state & DSPI_SR_RFOF_MASK)
   {
      dspi_ptr->SR |= DSPI_SR_RFOF_MASK;
      io_info_ptr->STATS.RX_OVERFLOWS++;
   }
}
/* EOF */
