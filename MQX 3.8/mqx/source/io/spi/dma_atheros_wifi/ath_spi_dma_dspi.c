/**HEADER********************************************************************
* 
* Copyright (c) 2011 Freescale Semiconductor;
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
* $FileName: spi_dma_dspi.c$
* $Version : 3.8.2.0$
* $Date    : Oct-13-2011$
*
* Comments:
*
*   The file contains low level DMA DSPI driver functions.
*
*END************************************************************************/

#include <mqx.h>
#include <mutex.h>
#include <bsp.h>
#include <io_prv.h>
#include <fio_prv.h>
#include <assert.h>
#include "spi.h"
#include "spi_int_prv.h"
#include "spi_dspi_prv.h"

extern uint_32 _dspi_polled_init(DSPI_INIT_STRUCT_PTR, pointer _PTR_, char_ptr);
extern uint_32 _dspi_polled_ioctl(VDSPI_INFO_STRUCT_PTR, uint_32, uint_32_ptr, uint_32);

static uint_32 _dspi_dma_deinit
  (
    /* [IN] The address of the device specific information */
    void                            *_io_dev_ptr,

    /* [IN] The address of the channel specific information */
    void                            *_io_info_ptr
  );

static uint_32 _dspi_dma_init
  (
    /* [IN] The initialization information for the device being opened */
    void                            *_dspi_init_ptr,

    /* [OUT] The address to store device specific information */
    pointer _PTR_                   io_info_ptr_ptr,

    /* [IN] The rest of the name of the device opened */
    char_ptr                        open_name_ptr
  );

static int_32 _dspi_dma_rx
  (
    /* [IN] The address of the device specific information */
    void                            *_io_dev_ptr,

    /* [IN] The array characters are to be read from */
    char_ptr                        buffer,

    /* [IN] Number of char's to transmit */
    int_32                          size
  );
static int_32 _dspi_dma_tx
  (
    /* [IN] The address of the device specific information */
    void                            *_io_dev_ptr,

    /* [IN] The array to store data */
    char_ptr                        buffer,

    /* [IN] Number of char's to transmit */
    int_32                          size
  );

static int_32 _dspi_dma_ioctl
  (
    /* [IN] The address of the device specific information */
    void                         *_io_info_ptr,

    /* [IN] The command to perform */
    uint_32                      cmd,

    /* [IN] Parameters for the command */
    uint_32_ptr                  param_ptr,

    /* [IN] Opening flags */
    uint_32                      flags
  );

static const uint_32 DMA_INTS[16] = {INT_DMA0, INT_DMA1, INT_DMA2, INT_DMA3, INT_DMA4, INT_DMA5, INT_DMA6, INT_DMA7, INT_DMA8, INT_DMA9, INT_DMA10, INT_DMA11, INT_DMA12, INT_DMA13, INT_DMA14, INT_DMA15}; 

#define SPI_QUICK_INIT

#include "spi_pol_prv.h"

int _bsp_dspi_dma_setup(void);



#ifndef offset_of
#define offset_of(ptr, str, field) ((uint_32)&((str *)NULL)->field)
#endif
#ifndef container_of
#define container_of(ptr, str, field) \
    ((void *)((uint_32)ptr - offset_of(ptr, str, field)))
#endif


/* Arbitrary DMA Channel base; we use this and 2 above it as well */
#define SPI_DMA_CHANBASE 0
#define SPI_DMA_NDMACH 3
#define RXCH (dmach + 0) /* SPI RX POPR channel */
#define TXCH (dmach + 1) /* SPI TX PUSHR channel */
#define CPCH (dmach + 2) /* SPI copy 8-bit -> 32-bit channel buffer */

#if 0
#define spi_dma_err(fmt...) \
    do { printf("spi_dma_err: " fmt); retval = -1; } while (0)
#else
#define spi_dma_err(fmt...)
#endif

/* Just so that we don't have to modify MQX spi_dspi_prv.h source, we'll hijack
 * an unused field.  This spidma macro also allows us to, some day, create a
 * proper spidma pointer inside DSPI_INFO_STRUCT (or spidma could point to
 * io_info_ptr, but that's more work). */
#define spi_dma_hijack OLD_ISR_DATA[0]
#define SPI_DMA_STATE io_info_ptr->spi_dma_hijack
#define spidma ((SPI_DMA_INFO *)SPI_DMA_STATE)

