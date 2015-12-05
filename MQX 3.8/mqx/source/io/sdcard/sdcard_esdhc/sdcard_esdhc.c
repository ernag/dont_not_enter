/**HEADER********************************************************************
* 
* Copyright (c) 2010 Freescale Semiconductor;
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
* $FileName: sdcard_esdhc.c$
* $Version : 3.8.8.0$
* $Date    : Sep-1-2011$
*
* Comments:
*
*   This file contains the SD card driver functions.
*
*END************************************************************************/


#include <mqx.h>
#include <bsp.h>
#include <io_prv.h>
#include <sdcard.h>
#include <sdcard_prv.h>
#include <sdcard_esdhc.h>
#include <esdhc.h>


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_esdhc_init
* Returned Value   : TRUE if successful, FALSE otherwise
* Comments         :
*    Initializes ESDHC communication, SD card itself and reads its parameters.
*
*END*----------------------------------------------------------------------*/

boolean _io_sdcard_esdhc_init 
(
    /* [IN/OUT] SD card file descriptor */
    MQX_FILE_PTR fd_ptr
)
{
    uint_32              baudrate, param, c_size, c_size_mult, read_bl_len;
    ESDHC_COMMAND_STRUCT command;
    IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
    SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;
    uint_8               buffer[64];
    
    /* Check parameters */
    if ((NULL == sdcard_ptr) || (NULL == sdcard_ptr->COM_DEVICE) || (NULL == sdcard_ptr->INIT))
    {
        return FALSE;
    }

    sdcard_ptr->TIMEOUT = 0;
    sdcard_ptr->NUM_BLOCKS = 0;
    sdcard_ptr->ADDRESS = 0;
    sdcard_ptr->SDHC = FALSE;
    sdcard_ptr->VERSION2 = FALSE;
    
    /* Initialize and detect card */
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_INIT, NULL))
    {
        return FALSE;
    }

    /* Set low baudrate for card setup */
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_GET_BAUDRATE, &baudrate))
    {
        return FALSE;
    }
    param = 400000;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SET_BAUDRATE, &param))
    {
        return FALSE;
    }
    
    /* SDHC check */
    param = 0;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_GET_CARD, &param))
    {
        return FALSE;
    }
    if ((ESDHC_CARD_SD == param) || (ESDHC_CARD_SDHC == param) || (ESDHC_CARD_SDCOMBO == param) || (ESDHC_CARD_SDHCCOMBO == param))
    {
        if ((ESDHC_CARD_SDHC == param) || (ESDHC_CARD_SDHCCOMBO == param))
        {
            sdcard_ptr->SDHC = TRUE;
        }
    }
    else
    {
        return FALSE;
    }

    /* Card identify */
    command.COMMAND = ESDHC_CMD2;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = 0;
    command.READ = FALSE;
    command.BLOCKS = 0;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }

    /* Get card address */
    command.COMMAND = ESDHC_CMD3;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = 0;
    command.READ = FALSE;
    command.BLOCKS = 0;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }
    sdcard_ptr->ADDRESS = command.RESPONSE[0] & 0xFFFF0000;
    
    /* Get card parameters */
    command.COMMAND = ESDHC_CMD9;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = sdcard_ptr->ADDRESS;
    command.READ = FALSE;
    command.BLOCKS = 0;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }
    if (0 == (command.RESPONSE[3] & 0x00C00000))
    {
        read_bl_len = (command.RESPONSE[2] >> 8) & 0x0F;
        c_size = command.RESPONSE[2] & 0x03;
        c_size = (c_size << 10) | (command.RESPONSE[1] >> 22);
        c_size_mult = (command.RESPONSE[1] >> 7) & 0x07;
        sdcard_ptr->NUM_BLOCKS = (c_size + 1) * (1 << (c_size_mult + 2)) * (1 << (read_bl_len - 9));
    }
    else
    {
        sdcard_ptr->VERSION2 = TRUE;
        c_size = (command.RESPONSE[1] >> 8) & 0x003FFFFF;
        sdcard_ptr->NUM_BLOCKS = (c_size + 1) << 10;
    }

    /* Select card */
    command.COMMAND = ESDHC_CMD7;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = sdcard_ptr->ADDRESS;
    command.READ = FALSE;
    command.BLOCKS = 0;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }
    
    /* Set block size to 64 */
    param = 64;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SET_BLOCK_SIZE, &param))
    {
        return FALSE;
    }
    command.COMMAND = ESDHC_CMD16;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = param;
    command.READ = FALSE;
    command.BLOCKS = 0;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }

    /* Application specific command */
    command.COMMAND = ESDHC_CMD55;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = sdcard_ptr->ADDRESS;
    command.READ = FALSE;
    command.BLOCKS = 0;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }

    /* Get SD status */
    command.COMMAND = ESDHC_ACMD13;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = 0;
    command.READ = TRUE;
    command.BLOCKS = 1;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }

    /* Read SD status */
    if (param != fread (buffer, 1, param, sdcard_ptr->COM_DEVICE))
    {
        return FALSE;
    }
    
    /* Wait for transfer complete */
    if (ESDHC_OK != fflush (sdcard_ptr->COM_DEVICE))
    {
        return FALSE;
    }

    /* Calculate and set baudrate */
    param = baudrate;
    baudrate = buffer[8];
    if (0 == baudrate)
    {
    	baudrate = 1;
    }
    baudrate <<= 24;
    switch (sdcard_ptr->INIT->SIGNALS)
    {
        case ESDHC_BUS_WIDTH_8BIT: 
            baudrate >>= 2;
        case ESDHC_BUS_WIDTH_4BIT: 
            baudrate >>= 2;
        default: 
            break;
    }
    if (baudrate > param)
    {
        baudrate = param;
    }
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SET_BAUDRATE, &baudrate))
    {
        return FALSE;
    }

    /* Set block size to 512 */
    param = IO_SDCARD_BLOCK_SIZE;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SET_BLOCK_SIZE, &param))
    {
        return FALSE;
    }
    command.COMMAND = ESDHC_CMD16;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = param;
    command.READ = FALSE;
    command.BLOCKS = 0;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }

    if (ESDHC_BUS_WIDTH_4BIT == sdcard_ptr->INIT->SIGNALS)
    {
        /* Application specific command */
        command.COMMAND = ESDHC_CMD55;
        command.TYPE = ESDHC_TYPE_NORMAL;
        command.ARGUMENT = sdcard_ptr->ADDRESS;
        command.READ = FALSE;
        command.BLOCKS = 0;
        if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
        {
            return FALSE;
        }

        /* Set bus width == 4 */
        command.COMMAND = ESDHC_ACMD6;
        command.TYPE = ESDHC_TYPE_NORMAL;
        command.ARGUMENT = 2;
        command.READ = FALSE;
        command.BLOCKS = 0;
        if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
        {
            return FALSE;
        }

        param = ESDHC_BUS_WIDTH_4BIT;
        if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SET_BUS_WIDTH, &param))
        {
            return FALSE;
        }
    }

    return TRUE;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_esdhc_read_block
