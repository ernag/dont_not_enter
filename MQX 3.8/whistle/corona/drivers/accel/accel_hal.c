/*
 * accel_hal.c
 *  ST LIS3DH Accelerometer Hardware Abstraction Layer
 *  
 */


#include <string.h>
#include <mqx.h>
#include <bsp.h>
#include <spi.h>
#include "app_errors.h"
#include "accel_hal.h"
#include "accdrv_spi_dma.h" 
#include "cfg_mgr.h"
#include "wassert.h"

// -- Compile Switches --

// Define this to turn on debug output
//#define ACCEL_DEBUG_FEATURES

#ifdef ACCEL_DEBUG_FEATURES
#define D(x) x
#else
#define D(x)
#endif

// -- Public Functions --

// Read the FIFO a single time using the same routine as the background mechanism 
// This API is provided primarily for testing.
err_t accel_read_fifo( uint8_t num_samples )
{
    err_t retval;
    accdrv_spidma_read_fifo( num_samples );
    retval = accdrv_wait_for_done();
    accdrv_cleanup();
    return retval;
}

// Read a LIS3DH register using the custom accelerometer driver.
//  Returns 0 on success, otherwise an errorcode
err_t accel_read_register (uint_8 cmd, uint_8 *pData, uint_16 numBytes)
{
    int i;
    err_t retval;
    uint_8 to_read[20];

    cmd |= SPI_LIS3DH_RW_BIT; // read commands have a '1' for MSBit.
    
    // For multiple byte reads, have the LIS3DH increment the address
    if (numBytes > 1) 
    {
        cmd |= SPI_LIS3DH_INC_BIT;  // TBD: try always setting this
    }
    
    // TBD: Place receive data at pData..
    to_read[0] = cmd;
    _task_stop_preemption();
    retval = accdrv_spidma_xfer( to_read, to_read, 1+numBytes );
    _task_start_preemption();

    if ( retval != ERR_OK)
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_READ_FAIL, NULL);
    }

    for (i=0;i<numBytes;i++)
    {
        pData[i] = to_read[1+i]; // upto 32-bit register reads are supported
    }

   return retval;
}

// Write a LIS3DH register using the custom Accelerometer SPI driver.
// Returns 0 on success, otherwise an errorcode 
err_t accel_write_register (uint_8 cmd, uint_8 *pData, uint_16 numBytes)
{
    err_t retval;  
    uint_8  to_send[5]; // supports upto 32-bit data
   
    if ( numBytes > 4 )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, NULL);
    }
   
    cmd &= ~SPI_LIS3DH_RW_BIT; // write commands have a '0' for MSBit, so clear that part of the command.
   
    // TBD: support multple byte write with inc'd address:
    // if (numBytes > 1) cmd |= SPI_LIS3DH_INC_BIT
   
    to_send[0] = cmd;
    to_send[1] = pData[0]; // NOTE: upto 32-bit registers are supported
    to_send[2] = pData[1];
    to_send[3] = pData[2];
    to_send[4] = pData[3];
    _task_stop_preemption();
    retval = accdrv_spidma_xfer( to_send, NULL, 1+numBytes );
    _task_start_preemption();
   
    if ( retval != ERR_OK )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, NULL);
    } 
   
    return retval;
}

