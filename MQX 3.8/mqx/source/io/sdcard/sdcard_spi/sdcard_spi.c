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
* $FileName: sdcard_spi.c$
* $Version : 3.8.14.0$
* $Date    : Sep-20-2011$
*
* Comments:
*
*   This file contains the SD card driver functions.
*
*END************************************************************************/


#include <string.h>
#include <mqx.h>
#include <bsp.h>
#include <io_prv.h>
#include <sdcard.h>
#include <sdcard_prv.h>
#include <sdcard_spi.h>
#include <spi.h>


enum 
{
    CMD0 = 0,
    CMD8,
    CMD58,
    CMD55,
    ACMD41,
    CMD9,
    CMD16,
    CMD59,
    CMD13,
    ACMD13
};


static const SDCARD_SPI_COMMAND SDCARD_SPI_COMMANDS[] =
{
    {SDCARD_SPI_CMD0_GO_IDLE_STATE,  0x00, 0x00, 0x00, 0x00, 0x95},
    {SDCARD_SPI_CMD8_SEND_IF_COND,   0x00, 0x00, 0x01, 0xAA, 0x87},
    {SDCARD_SPI_CMD58_READ_OCR,      0x00, 0x00, 0x00, 0x00, 0xFD},
    {SDCARD_SPI_CMD55_APP_CMD,       0x00, 0x00, 0x00, 0x00, 0x65},
    {SDCARD_SPI_ACMD41_SEND_OP_COND, 0x40, 0x00, 0x00, 0x00, 0x77},
    {SDCARD_SPI_CMD9_SEND_CSD,       0x00, 0x00, 0x00, 0x00, 0xAF},
    {SDCARD_SPI_CMD16_SET_BLOCK512,  0x00, 0x00, 0x02, 0x00, 0x15},
    {SDCARD_SPI_CMD59_CRC_ON,        0x00, 0x00, 0x00, 0x01, 0x83},
    {SDCARD_SPI_CMD13_SEND_STATUS,   0x00, 0x00, 0x00, 0x00, 0x0D},
    {SDCARD_SPI_ACMD13_SEND_STATUS,  0x00, 0x00, 0x00, 0x00, 0x0D},
};


static const uint_32 SDCARD_SPI_CRC7_POLY  = 0x89;
static const uint_32 SDCARD_SPI_CRC16_POLY = 0x11021;


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_crc
* Returned Value   : CRC value
* Comments         :
*    General function for any CRC code calculation.
*
*END*----------------------------------------------------------------------*/

