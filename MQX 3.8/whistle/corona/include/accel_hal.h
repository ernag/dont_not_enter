/*
 * accel_hal.h
 *  Header for the ST LIS3DH Accelerometer Hardware Abstraction Layer
 *  Created on: Mar 17, 2013
 *      Author: ben
 */

#ifndef ACCEL_HAL_H_
#define ACCEL_HAL_H_
 
#include "accdrv_spi_dma.h"

// Configuration Parameters (TBD: move to RAM configuration)
// Both of the variables must be changed to update the sample rate
#define CFG_SAMPLERATE  STLIS3DH_ODR_50HZ
#define SAMPLERATE_IN_HZ            (50)

#define MILLISECONDS_PER_SECOND     (1000)
#define MILLISECONDS_PER_SAMPLE     (MILLISECONDS_PER_SECOND/SAMPLERATE_IN_HZ)


#define CFG_DURATION    (0x00)

// Old settings. Leaving this here until new cfg_mgr settings are verified
// NOTE: switched from 4G to 8G for cfg_mgr settings and therefore changed
// sensitivity from 0x10 to 0x08.
//#define CFG_FULLSCALE   (FULLSCALE_8G << STLIS3DH_REG4_FS_SHIFT)
//#define CFG_THRESHOLD   (0x08)

// Factor for converting % of full-scale to # of LSBs for threshold
#define FULL_SCALE_PLUSMINUS_BITS   (128)

#define ACCEL_FIFO_WTM_THRESHOLD        (SPIDMA_ACCEL_WTM_SIZE)

// LIS3DH SPI Control Byte
// Bit0: Read/Write bit      
//      When 0, the data SDI is written into the chip. 
//      When 1, the data SDO from the chip is read.
// Bit1: Increment Address bit    
//      When 0, the address is not incremented in multiple byte readings
//      When 1, the address is incremented in multiple byte readings.                  
#define SPI_LIS3DH_RW_BIT               0x80
#define SPI_LIS3DH_INC_BIT              0x40

// ST Micro Accel Commands
#define SPI_STLIS3DH_WHO_AM_I           0x0F
#define SPI_STLIS3DH_CTRL_REG1          0x20
#define SPI_STLIS3DH_CTRL_REG2          0x21
#define SPI_STLIS3DH_CTRL_REG3          0x22
#define SPI_STLIS3DH_CTRL_REG4          0x23
#define SPI_STLIS3DH_CTRL_REG5          0x24
#define SPI_STLIS3DH_CTRL_REG6          0x25
#define SPI_STLIS3DH_REF_DATA_CAPTURE   0x26 // AKA "HP_RESET_FILTER" in the app note.
#define SPI_STLIS3DH_OUT_X_L            0x28
#define SPI_STLIS3DH_OUT_X_H            0x29
#define SPI_STLIS3DH_OUT_Y_L            0x2A
#define SPI_STLIS3DH_OUT_Y_H            0x2B
#define SPI_STLIS3DH_OUT_Z_L            0x2C
#define SPI_STLIS3DH_OUT_Z_H            0x2D
#define SPI_STLIS3DH_FIFO_CFG           0x2E
#define SPI_STLIS3DH_FIFO_SRC           0x2F
#define SPI_STLIS3DH_INT1_CFG           0x30
#define SPI_STLIS3DH_INT1_SRC           0x31
#define SPI_STLIS3DH_INT1_THS           0x32
#define SPI_STLIS3DH_INT1_THS_MASK      ( (uint8_t)0x7F )
#define SPI_STLIS3DH_INT1_DUR           0x33

// Register 1
#define STLIS3DH_REG1_XEN               (1 << 0)
#define STLIS3DH_REG1_YEN               (1 << 1)
#define STLIS3DH_REG1_ZEN               (1 << 2)
#define STLIS3DH_REG1_XYZEN             (7)
#define STLIS3DH_REG1_LPEN              (1 << 3)
#define STLIS3DH_REG1_ODR_SHIFT         (4)
#define STLIS3DH_REG1_ODR_MASK          (0xf << STLIS3DH_REG1_ODR_SHIFT)