* Returned Value   : TRUE if successful, FALSE otherwise
* Comments         :
*    Reads sector (512 byte) from SD card with given index into given buffer.
*
*END*----------------------------------------------------------------------*/

boolean _io_sdcard_esdhc_read_block 
(
    /* [IN] SD card info */
    MQX_FILE_PTR fd_ptr, 

    /* [OUT] Buffer to fill with read 512 bytes */
    uchar_ptr buffer, 

    /* [IN] Index of sector to read */
    uint_32   index
)
{
    ESDHC_COMMAND_STRUCT command;
    IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
    SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;
    
    /* Check parameters */
    if ((NULL == sdcard_ptr) || (NULL == sdcard_ptr->COM_DEVICE) || (NULL == sdcard_ptr->INIT) || (NULL == buffer))
    {
        return FALSE;
    }

    /* SD card data address adjustment */
    if (! sdcard_ptr->SDHC)
    {
        index <<= IO_SDCARD_BLOCK_SIZE_POWER;
    }

    /* Read block command */
    command.COMMAND = ESDHC_CMD17;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = index;
    command.READ = TRUE;
    command.BLOCKS = 1;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }

    /* Read data */
    if (IO_SDCARD_BLOCK_SIZE != fread (buffer, 1, IO_SDCARD_BLOCK_SIZE, sdcard_ptr->COM_DEVICE))
    {
        return FALSE;
    }

    /* Wait for transfer complete */
    if (ESDHC_OK != fflush (sdcard_ptr->COM_DEVICE))
    {
        return FALSE;
    }

    return TRUE;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_esdhc_write_block
* Returned Value   : TRUE if successful, FALSE otherwise
* Comments         :
*    Writes sector (512 byte) with given index to SD card from given buffer.
*
*END*----------------------------------------------------------------------*/

boolean _io_sdcard_esdhc_write_block 
(
    /* [IN] SD card file descriptor */
    MQX_FILE_PTR fd_ptr, 

    /* [IN] Buffer with 512 bytes to write */
    uchar_ptr buffer, 

    /* [IN] Index of sector to write */
    uint_32   index
)
{
    ESDHC_COMMAND_STRUCT command;
    IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
    SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;

    /* Check parameters */
    if ((NULL == sdcard_ptr) || (NULL == sdcard_ptr->COM_DEVICE) || (NULL == sdcard_ptr->INIT) || (NULL == buffer))
    {
        return FALSE;
    }

    /* SD card data address adjustment */
    if (! sdcard_ptr->SDHC)
    {
        index <<= IO_SDCARD_BLOCK_SIZE_POWER;
    }

    /* Write block command */
    command.COMMAND = ESDHC_CMD24;
    command.TYPE = ESDHC_TYPE_NORMAL;
    command.ARGUMENT = index;
    command.READ = FALSE;
    command.BLOCKS = 1;
    if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
    {
        return FALSE;
    }
    
    /* Write data */
    if (IO_SDCARD_BLOCK_SIZE != fwrite (buffer, 1, IO_SDCARD_BLOCK_SIZE, sdcard_ptr->COM_DEVICE))
    {
        return FALSE;
    }

    /* Wait for transfer complete */
    if (ESDHC_OK != fflush (sdcard_ptr->COM_DEVICE))
    {
        return FALSE;
    }

    /* Wait for card ready / transaction state */
    do
    {
        command.COMMAND = ESDHC_CMD13;
        command.TYPE = ESDHC_TYPE_NORMAL;
        command.ARGUMENT = sdcard_ptr->ADDRESS;
        command.READ = FALSE;
        command.BLOCKS = 0;
        if (ESDHC_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_ESDHC_SEND_COMMAND, &command))
        {
            return FALSE;
        }

        /* Card status error check */
        if (command.RESPONSE[0] & 0xFFD98008)
        {
            return FALSE;
        }
    } while (0x000000900 != (command.RESPONSE[0] & 0x00001F00));


    return TRUE;
}

/* EOF */