/* These could be modified to use full semaphores potentially */
#define SPI_DMA_SEM_CREATE    _lwsem_create
#define SPI_DMA_SEM_DESTROY   _lwsem_destroy
#define SPI_DMA_SEM_POST      _lwsem_post
#define SPI_DMA_SEM_WAIT      _lwsem_wait
#define SPI_DMA_SEM           LWSEM_STRUCT     // TODO:  Try to use a real semaphore here if multiple tasks are hitting it.

#define SPI_DMA_MTX_CREATE(p) "ERR"
#define SPI_DMA_MTX_DESTROY   _mutex_destroy
#define SPI_DMA_MTX_UNLOCK    _mutex_unlock
#define SPI_DMA_MTX_LOCK      _mutex_lock
#define SPI_DMA_MTX           MUTEX_STRUCT

#define ERR(val) do { retval = (val); goto error; } while (0)

struct spi_dma_info
{
    uint_32               channel;
    SPI_DMA_SEM           done;
    SPI_DMA_MTX           lock;
    uint_32               vector;
    uint_32               dmach;
    void                  (_CODE_PTR_ OLD_ISR)(pointer);
    pointer               OLD_ISR_DATA;
};
typedef struct spi_dma_info SPI_DMA_INFO;


/*FUNCTION****************************************************************
* 
* Function Name    : _dspi_dma_install
* Returned Value   : MQX error code
* Comments         :
*    Install an SPI device.
*
*END*********************************************************************/

uint_32 _dspi_dma_install
    (
        /* [IN] A string that identifies the device for fopen */
        char_ptr                         identifier,
  
        /* [IN] The I/O init data pointer */
        DSPI_INIT_STRUCT_CPTR            init_data_ptr
    )
{
    return _io_spi_polled_install(identifier,
        (uint_32 (_CODE_PTR_)(pointer, pointer _PTR_, char_ptr))_dspi_dma_init,
        (uint_32 (_CODE_PTR_)(pointer, pointer))_dspi_dma_deinit,
        (_mqx_int (_CODE_PTR_)(pointer, char_ptr, int_32))_dspi_dma_rx,
        (_mqx_int (_CODE_PTR_)(pointer, char_ptr, int_32))_dspi_dma_tx,
        (_mqx_int (_CODE_PTR_)(pointer, uint_32, _mqx_uint_ptr, uint_32))_dspi_dma_ioctl,
        (pointer)init_data_ptr);

}





static void
spi_dma_reset(int dmach, int nchans)
{
    int i;
    volatile uint8_t *chprio = (uint_8 *)(((long)&DMA_DCHPRI0) & ~3);

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
        chprio[i] |= DMA_DCHPRI0_ECP_MASK;
    }
    DMA_CR = DMA_CR_EMLM_MASK; /* use minor-loops, not that we need them... */
}

