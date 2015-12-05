/*
 * accdrv_spi_dma.h
 *  Header for the Custom Accelerometer Driver
 *  Created on: Mar 17, 2013
 *      Author: Ben
 */

#ifndef ACCDRV_SPI_DMA_H_
#define ACCDRV_SPI_DMA_H_

#include "app_errors.h" 

// STLIS3DH FIFO Watermark Threshold (# of samples in each transfer)  
#define SPIDMA_ACCEL_WTM_SIZE   (25)

// Note: changed FIFO threshold from 27 to 20 and # of buffers from 12 to 16
// Bytes per PB encode: 
// 20*3*8 = 480 payload bytes + 19 header&crc = 499 (just under 512 bytes is GOOD) 

#define SIZEOF_EACH_DMA_XFER    (SPIDMA_ACCEL_WTM_SIZE * STLIS3DH_BYTES_PER_FIFO_ENTRY)
#define DMA_BUFFER_SIZE         (SIZEOF_EACH_DMA_XFER+1)
#define PACKED_DMA_BUFFER_SIZE  (DMA_BUFFER_SIZE >> 1) // Downshift eliminates control byte

#define ACCEL_SPI_DEVICE    ((pointer)SPI1_BASE_PTR)

// SPI Chip Select #
#define ACCEL_CS    1

// Number of ticks to wait before timing out when waiting for a Watermark request
// TBD: this should be a function of samplerate and system tick time (assuming 10ms for now)
#define ACCDRV_DMA_WAIT_TIMEOUT_TICKS   (600)

// DMA base channel and interrupt vector (must match)
#define ACCEL_DMA_CHANBASE  0
#define ACCEL_DMA_VECTOR	INT_DMA0 
#define ACCEL_TRIGCH_VECTOR (ACCEL_DMA_VECTOR+3)

// Priority for the RXCH interrupt 
// Note: MQX assigns priority level 4 to DSPI interrupts
#define ACCEL_DMA_INT_LEVEL 4

// Priority for the TRIGCH interrupt (used for statistics)
#define ACCEL_TRIGCH_INT_LEVEL 3

// # of DMA channels used by the Accel. SPIDMA driver
#define ACCEL_SPI_NDMACH	4

// DMA channels
#define RXCH (ACCEL_DMA_CHANBASE + 0) // DMA RX channel: read from POPR, write to RAM
#define TXCH (ACCEL_DMA_CHANBASE + 1) // DMA TX channel: write to PUSHR from 32-bit buffer 
#define CPCH (ACCEL_DMA_CHANBASE + 2) // DMA CP channel: copy 8-bit RAM to 32-bit buffer (used for transmit only)

// From K60N512 RM, table 3-24 
#define ACCEL_SPI_RX_DMA_REQUEST    18
#define ACCEL_SPI_TX_DMA_REQUEST    19

// Make the priority for the Wake-up interrupt low compared to other hardware interrupts
// Note: the default BSP interrupt level for GPIO pins is 3
#define ACCEL_WAKEUP_INT_LEVEL  6

// Callback data types
typedef void (*accdrv_cbk_t)(void);

typedef enum
{
    ACCDRV_WAKEUP_PIN_CBK = 0,
    ACCDRV_DMA_BUFFER_RDY_CBK,
    ACCDRV_DMA_LAST_BUFFER_CBK,
    ACCDRV_NUM_CBK
} ACCDRV_CBK_ID_t;

typedef struct
{
    uint_32 WakeupInterrupts;
    uint_32 WatermarkRequests;
    uint_32 CompletedBuffers;
} ACCDRV_STATISTICS_t;

LWGPIO_VALUE accdrv_get_int1_level( void );
LWGPIO_VALUE accdrv_get_int2_level( void );
void         accdrv_spidma_init(void);
void         accdrv_spidma_set_baud( uint_32 baud );
err_t        accdrv_spidma_xfer( uchar_ptr txbuf, uchar_ptr rxbuf, int_32 _nbytes );
void         accdrv_spidma_read_fifo( uint8_t num_samples );
void         accdrv_start_bkgnd( void );
void         accdrv_stop_bkgnd( boolean force );
err_t        accdrv_cleanup( void );
err_t        accdrv_wait_for_done( void );
void         accdrv_reset_stats( void );
ACCDRV_STATISTICS_t * accdrv_get_stats( void );
err_t        accdrv_reg_cbk( ACCDRV_CBK_ID_t cbk_id, accdrv_cbk_t pCbk );
uint_8 *     accdrv_get_bufptr( void );
err_t        accdrv_pack_dmabuf( void );
void         accdrv_en_wtm_pin(boolean en_int);
void         accdrv_simulate_wakeup( void );
boolean      accdrv_bkgnd_is_running( void );
void         accdrv_wakeup_action( void );
void         accdrv_wtm_action( void );
void         accdrv_enable_spi( uint8_t en );
uint8_t      accdrv_spi_is_enabled( void );
void         accdrv_enable_wakeup( uint8_t en );
boolean      accdrv_motion_was_detected( void );
void         accdrv_deinit(void);

// TODO: remove all need for this:
void         accdrv_reset_wakeup( void );


// DEBUG__ display WTM and DMA times for debug
void accdrv_show_fifo_timestamps( void );


#endif /* ACCDRV_SPI_DMA_H_ */

/* EOF */
