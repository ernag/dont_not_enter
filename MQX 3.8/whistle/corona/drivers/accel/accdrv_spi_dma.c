/*
 * accdrv_spi_dma.c
 *  Custom Accelerometer Driver using SPI via DMA for minimal CPU intervention.
 *  
 */

#include <string.h>
#include <mqx.h>
#include <mutex.h>
#include <bsp.h>
#include <spi.h>
#include "acc_mgr.h"
#include "spi_dspi_prv.h"
#include "wassert.h"
#include "app_errors.h"
#include "accel_hal.h"
#include "accdrv_spi_dma.h"
#include "cfg_mgr.h"
#include "pwr_mgr.h"

// -- Datatypes --

// TBD: these defines are missing from lwgpio.h. need to put it in some header file
#define LWGPIO_DMAREQ_MODE_RISING    1
/* Interrupt mode definitions */
#define LWGPIO_INT_MODE_SHIFT (16) /* place it here to have on the same place as PCR_IRQC */
#define LWGPIO_INT_MODE_MASK (0x0F << LWGPIO_INT_MODE_SHIFT)

// These could be modified to use full semaphores potentially
#define SPI_DMA_SEM_CREATE    _lwsem_create
#define SPI_DMA_SEM_DESTROY   _lwsem_destroy
#define SPI_DMA_SEM_POST      _lwsem_post

// debug - trying orig. semw_wait (no ticks)
#define ACCDRV_SEM_WAIT       _lwsem_wait_ticks
//#define ACCDRV_SEM_WAIT     _lwsem_wait
#define SPI_DMA_SEM           LWSEM_STRUCT

#define SPI_DMA_MTX_CREATE(p) void(0)
#define SPI_DMA_MTX_DESTROY   _mutex_destroy
#define SPI_DMA_MTX_UNLOCK    _mutex_unlock
#define SPI_DMA_MTX_LOCK      _mutex_lock
#define SPI_DMA_MTX           MUTEX_STRUCT

#define FIFO_READ_CMD_BYTE  (SPI_STLIS3DH_OUT_X_L | SPI_LIS3DH_RW_BIT | SPI_LIS3DH_INC_BIT)

// -- Global Variables --

// DMA Buffer (Ignore the DMA control byte because this is ONLY used for reading accel data) 
#define DMA_BUFFER ((void *) &dma_buffer[1])

static uint_8 dma_buffer[DMA_BUFFER_SIZE];

static volatile boolean bkgnd_running = FALSE;
static volatile boolean bkgnd_time_to_stop = FALSE;
static volatile boolean baudrate_change_in_progress = FALSE;

// Registered callbacks
static accdrv_cbk_t accdrv_callbacks[ACCDRV_NUM_CBK] = { NULL };

static ACCDRV_STATISTICS_t stats;

// tbd: replace with a mechanism that uses the LIS3DH latched interrupt pin
static boolean wakeup_flag;

// GPIO Pins
LWGPIO_STRUCT acc_int1; // Watermark DMA request pin (active high)
LWGPIO_STRUCT acc_int2; // Wakeup Interrupt pin (active low)

// SPIDMA Driver Semaphores
SPI_DMA_SEM accdrv_spidma_done; // Done Flag
SPI_DMA_MTX accdrv_spidma_lock; // Mutex Lock

static const uint_32 BAUDRATE_PRESCALER[] =
{ 2, 3, 5, 7 };
static const uint_32 BAUDRATE_SCALER[] =
{ 2, 4, 6, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };
static const uint_32 DELAY_PRESCALER[] =
{ 1, 3, 5, 7 };
static const uint_8 trigger_chan = TXCH;
static const uint_32 fifo_rd_cmd = SPI_PUSHR_CONT_MASK | SPI_PUSHR_PCS(ACCEL_CS)
        | SPI_PUSHR_TXDATA(FIFO_READ_CMD_BYTE);


// -- Local Prototypes --
static void accdrv_dma_reset(int dmach, int nchans);
static void accdrv_spidma_int(void *);
static void accdrv_dma_error_int(void *);
static uint_8 accdrv_config_gpio(void);
static uint_32 accdrv_find_baudrate(uint_32 clock, uint_32 baudrate);
static void accdrv_spi_flush(VDSPI_REG_STRUCT_PTR spidev);
static void accdrv_spi_reset(VDSPI_REG_STRUCT_PTR spidev, boolean enable_spi);
static void accdrv_shutdown( void );
static void accdrv_en_wakeup_pin( uint8_t en_int);

#if 0
// DEBUG__ display WTM and DMA times for debug
void accdrv_show_fifo_timestamps( void )
{
    DATE_STRUCT     time_date;

    // Display WTM timestamp 1
    _time_to_date (&g_wtm_date[0], &time_date);
    UTIL_time_print_mqx_time( &time_date, &g_wtm_date[0] );
    
    // Display DMA timestamp 1
    _time_to_date (&g_dma_date[0], &time_date);
    UTIL_time_print_mqx_time( &time_date, &g_dma_date[0] );
    
    // Display WTM timestamp 2
    _time_to_date (&g_wtm_date[1], &time_date);
    UTIL_time_print_mqx_time( &time_date, &g_wtm_date[1] );
    
    // Display DMA timestamp 2
    _time_to_date (&g_dma_date[1], &time_date);
    UTIL_time_print_mqx_time( &time_date, &g_dma_date[1] );
}
#endif

// -- Interrupt Handlers --

// DMA Error Interrupt Handler for all channels
static void accdrv_dma_error_int(void *)
{
    // TODO: handle the error and resume or reset. Must first clear 
    // the DMA_ERR register bits with DMA_CERR = 0x40
    wassert(0);
}