static void
dump_dmach(int ch)
{
#if 0
#define DUMP_DMAREG(reg, ch) \
    do { printf("spi_dma: " #reg "(%d)=%08x\n", ch, DMA_##reg(ch)); } while (0)
#else
#define DUMP_DMAREG(reg, ch)
#endif
    
    DUMP_DMAREG(SADDR, ch);
    DUMP_DMAREG(SOFF, ch);
    DUMP_DMAREG(ATTR, ch);
    DUMP_DMAREG(NBYTES_MLNO, ch);
    DUMP_DMAREG(SLAST, ch);
    DUMP_DMAREG(DADDR, ch);
    DUMP_DMAREG(DOFF, ch);
    DUMP_DMAREG(CITER_ELINKNO, ch);
    DUMP_DMAREG(BITER_ELINKNO, ch);
    DUMP_DMAREG(DLAST_SGA, ch);
    DUMP_DMAREG(CSR, ch);
}

static void
spi_dma_int(pointer arg)
{
    DSPI_INFO_STRUCT_PTR io_info_ptr = arg;
    uint_32 dmach = spidma->dmach;
    DMA_CINT = RXCH;
    while(DMA_INT & (1 << RXCH)){};
    SPI_DMA_SEM_POST(&spidma->done);
}

/*FUNCTION****************************************************************
*
* Function Name    : _bsp_get_dma_vector
* Returned Value   : Interrupt vector number for desired DMA channel
* Comments         :
*    Return the vector number for a desired DMA channel
*
* NOTE: This should be moved to io/dma/dma_mk60.c, or something similar
*
*END*********************************************************************/
static uint_32
_bsp_get_dma_vector(int channel)
{
    switch (channel)
    {
#define CASE_CH(n) case (n): return INT_DMA##n;
        CASE_CH(0)
        CASE_CH(1)
        CASE_CH(2)
        CASE_CH(3)
        CASE_CH(4)
        CASE_CH(5)
        CASE_CH(6)
        CASE_CH(7)
        CASE_CH(8)
        CASE_CH(9)
        CASE_CH(10)
        CASE_CH(11)
        CASE_CH(12)
        CASE_CH(13)
        CASE_CH(14)
        CASE_CH(15)
        default: return (uint_32)-1;
    }
}

/*FUNCTION****************************************************************
*
* Function Name    : _bsp_get_dma_slot_dspi
* Returned Value   : DMA slot number (to go into DMAMUX register)
* Comments         :
*    Return the SPI controller's DMA slot number.
*
* NOTE: This should be moved to io/dma/dma_mk60.c, or something similar
*
*END*********************************************************************/
static uint_8
_bsp_get_dma_slot_dspi(int channel, int tx)
{
    switch (channel)
    {
        /* From K60N512 RM, table 3-24 */
        case 0: return tx ? 17 : 16;
        case 1: return tx ? 19 : 18;
        case 2: return tx ? 21 : 20;
        default: return (uint_8)-1;
    }
}

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_dma_deinit
* Returned Value   : MQX error code
* Comments         :
*    This function de-initializes the SPI module.
*
*END*********************************************************************/
static uint_32 _dspi_dma_deinit
  (
    /* [IN] The address of the device specific information */
    void                            *_io_dev_ptr,

    /* [IN] The address of the channel specific information */
    void                            *_io_info_ptr
  )
{
    IO_SPI_POLLED_DEVICE_STRUCT_PTR io_dev_ptr = _io_dev_ptr;
    DSPI_INFO_STRUCT_PTR            io_info_ptr = _io_info_ptr;
    /*SPI_MemMapPtr*/VDSPI_REG_STRUCT_PTR                   dspi_ptr;
    DSPI_INIT_STRUCT_PTR            dspi_init_ptr;
    uint_32                         i;

    if (!io_info_ptr || !io_dev_ptr)
        return SPI_ERROR_DEINIT_FAILED;

    if (SPI_DMA_STATE)
    {
        if (spidma->vector)
            _bsp_int_disable(spidma->vector);
        if (spidma->OLD_ISR)
            _int_install_isr(
                spidma->vector, spidma->OLD_ISR,
                spidma->OLD_ISR_DATA);
        spidma->vector = 0;
        spidma->OLD_ISR = 0;
        SPI_DMA_MTX_DESTROY(&spidma->lock);
        SPI_DMA_SEM_DESTROY(&spidma->done);
        _mem_free(SPI_DMA_STATE);
        //SPI_DMA_STATE = NULL;
    }

    /* Disable the SPI */
    dspi_ptr = io_info_ptr->DSPI_PTR;
    dspi_ptr->MCR |= SPI_MCR_HALT_MASK;
    dspi_ptr->RSER = 0;

    /* Disable all chip selects */
    if (dspi_ptr->MCR & SPI_MCR_MSTR_MASK)
    {
        for (i = 0; i < DSPI_CS_COUNT; i++)
            if (io_info_ptr->CS_CALLBACK[i] &&
                (io_info_ptr->CS_ACTIVE & (1 << i)))
                io_info_ptr->CS_CALLBACK[i] (SPI_PUSHR_PCS(1 << i), 1,
                    io_info_ptr->CS_USERDATA[i]);
        io_info_ptr->CS_ACTIVE = 0;
    }

    _mem_free(io_info_ptr);
    io_dev_ptr->DEV_INFO_PTR = NULL; /* io_info_ptr */
    return SPI_OK;
}


/*FUNCTION****************************************************************
*
* Function Name    : _dspi_dma_init
* Returned Value   : MQX error code
* Comments         :
*    This function initializes the SPI module
*
*END*********************************************************************/
static uint_32 _dspi_dma_init
  (
    /* [IN] The initialization information for the device being opened */
    void                            *_dspi_init_ptr,

    /* [OUT] The address to store device specific information */
    pointer _PTR_                   io_info_ptr_ptr,

    /* [IN] The rest of the name of the device opened */
    char_ptr                        open_name_ptr
  )
{
    DSPI_INIT_STRUCT_PTR            dspi_init_ptr = _dspi_init_ptr;
    /*SPI_MemMapPtr*/VDSPI_REG_STRUCT_PTR                   dspi_ptr;
    IO_SPI_POLLED_DEVICE_STRUCT_PTR io_dev_ptr;
    volatile DSPI_INFO_STRUCT_PTR            io_info_ptr = NULL;
    uint_32                         retval = SPI_OK, dmach;
    MUTEX_ATTR_STRUCT mutexattr;

    /* We happen to know that io_info_ptr_ptr is a member of io_dev_ptr */
    io_dev_ptr = container_of(io_info_ptr_ptr, IO_SPI_POLLED_DEVICE_STRUCT,
                              DEV_INFO_PTR);

    /* Initialization as in polled mode */
    retval = _dspi_polled_init(dspi_init_ptr, &(io_dev_ptr->DEV_INFO_PTR),
                               open_name_ptr);
    if (retval)
        ERR(retval);

    io_info_ptr = io_dev_ptr->DEV_INFO_PTR;
    dspi_ptr = io_info_ptr->DSPI_PTR;
   
    SPI_DMA_STATE =
            (pointer)_mem_alloc_system_zero((uint_32)sizeof(SPI_DMA_INFO));
    if (SPI_DMA_STATE == NULL)
        ERR(MQX_OUT_OF_MEMORY);
    
    SPI_DMA_SEM_CREATE(&spidma->done, 0);
    
    retval = _mutatr_init(&mutexattr);
    if (retval)
        ERR(retval);
    
    mutexattr.SCHED_PROTOCOL = MUTEX_PRIO_INHERIT;
    mutexattr.WAIT_PROTOCOL = MUTEX_PRIORITY_QUEUEING;
    retval = _mutex_init(&spidma->lock, &mutexattr);
    if (retval)
        ERR(retval);
    
    spidma->channel = dspi_init_ptr->CHANNEL;
   
    // Changed to use DMA_RX_CHANNEL definied in init_spi.c - BJM 3-APR-2013
    //spidma->dmach = dmach = SPI_DMA_CHANBASE;
    spidma->dmach = dmach = dspi_init_ptr->DMA_RX_CHANNEL;
    
    
    spidma->vector =
        _bsp_get_dma_vector(spidma->dmach);

    /* Install ISR and enable interrupt */
    spidma->OLD_ISR_DATA =
        _int_get_isr_data(spidma->vector);
    spidma->OLD_ISR = _int_install_isr(
        spidma->vector, spi_dma_int, io_info_ptr);
    if (spidma->OLD_ISR == NULL)
        ERR(SPI_ERROR_INVALID_PARAMETER);
    /* Just use the same IRQ level as DSPI, for now */
    _bsp_int_init(spidma->vector, BSP_DSPI_INT_LEVEL, 0, TRUE);
    
    _bsp_int_disable(spidma->vector);
    spi_dma_reset(dmach, SPI_DMA_NDMACH);
    _bsp_int_enable(spidma->vector);
#ifdef SPI_QUICK_INIT /* moved to init */
    /* Set up each channel with static values to speed up subsequent requests */
    DMA_SADDR(RXCH) = (uint_32)&dspi_ptr->POPR;
    DMA_SOFF(RXCH) = 0;
    DMA_SLAST(RXCH) = 0;
    DMA_DLAST_SGA(RXCH) = 0;
    DMA_ATTR(RXCH) = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0); /* (8-bits) */
    DMA_NBYTES_MLNO(RXCH) = 1; /* wait for req after every byte */

    DMA_SOFF(CPCH) = 1;
    DMA_DOFF(CPCH) = 0;
    DMA_SLAST(CPCH) = 0;
    DMA_DLAST_SGA(CPCH) = 0;
    DMA_ATTR(CPCH) = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0); /* (8-bits) */
    DMA_NBYTES_MLNO(CPCH) = 1;
    DMA_CSR(CPCH) |= DMA_CSR_MAJORELINK_MASK | DMA_CSR_MAJORLINKCH(TXCH);
    DMA_DADDR(TXCH) = (uint_32)&dspi_ptr->PUSHR;
    DMA_SOFF(TXCH) = 0;
    DMA_DOFF(TXCH) = 0;
    DMA_SLAST(TXCH) = 0;
    DMA_DLAST_SGA(TXCH) = 0;
    DMA_ATTR(TXCH) = DMA_ATTR_SSIZE(2) | DMA_ATTR_DSIZE(2); /* (32-bit) */
    DMA_NBYTES_MLNO(TXCH) = 4; /* 32-bits = 4 bytes */
    DMA_CSR(RXCH) |= DMA_CSR_INTMAJOR_MASK;
#endif

    dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
    dspi_ptr->RSER |= SPI_RSER_RFOF_RE_MASK | SPI_RSER_TFUF_RE_MASK | SPI_RSER_RFDF_RE_MASK | SPI_RSER_TFFF_RE_MASK | SPI_RSER_RFDF_DIRS_MASK | SPI_RSER_TFFF_DIRS_MASK;
    dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);

          
    //Added by Amit
	_int_disable(); // SIM_* can be accessed in other drivers not protected by our lock
    SIM_SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
    SIM_SCGC7 |= SIM_SCGC7_DMA_MASK;
    _int_enable();

    /* FALLTHROUGH */