// Sample Rate (ODR)
#define STLIS3DH_ODR_1HZ                (1 << STLIS3DH_REG1_ODR_SHIFT)
#define STLIS3DH_ODR_10HZ               (2 << STLIS3DH_REG1_ODR_SHIFT)
#define STLIS3DH_ODR_25HZ               (3 << STLIS3DH_REG1_ODR_SHIFT)
#define STLIS3DH_ODR_50HZ               (4 << STLIS3DH_REG1_ODR_SHIFT)
#define STLIS3DH_ODR_100HZ              (5 << STLIS3DH_REG1_ODR_SHIFT)
#define STLIS3DH_ODR_200HZ              (6 << STLIS3DH_REG1_ODR_SHIFT)
#define STLIS3DH_ODR_400HZ              (7 << STLIS3DH_REG1_ODR_SHIFT)
#define STLIS3DH_ODR_1500HZ             (8 << STLIS3DH_REG1_ODR_SHIFT)
#define STLIS3DH_ODR_5KHZLP             (9 << STLIS3DH_REG1_ODR_SHIFT)

// Register 3
#define STLIS3DH_REG3_OVERRUN_FLAG      (1 << 1)
#define STLIS3DH_REG3_WTM_IRQ_EN        (1 << 2)
#define STLIS3DH_REG3_I1_DRDY2_EN       (1 << 3)
#define STLIS3DH_REG3_I1_DRDY1_EN       (1 << 4)
#define STLIS3DH_REG3_I1_AOI1_EN        (1 << 6)

// Register 4
#define STLIS3DH_REG4_SIM               (1 << 0)
#define STLIS3DH_REG4_HR                (1 << 3)
#define STLIS3DH_REG4_FS_SHIFT          (4)
#define STLIS3DH_REG4_FS_MASK           (0x03 << STLIS3DH_REG4_FS_SHIFT)
#define STLIS3DH_REG4_BLE               (1 << 6)
#define STLIS3DH_REG4_BDU               (1 << 7)

// Register 5
#define STLIS3DH_REG5_LATCH_INT1        (1 << 3)
#define STLIS3DH_REG5_FIFO_EN           (1 << 6)

// Register 6
#define STLIS3DH_REG6_I2_INT1           (1 << 6)

// FIFO Control Register
#define STLIS3DH_FIFO_MODE_MASK         (0x03 << 6)
#define STLIS3DH_FIFO_BYPASS_MODE       (0x00 << 6)
#define STLIS3DH_FIFO_BASIC_MODE        (0x01 << 6)
#define STLIS3DH_FIFO_STREAM_MODE       (0x02 << 6)
#define STLIS3DH_FIFO_STREAMFIFO_MODE   (0x03 << 6)
#define STLIS3DH_FIFO_WATERMARK_MASK    (0x1F)

// FIFO Source Register
#define STLIS3DH_FIFO_LEVEL_MASK        (0x1F)
#define STLIS3DH_FIFO_FLAGS_MASK        (0xE0)
#define STLIS3DH_FIFO_EMPTY_FLAG        (1 << 5)
#define STLIS3DH_FIFO_OVERRUN_FLAG      (1 << 6)
#define STLIS3DH_FIFO_WTM_FLAG          (1 << 7)

// INT1_SRC Register bit masks
#define STLIS3DH_INT1_SRC_IA_FLAG       (1 << 6)


// LIS3DH Miscellanious Parameters
#define STLIS3DH_MAX_FIFO_ENTRIES       (32)
#define STLIS3DH_BYTES_PER_FIFO_ENTRY   (6)   

err_t   accel_read_fifo( uint8_t num_samples );
err_t   accel_read_register (uint_8 cmd, uint_8 *pData, uint_16 numBytes);
err_t   accel_write_register (uint_8 cmd, uint_8 *pData, uint_16 numBytes);
err_t   accel_init_regs(void);
err_t   accel_apply_configs( void );
err_t   accel_set_samplerate(  uint8_t rate );
err_t   accel_enable_fifo( boolean on, boolean latch );
err_t   accel_config_fifo( uint8_t mode, uint_8 watermark );
err_t   accel_read_fifo_status( uint8_t * fifo_status );
err_t   accel_shut_down( void );
err_t   accel_start_up( void );
uint8_t accel_poll_int2_status( void );

#endif // ACCEL_HAL_H_