static uint_32 _io_sdcard_crc 
(
    /* [IN] CRC polynomial, lowest bit is lowest degree constant */
    uint_32 poly, 

    /* [IN] Degree of the CRC polynomial */
    uint_32 degree, 

    /* [IN] Initial value, maybe previous partial result */
    uint_32 init, 

    /* [IN] Input bit stream (big endian) for CRC calculation */
    const uint_8 input[], 

    /* [IN] Length of the bit stream in bytes */
    uint_32 len
)
{
    static const uint_32 ZEROSONES[2] = {0x00000000, 0xFFFFFFFF};
    uint_32              i, mask;

    mask = (1 << degree) - 1;
    degree--;
    for (; 0 != len; len--, input++)
    {
        for (i = 7; i != (uint_32)-1; i--)
        {
            init = ((init << 1) ^ (poly & ZEROSONES[((init >> degree) ^ (input[0] >> i)) & 1])) & mask;
        }
    }
    return init;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_spi_read
* Returned Value   : number of bytes read
* Comments         :
*    Read wrapper that works upon both polled and interrupt SPI drivers.
*
*END*----------------------------------------------------------------------*/

static uint_32 _io_sdcard_spi_read 
(
    /* [OUT] Input buffer pointer */
	uchar_ptr    buffer,
	
    /* [IN] Size of buffer elements */
	uint_32      size,
	
    /* [IN] Number of elements to read */
	uint_32      number,
	
    /* [IN] Opened SPI driver handle */
	MQX_FILE_PTR spifd
)
{
	uint_32 count = 0;
	do
	{
		count += fread (buffer + count, size, number - count, spifd); 
	} while (count < number);
	return count;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_spi_write
* Returned Value   : number of bytes written
* Comments         :
*    Write wrapper that works upon both polled and interrupt SPI drivers.
*
*END*----------------------------------------------------------------------*/

static uint_32 _io_sdcard_spi_write 
(
	/* [IN] Output buffer pointer */
	uchar_ptr    buffer,
	
	/* [IN] Size of buffer elements */
	uint_32      size,
	
	/* [IN] Number of elements to write */
	uint_32      number,
	
	/* [IN] Opened SPI driver handle */
	MQX_FILE_PTR spifd
)
{
	uint_32 count = 0;
	do
	{
		count += fwrite (buffer + count, size, number - count, spifd); 
	} while (count < number);
	return count;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_spi_communicate
* Returned Value   : TRUE if successful, FALSE otherwise
* Comments         :
*    Actual low level communication (command, response, data) over SPI.
*
*END*----------------------------------------------------------------------*/

static boolean _io_sdcard_spi_communicate 
(
    /* [IN] SD card info */
    SDCARD_STRUCT_PTR sdcard_ptr, 

    /* [IN] Command byte stream */
    const uint_8 command[], 

    /* [OUT] Response byte stream */
    uint_8 response[], 

    /* [IN] Length of expected response */
    uint_32 resp_len, 

    /* [IN] Data to read/write - when different to response */
    uint_8 data[],
    
    /* [IN] Length of data */
    uint_32 data_len
)
{
    uint_32  i;
    uint_8   tmp[2];
    boolean  result = TRUE;
    MQX_FILE_PTR com = sdcard_ptr->COM_DEVICE;  
    
    /* assert CS, send command*/
    if (6 != _io_sdcard_spi_write ((uint_8_ptr)command, 1, 6, com))
    {
        fflush (com);
        return FALSE;
    }

    /* wait for response */
    for (i = 0; i < sdcard_ptr->TIMEOUT; i++)
    {
        response[0] = SDCARD_SPI_BUSY;
        if (1 != _io_sdcard_spi_read (response, 1, 1, com))
        {
            fflush (com);
            return FALSE;
        }
        if (0 == (response[0] & SDCARD_SPI_RESPONSE_START_MASK))
        {
            break;
        }
    }
    if (i >= sdcard_ptr->TIMEOUT)
    {
        fflush (com);
        return FALSE;
    }
    /* read the rest of response */
    resp_len--;
    if (0 != resp_len)
    {
        memset (response + 1, SDCARD_SPI_BUSY, resp_len);
        if (resp_len != _io_sdcard_spi_read (response + 1, 1, resp_len, com))
        {
            fflush (com);
            return FALSE;
        }
    }

    /* data handling */
    if (NULL != data)
    {
        if (response == data)
        {
            /* read token */
            for (i = 0; i < sdcard_ptr->TIMEOUT; i++)
            {
                tmp[0] = SDCARD_SPI_BUSY;
                if (1 != _io_sdcard_spi_read (tmp, 1, 1, com))
                {
                    fflush (com);
                    return FALSE;
                }
                if (SDCARD_SPI_BUSY != tmp[0]) 
                {
                    break;
                }
            }
            if (i >= sdcard_ptr->TIMEOUT)
            {
                fflush (com);
                return FALSE;
            }
            if (SDCARD_SPI_START != tmp[0])
            {
                result = FALSE;
            }
            else
            {
                /* read data */
                memset (data, SDCARD_SPI_BUSY, data_len);
                if (data_len != _io_sdcard_spi_read (data, 1, data_len, com))
                {
                    fflush (com);
                    return FALSE;
                }
                /* read CRC */
                tmp[0] = SDCARD_SPI_BUSY;
                tmp[1] = SDCARD_SPI_BUSY;
                if (2 != _io_sdcard_spi_read (tmp, 1, 2, com))
                {
                    fflush (com);
                    return FALSE;
                }
                i = tmp[0];
                i = (i << 8) | tmp[1];
                if (command[0] == SDCARD_SPI_COMMANDS[CMD9][0])
                {
                    if (((_io_sdcard_crc (SDCARD_SPI_CRC7_POLY, 7, 0, data, data_len - 1) << 1) | SDCARD_SPI_END_BIT) != data[data_len - 1])
                    {
                        fflush (com);
                        return FALSE;
                    }
                }
                else
                {
                    if (_io_sdcard_crc (SDCARD_SPI_CRC16_POLY, 16, 0, data, data_len) != i)
                    {
                        fflush (com);
                        return FALSE;
                    }
                }
            }
        }
        else
        {
            /* write start token */
            tmp[0] = SDCARD_SPI_START;
            if (1 != _io_sdcard_spi_write (tmp, 1, 1, com))
            {
                fflush (com);
                return FALSE;
            }
            /* write data */
            if (data_len != _io_sdcard_spi_write (data, 1, data_len, com))
            {
                fflush (com);
                return FALSE;
            }
            /* write CRC */
            i = _io_sdcard_crc (SDCARD_SPI_CRC16_POLY, 16, 0, data, data_len);
            tmp[0] = i >> 8;
            tmp[1] = i;
            if (2 != _io_sdcard_spi_write (tmp, 1, 2, com))
            {
                fflush (com);
                return FALSE;
            }
            
            /* read response */
            for (i = 0; i < sdcard_ptr->TIMEOUT; i++)
            {
                tmp[0] = SDCARD_SPI_BUSY;
                if (1 != _io_sdcard_spi_read (tmp, 1, 1, com))
                {
                    fflush (com);
                    return FALSE;
                }
                if ((tmp[0] & SDCARD_SPI_DATA_RESPONSE_MASK) < 16)
                {
                    break;
                }
            } 
            if (i >= sdcard_ptr->TIMEOUT)
            {
                fflush (com);
                return FALSE;
            }
            if (SDCARD_SPI_DATA_RESPONSE_OK != (tmp[0] & SDCARD_SPI_DATA_RESPONSE_MASK))
            {
                fflush (com);
                return FALSE;
            }
            /* wait not busy */
            for (i = 0; i < sdcard_ptr->TIMEOUT; i++)
            {
                tmp[0] = SDCARD_SPI_BUSY;
                if (1 != _io_sdcard_spi_read (tmp, 1, 1, com))
                {
                    fflush (com);
                    return FALSE;
                }
                if (SDCARD_SPI_OK != tmp[0])
                {
                    break;
                }
            } 
            if (i >= sdcard_ptr->TIMEOUT)
            {
                fflush (com);
                return FALSE;
            }
        }
    }

    /* dummy read byte */
    tmp[0] = SDCARD_SPI_BUSY;
    if (1 != _io_sdcard_spi_read (tmp, 1, 1, com))
    {
        fflush (com);
        return FALSE;
    }
    
    /* deassert CS */
    if (MQX_OK != fflush (com))
    {
        return FALSE;
    }
    return result;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_spi_init
* Returned Value   : TRUE if successful, FALSE otherwise
* Comments         :
*    Initializes SPI communication, SD card itself and reads its parameters.
*
*END*----------------------------------------------------------------------*/

boolean _io_sdcard_spi_init 
(
    /* [IN/OUT] SD card info */
    MQX_FILE_PTR fd_ptr
)
{
    uint_8               response[64];
    uint_32              i, param, c_size, c_size_mult, read_bl_len, baudrate;
    IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
    SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;
    
    if ((NULL == sdcard_ptr) || (NULL == sdcard_ptr->COM_DEVICE) || (NULL == sdcard_ptr->INIT))
    {
        return FALSE;
    }

    sdcard_ptr->TIMEOUT = 0;
    sdcard_ptr->NUM_BLOCKS = 0;
    sdcard_ptr->ADDRESS = 0;
    sdcard_ptr->SDHC = FALSE;
    sdcard_ptr->VERSION2 = FALSE;
    
    /* communication setup */
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_CS, &sdcard_ptr->INIT->SIGNALS)) 
    {
        return FALSE;
    }
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_GET_BAUD, &baudrate)) 
    {
        return FALSE;
    }
    param = 8;
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_FRAMESIZE, &param)) 
    {
        return FALSE;
    }
    param = SPI_DEVICE_BIG_ENDIAN;
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_ENDIAN, &param)) 
    {
        return FALSE;
    }
    param = SPI_CLK_POL_PHA_MODE0;
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_MODE, &param)) 
    {
        return FALSE;
    }
    param = SPI_DEVICE_MASTER_MODE;
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_GET_TRANSFER_MODE, &param)) 
    {
        return FALSE;
    }
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_DISABLE_MODF, NULL)) 
    {
        return FALSE;
    }
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_DEVICE_ENABLE, NULL)) 
    {
        return FALSE;
    }

    /* max timeout = 250 ms = 1 sec / (4 * 10 bits in transfer at baudrate) */
    sdcard_ptr->TIMEOUT = baudrate / 40;

    /* set 400 kbps for max compatibility */
    param = 400000;
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_BAUD, &param)) 
    {
        return FALSE;
    }

    /* disable CS for a while */
    param = 0;
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_CS, &param)) 
    {
        return FALSE;
    }
    
    /* send 80 dummy clocks without CS */
    response[0] = SDCARD_SPI_BUSY;
    for (i = 0; i < 10; i++)
    {
        if (1 != _io_sdcard_spi_write (response, 1, 1, sdcard_ptr->COM_DEVICE))
        {
            fflush (sdcard_ptr->COM_DEVICE);
            return FALSE;
        }
    }

    /* return original CS signal */
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_CS, &sdcard_ptr->INIT->SIGNALS)) 
    {
        return FALSE;
    }   
    
    /* send CMD0 = go to idle */
    for (i = 0; i < SDCARD_SPI_RETRIES; i++)
    {
        if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD0], response, 1, NULL, 0))
        {
            return FALSE;
        }
        if (SDCARD_SPI_RESPONSE_IDLE_STATE == response[0])
        {
            break;
        }
    }
    if (i >= SDCARD_SPI_RETRIES)
    {
        return FALSE;
    }

    /* send CMD8 = test version */
    if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD8], response, 5, NULL, 0))
    {
        return FALSE;
    }
    if (0 == (response[0] & SDCARD_SPI_RESPONSE_ILLEGAL_CMD))
    {
        sdcard_ptr->VERSION2 = TRUE;
        if ((response[4] != SDCARD_SPI_COMMANDS[CMD8][4]) || ((response[3] & SDCARD_SPI_IF_COND_VHS_MASK) != SDCARD_SPI_COMMANDS[CMD8][3]))
        {
            return FALSE;
        }
    }

    /* send CMD58 = read OCR ... 3.3V */
    if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD58], response, 5, NULL, 0))
    {
        return FALSE;
    }
    if (0 == (response[0] & SDCARD_SPI_RESPONSE_ILLEGAL_CMD))
    {
        if (response[0] & SDCARD_SPI_RESPONSE_ERROR_MASK)
        {
            return FALSE;
        }       
        if (SDCARD_SPI_OCR_VOLTAGE_3_3V != (response[2] & SDCARD_SPI_OCR_VOLTAGE_3_3V))
        {
            return FALSE;
        }
    }       
    
    /* send CMD55 + ACMD41 = initialize */
    for (i = 0; i < SDCARD_SPI_RETRIES; i++)
    {
        if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD55], response, 1, NULL, 0))
        {
            return FALSE;
        }
        if (response[0] & SDCARD_SPI_RESPONSE_ERROR_MASK)
        {
            return FALSE;
        }       
        if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[ACMD41], response, 5, NULL, 0))
        {
            return FALSE;
        }
        if (response[0] & SDCARD_SPI_RESPONSE_ERROR_MASK)
        {
            return FALSE;
        }       
        if (0 == (response[0] & SDCARD_SPI_RESPONSE_IDLE_STATE)) 
        {
            break;
        }
    }
    if (i >= SDCARD_SPI_RETRIES)
    {
        return FALSE;
    }

    /* version 2 or later card */
    if (sdcard_ptr->VERSION2)
    {
        /* send CMD58 = get CCS */
        if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD58], response, 5, NULL, 0))
        {
            return FALSE;
        }
        if (response[0] & SDCARD_SPI_RESPONSE_ERROR_MASK)
        {
            return FALSE;
        }       
        if (response[1] & SDCARD_SPI_OCR_CCS)
        {
            sdcard_ptr->SDHC = TRUE;
        }
    }
    
    /* send CMD9 = get CSD */
    if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD9], response, 1, response, 16))
    {
        return FALSE;
    }
    if (0 == (response[0] & SDCARD_SPI_CSD_VERSION_MASK))
    {
        read_bl_len = response[5] & 0x0F;
        c_size = response[6] & 0x03;
        c_size = (c_size << 8) | response[7];
        c_size = (c_size << 2) | (response[8] >> 6);
        c_size_mult = ((response[9] & 0x03) << 1) | (response[10] >> 7);
        sdcard_ptr->NUM_BLOCKS = (c_size + 1) * (1 << (c_size_mult + 2)) * (1 << (read_bl_len - 9));
    }
    else
    {
        c_size = response[7] & 0x3F;
        c_size = (c_size << 8) | response[8];
        c_size = (c_size << 8) | response[9];
        sdcard_ptr->NUM_BLOCKS = (c_size + 1) << 10;
    }

    /* send CMD59 = CRC ON */
    if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD59], response, 1, NULL, 0))
    {
        return FALSE;
    }
    if (response[0] & SDCARD_SPI_RESPONSE_ERROR_MASK)
    {
        return FALSE;
    }       

    /* send CMD55 + ACMD13 = get SD status */
    if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD55], response, 1, NULL, 0))
    {
        return FALSE;
    }
    if (response[0] & SDCARD_SPI_RESPONSE_ERROR_MASK)
    {
        return FALSE;
    }       
    if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[ACMD13], response, 2, response, 64))
    {
        return FALSE;
    }
    
    /* calculate baudrate */
    param = baudrate;
    baudrate = response[8];
    if (0 == baudrate)
    {
    	baudrate = 1;
    }
    baudrate <<= 24;
    if (baudrate > param)
    {
        baudrate = param;
    }

    /* send CMD16 = set block to 512 */
    if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD16], response, 1, NULL, 0))
    {
        return FALSE;
    }
    if (response[0] & SDCARD_SPI_RESPONSE_ERROR_MASK)
    {
        return FALSE;
    }       
    
    /* set baudrate */
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_BAUD, &baudrate)) 
    {
        return FALSE;
    }

    return TRUE;
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_spi_read_block
* Returned Value   : TRUE if successful, FALSE otherwise
* Comments         :
*    Reads sector (512 byte) from SD card with given index into given buffer.
*
*END*----------------------------------------------------------------------*/

