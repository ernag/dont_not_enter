/*
 * spihelper.c
 *
 *  Created on: Feb 27, 2013
 *      Author: Chris
 */

#include <mqx.h>
#include <bsp.h>
#include <spi.h>
#include "spihelper.h"

///////////////////////////////////////////////////////////////////////////////
//! \brief SPI Initialization Helper
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Open and initialize spi driver.
//!
//! \return MQX_FILE_PTR file descriptor
//!
///////////////////////////////////////////////////////////////////////////////
MQX_FILE_PTR spiInit(uint_32 baud, uint_32 clock_mode, uint_32 endian, uint_32 transfer_mode)
{
    MQX_FILE_PTR spifd;

    spifd = fopen("ispi2:", (pointer)(SPI_FLAG_HALF_DUPLEX));

    if (NULL == spifd)
    {
        return NULL;
    }

    /* set clock speed */
    if (SPI_OK != ioctl(spifd, IO_IOCTL_SPI_SET_BAUD, &baud))
    {
        return NULL;
    }

    /* Set clock mode - one of the SPI_CLK_POL_PHA_MODE defines */
    if (SPI_OK != ioctl(spifd, IO_IOCTL_SPI_SET_MODE, &clock_mode))
    {
        return NULL;
    }

    /* Set big endian */
    if (SPI_OK != ioctl(spifd, IO_IOCTL_SPI_SET_ENDIAN, &endian))
    {
        return NULL;
    }

    /* Set transfer mode */
    if (SPI_OK != ioctl(spifd, IO_IOCTL_SPI_SET_TRANSFER_MODE, &transfer_mode))
    {
        return NULL;
    }

    return spifd;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Change SPI baud rate.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Changes the SPI baud rate for an already opened interface.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int_32 spiChangeBaud( MQX_FILE_PTR spifd, uint_32 baud )
{
    return  ioctl( spifd, IO_IOCTL_SPI_SET_BAUD, &baud );
}


///////////////////////////////////////////////////////////////////////////////
//! \brief SPI Write Helper
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Write txBytes from buffer pTXBuf to spi file descriptor fd.
//! Flush if flush and deassert CS if deassertAfterFlush.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int_32 spiWrite(uint8_t *pTXBuf, uint_32 txBytes, boolean flush, boolean deassertAfterFlush, MQX_FILE_PTR fd)
{
    int_32 result;

    result = 0;
    do
    {
        result += fwrite(pTXBuf + result, 1, txBytes - result, fd);
    } while (result < txBytes);

    if (result != txBytes)
    {
        /* Stop transfer */
        result = -1;
        goto spiWrite_done;
    }

    if (flush)
    {
        if (!deassertAfterFlush)
        {
            uint_32 param;

            /* Get actual flags */
            if (SPI_OK != ioctl(fd, IO_IOCTL_SPI_GET_FLAGS, &param))
            {
                result = -2;
                goto spiWrite_done;
            }

            /* Flush but do not de-assert CS */
            param |= SPI_FLAG_NO_DEASSERT_ON_FLUSH;
            if (SPI_OK != ioctl(fd, IO_IOCTL_SPI_SET_FLAGS, &param))
            {
                result = -3;
                goto spiWrite_done;
            }
        }

        fflush(fd);
    }

spiWrite_done:
    return result;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief SPI Read Helper
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Read rxBytes from buffer pRXBuf from spi file descriptor fd.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int_32 spiRead(uint8_t *pRXBuf, uint_32 rxBytes, MQX_FILE_PTR fd)
{
    int_32 result;

    /* Read size bytes of data */
    result = 0;
    do
    {
        result += fread(pRXBuf + result, 1, rxBytes - result, fd);
    } while (result < rxBytes);

    if (result != rxBytes)
    {
        /* Stop transfer */
        result = -1;
        goto spiRead_done;
    }

spiRead_done:
    return result;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief SPI Manual Chip Select Helper
//!
//! \fntype Non-Reentrant Function
//!
//! \detail Deassert CS manually
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int_32 spiManualDeassertCS(MQX_FILE_PTR fd)
{
    uint_32 param;

    /* Wait till transfer end and de-assert CS */
    if (SPI_OK != ioctl(fd, IO_IOCTL_SPI_FLUSH_DEASSERT_CS, &param))
    {
        return -1;
    }

    /* Get actual flags */
    if (SPI_OK != ioctl(fd, IO_IOCTL_SPI_GET_FLAGS, &param))
    {
        return -2;
    }

    /* Revert opening flags */
    param &= (~SPI_FLAG_NO_DEASSERT_ON_FLUSH);
    if (SPI_OK != ioctl(fd, IO_IOCTL_SPI_SET_FLAGS, &param))
    {
        return -3;
    }

    return 0;
}
