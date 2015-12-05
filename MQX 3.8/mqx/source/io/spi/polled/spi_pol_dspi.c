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
* $FileName: spi_pol_dspi.c$
* $Version : 3.8.B.3$
* $Date    : Dec-8-2011$
*
* Comments:
*
*   The file contains low level SPI driver functions for 51xx Coldfire family.
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>
#include <io_prv.h>
#include <fio_prv.h>
#include "spi.h"
#include "spi_pol_prv.h"
#include "spi_dspi_prv.h"

uint_32 _dspi_polled_init(DSPI_INIT_STRUCT_PTR, pointer _PTR_, char_ptr);
uint_32 _dspi_polled_ioctl(DSPI_INFO_STRUCT_PTR, uint_32, uint_32_ptr, uint_32);

static uint_32 _dspi_polled_deinit(IO_SPI_POLLED_DEVICE_STRUCT_PTR, DSPI_INFO_STRUCT_PTR);
static uint_32 _dspi_polled_rx(IO_SPI_POLLED_DEVICE_STRUCT_PTR, uchar_ptr, uint_32);
static uint_32 _dspi_polled_tx(IO_SPI_POLLED_DEVICE_STRUCT_PTR, uchar_ptr, uint_32);
static uint_16 _dspi_polled_tx_rx(DSPI_INFO_STRUCT_PTR, uint_8_ptr, boolean, uint_32);

static const uint_32 BAUDRATE_PRESCALER[] = { 2, 3, 5, 7 };
static const uint_32 BAUDRATE_SCALER[] = { 2, 4, 6, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };

static const uint_32 DELAY_PRESCALER[] = { 1, 3, 5, 7 };

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_find_baudrate
* Returned Value   : uint_8 divider register setting
* Comments         :
*    Find closest setting of divider register for given baudrate.
*
*END*********************************************************************/
static uint_32 _dspi_find_baudrate
    (
        /* [IN] Module input clock in Hz */
        uint_32 clock,

        /* [IN] Desired baudrate in Hz */
        uint_32 baudrate
    )
{
    uint_32 pres, scaler, dbr, mindbr = 1;
    uint_32 best_pres = 0;
    uint_32 best_scaler = 0;
    uint_32 delayrate;

    int_32 diff;
    uint_32 min_diff;

    uint_32 result;

    /* find combination of prescaler and scaler resulting in baudrate closest to the requested value */
    min_diff = (uint_32)-1;
    for (pres = 0; pres < 4; pres++)
    {
        for (scaler = 0; scaler < 16; scaler++)
        {
            for (dbr = 1; dbr < 3; dbr++)
            {
                diff = baudrate - ((clock * dbr) / (BAUDRATE_PRESCALER[pres] * (BAUDRATE_SCALER[scaler])));
                if (diff < 0) diff = -diff;
                if (min_diff > diff)
                {
                    /* a better match found */
                    min_diff = diff;
                    best_pres = pres;
                    best_scaler = scaler;
                    mindbr = dbr;
                }
            }
        }
    }

    /* store baudrate scaler and prescaler settings to the result */
    result = DSPI_CTAR_PBR(best_pres) | DSPI_CTAR_BR(best_scaler);
    result |= ((mindbr - 1) * DSPI_CTAR_DBR_MASK);

    /* similar lookup for delay prescalers */
    min_diff = (uint_32)-1;
    delayrate = baudrate * 4; /* double the baudrate (half period delay is sufficient) and divisor is (2<<scaler), thus times 4 */
    for (pres = 0; pres < 4; pres++)
    {
        for (scaler = 0; scaler < 16; scaler++)
        {
            diff = ((DELAY_PRESCALER[pres] * delayrate) << scaler) - clock;
            if (diff < 0) diff = -diff;
            if (min_diff > diff)
            {
                /* a better match found */
                min_diff = diff;
                best_pres = pres;
                best_scaler = scaler;
            }
        }
    }

    /* add delay scalers and prescaler settings to the result */
    result |= DSPI_CTAR_CSSCK(best_scaler) | DSPI_CTAR_PCSSCK(best_pres);
    result |= DSPI_CTAR_DT(best_scaler) | DSPI_CTAR_PDT(best_pres);
    // result |= DSPI_CTAR_ASC(best_scaler) | DSPI_CTAR_PASC(best_pres); /* CS deasserted in sw, slow enough */

    return result;
}

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_polled_install
* Returned Value   : MQX error code
* Comments         :
*    Install an SPI device.
*
*END*********************************************************************/
uint_32 _dspi_polled_install
    (
        /* [IN] A string that identifies the device for fopen */
        char_ptr                         identifier,

        /* [IN] The I/O init data pointer */
        DSPI_INIT_STRUCT_CPTR            init_data_ptr
    )
{
    return _io_spi_polled_install(identifier,
    (uint_32 (_CODE_PTR_)(pointer, pointer _PTR_, char_ptr))_dspi_polled_init,
    (uint_32 (_CODE_PTR_)(pointer, pointer))_dspi_polled_deinit,
    (_mqx_int (_CODE_PTR_)(pointer, char_ptr, int_32))_dspi_polled_rx,
    (_mqx_int (_CODE_PTR_)(pointer, char_ptr, int_32))_dspi_polled_tx,
    (_mqx_int (_CODE_PTR_)(pointer, uint_32, _mqx_uint_ptr, uint_32))_dspi_polled_ioctl,
    (pointer)init_data_ptr);
}

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_polled_init
* Returned Value   : MQX error code
* Comments         :
*    This function initializes the SPI module
*
*END*********************************************************************/
uint_32 _dspi_polled_init
    (
        /* [IN] The initialization information for the device being opened */
        DSPI_INIT_STRUCT_PTR          dspi_init_ptr,

        /* [OUT] The address to store device specific information */
        pointer _PTR_                 io_info_ptr_ptr,

        /* [IN] The rest of the name of the device opened */
        char_ptr                      open_name_ptr
    )
{
    VDSPI_REG_STRUCT_PTR              dspi_ptr;
    DSPI_INFO_STRUCT_PTR              io_info_ptr;
    uint_32                           i, clock;

    #if PSP_HAS_DEVICE_PROTECTION
    if (!_bsp_dspi_enable_access(dspi_init_ptr->CHANNEL)) {
        return SPI_ERROR_CHANNEL_INVALID;
    }
    #endif

    // Check channel
    dspi_ptr = _bsp_get_dspi_base_address (dspi_init_ptr->CHANNEL);
    if (NULL == dspi_ptr)
    {
        return SPI_ERROR_CHANNEL_INVALID;
    }

    if (_bsp_dspi_io_init (dspi_init_ptr->CHANNEL) == -1)
    {
        return SPI_ERROR_CHANNEL_INVALID;
    }

    // Disable and clear SPI
    dspi_ptr->MCR &= (~ DSPI_MCR_MDIS_MASK);
    dspi_ptr->MCR = DSPI_MCR_HALT_MASK | DSPI_MCR_CLR_TXF_MASK | DSPI_MCR_CLR_RXF_MASK | DSPI_MCR_DIS_RXF_MASK | DSPI_MCR_DIS_TXF_MASK;

    /* Initialize internal data */
    io_info_ptr = (DSPI_INFO_STRUCT_PTR)_mem_alloc_system_zero((uint_32)sizeof(DSPI_INFO_STRUCT));
    if (io_info_ptr == NULL)
    {
        return MQX_OUT_OF_MEMORY;
    }
    _mem_set_type (io_info_ptr,MEM_TYPE_IO_SPI_INFO_STRUCT);

    *io_info_ptr_ptr = io_info_ptr;

    io_info_ptr->DSPI_PTR           = dspi_ptr;
    io_info_ptr->INIT               = *dspi_init_ptr;
    io_info_ptr->RX_REQUEST         = 0;
    io_info_ptr->RX_BUFFER          = NULL;
    io_info_ptr->RX_IN              = 0;
    io_info_ptr->RX_OUT             = 0;
    io_info_ptr->RX_COUNT           = 0;
    io_info_ptr->RX_DATA            = 0;
    io_info_ptr->TX_BUFFER          = NULL;
    io_info_ptr->TX_IN              = 0;
    io_info_ptr->TX_OUT             = 0;
    io_info_ptr->TX_COUNT           = 0;
    io_info_ptr->TX_DATA            = DSPI_PUSHR_CONT_MASK | DSPI_PUSHR_PCS(0) | DSPI_PUSHR_CTAS(0) | DSPI_PUSHR_TXDATA(0xFFFF);
    io_info_ptr->DMA_FLAGS          = 0;
    io_info_ptr->CS                 = DSPI_PUSHR_PCS_GET(dspi_init_ptr->CS);
    io_info_ptr->CS_ACTIVE          = 0;
    for (i = 0; i < DSPI_CS_COUNT; i++)
    {
        io_info_ptr->CS_CALLBACK[i] = NULL;
        io_info_ptr->CS_USERDATA[i] = NULL;
    }
    io_info_ptr->STATS.INTERRUPTS   = 0;
    io_info_ptr->STATS.RX_PACKETS   = 0;
    io_info_ptr->STATS.RX_OVERFLOWS = 0;
    io_info_ptr->STATS.TX_PACKETS   = 0;
    io_info_ptr->STATS.TX_ABORTS    = 0;
    io_info_ptr->STATS.TX_UNDERFLOWS= 0;

    /* Set the SPI clock baud rate divider */
    dspi_ptr->CTAR[0] = DSPI_CTAR_FMSZ(7);
    
    // Whistle clock configuration. Get the current system clock speed which may have been
    // changed by entry into a low power mode.
    clock = _cm_get_clock_configuration ();
    if (clock >= BSP_CLOCK_CONFIGURATIONS)
    {
       return MQX_INVALID_CONFIGURATION;
    }
    // Bus clock is the clock source for all SPI peripherals
    clock = _cm_get_clock ((BSP_CLOCK_CONFIGURATION)clock, CM_CLOCK_SOURCE_BUS);
    dspi_ptr->CTAR[0] |= _dspi_find_baudrate (clock, dspi_init_ptr->BAUD_RATE);
    
    /* Set up SPI clock polarity and phase */
    switch (dspi_init_ptr->CLOCK_POL_PHASE)
    {
        case (SPI_CLK_POL_PHA_MODE0):
            /* Inactive state of SPI_CLK = logic 0 */
            dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPOL_MASK);
            /* SPI_CLK transitions middle of bit timing */
            dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPHA_MASK);
            break;
        case (SPI_CLK_POL_PHA_MODE1):
            /* Inactive state of SPI_CLK = logic 0 */
            dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPOL_MASK);
            /* SPI_CLK transitions begining of bit timing */
            dspi_ptr->CTAR[0] |= DSPI_CTAR_CPHA_MASK;
            break;
        case (SPI_CLK_POL_PHA_MODE2):
            /* Inactive state of SPI_CLK = logic 1 */
            dspi_ptr->CTAR[0] |= DSPI_CTAR_CPOL_MASK;
            /* SPI_CLK transitions middle of bit timing */
            dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPHA_MASK);
            break;
        case (SPI_CLK_POL_PHA_MODE3):
            /* Inactive state of SPI_CLK = logic 1 */
            dspi_ptr->CTAR[0] |= DSPI_CTAR_CPOL_MASK;
            /* SPI_CLK transitions begining of bit timing */
            dspi_ptr->CTAR[0] |= DSPI_CTAR_CPHA_MASK;
            break;
        default:
            _mem_free (*io_info_ptr_ptr);
            *io_info_ptr_ptr = NULL;
            return SPI_ERROR_MODE_INVALID;
    }

    /* Receive FIFO overflow disable */
    dspi_ptr->MCR |= DSPI_MCR_ROOE_MASK;

    /* Set CS0-7 inactive high */
    dspi_ptr->MCR |= DSPI_MCR_PCSIS(0xFF);

    /* Set transfer mode */
    if (dspi_init_ptr->TRANSFER_MODE == SPI_DEVICE_SLAVE_MODE)
    {
        dspi_ptr->MCR &= (~ DSPI_MCR_MSTR_MASK);
    }
    else if (dspi_init_ptr->TRANSFER_MODE == SPI_DEVICE_MASTER_MODE)
    {
        dspi_ptr->MCR |= DSPI_MCR_MSTR_MASK;
    }
    else
    {
        _mem_free (*io_info_ptr_ptr);
        *io_info_ptr_ptr = NULL;
        return SPI_ERROR_TRANSFER_MODE_INVALID;
    }

    /* Disable interrupts */
    dspi_ptr->RSER = 0;

    /* Clear all flags */
    dspi_ptr->SR = ~ DSPI_SR_TFFF_MASK;

    /* Enable SPI */
    dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);

    return SPI_OK;
}

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_polled_deinit
* Returned Value   : MQX error code
* Comments         :
*    This function de-initializes the SPI module
*
*END*********************************************************************/
static uint_32 _dspi_polled_deinit
    (
        /* [IN] the initialization information for the device being opened */
        IO_SPI_POLLED_DEVICE_STRUCT_PTR io_dev_ptr,

        /* [IN] the address of the device specific information */
        DSPI_INFO_STRUCT_PTR            io_info_ptr
    )
{
    VDSPI_REG_STRUCT_PTR                dspi_ptr;
    uint_32                             index;

    if ((NULL == io_info_ptr) || (NULL == io_dev_ptr))
    {
        return SPI_ERROR_DEINIT_FAILED;
    }

    /* Disable the SPI */
    dspi_ptr = io_info_ptr->DSPI_PTR;
    dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;

    /* Disable all chip selects */
    if (dspi_ptr->MCR & DSPI_MCR_MSTR_MASK)
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

    _mem_free(io_dev_ptr->DEV_INFO_PTR);
    io_dev_ptr->DEV_INFO_PTR = NULL;
    return SPI_OK;
}

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_polled_ioctl
* Returned Value   : MQX error code
* Comments         :
*    This function performs miscellaneous services for
*    the SPI I/O device.
*
*END*********************************************************************/
uint_32 _dspi_polled_ioctl
    (
        /* [IN] The address of the device specific information */
        DSPI_INFO_STRUCT_PTR         io_info_ptr,

        /* [IN] The command to perform */
        uint_32                      cmd,

        /* [IN] Parameters for the command */
        uint_32_ptr                  param_ptr,

        /* [IN] Opening flags */
        uint_32                      flags
    )
{
    VDSPI_REG_STRUCT_PTR             dspi_ptr;
    SPI_CS_CALLBACK_STRUCT_PTR       callbacks;
    uint_32                          val, num, size, command, clock;
    uint_32                          result = SPI_OK;
    SPI_READ_WRITE_STRUCT_PTR        rw_ptr;
    uchar_ptr                        input, output;

    dspi_ptr = io_info_ptr->DSPI_PTR;

    switch (cmd)
    {
        case IO_IOCTL_SPI_GET_BAUD:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                val = dspi_ptr->CTAR[0];
                val = (BAUDRATE_PRESCALER[DSPI_CTAR_PBR_GET(val)] * BAUDRATE_SCALER[DSPI_CTAR_BR_GET(val)]) >> ((val & DSPI_CTAR_DBR_MASK) != 0);
                // Whistle clock configuration. Get the current system clock speed which may have been
                // changed by entry into a low power mode.
                clock = _cm_get_clock_configuration ();
                if (clock >= BSP_CLOCK_CONFIGURATIONS)
                {
                   return MQX_INVALID_CONFIGURATION;
                }
                // Bus clock is the clock source for all SPI peripherals
                clock = _cm_get_clock ((BSP_CLOCK_CONFIGURATION)clock, CM_CLOCK_SOURCE_BUS);                                                       
                *param_ptr = (uint_32)( clock / val);
            }
            break;
        case IO_IOCTL_SPI_SET_BAUD:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else if (0 == (*param_ptr))
            {
                result = SPI_ERROR_BAUD_RATE_INVALID;
            }
            else
            {
                /* Disable SPI module */
                dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
                /* Read current settings and clear those to be changed */
                val = dspi_ptr->CTAR[0];
                val &= ~(DSPI_CTAR_PBR_MASK | DSPI_CTAR_BR_MASK | DSPI_CTAR_DBR_MASK);
                val &= ~(DSPI_CTAR_PCSSCK_MASK | DSPI_CTAR_CSSCK_MASK);
                val &= ~(DSPI_CTAR_PASC_MASK | DSPI_CTAR_ASC_MASK);
                val &= ~(DSPI_CTAR_PDT_MASK | DSPI_CTAR_DT_MASK);
                /* Calculate dividers */
                // Whistle clock configuration. Get the current system clock speed which may have been
                // changed by entry into a low power mode.
                clock = _cm_get_clock_configuration ();
                if (clock >= BSP_CLOCK_CONFIGURATIONS)
                {
                   return MQX_INVALID_CONFIGURATION;
                }
                // Bus clock is the clock source for all SPI peripherals
                clock = _cm_get_clock ((BSP_CLOCK_CONFIGURATION)clock, CM_CLOCK_SOURCE_BUS);      
                val |= _dspi_find_baudrate (clock, *param_ptr);
                /* Write settins to register */
                dspi_ptr->CTAR[0] = val;
                /* Re-enable SPI module */
                dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);
            }
            break;
        case IO_IOCTL_SPI_DEVICE_DISABLE:
            dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
            break;
        case IO_IOCTL_SPI_DEVICE_ENABLE:
            dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);
            break;
        case IO_IOCTL_SPI_GET_MODE:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                if (dspi_ptr->CTAR[0] & DSPI_CTAR_CPOL_MASK)
                {
                    if (dspi_ptr->CTAR[0] & DSPI_CTAR_CPHA_MASK)
                    {
                        *param_ptr = SPI_CLK_POL_PHA_MODE3;
                    }
                    else
                    {
                        *param_ptr = SPI_CLK_POL_PHA_MODE2;
                    }
                }
                else
                {
                    if (dspi_ptr->CTAR[0] & DSPI_CTAR_CPHA_MASK)
                    {
                        *param_ptr = SPI_CLK_POL_PHA_MODE1;
                    }
                    else
                    {
                        *param_ptr = SPI_CLK_POL_PHA_MODE0;
                    }
                }
            }
            break;
        case IO_IOCTL_SPI_SET_MODE:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                /* Disable SPI module */
                dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
                switch (*param_ptr)
                {
                    case (SPI_CLK_POL_PHA_MODE0):
                        /* Inactive state of SPI_CLK = logic 0 */
                        dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPOL_MASK);
                        /* SPI_CLK transitions middle of bit timing */
                        dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPHA_MASK);
                        break;
                    case (SPI_CLK_POL_PHA_MODE1):
                        /* Inactive state of SPI_CLK = logic 0 */
                        dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPOL_MASK);
                        /* SPI_CLK transitions begining of bit timing */
                        dspi_ptr->CTAR[0] |= DSPI_CTAR_CPHA_MASK;
                        break;
                    case (SPI_CLK_POL_PHA_MODE2):
                        /* Inactive state of SPI_CLK = logic 1 */
                        dspi_ptr->CTAR[0] |= DSPI_CTAR_CPOL_MASK;
                        /* SPI_CLK transitions middle of bit timing */
                        dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPHA_MASK);
                        break;
                    case (SPI_CLK_POL_PHA_MODE3):
                        /* Inactive state of SPI_CLK = logic 1 */
                        dspi_ptr->CTAR[0] |= DSPI_CTAR_CPOL_MASK;
                        /* SPI_CLK transitions begining of bit timing */
                        dspi_ptr->CTAR[0] |= DSPI_CTAR_CPHA_MASK;
                        break;
                    default:
                        result = SPI_ERROR_MODE_INVALID;
                        break;
                }
                /* Enable SPI module */
                dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);
            }
            break;
        case IO_IOCTL_SPI_GET_TRANSFER_MODE:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                if (dspi_ptr->MCR & DSPI_MCR_MSTR_MASK)
                {
                    *param_ptr = SPI_DEVICE_MASTER_MODE;
                }
                else
                {
                    *param_ptr = SPI_DEVICE_SLAVE_MODE;
                }
            }
            break;
        case IO_IOCTL_SPI_SET_TRANSFER_MODE:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                /* Disable SPI module */
                dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
                /* Set transfer mode */
                if (*param_ptr == SPI_DEVICE_SLAVE_MODE)
                {
                    dspi_ptr->MCR &= (~ DSPI_MCR_MSTR_MASK);
                }
                else if (*param_ptr == SPI_DEVICE_MASTER_MODE)
                {
                    dspi_ptr->MCR |= DSPI_MCR_MSTR_MASK;
                }
                else
                {
                    result = SPI_ERROR_TRANSFER_MODE_INVALID;
                }
                /* Enable SPI module */
                dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);
            }
            break;
        case IO_IOCTL_SPI_GET_ENDIAN:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else if (dspi_ptr->CTAR[0] & DSPI_CTAR_LSBFE_MASK)
            {
                *param_ptr = SPI_DEVICE_LITTLE_ENDIAN;
            }
            else
            {
                *param_ptr = SPI_DEVICE_BIG_ENDIAN;
            }
            break;
        case IO_IOCTL_SPI_SET_ENDIAN:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else if (! (dspi_ptr->MCR & DSPI_MCR_MSTR_MASK))
            {
                result = SPI_ERROR_ENDIAN_INVALID;
            }
            else if (*param_ptr == SPI_DEVICE_LITTLE_ENDIAN)
            {
                dspi_ptr->CTAR[0] |= DSPI_CTAR_LSBFE_MASK;
            }
            else if (*param_ptr == SPI_DEVICE_BIG_ENDIAN)
            {
                dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_LSBFE_MASK);
            }
            else
            {
                result = SPI_ERROR_ENDIAN_INVALID;
            }
            break;
        case IO_IOCTL_SPI_GET_STATS:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                *((SPI_STATISTICS_STRUCT_PTR)param_ptr) = io_info_ptr->STATS;
            }
            break;
        case IO_IOCTL_SPI_CLEAR_STATS:
            io_info_ptr->STATS.INTERRUPTS   = 0;
            io_info_ptr->STATS.RX_PACKETS   = 0;
            io_info_ptr->STATS.RX_OVERFLOWS = 0;
            io_info_ptr->STATS.TX_PACKETS   = 0;
            io_info_ptr->STATS.TX_ABORTS    = 0;
            io_info_ptr->STATS.TX_UNDERFLOWS= 0;
            break;
        case IO_IOCTL_FLUSH_OUTPUT:
        case IO_IOCTL_SPI_FLUSH_DEASSERT_CS:
            while ((io_info_ptr->RX_COUNT) || (io_info_ptr->TX_COUNT) || (io_info_ptr->DMA_FLAGS))
               {};  // waiting for completion
            if ((0 == (flags & SPI_FLAG_NO_DEASSERT_ON_FLUSH)) || (IO_IOCTL_SPI_FLUSH_DEASSERT_CS == cmd))
            {
                /* Deassert all chip selects */
                if (dspi_ptr->MCR & DSPI_MCR_MSTR_MASK)
                {
                    for (num = 0; num < DSPI_CS_COUNT; num++)
                    {
                        if ((NULL != io_info_ptr->CS_CALLBACK[num]) && (0 != (io_info_ptr->CS_ACTIVE & (1 << num))))
                        {
                            io_info_ptr->CS_CALLBACK[num] (DSPI_PUSHR_PCS(1 << num), 1, io_info_ptr->CS_USERDATA[num]);
                        }
                    }
                    io_info_ptr->CS_ACTIVE = 0;

                    dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
                    dspi_ptr->MCR &= (~ DSPI_MCR_MSTR_MASK);
                    dspi_ptr->MCR |= DSPI_MCR_MSTR_MASK;
                    dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);
                }
            }
            break;
        case IO_IOCTL_SPI_GET_FRAMESIZE:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                *param_ptr = DSPI_CTAR_FMSZ_GET(dspi_ptr->CTAR[0]) + 1;
            }
            break;
        case IO_IOCTL_SPI_SET_FRAMESIZE:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                /* Disable SPI module */
                dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
                dspi_ptr->CTAR[0] &= (~ (DSPI_CTAR_FMSZ(0x0F)));
                dspi_ptr->CTAR[0] |= DSPI_CTAR_FMSZ(*param_ptr - 1);
                /* Enable SPI module */
                dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);
            }
            break;
        case IO_IOCTL_SPI_GET_CS:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                *param_ptr = DSPI_PUSHR_PCS(io_info_ptr->CS);
            }
            break;
        case IO_IOCTL_SPI_SET_CS:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                io_info_ptr->CS = DSPI_PUSHR_PCS_GET(*param_ptr);
            }
            break;
        case IO_IOCTL_SPI_ENABLE_MODF:
            result = MQX_IO_OPERATION_NOT_AVAILABLE;
            break;
        case IO_IOCTL_SPI_DISABLE_MODF:
            break;
        case IO_IOCTL_SPI_SET_CS_CALLBACK:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                callbacks = (SPI_CS_CALLBACK_STRUCT_PTR)(param_ptr);
                for (num = 0; num < DSPI_CS_COUNT; num++)
                {
                    if (0 != (callbacks->MASK & (DSPI_PUSHR_PCS(1 << num))))
                    {
                        io_info_ptr->CS_CALLBACK[num] = callbacks->CALLBACK;
                        io_info_ptr->CS_USERDATA[num] = callbacks->USERDATA;
                    }
                }
            }
            break;
        case IO_IOCTL_SPI_READ_WRITE:
            if (NULL == param_ptr)
            {
                result = SPI_ERROR_INVALID_PARAMETER;
            }
            else
            {
                rw_ptr = (SPI_READ_WRITE_STRUCT_PTR)param_ptr;
                command = DSPI_PUSHR_CONT_MASK | DSPI_PUSHR_PCS(io_info_ptr->CS);
                size = rw_ptr->BUFFER_LENGTH;
                input = (uchar_ptr)rw_ptr->WRITE_BUFFER;
                output = (uchar_ptr)rw_ptr->READ_BUFFER;
                for (num = 0; num < size; num++)
                {
                    val = _dspi_polled_tx_rx (io_info_ptr, input, TRUE, command);
                    if (DSPI_CTAR_FMSZ_GET(dspi_ptr->CTAR[0]) > 7)
                    {
                        output[0] = (uint_8)(val >> 8);
                        output++;
                        input++;
                    }
                    output[0] = (uint_8)val;
                    output++;
                    input++;
                    io_info_ptr->STATS.RX_PACKETS++;
                    io_info_ptr->STATS.TX_PACKETS++;
                }
            }
            break;
        case IO_IOCTL_SPI_KEEP_QSPI_CS_ACTIVE:
            result = MQX_IO_OPERATION_NOT_AVAILABLE;
            break;
        default:
            result = IO_ERROR_INVALID_IOCTL_CMD;
            break;
    }
    return result;
}

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_polled_tx_rx
* Returned Value   : byte received
* Comments         :
*   Actual transmit and receive function.
*
*END*********************************************************************/
static uint_16 _dspi_polled_tx_rx
    (
        /* [IN] The address of the device registers */
        DSPI_INFO_STRUCT_PTR         io_info_ptr,

        /* [IN] Transmitted byte */
        uint_8_ptr                   output,

        /* [IN] Whether to transmit */
        boolean                      tx,

        /* [IN] Command parameters for transmit */
        uint_32                      command
    )
{
    VDSPI_REG_STRUCT_PTR             dspi_ptr = io_info_ptr->DSPI_PTR;
    uint_16                          data;
    uint_32                          index;

    if (tx)
    {
        /* Assert actual chip select if master and not flush */
        if (io_info_ptr->CS ^ io_info_ptr->CS_ACTIVE)
        {
            if ((dspi_ptr->MCR & DSPI_MCR_MSTR_MASK) && (0 != command))
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

        data = output[0];
        if (DSPI_CTAR_FMSZ_GET(dspi_ptr->CTAR[0]) > 7)
        {
            data = (data << 8) | output[1];
        }

        // wait write buffer not full flag
        while (! (dspi_ptr->SR & DSPI_SR_TFFF_MASK))
            { };
        dspi_ptr->PUSHR = command | DSPI_PUSHR_CTAS(0) | DSPI_PUSHR_TXDATA(data);
    }

    // wait read buffer not empty flag
    while (! (dspi_ptr->SR & DSPI_SR_RFDF_MASK))
        { };
    data = DSPI_POPR_RXDATA_GET(dspi_ptr->POPR);
    dspi_ptr->SR |= DSPI_SR_RFDF_MASK;
    if (dspi_ptr->SR & DSPI_SR_RFOF_MASK)
    {
        dspi_ptr->SR |= DSPI_SR_RFOF_MASK;
        io_info_ptr->STATS.RX_OVERFLOWS++;
    }
    if (dspi_ptr->SR & DSPI_SR_TFUF_MASK)
    {
        dspi_ptr->SR |= DSPI_SR_TFUF_MASK;
        io_info_ptr->STATS.TX_UNDERFLOWS++;
    }
    return data;
}

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_polled_rx
* Returned Value   : Returns the number of bytes received
* Comments         :
*   Reads the data into provided array.
*
*END*********************************************************************/
static uint_32 _dspi_polled_rx
    (
        /* [IN] The address of the device specific information */
        IO_SPI_POLLED_DEVICE_STRUCT_PTR io_dev_ptr,

        /* [IN] The array the characters are written to */
        uchar_ptr                       buffer,

        /* [IN] Number of transfers to receive */
        uint_32                         size
    )
{
    DSPI_INFO_STRUCT_PTR                io_info_ptr;
    VDSPI_REG_STRUCT_PTR                dspi_ptr;
    uint_16                             data;
    uint_32                             full, num = 0;

    io_info_ptr  = io_dev_ptr->DEV_INFO_PTR;
    dspi_ptr = io_info_ptr->DSPI_PTR;

    full = io_dev_ptr->FLAGS & SPI_FLAG_FULL_DUPLEX;

    for (num = 0; num < size; num++)
    {
        data = _dspi_polled_tx_rx (io_info_ptr, buffer,
                                        0 != (full | dspi_ptr->MCR & DSPI_MCR_MSTR_MASK),
                                        DSPI_PUSHR_CONT_MASK | DSPI_PUSHR_PCS(io_info_ptr->CS));
        if (0 != full)
        {
            io_info_ptr->STATS.TX_PACKETS++;
        }
        io_info_ptr->STATS.RX_PACKETS++;
        if (DSPI_CTAR_FMSZ_GET(dspi_ptr->CTAR[0]) > 7)
        {
            buffer[0] = (uint_8)(data >> 8);
            buffer++;
            num++;
        }
        buffer[0] = (uint_8)data;
        buffer++;
    }
    return num;
}

/*FUNCTION****************************************************************
*
* Function Name    : _mcf52_dspi_polled_tx
* Returned Value   : return number of byte transmitted
* Comments         :
*   Writes the provided data buffer and loops until transmission complete
*
*END*********************************************************************/
static uint_32 _dspi_polled_tx
    (
        /* [IN] The address of the device specific information */
        IO_SPI_POLLED_DEVICE_STRUCT_PTR io_dev_ptr,

        /* [IN] The array to store data */
        uchar_ptr                       buffer,

        /* [IN] Number of char's to transmit */
        uint_32                         size
    )
{
    DSPI_INFO_STRUCT_PTR                io_info_ptr;
    VDSPI_REG_STRUCT_PTR                dspi_ptr;
    uint_32                             num;

    io_info_ptr  = io_dev_ptr->DEV_INFO_PTR;
    dspi_ptr = io_info_ptr->DSPI_PTR;

    for (num = 0; num < size; num++)
    {
        _dspi_polled_tx_rx (io_info_ptr, buffer, TRUE, DSPI_PUSHR_CONT_MASK | DSPI_PUSHR_PCS(io_info_ptr->CS));
        if (DSPI_CTAR_FMSZ_GET(dspi_ptr->CTAR[0]) > 7)
        {
            buffer++;
            num++;
        }
        buffer++;
        io_info_ptr->STATS.TX_PACKETS++;
    }
    return num;
}
/* EOF */