error:
    if (retval != SPI_OK)
    {
        _dspi_dma_deinit(io_dev_ptr, io_info_ptr);
        //assert(0);
    }
        
    return retval;
}


/*FUNCTION****************************************************************
*
* Function Name    : _dspi_dma_xfer
* Returned Value   : MQX error code
* Comments         :
*    This function transmits or receives SPI data.
*
*END*********************************************************************/
static int_32
_dspi_dma_xfer
  (
    /* [IN] The address of the channel specific information */
    DSPI_INFO_STRUCT_PTR io_info_ptr,

    /* [IN] Transmit (1) or Receive (0) */
    uchar_ptr            txbuf,

    /* [IN/OUT] The array to store data */
    uchar_ptr            rxbuf,

    /* [IN] Number of char's to transfer */
    int_32               _nbytes
  )
{
	VDSPI_REG_STRUCT_PTR dspi_ptr = io_info_ptr->DSPI_PTR;
    uint_32 command = SPI_PUSHR_CONT_MASK | SPI_PUSHR_PCS(io_info_ptr->CS), i;
    uint_32 dmach;
    /* NB: DCHPRI0 is incorrect so and out the lower bits */
    volatile uint8_t *chprio = (uint_8 *)(((uint_32)&DMA_DCHPRI0) & ~3); // TODO
    int retval = 0, dummy, nbytes, xfered = 0;
    
    if (_nbytes <= 0)
        return 0;

    SPI_DMA_MTX_LOCK(&spidma->lock);

    /* Ensure that DMAMUX and DMA are being clocked */
	_int_disable(); // SIM_* can be accessed in other drivers not protected by our lock
    if (!(SIM_SCGC6 & SIM_SCGC6_DMAMUX_MASK))
    {
        //printf("%s: enable dmamux clk\n", __func__);
        SIM_SCGC6 |= SIM_SCGC6_DMAMUX_MASK;
    }
    if (!(SIM_SCGC7 & SIM_SCGC7_DMA_MASK))
    {
        //printf("%s: enable dma clk\n", __func__);
        SIM_SCGC7 |= SIM_SCGC7_DMA_MASK;
    }
    _int_enable();
   
    /* Assert actual chip select if master
     * NOTE: This potentially only asserts once during the whole xfer, which
     *       may be problematic depending on the other device. */
    if ((io_info_ptr->CS ^ io_info_ptr->CS_ACTIVE) &&
        (dspi_ptr->MCR & SPI_MCR_MSTR_MASK))
    {
        for (i = 0; i < DSPI_CS_COUNT; i++)
            if (((io_info_ptr->CS ^ io_info_ptr->CS_ACTIVE) & (1 << i)) &&
                (io_info_ptr->CS_CALLBACK[i]))
                io_info_ptr->CS_CALLBACK[i](
                    SPI_PUSHR_PCS(1 << i),
                    (io_info_ptr->CS_ACTIVE >> i) & 1,
                    io_info_ptr->CS_USERDATA[i]);
        io_info_ptr->CS_ACTIVE = io_info_ptr->CS;
    }
    
    dmach = spidma->dmach;
    
    /*
     *   Ernie:  Just make sure nothing pending.
     */
    DMA_CINT = dmach;
    while(DMA_INT & (1 << dmach)){};
    
    //dump_dmach(RXCH);
    _DCACHE_FLUSH_MBYTES(&command, sizeof(command)); /* flush command to RAM */

    /* Route SPIn RX peripheral request to RXCH */
    DMAMUX_CHCFG(RXCH) = DMAMUX_CHCFG_ENBL_MASK |
        _bsp_get_dma_slot_dspi(spidma->channel, 0);

    /* Enable RFDF DMA peripheral request */
    dspi_ptr->RSER |= SPI_RSER_RFDF_DIRS_MASK | SPI_RSER_RFDF_RE_MASK;
  
    /* FALLTHROUGH */
again:
    if (_nbytes >= DMA_CITER_ELINKYES_CITER_MASK)
        nbytes = DMA_CITER_ELINKYES_CITER_MASK;
    else nbytes = _nbytes;

    /* Flush txbuf contents to RAM */
    if (txbuf) _DCACHE_FLUSH_MBYTES(txbuf, nbytes);

    /* Step 1: Send first byte with manual PUSH write (below) */

    /* Step 2: Read from POPR and write to rxbuf/dummy */
#ifndef SPI_QUICK_INIT /* moved to init */
    DMA_SADDR(RXCH) = (uint_32)&dspi_ptr->POPR;
#endif
    DMA_DADDR(RXCH) = (uint_32)(rxbuf ? rxbuf : (uchar_ptr)&dummy);
    DMA_DOFF(RXCH) = rxbuf ? 1 : 0; /* inc by 1 byte every xfer */
#ifndef SPI_QUICK_INIT /* moved to init */
    DMA_ATTR(RXCH) = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0); /* (8-bits) */
    DMA_NBYTES_MLNO(RXCH) = 1; /* wait for req after every byte */