// SPI1 DMA Interrupt Handler for Receive Channel
static void accdrv_spidma_int(void *)
{
    uint_32 pDMA_dest = DMA_DADDR(RXCH);
    
    // Clear the interrupt request from RXCH, which is why we're here
    DMA_CINT = RXCH; // our RXCH
    
    // Log the start time of this buffer
    ACCMGR_log_buffer_end_time(); 

    if (!bkgnd_running)
    {   
        // background not running - just post to the 'done' semaphore 
        SPI_DMA_SEM_POST(&accdrv_spidma_done);
        goto DONE;
    }

    // Process the background  
             
    // Time to shut down?
    if ( bkgnd_time_to_stop)
    {
        accdrv_shutdown();
            
        // Last Buffer callback
        if (accdrv_callbacks[ACCDRV_DMA_LAST_BUFFER_CBK] != NULL)
        {
            accdrv_callbacks[ACCDRV_DMA_LAST_BUFFER_CBK]();
        }
        goto DONE;
    }     
    else
    {
        if (baudrate_change_in_progress)
        {
            // exit now - accdrv_set_baud() will resume the background
            baudrate_change_in_progress = FALSE;
            goto DONE;
        }       
    }
 
    // Buffer Ready callback
    if (accdrv_callbacks[ACCDRV_DMA_BUFFER_RDY_CBK] != NULL)
    {
        accdrv_callbacks[ACCDRV_DMA_BUFFER_RDY_CBK]();
    } 
DONE:
    // Clear the ACCDMA awake vote
    PWRMGR_VoteForSleep( PWRMGR_ACCDMA_VOTE );

    // Release the SPIDMA lock semaphore
    SPI_DMA_MTX_UNLOCK(&accdrv_spidma_lock);
    return;
}

static void acc_int1_wtm_int(void *pin)
{    
    // Clear the ACC_INT1 pin interrupt flag
    lwgpio_int_clear_flag(pin);
    
    accdrv_wtm_action();
}

static void acc_int2_wakeup_int(void *pin)
{   
    // Clear the ACC_INT2 pin interrupt flag
    lwgpio_int_clear_flag(pin);

    accdrv_wakeup_action();
}


// -- Private Functions --

// Configure GPIOB for SPI1 peripheral function
static uint_8 accdrv_config_gpio(void)
{
    PORT_MemMapPtr pctl = (PORT_MemMapPtr) PORTB_BASE_PTR;

    // Configure SPI1 interface 
    pctl->PCR[16] = PORT_PCR_MUX(2); // PTB16 = SPI_ACCEL_MOSI
    pctl->PCR[11] = PORT_PCR_MUX(2); // PTB11 = SPI_ACCEL_SCK
    pctl->PCR[17] = PORT_PCR_MUX(2); // PTB17 = SPI_ACCEL_MISO
    pctl->PCR[10] = PORT_PCR_MUX(2); // PTB10 = SPI_ACCEL_CS0 

    // Configure INT1_WTM to generate an interrupt:
    
    // -- Configure acc_int1 for Interrupt
    if (!lwgpio_init(&acc_int1, BSP_GPIO_ACC_INT1_PIN, LWGPIO_DIR_INPUT,
            LWGPIO_VALUE_NOCHANGE))
    {
        return 1;
    }
    lwgpio_set_functionality(&acc_int1, BSP_GPIO_ACC_INT1_MUX_GPIO);

    // Enable rising(?) edge interrupt for acc_int1
    if (!lwgpio_int_init(&acc_int1, LWGPIO_INT_MODE_RISING))
    {
        return 2;
    }

    // install the ACC_INT1 pin handler using GPIO port isr mux 
    whistle_lwgpio_isr_install(&acc_int1, BSP_PORTA_INT_LEVEL, acc_int1_wtm_int);  
    
    // Clear the acc_int1 interrupt flag
    lwgpio_int_clear_flag(&acc_int1);

    // -- Configure acc_int2 pin for Wakeup Interrupt
    if (!lwgpio_init(&acc_int2, BSP_GPIO_ACC_INT2_PIN, LWGPIO_DIR_INPUT,
            LWGPIO_VALUE_NOCHANGE))
    {
        return 1;
    }
    
    /* disable in case it's already enabled */
    lwgpio_int_enable(&acc_int2, FALSE);
    

    lwgpio_set_functionality(&acc_int2, BSP_GPIO_ACC_INT2_MUX_GPIO);

// DEBUG__ ACC_INT2 should be a falling edge interrupt!
#if 0
// orig code (its been this way for eternity)
    // Enable rising edge interrupt for acc_int2
    if (!lwgpio_int_init(&acc_int2, LWGPIO_INT_MODE_RISING))
#else
// new: config as a falling edge interrupt, as it should be
    if (!lwgpio_int_init(&acc_int2, LWGPIO_INT_MODE_RISING))
#endif
    {
        return 2;
    }
 
    

    // install the ACC_INT2 pin handler using GPIO port isr mux 
    whistle_lwgpio_isr_install(&acc_int2, BSP_PORTC_INT_LEVEL, acc_int2_wakeup_int);  

    return 0;
}

