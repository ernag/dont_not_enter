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
* $FileName: sdcard.c$
* $Version : 3.7.6.0$
* $Date    : Feb-7-2011$
*
* Comments:
*
*   This file contains the sd card driver functions.
*
*END************************************************************************/

#include <mqx.h>
#include <fio.h>
#include <io_prv.h>
#include <sdcard_prv.h>
#include <sdcard.h>

/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_install
* Returned Value   : MQX error code
* Comments         :
*    Install a SDCARD driver.
*
*END*----------------------------------------------------------------------*/

_mqx_int _io_sdcard_install
   (
      /* [IN] A string that identifies the device for fopen */
      char_ptr               identifier,

      /* [IN] SD card initialization parameters */
      SDCARD_INIT_STRUCT_PTR init,

      /* [IN] Already opened communication descriptor */
      MQX_FILE_PTR           com_device
   )
{
   SDCARD_STRUCT_PTR dev_ptr;
   
   if ((NULL == init) || (NULL == com_device))
   {
      return MQX_INVALID_PARAMETER;
   }

   dev_ptr = (SDCARD_STRUCT_PTR)
      _mem_alloc_system_zero((_mem_size)sizeof(SDCARD_STRUCT));
#if MQX_CHECK_MEMORY_ALLOCATION_ERRORS
   if (dev_ptr == NULL) 
   {
      return MQX_OUT_OF_MEMORY;
   }
#endif

   _mem_set_type(dev_ptr,MEM_TYPE_IO_SDCARD_STRUCT);

   dev_ptr->INIT = init;

   dev_ptr->COM_DEVICE = com_device;
   
   _lwsem_create(&dev_ptr->LWSEM, 1);

   return (_io_dev_install_ext(identifier,
      _io_sdcard_open,
      _io_sdcard_close,
      _io_sdcard_read_blocks,
      _io_sdcard_write_blocks,
      _io_sdcard_ioctl,
      _io_sdcard_uninstall,
      (pointer)dev_ptr)); 
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_uninstall
* Returned Value   : MQX error code
* Comments         :
*    Install a SDCARD driver.
*
*END*----------------------------------------------------------------------*/

_mqx_int _io_sdcard_uninstall
   (
      /* [IN] The devices context */
      IO_DEVICE_STRUCT_PTR io_dev_ptr
   )
{
   SDCARD_STRUCT_PTR dev_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr;

   _lwsem_destroy(&dev_ptr->LWSEM);

   _mem_free(dev_ptr);

   return MQX_OK;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_open
* Returned Value   : MQX error code
* Comments         : Opens and initializes SDCARD driver.
* 
*END*----------------------------------------------------------------------*/

_mqx_int _io_sdcard_open
   (
      /* [IN] the file handle for the device being opened */
      MQX_FILE_PTR fd_ptr,
       
      /* [IN] the remaining portion of the name of the device */
      char_ptr open_name_ptr,

      /* [IN] the flags to be used during operation */
      char_ptr flags
   )
{
   IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
   SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;
   _mqx_int             result = MQX_OK;

   if (NULL != sdcard_ptr->INIT->INIT_FUNC)
   {
      _lwsem_wait(&sdcard_ptr->LWSEM);
      if (! sdcard_ptr->INIT->INIT_FUNC (fd_ptr))
      {
         result = MQX_IO_OPERATION_NOT_AVAILABLE;
      }
      _lwsem_post(&sdcard_ptr->LWSEM);      
   }
      
   return result;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_close
* Returned Value   : MQX error code
* Comments         : Closes SDCARD driver
* 
*END*----------------------------------------------------------------------*/

_mqx_int _io_sdcard_close
   (
      /* [IN] the file handle for the device being closed */
      MQX_FILE_PTR fd_ptr
   )
{
   return MQX_OK;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_ioctl
* Returned Value   : MQX error code
* Comments         :
*    Returns result of ioctl operation.
*
*END*----------------------------------------------------------------------*/

_mqx_int _io_sdcard_ioctl
   (
      /* [IN] the file handle for the device */
      MQX_FILE_PTR fd_ptr,

      /* [IN] the ioctl command */
      _mqx_uint cmd,

      /* [IN] the ioctl parameters */
      pointer   param_ptr
   )
{
   IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
   SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;
   _mem_size_ptr        mparam_ptr;
   _mqx_uint_ptr        uparam_ptr;
   uint_32_ptr          u32param_ptr;
   _mqx_int             result = MQX_OK;

   switch (cmd) 
   {
      case IO_IOCTL_SET_BLOCK_MODE:
         break;
      case IO_IOCTL_GET_BLOCK_SIZE:
         uparam_ptr  = (_mqx_uint_ptr)param_ptr;
         *uparam_ptr = IO_SDCARD_BLOCK_SIZE;
         break;
      case IO_IOCTL_GET_NUM_SECTORS:
         mparam_ptr = (_mem_size_ptr)param_ptr;
         *mparam_ptr = (_mem_size)sdcard_ptr->NUM_BLOCKS;
         break;
      case IO_IOCTL_DEVICE_IDENTIFY:
         u32param_ptr  = (uint_32_ptr)param_ptr;
         u32param_ptr[IO_IOCTL_ID_PHY_ELEMENT]  = IO_DEV_TYPE_PHYS_SDCARD;
         u32param_ptr[IO_IOCTL_ID_LOG_ELEMENT]  = IO_DEV_TYPE_LOGICAL_MFS;
         u32param_ptr[IO_IOCTL_ID_ATTR_ELEMENT] = IO_SDCARD_ATTRIBS;
         if (fd_ptr->FLAGS & IO_O_RDONLY)
         {
             u32param_ptr[IO_IOCTL_ID_ATTR_ELEMENT] &= (~ IO_DEV_ATTR_WRITE);
         }
         break;
      default:
         result = IO_ERROR_INVALID_IOCTL_CMD;
         break;
   }
   
   return result;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_read_blocks
* Returned Value   : number of blocks read
* Comments         : Reads data from SDCARD driver
* 
*END*----------------------------------------------------------------------*/

_mqx_int _io_sdcard_read_blocks
   (
      /* [IN] the file handle for the device */
      MQX_FILE_PTR fd_ptr,

      /* [IN] where the blocks are to be stored */
      char_ptr data_ptr,

      /* [IN] the number of blocks to input */
      _mqx_int num
   )
{
   IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
   SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;
   _mqx_int             i;

   fd_ptr->ERROR = IO_OK;
   if (NULL == sdcard_ptr->INIT->READ_FUNC)
   {
      fd_ptr->ERROR = IO_ERROR_READ;
      return IO_ERROR;
   }
   
   if ((fd_ptr->LOCATION + num) > sdcard_ptr->NUM_BLOCKS) 
   {
      fd_ptr->FLAGS |= IO_FLAG_AT_EOF;
      num = sdcard_ptr->NUM_BLOCKS - fd_ptr->LOCATION;
   }

   for (i = 0; i < num; i++)
   {
      _lwsem_wait(&sdcard_ptr->LWSEM);
      if (sdcard_ptr->INIT->READ_FUNC (fd_ptr, (uchar_ptr)data_ptr, fd_ptr->LOCATION))
      {
         data_ptr += IO_SDCARD_BLOCK_SIZE;
         fd_ptr->LOCATION++;
      }
      else
      {
         fd_ptr->ERROR = IO_ERROR_READ;
         num = IO_ERROR;
      }
      _lwsem_post(&sdcard_ptr->LWSEM);
   }

   return num;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_write_blocks
* Returned Value   : number of blocks written
* Comments         : Writes data to SDCARD driver
* 
*END*----------------------------------------------------------------------*/

_mqx_int _io_sdcard_write_blocks
   (
      /* [IN] the file handle for the device */
      MQX_FILE_PTR fd_ptr,

      /* [IN] where the blocks are stored */
      char_ptr data_ptr,

      /* [IN] the number of blocks to output */
      _mqx_int num
   )
{
   IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
   SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;
   _mqx_int             i;

   fd_ptr->ERROR = IO_OK;
   if ((NULL == sdcard_ptr->INIT->WRITE_FUNC) || (fd_ptr->FLAGS & IO_O_RDONLY))
   {
      fd_ptr->ERROR = IO_ERROR_WRITE_PROTECTED;
      return IO_ERROR;
   }

   if ((fd_ptr->LOCATION + num) > sdcard_ptr->NUM_BLOCKS) 
   {
      fd_ptr->FLAGS |= IO_FLAG_AT_EOF;
      num = sdcard_ptr->NUM_BLOCKS - fd_ptr->LOCATION;
   }

   for (i = 0; i < num; i++)
   {
      _lwsem_wait(&sdcard_ptr->LWSEM);
      if (sdcard_ptr->INIT->WRITE_FUNC (fd_ptr, (uchar_ptr)data_ptr, fd_ptr->LOCATION))
      {
         data_ptr += IO_SDCARD_BLOCK_SIZE;
         fd_ptr->LOCATION++;
      }
      else
      {
         fd_ptr->ERROR = IO_ERROR_WRITE;
         num = IO_ERROR;
      }
      _lwsem_post(&sdcard_ptr->LWSEM);
   }

   return num;
}

/* EOF */