#endif
    DMA_CITER_ELINKNO(RXCH) =
        ((nbytes > 1) ?
         (DMA_CITER_ELINKYES_ELINK_MASK |
          DMA_CITER_ELINKYES_LINKCH(txbuf ? CPCH : TXCH)) : 0) |
        nbytes;
    DMA_BITER_ELINKNO(RXCH) = DMA_CITER_ELINKNO(RXCH); /* = CITER_ELINKNO */

    /* Step 3: (tx only) Grab next byte and place into 32-bit PUSHR buffer */
    if (txbuf)
    {
        DMA_SADDR(CPCH) = (uint_32)(txbuf + 1); /* NB: CPU sends first byte */
        DMA_DADDR(CPCH) = (uint_32)&command; /* (NOTE: little-endian only) */
#ifndef SPI_QUICK_INIT /* moved to init */
        DMA_SOFF(CPCH) = 1;
        DMA_DOFF(CPCH) = 0;
        DMA_SLAST(CPCH) = 0;
        DMA_DLAST_SGA(CPCH) = 0;
        DMA_ATTR(CPCH) = DMA_ATTR_SSIZE(0) | DMA_ATTR_DSIZE(0); /* (8-bits) */
        DMA_NBYTES_MLNO(CPCH) = 1;
#endif
        /* CPCH and TXCH xfer one less byte than buf length because the CPU
         * sends the first byte w/o DMA */
        DMA_CITER_ELINKNO(CPCH) = ((nbytes > 1) ?
            (DMA_CITER_ELINKYES_ELINK_MASK | DMA_CITER_ELINKYES_LINKCH(TXCH)) : 0)
            | (nbytes - 1);
        DMA_BITER_ELINKNO(CPCH) = DMA_CITER_ELINKNO(CPCH); /* = CITER_ELINKNO */
#ifndef SPI_QUICK_INIT /* moved to init */
        /* ensure that cpch triggers the txch on the last xfer */
        DMA_CSR(CPCH) |= DMA_CSR_MAJORELINK_MASK | DMA_CSR_MAJORLINKCH(TXCH);
#endif
    }
    
    /* Step 4: Read 32-bit PUSHR buffer and write to actual PUSHR */
    DMA_SADDR(TXCH) = (uint_32)&command;