// Find the closest divisor for a given bus clock and desired baudrate.
// Taken from spi_pol_dspi.c
// Returns: SPI CTAR register clock divisor value
static uint_32 accdrv_find_baudrate(
/* [IN] SPI bus input clock in Hz */
uint_32 clock,

/* [IN] SPI desired baudrate in Hz */
uint_32 baudrate)
{
    uint_32 pres, scaler, dbr, mindbr = 1;
    uint_32 best_pres = 0;
    uint_32 best_scaler = 0;
    uint_32 delayrate;

    int_32 diff;
    uint_32 min_diff;

    uint_32 result;

    /* find combination of prescaler and scaler resulting in baudrate closest to the requested value */
    min_diff = (uint_32) -1;
    for (pres = 0; pres < 4; pres++)
    {
        for (scaler = 0; scaler < 16; scaler++)
        {
            for (dbr = 1; dbr < 3; dbr++)
            {
                diff = baudrate
                        - ((clock * dbr)
                                / (BAUDRATE_PRESCALER[pres]
                                        * (BAUDRATE_SCALER[scaler])));
                if (diff < 0)
                    diff = -diff;
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
    min_diff = (uint_32) -1;
    delayrate = baudrate * 4; /* double the baudrate (half period delay is sufficient) and divisor is (2<<scaler), thus times 4 */
    for (pres = 0; pres < 4; pres++)
    {
        for (scaler = 0; scaler < 16; scaler++)
        {
            diff = ((DELAY_PRESCALER[pres] * delayrate) << scaler) - clock;
            if (diff < 0)
                diff = -diff;
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

static void accdrv_shutdown( void )
{
    bkgnd_time_to_stop = bkgnd_running = FALSE;
    
    DMA_CERQ = RXCH; // Disable the RXCH request to prevent further writes to the buffer
    
    // Disable the ACC1_WTM pin interrupt to prevent further WTM requests from being processed
    accdrv_en_wtm_pin( FALSE );
    
    DMA_CSR(RXCH) &= ~(DMA_CSR_INTMAJOR_MASK); // Disable the RXCH interrupts
}

// Reset the DMA channels
static void accdrv_dma_reset(int dmach, int nchans)
{
    int i;
    volatile uint8_t *chprio = (uint_8 *) (((long) &DMA_DCHPRI0) & ~3);

    for (i = dmach; i < dmach + nchans; i++)
    {
        // DMAMUX_CHCFG(i) = 0;
        DMA_CSR(i) = DMA_CSR_DREQ_MASK;
        DMA_CEEI = i;
        DMA_CERQ = i;
        DMA_CINT = i;
        DMA_CDNE = i;
        DMA_CERR = i;
        /* Channel priorities may not be necessary, but we set this here just
         * in case we need them some day */
        chprio[i] &= ~DMA_DCHPRI0_ECP_MASK;
    }
    DMA_CR = DMA_CR_EMLM_MASK; /* use minor-loops, not that we need them... */
}

// Reset the SPI module
static void accdrv_spi_reset(VDSPI_REG_STRUCT_PTR spidev, boolean enable_spi)
{
    spidev->MCR |= DSPI_MCR_HALT_MASK;

    // Wait for SPI to become disabled. Happens either immediately or 
    // at the end of the current frame if one is in progress.
    while (spidev->SR & DSPI_SR_TXRXS_MASK)
        ;

    // This code causes the chip select to de-assert 
    spidev->MCR &= (~DSPI_MCR_MSTR_MASK); // Disable Master Mode
    spidev->MCR |= DSPI_MCR_MSTR_MASK; // Re-enable Master Mode

    // Clear and disable the Rx and Tx FIFOs
    spidev->MCR |= DSPI_MCR_CLR_TXF_MASK | DSPI_MCR_CLR_RXF_MASK
            | DSPI_MCR_DIS_RXF_MASK | DSPI_MCR_DIS_TXF_MASK;

    // Clear the RFDF and TFFF flags
    spidev->SR |= (SPI_SR_RFDF_MASK | SPI_SR_RFOF_MASK | SPI_SR_TFFF_MASK);

    if (enable_spi)
    {
        // Enable RFDF and TFFF DMA peripheral requests
        spidev->RSER |= (SPI_RSER_RFDF_DIRS_MASK | SPI_RSER_RFDF_RE_MASK
                | SPI_RSER_TFFF_DIRS_MASK | SPI_RSER_TFFF_RE_MASK);
        // Un-halt the SPI module and enable the Rx and Tx FIFOs
        spidev->MCR &= ~(DSPI_MCR_HALT_MASK | DSPI_MCR_DIS_RXF_MASK
                | DSPI_MCR_DIS_TXF_MASK);
    } else
    {
        // Disable RFDF and TFFF DMA peripheral requests
        spidev->RSER &= ~(SPI_RSER_RFDF_DIRS_MASK | SPI_RSER_RFDF_RE_MASK
                | SPI_RSER_TFFF_DIRS_MASK | SPI_RSER_TFFF_RE_MASK);
    }
}

// Enable/Disable the ACC_INT2 Wake-up pin interrupt
static void accdrv_en_wakeup_pin( uint8_t en_int)
{
    // Disable acc_int2 BSP vector to prevent spurious interrupts
    _bsp_int_disable( lwgpio_int_get_vector( &acc_int2 ));

    // Enable/disable acc_int2 interrupt pin
    lwgpio_int_enable(&acc_int2, en_int);

    // Set the PWRMGR flag for motion wakeup to match the interrupt setting
    PWRMGR_enable_motion_wakeup( en_int );
    
    // Clear the acc_int2 interrupt flag
    lwgpio_int_clear_flag(&acc_int2);

    // Re-enable the acc_int2 BSP vector
    _bsp_int_enable( lwgpio_int_get_vector( &acc_int2 ));
}

// -- Public Functions --

// Process an assertion of the Wakeup (Motion Detect) pin (acc_int2)
void accdrv_wakeup_action( void )
{
    // Disable the interrupt    
    accdrv_en_wakeup_pin( 0 );
    
    // Set the wakeup flag 
    wakeup_flag = TRUE;
    
    // Callback registered by ACCMGR
    if (accdrv_callbacks[ACCDRV_WAKEUP_PIN_CBK] != NULL)
    {
        accdrv_callbacks[ACCDRV_WAKEUP_PIN_CBK]();
    }
    
}

// Process an assertion of the WTM (FIFO Ready) pin (acc_int1)
void accdrv_wtm_action( void )
{
    // NOTE: There is potentially a big issue here. We have just started the DMA transfer, but we can't
    // go to sleep until its done. Then we need to get the DMA data. We can't sleep until the DMA interrupt
    // is complete.  
    
    // Raise the ACCDMA awake vote
    PWRMGR_VoteForAwake( PWRMGR_ACCDMA_VOTE );

    accdrv_spidma_read_fifo( SPIDMA_ACCEL_WTM_SIZE );
}


// Arm SPI and DMA for transfer on Watermark DMA request
void accdrv_spidma_read_fifo( uint8_t num_samples )
{
    VDSPI_REG_STRUCT_PTR spidev = ACCEL_SPI_DEVICE;
    
    // # of bytes in the transfer 
    //  = 1 SPI Control Byte + (Bytes per Transfer * # of Transfers)
    uint_32 _nbytes = 1 + STLIS3DH_BYTES_PER_FIFO_ENTRY*num_samples;

    if (0==num_samples)
        return; 

    SPI_DMA_MTX_LOCK(&accdrv_spidma_lock);

    // Route SPI1 RX peripheral request to RXCH
    DMAMUX_CHCFG(RXCH) = DMAMUX_CHCFG_ENBL_MASK | ACCEL_SPI_RX_DMA_REQUEST;
    
    // Route SPI1 TX peripheral request to TXCH
    DMAMUX_CHCFG(TXCH) = DMAMUX_CHCFG_ENBL_MASK | ACCEL_SPI_TX_DMA_REQUEST;

    // Reset and enable the SPI module
    accdrv_spi_reset(spidev, TRUE);

    // (RXCH): DMA receive channel
    // Responds to SPI RFDF request
    // Source: SPI POPR register
    // Dest: dma_buffer 
    DMA_DADDR(RXCH) = (uint_32) dma_buffer; // store received bytes at rxbuf pointer
    DMA_DOFF(RXCH) = 1; // inc dest by 1 byte for each received byte
    
    // Note: due to chip select being low during the entire transfer, the driver must send/receive
    // the SPI control byte now and only send enough dummy bytes to read the fifo contents on
    // each DMA request from the watermark signal.
    // Changes related to this hack are labeled "spictl hack"
    
    /// # of major loop iterations
    DMA_CITER_ELINKNO(RXCH) = _nbytes; 
    DMA_BITER_ELINKNO(RXCH) = DMA_CITER_ELINKNO(RXCH); // BITER=CITER

    // (TXCH): DMA transmit channel
    // 32-bit to 32-bit
    // Source: command buffer (32-bit)
    // Dest: SPI1 PUSHR (32-bit Tx FIFO write register)
    DMA_SADDR(TXCH) = (uint_32) &fifo_rd_cmd;
     
    // # of major loop iterations (bytes) for TXCH     
    DMA_CITER_ELINKNO(TXCH) = _nbytes - 1; 
    DMA_BITER_ELINKNO(TXCH) = DMA_CITER_ELINKNO(TXCH); // BITER=CITER
    
    // spictl hack: transmit the SPI control byte now
    spidev->PUSHR = fifo_rd_cmd;
    
#if 0
// DEBUG__ don't do this anymore. instead, send the cmd byte for each FIFO xfer and
// discard it when packing the data
    // stick around and discard the received byte. This should not take long.
    while ( !(spidev->SR & SPI_SR_RFDF_MASK) )
        spins++;
    // tbd: verify that we never wait long here: should be around 96/4 cycles max.
    dummy=spidev->POPR; // read and discard the received byte
    spidev->SR = SPI_SR_RFDF_MASK; // clear the RFDF flag
#endif

    // Enable the RXCH major loop completion interrupt
    // Used for both background DMA driven reads and normal cpu-driven register reads and writes
    DMA_CSR(RXCH) |= DMA_CSR_INTMAJOR_MASK;
    
    // RXCH waits for the RFDF request from the SPI controller
    DMA_SERQ = RXCH; // Set theRXCH  enable request in the DMA_ERQ register 

    // Enable the TXCH channel to service the SPI TFFF request
    DMA_SERQ = TXCH;  // Set the TXCH enable request in the DMA_ERQ register 
}

err_t accdrv_cleanup(void)
{
    VDSPI_REG_STRUCT_PTR spidev = ACCEL_SPI_DEVICE;
    err_t retval = ERR_OK;

    // Disable RXCH interrupts
    DMA_CSR(RXCH) &= ~(DMA_CSR_INTMAJOR_MASK);

    // Disable the DMA requests
    DMA_CERQ = CPCH;
    DMA_CERQ = RXCH;
    DMA_CERQ = TXCH;

    // Check for errors 
    if (DMA_ERR)
    {
        WPRINT_ERR("DMA %08x ES %08x", DMA_ERR, DMA_ES);
        retval = ERR_ACCDRV_DMA_ERROR;
    }

    // Clear the DMA done bits
    DMA_CDNE = RXCH;
    DMA_CDNE = CPCH;
    DMA_CDNE = TXCH;

    // Reset and disable the SPI module
    accdrv_spi_reset(spidev, FALSE);
    
    // Release the SPIDMA lock semaphore
    SPI_DMA_MTX_UNLOCK(&accdrv_spidma_lock);

    return retval;
}

// Return the Watermark Request pin level
LWGPIO_VALUE accdrv_get_int1_level(void)
{
    return lwgpio_get_value(&acc_int1);
}

// Return the Wakeup Interrupt pin level
LWGPIO_VALUE accdrv_get_int2_level(void)
{
    return lwgpio_get_value(&acc_int2);
}

// Transmit and receive SPI data via DMA 
// Returns ERR_OK if there is no error, otherwise returns an error code
err_t accdrv_spidma_xfer(uchar_ptr txbuf, uchar_ptr rxbuf, int_32 _nbytes)
{
    uint_32 command = SPI_PUSHR_CONT_MASK | SPI_PUSHR_PCS(ACCEL_CS);
    uint_32 i;
    int dummy, nbytes, num_to_preload;
    VDSPI_REG_STRUCT_PTR spidev = ACCEL_SPI_DEVICE;
    err_t retval = ERR_OK;

    // Must send at least 2 bytes: command plus data for write/read
    if (_nbytes < 2)
        return ERR_ACCDRV_TOO_FEW_BYTES;

    // TBD: next, try pre-loading upto 4 bytes
    num_to_preload = 1;

    SPI_DMA_MTX_LOCK(&accdrv_spidma_lock);

    // Route SPI1 RX peripheral request to RXCH
    DMAMUX_CHCFG(RXCH) = DMAMUX_CHCFG_ENBL_MASK | ACCEL_SPI_RX_DMA_REQUEST;

    // Reset and enable the SPI module
    accdrv_spi_reset(spidev, TRUE);

    // Max. # of bytes that can be sent = 0x1ff
    if (_nbytes >= DMA_CITER_ELINKYES_CITER_MASK)
        nbytes = DMA_CITER_ELINKYES_CITER_MASK;
    else
        nbytes = _nbytes;

    // (RXCH): DMA receive channel
    // -Responds to DMA request from SPI RFDF (receive drain fifo request)
    // Set the DMA dest. addr. for RXCH = rxbuf 
    DMA_DADDR(RXCH) = (uint_32) (rxbuf ? rxbuf : (uchar_ptr) &dummy);
    DMA_DOFF(RXCH) = rxbuf ? 1 : 0; // inc by 1 byte for each xfer (RX only)
    DMA_CITER_ELINKNO(RXCH) = nbytes;
    DMA_BITER_ELINKNO(RXCH) = DMA_CITER_ELINKNO(RXCH); // BITER=CITER

    // Enable the RXCH major loop completion interrupt
    DMA_CSR(RXCH) |= DMA_CSR_INTMAJOR_MASK;

    // Only enable CPCH and TXCH if there is more data to load
    if (nbytes > num_to_preload)
    {
        // Route SPI1 TX peripheral request to CPCH
        DMAMUX_CHCFG(CPCH) = DMAMUX_CHCFG_ENBL_MASK | ACCEL_SPI_TX_DMA_REQUEST;

        // (CPCH): DMA copy channel -> links to TXCH 
        DMA_SADDR(CPCH) = (uint_32) (txbuf + num_to_preload); // CPU sends first byte(s)
        DMA_DADDR(CPCH) = (uint_32) &command; // Note: this assumes little-endian format
        // Note: CPCH and TXCH xfer one less byte than nbytes because the CPU sends
        // the first byte.
        DMA_CITER_ELINKNO(CPCH) = DMA_CITER_ELINKYES_ELINK_MASK
                | DMA_CITER_ELINKYES_LINKCH(TXCH) | (nbytes - num_to_preload); // # of major loop iterations (bytes) for CPCH
        DMA_BITER_ELINKNO(CPCH) = DMA_CITER_ELINKNO(CPCH); // BITER = CITER

        // (TXCH): DMA transmit channel
        // Source: command buffer (32-bit)
        // Dest: SPI1 PUSHR (32-bit Tx FIFO write register)
        DMA_SADDR(TXCH) = (uint_32) &command;
        DMA_CITER_ELINKNO(TXCH) = nbytes - num_to_preload; // # of major loop iterations (bytes) for TXCH
        DMA_BITER_ELINKNO(TXCH) = DMA_CITER_ELINKNO(TXCH); // BITER=CITER
    } else
    {
        // Not enough bytes to need CPCH and TXCH
        DMAMUX_CHCFG(CPCH) = 0; // clear the CPCH mux configuration
        DMA_CERQ = CPCH; // disable the TFFF request flag
    }

    // RXCH waits for the RFDF request from the SPI controller
    DMA_SERQ = RXCH; // Enable the request for RXCH

    // Send the first bytes manually (by CPU) to get the SPI xfer started 
    // Preload the bytes
    for (i = 0; i < num_to_preload; i++)
    {
        spidev->PUSHR = SPI_PUSHR_CONT_MASK | SPI_PUSHR_PCS(ACCEL_CS)
                | SPI_PUSHR_CTAS(0) | SPI_PUSHR_TXDATA(txbuf[i]);
    }

    // IMPORTANT: Don't enable the CPCH request until AFTER the PUSHR register is loaded (above). 
    if (nbytes > num_to_preload)
    {
        // CPCH services the SPI controller TFFF request
        DMA_SERQ = CPCH; // Set the Request Enable for CPCH
    }

    // Wait for the SPI transfer to complete: timeout = 100 ticks = 1 second
    if (ACCDRV_SEM_WAIT(&accdrv_spidma_done,
            ACCDRV_DMA_WAIT_TIMEOUT_TICKS) != MQX_OK)
    {
        return ERR_ACCDRV_SEM_TIMEOUT;
    }
    //ACCDRV_SEM_WAIT( &accdrv_spidma_done );

    // Disable and reset DMA, SPI. Release the semaphore
    retval = accdrv_cleanup();

    return retval;
}

err_t accdrv_wait_for_done(void)
{
    _mqx_uint retval;
    // Wait for the SPI transfer to complete
    if ((retval = ACCDRV_SEM_WAIT(&accdrv_spidma_done,
            ACCDRV_DMA_WAIT_TIMEOUT_TICKS)) != MQX_OK)
    {
        if (retval == MQX_LWSEM_WAIT_TIMEOUT)
            retval = (err_t) ERR_ACCDRV_SEM_TIMEOUT;
        else
            retval = (err_t) ERR_ACCDRV_SEM_ERROR;
    }
    return retval;
}

// Start background FIFO reads, triggered by the watermark request pin
void accdrv_start_bkgnd(void)
{
    // Enable the WTM pin. Each WTM interrupt will kick off a FIFO read
    // which is finished in the DMA interrupt handler
    accdrv_en_wtm_pin( TRUE );
    bkgnd_running = TRUE;
}

// Command the background FIFO reads to stop
void accdrv_stop_bkgnd( boolean force )
{   
    if ( !force )
    {
        bkgnd_time_to_stop = TRUE; // ask the background to stop
    }
    else
    {
        accdrv_shutdown(); // force the background to stop!
    }
}

/*
 * Enable/Disable the accel SPI interface
 */
void accdrv_enable_spi( uint8_t en )
{
    VDSPI_REG_STRUCT_PTR spidev = ACCEL_SPI_DEVICE;
    // Ensure that the SPI1 clock is turned on
	wint_disable(); // SIM_* can be accessed in other drivers not protected by our lock
    SIM_SCGC6 |= SIM_SCGC6_SPI1_MASK;
	wint_enable();

    if (en)
    {
        // Un-halt the SPI device
        spidev->MCR &= ~(DSPI_MCR_HALT_MASK);
    }
    else
    {
        // Halt the SPI device
        spidev->MCR |= DSPI_MCR_HALT_MASK;
    }
}

/*
 * Return the enabled status of accel SPI interface
 */
uint8_t accdrv_spi_is_enabled( void )
{
    VDSPI_REG_STRUCT_PTR spidev = ACCEL_SPI_DEVICE;
    // Ensure that the SPI1 clock is turned on
	wint_disable(); // SIM_* can be accessed in other drivers not protected by our lock
    SIM_SCGC6 |= SIM_SCGC6_SPI1_MASK;
    wint_enable();
    return ((spidev->MCR & DSPI_MCR_HALT_MASK) ? 0:1);
}

// Set the SPI interface clock rate
void accdrv_spidma_set_baud( uint_32 baud )
{
    VDSPI_REG_STRUCT_PTR spidev = ACCEL_SPI_DEVICE;
    uint_32 val; 
    uint_8 spi_is_enabled = accdrv_spi_is_enabled();
    
    // VLPR mode uses clock config 2
    uint_32 bus_clock = (Cpu_GetClockConfiguration()==2) ? VLPR_BUS_CLOCK : BSP_BUS_CLOCK;
    
#if 0
    // Wait for any in-progress transmit to complete
    if ( bkgnd_running && !bkgnd_time_to_stop )
    {
        baudrate_change_in_progress = TRUE;
        while ( baudrate_change_in_progress )
        {
            _time_delay(1);
            // todo: timeout and error
        }   
    }
#endif
    
    // Disable SPI module
    spidev->MCR |= DSPI_MCR_HALT_MASK;
     
    // Read current settings and clear those to be changed
    val = spidev->CTAR[0];
    val &= ~(DSPI_CTAR_PBR_MASK | DSPI_CTAR_BR_MASK | DSPI_CTAR_DBR_MASK);
    val &= ~(DSPI_CTAR_PCSSCK_MASK | DSPI_CTAR_CSSCK_MASK);
    val &= ~(DSPI_CTAR_PASC_MASK | DSPI_CTAR_ASC_MASK);
    val &= ~(DSPI_CTAR_PDT_MASK | DSPI_CTAR_DT_MASK);
    /* Calculate dividers */
    val |= accdrv_find_baudrate( bus_clock, baud );
    /* Write settins to register */
    spidev->CTAR[0] = val;
    
    // Reenable  SPI if it was enabled
    accdrv_enable_spi( spi_is_enabled );
}

// Initialize SPI1 and DMA peripherals
void accdrv_spidma_init(void)
{
    VDSPI_REG_STRUCT_PTR spidev = ACCEL_SPI_DEVICE;
    MUTEX_ATTR_STRUCT mutexattr;

    // Create the Done and Lock mutexes.
    SPI_DMA_SEM_CREATE(&accdrv_spidma_done, 0);
    
    // Destroy previously created mutex
    if ( MQX_EOK == _mutex_unlock( &accdrv_spidma_lock ) )
    {
        wassert( MQX_OK == _mutex_destroy( &accdrv_spidma_lock ) );
    }
    
    wassert( MQX_OK == _mutatr_init(&mutexattr) );
    mutexattr.SCHED_PROTOCOL = MUTEX_PRIO_INHERIT;
    mutexattr.WAIT_PROTOCOL = MUTEX_PRIORITY_QUEUEING;
    wassert( MQX_OK == _mutex_init(&accdrv_spidma_lock, &mutexattr) );
    
    // Configure the GPIO pins for the Accelerometer
    accdrv_config_gpio();

	wint_disable(); // SIM_* can be accessed in other drivers not protected by our lock
    // Ensure that the SPI1 clock is turned on
    SIM_SCGC6 |= SIM_SCGC6_SPI1_MASK;

    // Ensure that DMAMUX and DMA are being clocked
    SIM_SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
    SIM_SCGC7 |= SIM_SCGC7_DMA_MASK;
    wint_enable();

    // Start off with the RXCH interrupts disabled
    DMA_CSR(RXCH) &= ~(DMA_CSR_INTMAJOR_MASK);

    // Make sure SPI is always enabled. Turning off the MDIS bit
    // stops external gating of the module clock. 
    // NOTE: when power management is implemented it might be 
    // necessary to leave this bit set to allow the SPI module
    // clock to be externally gated.
    spidev->MCR &= (~DSPI_MCR_MDIS_MASK);

    // Reset the SPI module and leave it disabled
    accdrv_spi_reset(spidev, FALSE);

    // Set up Clock and Transfer Attributes register        
    spidev->CTAR[0] = DSPI_CTAR_FMSZ(7); // FMSZ=7: framesize = 7+1 = 8 bits
    spidev->CTAR[0] &= ~DSPI_CTAR_LSBFE_MASK; // LSBFE=0: Data is transferred LSB first

    accdrv_spidma_set_baud( ACCEL_BAUD_RATE );       

    // Choose SPI_CLK_POL_PHA_MODE0: 
    spidev->CTAR[0] &= (~DSPI_CTAR_CPOL_MASK); // CPOL=0: Inactive state of SPI_CLK = Logic 0, 
    spidev->CTAR[0] &= (~DSPI_CTAR_CPHA_MASK); // CPHA=0: SPI_CLK transitions on middle of bit timing

#if 0
// We're not using automatic chip select generation because it always deasserted between bytes which
// causes the LIS3DH to terminate the frame. 
    // Set delay between assertion of chip select and first edge of SCK
    spidev->CTAR[0] |= ( 0x1 << DSPI_CTAR_PCSSCK_SHIFT );// Delay prescaler = 3
    spidev->CTAR[0] |= ( 0x3 << DSPI_CTAR_CSSCK_SHIFT );// Delay = 16 clocks

    // Set the delay between the last edge of SCK and the nation of the chip select
    spidev->CTAR[0] |= ( 0x2 << DSPI_CTAR_PASC_SHIFT );// Delay prescaler = 3
    spidev->CTAR[0] |= ( 0xf << DSPI_CTAR_ASC_SHIFT );// Delay = 7 clocks
#endif

    // Receive FIFO overflow disabled: If the Rx FIFO is full and new data is received, it will
    // overwrite existing FIFO data without generating an overflow error
    spidev->MCR |= DSPI_MCR_ROOE_MASK;

    // Set CS0-7 inactive high
    spidev->MCR |= DSPI_MCR_PCSIS(0xFF);

    // SPI Master Mode
    spidev->MCR |= DSPI_MCR_MSTR_MASK;

    // Enable only the RFDF request flag to give a DMA request when the Rx FIFO is not empty
    spidev->RSER = SPI_RSER_RFDF_RE_MASK | SPI_RSER_RFDF_DIRS_MASK
            | SPI_RSER_TFFF_RE_MASK | SPI_RSER_TFFF_DIRS_MASK;

    // Clear the transmit FIFO Fill Flag
    spidev->SR = ~(DSPI_SR_TFFF_MASK);

    // Install the DMA interrupt service routine in the vector table
    _int_install_isr((uint_32) ACCEL_DMA_VECTOR, accdrv_spidma_int, (void *) NULL);

    // Initialize and enable the Accel. DMA RX channel BSP interrupt
    _bsp_int_init( ACCEL_DMA_VECTOR, ACCEL_DMA_INT_LEVEL, 0, TRUE);
    
    // Install the DMA interrupt service routine in the vector table
    _int_install_isr((uint_32) INT_DMA_Error, accdrv_dma_error_int, (void *) NULL);
    
    // Initialize the DMA Error BSP interrupt (use same interrupt priority)
    _bsp_int_init( INT_DMA_Error, ACCEL_DMA_INT_LEVEL, 0, TRUE);  

    // Disable DMA interrupts before resetting DMA
    _bsp_int_disable( ACCEL_DMA_VECTOR);

    // Reset the DMA 
    accdrv_dma_reset(ACCEL_DMA_CHANBASE, ACCEL_SPI_NDMACH);
   
    // Re-enable DMA interrupts
    _bsp_int_enable( ACCEL_DMA_VECTOR);
    // Enable the DMA error interrupt
    _bsp_int_enable( INT_DMA_Error );

    // Initialise the RXCH (DMA receive Channel)
    // 8-bit -> 8-bit
    // Source: SPI1 POPR register (Rx FIFO read)
    // Dest: rxbuf/dma_buffer (initialized in _xfer() or _arm() function)
    DMA_SADDR(RXCH) = (uint_32) &spidev->POPR;
    DMA_SOFF(RXCH) = 0;
    DMA_SLAST(RXCH) = 0;
    DMA_DLAST_SGA(RXCH) = 0;
    DMA_ATTR(RXCH) = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0); /* (8-bits) */
    DMA_NBYTES_MLNO(RXCH) = 1; /* wait for req after every byte */

    // Route SPI1 RX peripheral request (RFDF) to RXCH
    DMAMUX_CHCFG(RXCH) = DMAMUX_CHCFG_ENBL_MASK | ACCEL_SPI_RX_DMA_REQUEST;

    // Initialise CPCH (DMA copy channel)
    // NOTE: This channel is only used for register writes to the LIS3DH
    // 8-bit -> 8-bit
    // Source: transmit buffer in ram (8 bits)
    // Dest:  command buffer (lower 8 bits)
    DMA_SOFF(CPCH) = 1;
    DMA_DOFF(CPCH) = 0;
    DMA_SLAST(CPCH) = 0;
    DMA_DLAST_SGA(CPCH) = 0;
    DMA_ATTR(CPCH) = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0); /* (8-bits) */
    DMA_NBYTES_MLNO(CPCH) = 1;

    // Link to TXCH after completion of service request
    DMA_CSR(CPCH) |= DMA_CSR_MAJORELINK_MASK | DMA_CSR_MAJORLINKCH(TXCH);
    
    // Initialise TXCH (DMA transmit channel)
    // 32-bit to 32-bit
    // Source: command buffer (all 32 bits)
    // Dest: PUSHR register
    DMA_DADDR(TXCH) = (uint_32) &spidev->PUSHR;
    DMA_SOFF(TXCH) = 0;
    DMA_DOFF(TXCH) = 0;
    DMA_SLAST(TXCH) = 0;
    DMA_DLAST_SGA(TXCH) = 0;
    DMA_ATTR(TXCH) = DMA_ATTR_SSIZE(2) | DMA_ATTR_DSIZE(2); // 32-bit to 32-bit transfer
    DMA_NBYTES_MLNO(TXCH) = 4; // 32-bits = 4 bytes
    
    // Start off with DMAMUX request for CPCH and TXCH disabled.
    // - If a write transfer is used, the CPCH will service SPI TFFF request
    // - If a read transfer or background is used, TXCH will service SPI TFFF request
    DMAMUX_CHCFG(TXCH) = 0;
    DMAMUX_CHCFG(CPCH) = 0;

    // Enable the DMA Error interrupt sources for the active channels in the system
    // NOTE: here we assume knowledge about configuration of the other drivers. Ideally
    // the DMA error interrupt handler would be set up and exist at the MQX level.
    // TODO: See if MQX already has a mechanism for DMA error handling which can be
    // enabled and exploited instead of doing DMA error handling in this driver.
    DMA_EEI = (1<<RXCH) | (1<<TXCH) | (1<<CPCH) |  // Accel. DMA chans 0-2
              (1<<4) | (1<<5) | // Bluetooth DMA chans 4-5
              (0x7 << 8) | // Atheros DMA chans 8-10
              (0x7 << 12); // SPI flash DMA chans 12-14
    
    // Enable DMA debug mode: causes DMA to halt on debugger breakpoints
    DMA_CR |= DMA_CR_EDBG_MASK;
    
    // Reset statistics
    accdrv_reset_stats(); 

    // Enable the SPI module
    spidev->MCR &= (~DSPI_MCR_HALT_MASK);
}

// Destroy SPI DMA mutex so that it can be re-init correctly
void accdrv_deinit(void)
{
    wassert( MQX_OK == SPI_DMA_MTX_DESTROY(&accdrv_spidma_lock) );
}

// Reset driver statistics
void accdrv_reset_stats(void)
{
    // Clear the statistics    
    stats.WakeupInterrupts = 0;
    stats.WatermarkRequests = 0;
    stats.CompletedBuffers = 0;
}

// Get the custom SPIDMA driver statistics
// Returns a pointer to the statistics structure
ACCDRV_STATISTICS_t * accdrv_get_stats(void)
{
    return &stats;
}

// API to return the current DMA buffer pointer.
// This function is provided for unit testing and debug.
uint_8 * accdrv_get_bufptr(void)
{
    return (uint_8 *) DMA_BUFFER;
}


// Pack the buffer by removing the upper 8 bits of each sample word
err_t accdrv_pack_dmabuf( void )
{
    uint_8 * pPacked, * pIdx;
    int i;
    
    // Skip the command byte which is the first byte in the DMA buffer
    pPacked = pIdx = (uint_8 *) &dma_buffer[1];
    if (g_st_cfg.acc_st.use_fake_data)
    {
        for (i=0;i < (1*SPIDMA_ACCEL_WTM_SIZE); i++ )
        {
            *pPacked++ = i;
            *pPacked++ = i;
            *pPacked++ = i;
        }
    }
    else 
    {
        // Upload real data
        for ( i=0; i < (1*SPIDMA_ACCEL_WTM_SIZE*STLIS3DH_BYTES_PER_FIFO_ENTRY); 
            i+=STLIS3DH_BYTES_PER_FIFO_ENTRY)
        {
            // Upper 8 bits of each sample word must be zero
            if ( pIdx[0] || pIdx[2] || pIdx[4] )
            {
                corona_print("DMA_DATA_START_PTR: %p\n", (uint_8 *) &dma_buffer[1]);
                corona_print("CUR_DATA_PTR: %p\n", pIdx);
                corona_print("X0: %u, X1: %u; Y0: %u, Y1: %u; Z0: %u, Z1: %u\n", 
                             pIdx[0], pIdx[1], pIdx[2], pIdx[3], pIdx[4], pIdx[5]);
                return ERR_ACCDRV_BAD_BUFFER;
            }
            // Pack lower 8 bits of each sample word
            *pPacked++ = pIdx[1]; // X_l
            *pPacked++ = pIdx[3]; // Y_l
            *pPacked++ = pIdx[5]; // Z_l
            
//            // Check if the data makes sense for sitting on my desk
//            if ((int8_t) pIdx[1] > 3 || (int8_t) pIdx[1] < -3)
//                corona_print("X: %d\n", pIdx[1]);
//            if ((int8_t) pIdx[3] > 3 || (int8_t) pIdx[3] < -3)
//                corona_print("Y: %d\n", pIdx[3]);
//            if ((int8_t) pIdx[5] > 19 || (int8_t) pIdx[5] < 13)
//                            corona_print("Z: %d\n", pIdx[5]);
            
            pIdx+=STLIS3DH_BYTES_PER_FIFO_ENTRY; // bump to next sample        
        }
    }
    
//    memset(pPacked+1, 0x7F, SPIDMA_ACCEL_WTM_SIZE * 3); // Copy over the old data so you can be alerted if it didn't update
    
    return ERR_OK; 
}

err_t accdrv_reg_cbk(ACCDRV_CBK_ID_t cbk_id, accdrv_cbk_t pCbk)
{
    if ((cbk_id > ACCDRV_NUM_CBK) || (pCbk == NULL))
    {
        return 1;
    }
    accdrv_callbacks[cbk_id] = pCbk;
    return 0;
}

// Enable/Disable the ACC_INT1 Watermark pin interrupt
void accdrv_en_wtm_pin(boolean en_int)
{
    // Disable acc_int1 BSP vector to prevent spurious interrupts
    _bsp_int_disable( lwgpio_int_get_vector( &acc_int1 ));

    // Enable/disable acc_int1 interrupt pin
    lwgpio_int_enable(&acc_int1, en_int);

    // Clear the acc_int1 interrupt flag
    lwgpio_int_clear_flag(&acc_int1);

    // Re-enable the acc_int1 BSP vector
    _bsp_int_enable( lwgpio_int_get_vector( &acc_int1 ));
}

// TBD: use LIS3DH latched interrupt in place of SW registered flag
boolean accdrv_motion_was_detected(void)
{
    return wakeup_flag;
}

// Clear the wakeup flag and re-enable the wakeup pin interrupt
void accdrv_enable_wakeup( uint8_t en )
{
    if ( en )
    {
        #if 0
        if( !g_st_cfg.acc_st.continuous_logging_mode )
        {
            // We want to make sure nobody tries to enable wakeup if USB is connected.
            wassert( 0 == PWRMGR_usb_is_inserted() );
        }
        #endif
        
        // Clear the wakeup flag
        wakeup_flag = FALSE;
        // Re-enable the motion detect wakeup interrupt
        accdrv_en_wakeup_pin( 1 );
    }
    else
    {    
        accdrv_en_wakeup_pin( 0 );
    }
}

// Simulate a wakeup pin rising edge (used by unit test)
void accdrv_simulate_wakeup( void )
{
    accdrv_wakeup_action();
}

boolean accdrv_bkgnd_is_running( void )
{
    return bkgnd_running;
}