boolean _io_sdcard_spi_read_block 
(
    /* [IN] SD card info */
    MQX_FILE_PTR fd_ptr, 

    /* [OUT] Buffer to fill with read 512 bytes */
    uchar_ptr buffer, 

    /* [IN] Index of sector to read */
    uint_32   index
)
{
    uint_8               command[6];
    IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
    SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;
    
    if ((NULL == sdcard_ptr) || (NULL == sdcard_ptr->COM_DEVICE) || (NULL == sdcard_ptr->INIT) || (NULL == buffer))
    {
        return FALSE;
    }

    /* Create SD SPI read command */
    command[0] = SDCARD_SPI_CMD17_READ_BLOCK;
    if (! sdcard_ptr->SDHC)
    {
        index <<= IO_SDCARD_BLOCK_SIZE_POWER;
    }
    command[1] = (index >> 24) & 0xFF;
    command[2] = (index >> 16) & 0xFF;
    command[3] = (index >>  8) & 0xFF;
    command[4] = (index >>  0) & 0xFF;
    command[5] = (_io_sdcard_crc (SDCARD_SPI_CRC7_POLY, 7, 0, command, 5) << 1) | SDCARD_SPI_END_BIT;
    
    /* Set proper SPI chip select */
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_CS, &sdcard_ptr->INIT->SIGNALS)) 
    {
        return FALSE;
    }

    /* Read */
    return _io_sdcard_spi_communicate (sdcard_ptr, command, buffer, 1, buffer, IO_SDCARD_BLOCK_SIZE);
}