#ifndef SPI_QUICK_INIT /* moved to init */
    DMA_DADDR(TXCH) = (uint_32)&dspi_ptr->PUSHR;
    DMA_SOFF(TXCH) = 0;
    DMA_DOFF(TXCH) = 0;
    DMA_SLAST(TXCH) = 0;
    DMA_DLAST_SGA(TXCH) = 0;
    DMA_ATTR(TXCH) = DMA_ATTR_SSIZE(2) | DMA_ATTR_DSIZE(2); /* (32-bit) */
    DMA_NBYTES_MLNO(TXCH) = 4; /* 32-bits = 4 bytes */
#endif
    DMA_CITER_ELINKNO(TXCH) = nbytes - 1;
    DMA_BITER_ELINKNO(TXCH) = DMA_CITER_ELINKNO(TXCH); /* = CITER_ELINKNO */

    /* Step 5: RXCH waits for next RFDF service request from SPI ctlr */

    DMA_SERQ = RXCH;
#ifndef SPI_QUICK_INIT /* moved to init */
    DMA_CSR(RXCH) |= DMA_CSR_INTMAJOR_MASK;
#endif

    /* NB: Manual CPU write, to get the SPI xfer started */
    dspi_ptr->PUSHR = SPI_PUSHR_CONT_MASK | SPI_PUSHR_PCS(io_info_ptr->CS) |
        SPI_PUSHR_CTAS(0) | SPI_PUSHR_TXDATA(txbuf ? txbuf[0] : 0);

    SPI_DMA_SEM_WAIT(&spidma->done);
    /* while (!(DMA_INT & (1 << RXCH)) && --to) {} */

    if (rxbuf)
        _DCACHE_INVALIDATE_MBYTES(rxbuf, nbytes);
   
    if (txbuf && (nbytes > 1) && !(DMA_CSR(CPCH) & DMA_CSR_DONE_MASK))
        spi_dma_err("cpch didn't finish!\n");

    if ((nbytes > 1) && !(DMA_CSR(TXCH) & DMA_CSR_DONE_MASK))
        spi_dma_err("txch didn't finish!\n");

    if (dspi_ptr->SR & SPI_SR_RXCTR_MASK)
        spi_dma_err("rx fifo has junk!!\n");

    if (DMA_CITER_ELINKNO(RXCH) != DMA_BITER_ELINKNO(RXCH) ||
        DMA_CITER_ELINKNO(TXCH) != DMA_BITER_ELINKNO(TXCH) ||
        DMA_CITER_ELINKNO(CPCH) != DMA_BITER_ELINKNO(CPCH))
        spi_dma_err("citer: %04x/%04x/%04x, biter: %04x/%04x/%04x "
            "stat: %08x/%08x/%08x\n",
            DMA_CITER_ELINKNO(RXCH), DMA_CITER_ELINKNO(TXCH),
            DMA_CITER_ELINKNO(CPCH), DMA_BITER_ELINKNO(RXCH),
            DMA_BITER_ELINKNO(TXCH), DMA_BITER_ELINKNO(CPCH),
            DMA_CSR(RXCH), DMA_CSR(TXCH), DMA_CSR(CPCH));

    if (DMA_ERR)
        spi_dma_err("DMA_ERR: %08x, DMA_ES: %08x\n", DMA_ERR, DMA_ES);

    if (retval)
    {
        dump_dmach(RXCH);
        dump_dmach(TXCH);
        if (txbuf) dump_dmach(CPCH);
    }

    /* DMA_CINT = RXCH; (for polled-mode) */
    DMA_CDNE = RXCH;
    DMA_CDNE = TXCH;
    if (txbuf) DMA_CDNE = TXCH;

    if (!retval)
    {
        _nbytes -= nbytes;
        xfered += nbytes;
        if (_nbytes)
        {
            if (rxbuf) rxbuf += nbytes;
            if (txbuf) txbuf += nbytes;
            goto again;
        }
    }

    DMAMUX_CHCFG(RXCH) = 0;

    dspi_ptr->RSER &= ~(SPI_RSER_TFFF_DIRS_MASK | SPI_RSER_TFFF_RE_MASK |
                        SPI_RSER_RFDF_DIRS_MASK | SPI_RSER_RFDF_RE_MASK);
    dspi_ptr->MCR &= ~(SPI_MCR_DIS_TXF_MASK | SPI_MCR_DIS_RXF_MASK);
    dspi_ptr->SR |= SPI_SR_RFDF_MASK;

    /* Deassert all chip selects */
    if (dspi_ptr->MCR & SPI_MCR_MSTR_MASK)
    {
        for (i = 0; i < DSPI_CS_COUNT; i++)
            if (io_info_ptr->CS_CALLBACK[i] &&
                (io_info_ptr->CS_ACTIVE & (1 << i)))
                io_info_ptr->CS_CALLBACK[i](SPI_PUSHR_PCS(1 << i), 1,
                                            io_info_ptr->CS_USERDATA[i]);
        io_info_ptr->CS_ACTIVE = 0;
    }

    /* rx overflow is ok */
    if (dspi_ptr->SR & SPI_SR_RFOF_MASK)
        dspi_ptr->SR |= SPI_SR_RFOF_MASK;

    /*
     * ERNIE:  Experimenting here with _ALLOWING_ DMA pre-emption to the 
     *         Atheros chip and SPI Flash.
     *         
     *         Potentially related to COR_174_WORKAROUND.
     *         4/23/2013 - Making the following 3 lines live again,
     *                     like they originally were.  SPI Flash and
     *                     Atheros should not be pre-emptable.
     * 
     */
        chprio[RXCH] &= ~DMA_DCHPRI0_ECP_MASK;
        if (txbuf) chprio[CPCH] &= ~DMA_DCHPRI0_ECP_MASK;
        chprio[TXCH] &= ~DMA_DCHPRI0_ECP_MASK;
    

    SPI_DMA_MTX_UNLOCK(&spidma->lock);

    if (rxbuf)
        io_info_ptr->STATS.RX_PACKETS += nbytes;
    if (txbuf)
        io_info_ptr->STATS.TX_PACKETS += nbytes;

    return retval ? retval : xfered;
}





