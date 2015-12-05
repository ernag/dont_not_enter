/**HEADER********************************************************************
* 
* Copyright (c) 2008 Freescale Semiconductor;
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
* $FileName: bma280_polled.c$
* $Version : 3.7.10.0$
* $Date    : Feb-7-2011$
*
* Comments:
*
*   This file contains the functions which write and read the SPI memories
*   using the SPI driver in polled mode.
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>
#include "bma280.h"

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_chip_erase
* Comments  : This function erases the whole memory SPI chip
*
*END*----------------------------------------------------------------------*/
void memory_chip_erase (MQX_FILE_PTR spifd)
{
   uint_32 result;

   /* This operation must be write-enabled */
   memory_set_write_latch (spifd, TRUE);

   memory_read_status (spifd);

   printf ("Erase whole memory chip:\n");
   send_buffer[0] = SPI_MEMORY_CHIP_ERASE;
     
   /* Write instruction */
   result = fwrite (send_buffer, 1, 1, spifd);

   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   while (memory_read_status (spifd) & 1)
   {
      _time_delay (1000);
   }

   printf ("Erase chip ... ");
   if (result != 1) 
   {
      printf ("ERROR\n");
   }
   else
   {
      printf ("OK\n");
   }
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_set_write_latch
* Comments  : This function sets latch to enable/disable memory write 
*             operation
*
*END*----------------------------------------------------------------------*/
void memory_set_write_latch (MQX_FILE_PTR spifd, boolean enable)
{
   uint_32 result;
   
   if (enable)
   {
      printf ("Enable write latch in memory ... ");
      send_buffer[0] = SPI_MEMORY_WRITE_LATCH_ENABLE;
   } else {
      printf ("Disable write latch in memory ... ");
      send_buffer[0] = SPI_MEMORY_WRITE_LATCH_DISABLE;
   }
     
   /* Write instruction */
   result = fwrite (send_buffer, 1, 1, spifd);

   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   if (result != 1) 
   {
      printf ("ERROR\n");
   }
   else
   {
      printf ("OK\n");
   }
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_set_protection
* Comments  : This function sets write protection in memory status register
*
*END*----------------------------------------------------------------------*/
void memory_set_protection (MQX_FILE_PTR spifd, boolean protect)
{
   uint_32 result, i;
   uint_8 protection;
   
   /* Must do it twice to ensure right transitions in protection status register */
   for (i = 0; i < 2; i++)
   {
      /* Each write operation must be enabled in memory */
      memory_set_write_latch (spifd, TRUE);
      
      memory_read_status (spifd);
   
      if (protect)
      {
         printf ("Write protect memory ... ");
         protection = 0xFF;
      } else {
         printf ("Write unprotect memory ... ");
         protection = 0x00;
      }
     
      send_buffer[0] = SPI_MEMORY_WRITE_STATUS;
      send_buffer[1] = protection;
   
      /* Write instruction */
      result = fwrite (send_buffer, 1, 2, spifd);
      
      /* Wait till transfer end (and deactivate CS) */
      fflush (spifd);
   
      if (result != 2) 
      {
         printf ("ERROR\n");
      }
      else
      {
         printf ("OK\n");
      }
   }
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_read_status
* Comments  : This function reads memory status register 
* Return: 
*         Status read.             
*
*END*----------------------------------------------------------------------*/
uint_8 memory_read_status (MQX_FILE_PTR spifd)
{
   uint_32 result;
   uint_8 state = 0xFF;

   printf ("Read memory status ... ");

   send_buffer[0] = SPI_MEMORY_READ_STATUS;

   /* Write instruction */
   result = fwrite (send_buffer, 1, 1, spifd);

   if (result != 1)
   {
     /* Stop transfer */
      printf ("ERROR (1)\n");
      return state;
   }
  
   /* Read memory status */
   result = fread (&state, 1, 1, spifd);

   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);
   
   if (result != 1) 
   {
      printf ("ERROR (2)\n");
   }
   else
   {
      printf ("0x%02x\n", state);
   }
   
   return state;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_write_byte
* Comments  : This function writes a data byte to memory  
*     
*
*END*----------------------------------------------------------------------*/
void memory_write_byte (MQX_FILE_PTR spifd, uint_32 addr, uchar data)
{
   uint_32 result;

   /* Each write operation must be enabled in memory */
   memory_set_write_latch (spifd, TRUE);

   memory_read_status (spifd);

   send_buffer[0] = SPI_MEMORY_WRITE_DATA;                                               // Write instruction
   for (result = SPI_MEMORY_ADDRESS_BYTES; result != 0; result--)
   {
      send_buffer[result] = (addr >> ((SPI_MEMORY_ADDRESS_BYTES - result) << 3)) & 0xFF; // Address
   }
   send_buffer[1 + SPI_MEMORY_ADDRESS_BYTES] = data;                                     // Data
   
   printf ("Write byte to location 0x%08x in memory ... ", addr);
   
   /* Write instruction, address and byte */
   result = fwrite (send_buffer, 1, 1 + SPI_MEMORY_ADDRESS_BYTES + 1, spifd);

   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   if (result != 1 + SPI_MEMORY_ADDRESS_BYTES + 1) 
   {
      printf ("ERROR\n");
   }
   else 
   {
      printf ("0x%02x\n", data);
   }
   
   /* There is 5 ms internal write cycle needed for memory */
   _time_delay (5);
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_read_byte
* Comments  : This function reads a data byte from memory
* Return: 
*         Byte read.             
*
*END*----------------------------------------------------------------------*/
uint_8 memory_read_byte (MQX_FILE_PTR spifd, uint_32 addr)
{
   uint_32 result;
   uint_8 data = 0;    
   
   send_buffer[0] = SPI_MEMORY_READ_DATA;                                                // Read instruction
   for (result = SPI_MEMORY_ADDRESS_BYTES; result != 0; result--)
   {
      send_buffer[result] = (addr >> ((SPI_MEMORY_ADDRESS_BYTES - result) << 3)) & 0xFF; // Address
   }

   printf ("Read byte from location 0x%08x in memory ... ", addr);

   /* Write instruction and address */
   result = fwrite (send_buffer, 1, 1 + SPI_MEMORY_ADDRESS_BYTES, spifd);

   if (result != 1 + SPI_MEMORY_ADDRESS_BYTES)
   {
      /* Stop transfer */
      printf ("ERROR (1)\n");
      return data;
   }
   
   /* Read data from memory */
   result = fread (&data, 1, 1, spifd);

   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   if (result != 1) 
   {
      printf ("ERROR (2)\n");
   }
   else
   {
      printf ("0x%02x\n", data);
   }
   
   return data;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_write_data
* Comments  : This function writes data buffer to memory using page write
* Return: 
*         Number of bytes written.             
*
*END*----------------------------------------------------------------------*/
uint_32 memory_write_data (MQX_FILE_PTR spifd, uint_32 addr, uint_32 size, uchar_ptr data)
{
   uint_32 i, len;
   uint_32 result = size;

   while (result > 0) 
   {
      /* Each write operation must be enabled in memory */
      memory_set_write_latch (spifd, TRUE);

      memory_read_status (spifd);
    
      len = result;
      if (len > SPI_MEMORY_PAGE_SIZE - (addr & (SPI_MEMORY_PAGE_SIZE - 1))) len = SPI_MEMORY_PAGE_SIZE - (addr & (SPI_MEMORY_PAGE_SIZE - 1));
      result -= len;

      printf ("Page write %d bytes to location 0x%08x in memory:\n", len, addr);

      send_buffer[0] = SPI_MEMORY_WRITE_DATA;                                     // Write instruction
      for (i = SPI_MEMORY_ADDRESS_BYTES; i != 0; i--)
      {
         send_buffer[i] = (addr >> ((SPI_MEMORY_ADDRESS_BYTES - i) << 3)) & 0xFF; // Address
      }
      for (i = 0; i < len; i++)
      {
         send_buffer[1 + SPI_MEMORY_ADDRESS_BYTES + i] = data[i];                 // Data
      }

      /* Write instruction, address and data */
      i = fwrite (send_buffer, 1, 1 + SPI_MEMORY_ADDRESS_BYTES + len, spifd);

      /* Wait till transfer end (and deactivate CS) */
      fflush (spifd);

      if (i != 1 + SPI_MEMORY_ADDRESS_BYTES + len) 
      {
         printf ("ERROR\n"); 
         return size - result;
      }
      else
      {
         for (i = 0; i < len; i++)
            printf ("%c", data[i]);
         printf ("\n"); 
      }
     
      /* Move to next block */
      addr += len;
      data += len;

      /* There is 5 ms internal write cycle needed for memory */
      _time_delay (5);
   }
   
   return size;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_read_data
* Comments  : This function reads data from memory into given buffer
* Return: 
*         Number of bytes read.             
*
*END*----------------------------------------------------------------------*/
uint_32 memory_read_data (MQX_FILE_PTR spifd, uint_32 addr, uint_32 size, uchar_ptr data)
{
   uint_32 result, i, param;
   
   printf ("Reading %d bytes from location 0x%08x in memory: ", size, addr);

   send_buffer[0] = SPI_MEMORY_READ_DATA;                                                // Read instruction
   for (result = SPI_MEMORY_ADDRESS_BYTES; result != 0; result--)
   {
      send_buffer[result] = (addr >> ((SPI_MEMORY_ADDRESS_BYTES - result) << 3)) & 0xFF; // Address
   }

   /* Write instruction and address */
   result = fwrite (send_buffer, 1, 1 + SPI_MEMORY_ADDRESS_BYTES, spifd);

   if (result != 1 + SPI_MEMORY_ADDRESS_BYTES)
   {
      /* Stop transfer */
      printf ("ERROR (1)\n");
      return 0;
   }

   /* Get actual flags */
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_GET_FLAGS, &param)) 
   {
      printf ("ERROR (2)\n");
      return 0;
   }

   /* Flush but do not de-assert CS */
   param |= SPI_FLAG_NO_DEASSERT_ON_FLUSH;
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_SET_FLAGS, &param)) 
   {
      printf ("ERROR (3)\n");
      return 0;
   }
   fflush (spifd);

   /* Read size bytes of data */
   result = fread (data, 1, size, spifd);

   /* Wait till transfer end and de-assert CS */
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_FLUSH_DEASSERT_CS, &param)) 
   {
      printf ("ERROR (4)\n");
      return 0;
   }
   
   /* Get actual flags */
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_GET_FLAGS, &param)) 
   {
      printf ("ERROR (5)\n");
      return 0;
   }

   /* Revert opening flags */
   param &= (~ SPI_FLAG_NO_DEASSERT_ON_FLUSH);
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_SET_FLAGS, &param)) 
   {
      printf ("ERROR (6)\n");
      return 0;
   }

   if (result != size) 
   {
      printf ("ERROR (7)\n"); 
   } 
   else
   {
      printf ("\n"); 
      for (i = 0; i < result; i++)
         printf ("%c", data[i]);
      printf ("\n"); 
   }
   
   return result;
}

/* EOF */