/*FUNCTION*-------------------------------------------------------------------
* 
* Function Name    : _io_sdcard_spi_write_block
* Returned Value   : TRUE if successful, FALSE otherwise
* Comments         :
*    Writes sector (512 byte) with given index to SD card from given buffer.
*
*END*----------------------------------------------------------------------*/

boolean _io_sdcard_spi_write_block 
(
    /* [IN] SD card info */
    MQX_FILE_PTR fd_ptr, 

    /* [IN] Buffer with 512 bytes to write */
    uchar_ptr buffer, 

    /* [IN] Index of sector to write */
    uint_32   index
)
{
    uint_8               command[6];
    uint_8               response[2];
    IO_DEVICE_STRUCT_PTR io_dev_ptr = fd_ptr->DEV_PTR;
    SDCARD_STRUCT_PTR    sdcard_ptr = (SDCARD_STRUCT_PTR)io_dev_ptr->DRIVER_INIT_PTR;

    if ((NULL == sdcard_ptr) || (NULL == sdcard_ptr->COM_DEVICE) || (NULL == sdcard_ptr->INIT) || (NULL == buffer))
    {
        return FALSE;
    }
    
    /* Create SD SPI write command */
    command[0] = SDCARD_SPI_CMD24_WRITE_BLOCK; 
    if (! sdcard_ptr->SDHC)
    {
        index <<= IO_SDCARD_BLOCK_SIZE_POWER;
    }
    command[1] = (index >> 24) & 0xFF;
    command[2] = (index >> 16) & 0xFF;
    command[3] = (index >>  8) & 0xFF;
    command[4] = (index >>  0) & 0xFF;
    command[5] = (_io_sdcard_crc (SDCARD_SPI_CRC7_POLY, 7, 0, command, 5) << 1) | SDCARD_SPI_END_BIT;

    /* Set proper SPI chip select */
    if (SPI_OK != ioctl (sdcard_ptr->COM_DEVICE, IO_IOCTL_SPI_SET_CS, &sdcard_ptr->INIT->SIGNALS)) 
    {
        return FALSE;
    }

    /* Write and check result */
    if (! _io_sdcard_spi_communicate (sdcard_ptr, command, response, 1, buffer, IO_SDCARD_BLOCK_SIZE))
    {
        return FALSE;
    }
    if (! _io_sdcard_spi_communicate (sdcard_ptr, SDCARD_SPI_COMMANDS[CMD13], response, 2, NULL, 0))
    {
        return FALSE;
    }
    if ((0 != response[1]) || (response[0] & SDCARD_SPI_RESPONSE_ERROR_MASK))
    {
        return FALSE;
    }
    
    return TRUE;
}

/* EOF */