/*FUNCTION****************************************************************
*
* Function Name    : _dspi_dma_ioctl
* Returned Value   : MQX error code
* Comments         :
*    This function performs miscellaneous services for
*    the SPI I/O device.
*
*END*********************************************************************/
static int_32 _dspi_dma_ioctl
  (
    /* [IN] The address of the device specific information */
    void                         *_io_info_ptr,

    /* [IN] The command to perform */
    uint_32                      cmd,

    /* [IN] Parameters for the command */
    uint_32_ptr                  param_ptr,

    /* [IN] Opening flags */
    uint_32                      flags
  )
{
    DSPI_INFO_STRUCT_PTR         io_info_ptr = _io_info_ptr;
    int                          retval;
    SPI_READ_WRITE_STRUCT_PTR    rw_ptr;

    if (!io_info_ptr)
        return SPI_ERROR_INVALID_PARAMETER;

    if (cmd == IO_IOCTL_SPI_READ_WRITE)
    {
        if (!param_ptr)
            ERR(SPI_ERROR_INVALID_PARAMETER);

        rw_ptr = (SPI_READ_WRITE_STRUCT_PTR)param_ptr;
        retval = _dspi_dma_xfer(
            io_info_ptr, (uchar_ptr)rw_ptr->WRITE_BUFFER,
            (uchar_ptr)rw_ptr->READ_BUFFER, rw_ptr->BUFFER_LENGTH);
        if (retval == rw_ptr->BUFFER_LENGTH)
            retval = SPI_OK;
    } else
    {
        SPI_DMA_MTX_LOCK(&spidma->lock);
        retval = (int_32)_dspi_polled_ioctl(io_info_ptr, cmd, param_ptr, flags);
        SPI_DMA_MTX_UNLOCK(&spidma->lock);
    }

    /* FALLTHROUGH */
error:
    return retval;
}