// Set the ST LIS3DH Accelerometer samplerate. The operating mode is
// left as configured during initialization
// Returns 0 on success, otherwise an errorcode
err_t accel_set_samplerate(  uint8_t rate )
{
    uint8_t regval = (STLIS3DH_REG1_LPEN | STLIS3DH_REG1_XYZEN);
    switch (rate)
    {
        case STLIS3DH_ODR_1HZ:
        case STLIS3DH_ODR_10HZ:
        case STLIS3DH_ODR_25HZ:
        case STLIS3DH_ODR_50HZ:
        case STLIS3DH_ODR_100HZ:
        case STLIS3DH_ODR_200HZ:
        case STLIS3DH_ODR_400HZ:
        case STLIS3DH_ODR_1500HZ:
            regval |= rate;
            break;
        default:
            return ERR_ACCDRV_INVALID_RATE;
    }
    if ( accel_write_register( SPI_STLIS3DH_CTRL_REG1, &regval, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg1");
    }
    return ERR_OK;
}


// Enable/disable the ST LIS3DH Accelerometer Watermark Interrupt
// Returns 0 on success, otherwise an errorcode
err_t accel_enable_wtm( boolean on )
{
    uint8_t regval = on ? (STLIS3DH_REG3_WTM_IRQ_EN) : 0;

    if ( accel_write_register( SPI_STLIS3DH_CTRL_REG3, &regval, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg3");
    }
    return ERR_OK;    
}

// Enable/disable the ST LIS3DH Accelerometer FIFO 
// Returns 0 on success, otherwise an errorcode
err_t accel_enable_fifo( boolean on, boolean latch )
{
    // If latch=1 then configure latching behavior for the Motion Detect interrupt 
    uint8_t regval = (on ? STLIS3DH_REG5_FIFO_EN : 0) | (latch ? STLIS3DH_REG5_LATCH_INT1 : 0);

    if ( accel_write_register( SPI_STLIS3DH_CTRL_REG5, &regval, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg5");
    }
    return ERR_OK;
}

// Set the ST LIS3DH Accelerometer FIFO mode
// Returns 0 on success, otherwise an errorcode
err_t accel_config_fifo( uint8_t mode, uint_8 watermark )
{
    uint8_t regval = STLIS3DH_FIFO_BYPASS_MODE;
    
#if 0 // fifo is already in bypass mode 
    // As per datasheet, place FIFO in bypass mode before switching to the new mode 
    if ( accel_write_register( SPI_STLIS3DH_FIFO_CFG, &regval, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg6");
    }
#endif

    // Set the new mode and watermark level
    // Subtracting 1 from the watermark makes the interrupt fire after exactly
    //  watermark samples are ready to be transfered out of the FIFO
    regval = (mode & STLIS3DH_FIFO_MODE_MASK) |
                ((watermark - 1) & STLIS3DH_FIFO_WATERMARK_MASK);
    if ( accel_write_register( SPI_STLIS3DH_FIFO_CFG, &regval, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg6");
    }
 
    return ERR_OK;
}

// Read the ST LIS3DH Accelerometer FIFO Status Register
// Returns 0 on success, otherwise an errorcode
err_t accel_read_fifo_status( uint8_t * fifo_status )
{
    return accel_read_register(SPI_STLIS3DH_FIFO_SRC, (uint8_t *) fifo_status, 1);
}

err_t accel_shut_down( void )
{
    uint8_t data;
    data = STLIS3DH_REG1_LPEN;
    if ( accel_write_register( SPI_STLIS3DH_CTRL_REG1, &data, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg1");
    }
#if 0 // REG4 doesn't need to be written for shut-down
    data = 0x00;
    if ( accel_write_register( SPI_STLIS3DH_CTRL_REG4, &data, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg4");
    }
#endif
    return ERR_OK;
}

err_t accel_start_up( void )
{
    uint8_t data;
    data = ( CFG_SAMPLERATE | STLIS3DH_REG1_LPEN | STLIS3DH_REG1_XYZEN ); 
    if ( accel_write_register( SPI_STLIS3DH_CTRL_REG1, &data, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg1");
    }
    return ERR_OK;
}

// Poll the INT1_SRC register, which is actually the status for INT2 (motion detect)
uint8_t accel_poll_int2_status( void )
{
    uint8_t status;
    wassert( accel_read_register(SPI_STLIS3DH_INT1_SRC, &status, 1) == 0);
    if ( status & STLIS3DH_INT1_SRC_IA_FLAG )
    {
        return 1;
    }
    return 0;
}

// Initialise Configuration Manager Parameters
err_t accel_apply_configs( void )
{
    uint8_t data;
    
    // Write CTRL_4 
    // FS bits 5:4: 00=2G, 01=4G, 10=8G, 11=16G
    data = (g_st_cfg.acc_st.full_scale  << STLIS3DH_REG4_FS_SHIFT); // BDU=0, BLE=0, FullScale=2G, HiRes=0 (low power), SelfTest disabled, SIM=0
    D(corona_print("CTRL_REG4=%x\n", data));
    if ( accel_write_register(SPI_STLIS3DH_CTRL_REG4, &data, 1) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg4");
    }
    _time_delay(1);

    // Write INT1_THS: Threshold for Motion Detect
    data = g_st_cfg.acc_st.any_motion_thresh & SPI_STLIS3DH_INT1_THS_MASK;
    D(corona_print("INT1_THS=%x\n", data));
    if ( accel_write_register(SPI_STLIS3DH_INT1_THS, &data, 1) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "int1_ths");
    }
    return ERR_OK;
}

// Initialise the ST LIS3DH Accelerometer 
// Returns 0 on success, otherwise an errorcode
err_t accel_init_regs( void )
{
    uint8_t data;

    // Write CTRL_1
    // ODR<3:0>=50Hz, LPEn=1, XYZEn=1
    data = ( CFG_SAMPLERATE | STLIS3DH_REG1_LPEN | STLIS3DH_REG1_XYZEN ); 
    D(corona_print("CTRL_REG1=%x\n", data));
    if ( accel_write_register( SPI_STLIS3DH_CTRL_REG1, &data, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg1");
    }
    _time_delay(1);
     
    // Write CTRL_2 
    // Not sure why we need to filter Interrupt 1 instead of 2 but it appears we do
    data = 0x01; // Normal HP (reset reading HP_RESET_FILTER) on Interrupt 1
//    data = 0x82; // Normal HP on Interrupt 2
    D(corona_print("CTRL_REG2=%x\n", data));
    if ( accel_write_register(SPI_STLIS3DH_CTRL_REG2, &data, 1) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg2");
    }
    _time_delay(1);
    
    // Write CTRL_3
    // data = STLIS3DH_REG3_I1_AOI1_EN; // Generate AOI interrupt 1 on INT1
    // ^^^^ note: don't turn on AOI1 bit. It causes issue where WTM on INT1 stops happening 
    data = 0;
    D(corona_print("CTRL_REG3=%x\n", data));
    if ( accel_write_register(SPI_STLIS3DH_CTRL_REG3, &data, 1) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg3");
    }
    _time_delay(1);

    // Write CTRL_5
    data = STLIS3DH_REG5_FIFO_EN; // FIFO enabled, LIR_INT1=0 (don't latch interrupt)
    D(corona_print("CTRL_REG5=%x\n", data));
    if ( accel_write_register(SPI_STLIS3DH_CTRL_REG5, &data, 1) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg5");
    }
    _time_delay(1);
    
    // Write CTRL_6
    data = STLIS3DH_REG6_I2_INT1; // Interrupts active-high, use INT2 for Interrupt generator 1    
    D(corona_print("CTRL_REG6=%x\n", data));
    if ( accel_write_register(SPI_STLIS3DH_CTRL_REG6, &data, 1) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "reg6");
    }
    _time_delay(1);

    // Write INT1_DUR
    data = CFG_DURATION; // 0x32 = 50LSBs. duration = 50LSBs * (1/50Hz) = 1s  
    D(corona_print("INT1_DUR=%x\n", data));
    if ( accel_write_register(SPI_STLIS3DH_INT1_DUR, &data, 1) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "int1 dur");
    }

    // Write INT1_CFG
    // Note: contents of this register is loaded at boot.
    // Datasheet: "Write operation at this address is possible only after system boot."
    //data = 0x95; // AOI=1 (use AND logic for interrupt), ZLIE/YLIE/XLIE=1 (gen. on low event)
    //data = 0x15;  // AOI=0 (use OR logic for interrupt), ZLIE/YLIE/XLIE=1 (gen. on low event)
    //data = 0x0A; // enable X and Y only (this is from the app note)
    data = 0x2A; // AOI=0 (use OR logic for interrupt), ZLIE/YLIE/XLIE=1 (gen. on high event)
    D(corona_print("INT1_CFG=%x\n", data));
    if ( accel_write_register(SPI_STLIS3DH_INT1_CFG, &data, 1) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "int1 cfg");
    }
    
    // Put the FIFO in bypass mode
    data = STLIS3DH_FIFO_BYPASS_MODE;
    if ( accel_write_register( SPI_STLIS3DH_FIFO_CFG, &data, 1 ) )
    {
        return PROCESS_NINFO(ERR_ACCDRV_REG_WRITE_FAIL, "fifo cfg");
    }
    
    // Apply Configuration Manager parameters.
    // - Full Scale setting
    // - Motion Detect Threshold setting
    accel_apply_configs();

    return ERR_OK;
}