/*FUNCTION****************************************************************
*
* Function Name    : _dspi_dma_rx
* Returned Value   : Returns the number of bytes received
* Comments         :
*   Reads the data into provided array.
*
*END*********************************************************************/
static int_32 _dspi_dma_rx
  (
    /* [IN] The address of the device specific information */
    void                            *_io_dev_ptr,

    /* [IN] The array characters are to be read from */
    char_ptr                        buffer,

    /* [IN] Number of char's to transmit */
    int_32                          size
  )
{
    IO_SPI_POLLED_DEVICE_STRUCT_PTR io_dev_ptr = _io_dev_ptr;
    return _dspi_dma_xfer(io_dev_ptr->DEV_INFO_PTR, NULL, (uchar_ptr)buffer,
                          size);
}

/*FUNCTION****************************************************************
*
* Function Name    : _dspi_dma_tx
* Returned Value   : return number of byte transmitted
* Comments         :
*   Writes the provided data into trasmit buffer
*
*END*********************************************************************/
static int_32 _dspi_dma_tx
  (
    /* [IN] The address of the device specific information */
    void                            *_io_dev_ptr,

    /* [IN] The array to store data */
    char_ptr                        buffer,

    /* [IN] Number of char's to transmit */
    int_32                          size
  )
{
    IO_SPI_POLLED_DEVICE_STRUCT_PTR io_dev_ptr = _io_dev_ptr;
    return _dspi_dma_xfer(io_dev_ptr->DEV_INFO_PTR, (uchar_ptr)buffer,
                          NULL, size);
}
/*FUNCTION****************************************************************
*
* Function Name    : _bsp_dspi_dma_setup
* Returned Value   : MQX error code
* Comments         :
*    Call this if the BSP does not install the dspin: devices for you.
*
* NOTE: This should be moved to the BSPs' init_bsp.c file.
*
*END*********************************************************************/
int
_bsp_dspi_dma_setup(void)
{
#if BSPCFG_ENABLE_SPI0
    _dspi_dma_install("dspi0:", &_bsp_dspi0_init);
#endif
#if BSPCFG_ENABLE_SPI1
    _dspi_dma_install("dspi1:", &_bsp_dspi1_init);
#endif
#if BSPCFG_ENABLE_SPI2
    _dspi_dma_install("dspi2:", &_bsp_dspi2_init);
#endif
    return 0;
}
