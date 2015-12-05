/*
 * accel_local.c
 *  Test code for bringing up local SPI driver for Accelerometer
 *  
 */

#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include <string.h>
#include <mqx.h>
#include <bsp.h>
#include <spi.h>

#include "accel_hal.h"
#include "accdrv_spi_dma.h"

// -- Macros and Datatypes --

// -- Test 3 Parameters

// Samplerate
#define TEST3_SAMPLERATE        (100)

// Number of complete buffer fills to perform
#define TEST3_NUM_BUFFER_FILLS  (10)


// -- Private Prototypes --
static void    display_statistics( void );
static uint_8  stlis3dh_print_ctrl_regs( void );
static uint_8  check_and_clear_int1( char * test_id );
static uint_8  poll_for_watermark( void );
static uint_32 accdrv_unit_test(void);

// -- Private Functions  --

static void display_statistics( void )
{
    ACCDRV_STATISTICS_t *pstats = accdrv_get_stats();
    corona_print ("Displaying statistics:\n");
    corona_print ("Wake-up Interrupts:  %d\n", pstats->WakeupInterrupts );
    corona_print ("Watermark Requests:  %d\n", pstats->WatermarkRequests );
    corona_print ("Completed Buffers:   %d\n", pstats->CompletedBuffers );
//    corona_print ("Rx overflow:  %d\n", pstats->RxOverflows);
//    corona_print ("Tx underflow: %d\n", pstats->TxUnderflows);
    corona_print ("\n");
}

// Read LIS3DH register values and print to the console.
// Returns 0 on success, otherwise an errorcode 
static uint_8 stlis3dh_print_ctrl_regs( void )
{
    uint8_t data[9];
    
    corona_print("LIS3DH Control Registers\n");
    if ( accel_read_register(SPI_STLIS3DH_CTRL_REG1, &data[0], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_CTRL_REG1!\n");
       return 1;
    }
    corona_print("CTRL_REG1: 0x%x\n", data[0]);
    _time_delay(1);
    
    if ( accel_read_register(SPI_STLIS3DH_CTRL_REG2, &data[1], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_CTRL_REG2!\n");
       return 2;
    }
    corona_print("CTRL_REG2: 0x%x\n", data[1]);
    _time_delay(1);
    
    if ( accel_read_register(SPI_STLIS3DH_CTRL_REG3, &data[2], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_CTRL_REG3!\n");
       return 3;
    }
    corona_print("CTRL_REG3: 0x%x\n", data[2]);
    _time_delay(1);
    
    if ( accel_read_register(SPI_STLIS3DH_CTRL_REG4, &data[3], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_CTRL_REG4!\n");
       return 4;
    }
    corona_print("CTRL_REG4: 0x%x\n", data[3]);
    _time_delay(1);
    
    if ( accel_read_register(SPI_STLIS3DH_CTRL_REG5, &data[4], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_CTRL_REG5!\n");
       return 5;
    }
    corona_print("CTRL_REG5: 0x%x\n", data[4]);
    _time_delay(1);
    
    if ( accel_read_register(SPI_STLIS3DH_CTRL_REG6, &data[5], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_CTRL_REG6!\n");
       return 6;
    }
    corona_print("CTRL_REG6: 0x%x\n", data[5]);
    _time_delay(1);
    
    if ( accel_read_register(SPI_STLIS3DH_INT1_THS, &data[6], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_INT1_THS!\n");
       return 6;
    }
    corona_print("INT1_THS: 0x%x\n", data[6]);
    _time_delay(1);
    
    if ( accel_read_register(SPI_STLIS3DH_INT1_DUR, &data[7], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_INT1_DUR!\n");
       return 6;
    }
    corona_print("INT1_DUR: 0x%x\n", data[7]);
    _time_delay(1);
    
    if ( accel_read_register(SPI_STLIS3DH_INT1_CFG, &data[8], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_INT1_CFG!\n");
       return 6;
    }
    corona_print("INT1_CFG: 0x%x\n", data[8]);
    _time_delay(1);

    if ( accel_read_register(SPI_STLIS3DH_INT1_SRC, &data[9], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_INT1_SRC!\n");
       return 6;
    }
    corona_print("INT1_SRC: 0x%x\n", data[9]);
    
    // Read INT1_SRC again to see if first read cleared the IA bit
    if ( accel_read_register(SPI_STLIS3DH_INT1_SRC, &data[9], 1) )
    {
       corona_print("Error reading SPI_STLIS3DH_INT1_SRC!\n");
       return 6;
    }
    corona_print("INT1_SRC: 0x%x\n", data[9]);
    _time_delay(1);

#if 0
    // Now, read the same registers using a multi-byte read and check the values
    corona_print("Checking multi-read ")
    if ( accel_read_multi(SPI_STLIS3DH_CTRL_REG1, (uint_8 *) databuf, 6) )
    {
        return 7;
    }
    
    for ( i = 0; i < 6; i++ )
    {
        if ( databuf[1+i] != data[i] ) 
            corona_print("Error! Mismatch on register ##%d: 0x%x, 0x%x\n", (uint_8) data[i], databuf[i+1]);
        return 8;
    }
    fflush(stdout);
#endif
    
    return 0;
}
 
// Poll for Watermark = 1, then read out FIFO contents
static uint_8 poll_for_watermark( void )
{
    int_8 * pdatabuf = (int_8 *) accdrv_get_bufptr();
    uint_8 fifo_level;
    int16_t x, y, z;
    int i;
    int errorcode;
   
    if ( accdrv_get_int1_level() )
    {
        corona_print("INT1=1! Reading fifo..\n");
        
        if ( (errorcode = accel_read_fifo( ACCEL_FIFO_WTM_THRESHOLD )) != 0 ) 
        {
            corona_print("Error: timed out waiting for DMA to complete.\n");  
            return errorcode;
        }
        
        for (i = 0; i < ACCEL_FIFO_WTM_THRESHOLD;i++ )
        {
            // Data starts at databuf[1] which is X_L (databuf[0] contains SPI control byte)
            x = pdatabuf[i*6 + 1]; 
            y = pdatabuf[i*6 + 3];
            z = pdatabuf[i*6 + 5];
            corona_print("%hi, %hi, %hi\n", x, y, z);
        } 
    
        // Wait for pin to be deasserted..
        corona_print("Waiting for INT1 to deassert...\n");

        while ( accdrv_get_int1_level() )
           _time_delay(1);
        corona_print("INT1=0!\n");
  
    }
    
    
#if 0
    else
    {
        // Watermark Request not present: display FIFO level and status
        if ( accel_read_fifo_status( &fifo_level ) )
        {
            corona_print("Error reading FIFO status register.\n");
            return 1;
        }
        corona_print("Level=%d, Flags=%x\n", fifo_level & STLIS3DH_FIFO_LEVEL_MASK, (fifo_level & STLIS3DH_FIFO_FLAGS_MASK) );
    }
#endif
    
    
    return 0;
}

// Check to see if INT0 (watermark request) is low. If not, read FIFO to clear it.
static uint_8 check_and_clear_int1( char * test_id )
{
    uint_8 fifo_status;
    uint_8 tries = 3;
    
    do {
        // Read FIFO level to determine how many bytes we should read
        if ( accel_read_fifo_status( &fifo_status ) )
        {
            corona_print("Error reading FIFO status register.\n");
            return 1;
        }
        
        if ( accel_read_fifo( fifo_status & STLIS3DH_FIFO_LEVEL_MASK ) )
        {
            // tbd: return accel_read_fifo() error code (contains semaphore error status)
            return 2; // Error reading FIFO!
        }
        _time_delay(1);
        if ( accdrv_get_int1_level() )
        {
            return 3; // Error! INT1 didn't clear after reading FIFO
        }
    } while ( accdrv_get_int1_level() && --tries);
    
    if ( !tries )
        return 4;
    else
        return 0;
}

// Unit Test the ST LIS3DH Accelerometer Driver
// Returns 0 on success, otherwise an error code 
static uint_32 accdrv_unit_test(void)
{
	uint_8 whoami = 0, data;
//	int_8  x_l, x_h, y_l, y_h, z_l, z_h;
	//int_8 databuf[STLIS3DH_MAX_FIFO_ENTRIES*STLIS3DH_BYTES_PER_FIFO_ENTRY];
    uint_8 fifo_status;
	int16_t x, y, z;
	int loop, i, tries;
	uint_32 errorcode;
	ACCDRV_STATISTICS_t *pstats = accdrv_get_stats();
    uint_32 wakeup_count = pstats->WakeupInterrupts; 

  	accel_stlis3dh_init(); // Initialise the LIS3DH for run-time setting
	_time_delay(1);
	
    // print out default register settings
	stlis3dh_print_ctrl_regs();
    _time_delay(1); 

    // Enable FIFO streaming mode and set the watermark
    // Note on LIS3DH fifo mode operation:
    // - FIFO Streaming Mode: if the FIFO becomes full it switches from streaming mode to fifo mode 
    //   and stays there (requires CPU to restart)
    // - Streaming Mode: if FIFO becomes full old data is overwritten and FIFO keeps filling with data 
    // Which? Either one will work but Streaming mode is better for debug because FIFO keeps going after a breakpoint.
    // In either case, it would be good to generate an interrupt on the INT2 to wake the CPU if the FIFO ever experiences
    // an overrun. That means first CPU behavior on INT2 interrupt is probably to poll registers to find cause of INT2.
   // accel_config_fifo( STLIS3DH_FIFO_STREAMFIFO_MODE, 15 );
    accel_config_fifo( STLIS3DH_FIFO_STREAM_MODE, ACCEL_FIFO_WTM_THRESHOLD );
    _time_delay(1);
    
#if 0
    // Test 1: Simple DMA read test
    corona_print("Starting test 1..\n");
    fflush(stdout);
    accdrv_reset_stats();
    if ( accel_read_fifo(ACCEL_FIFO_WTM_THRESHOLD) )
    {
        corona_print("Error! Test 1 failed.\n");
        fflush(stdout);
        return 1;
    }
    corona_print("Test 1: SUCCESS!\n\n");
    fflush(stdout);
    _time_delay(1);
#endif // Test 1
    
    corona_print("Enabling WTM output on INT1.\n");
    // data = STLIS3DH_REG3_WTM_IRQ_EN | STLIS3DH_REG3_I1_AOI1_EN;
    data = STLIS3DH_REG3_WTM_IRQ_EN; // NOTE: turning on the AOI1 bit seems to have screwed up INT2.. issue with FIFO stopping (sem timeout)
    if ( accel_write_register(SPI_STLIS3DH_CTRL_REG3, &data, 1) )
    {
       corona_print("Error writing CTRL_REG3!\n");
       fflush(stdout);
       return 3;
    }
    
#if 0 
    // Test 2 - Test INT1 Watermark as a GPIO input (not as a DMA Request)
    corona_print("Starting test 2..\n\n");
    fflush(stdout);

    _time_delay(1);
    if ( check_and_clear_int1("T2") )
    {
        corona_print("T2: Error: check_and_clear_int1() failed!\n");
        fflush(stdout);
    }
    else
    {
        corona_print("T2: Ok, INT1=0.\n");
    }
    fflush(stdout);
    _time_delay(1);    
    
    // Test 2: Wait for WTM request to assert and read fifo contents, then
    // verify that WTM deasserts. 
    for (loop=0; loop<1;loop++)
    {
        // Now, wait for WTM on INT1 to assert...      
        tries = 10;
        while ( !accdrv_get_int1_level() )
        {
            _time_delay(20);
            if (!--tries)
            {
                // Timeout after waiting for >X seconds
                corona_print("Error! INT1 did not assert!\n");
                fflush(stdout);
                return 6;
            }
            else
            {
                corona_print("Waiting for INT1=1...\n");
                fflush(stdout);
            }
        }
        // Ok, INT1 asserted. Let's read the FIFO and see if it de-asserts
        corona_print("1: INT1=1. Reading FIFO to clear it..\n");
        if ( accel_read_fifo( ACCEL_FIFO_WTM_THRESHOLD ) )
        {
            corona_print("Error! accel_read_fifo() failed!\n");
            return 7;
        }
        _time_delay(1);
        if ( accdrv_get_int1_level() )
        {
            corona_print("Error. INT1 didn't clear!.\n");
            fflush(stdout);
            return 8;
        }
        corona_print("1: INT1=0.\n");
        fflush(stdout);
    }
    corona_print("\nTest 2 SUCCESS!\n\n");
    fflush(stdout);    
    
    display_statistics();
#endif // test 2
// --- End Test 2 ---
     
    // Test 3: INT1 Watermark DMA Request and INT2 Wakeup Interrupt 
    corona_print("\nTest 3 (about %d seconds to complete)..\n\n", (1*TEST3_NUM_BUFFER_FILLS*ACCEL_FIFO_WTM_THRESHOLD)/(TEST3_SAMPLERATE));
    fflush(stdout);     
    // Set the sample rate for the test
    accel_set_samplerate( TEST3_SAMPLERATE );
    _time_delay(1);
    
    if ( check_and_clear_int1("T3") )
    {
        corona_print("T3: Error: check_and_clear_int1() failed.\n");
        return 8;
    }
    // Now, read and display fifo level. It should be near 0
    // Display FIFO level and status
    if ( accel_read_fifo_status( &fifo_status ) )
    {
        corona_print("Error reading FIFO status register.\n");
        return 9;
    }
    corona_print("T3: Fifo WTM=%d, OVRN=%d, Level= %d\n", fifo_status & STLIS3DH_FIFO_WTM_FLAG,
                                                fifo_status & STLIS3DH_FIFO_OVERRUN_FLAG,
                                                fifo_status & STLIS3DH_FIFO_LEVEL_MASK ); 
    accdrv_reset_stats();
              
    // Arm the driver to trigger on each Watermark request and read LIS3DH FIFO data
    accdrv_start_bkgnd();
    
    // Each loop completes 1/2 buffer fill 
    for (loop=0;loop < 2*TEST3_NUM_BUFFER_FILLS - 1; loop++ )
    {     
        // Wait for the DMA transfer to complete, then clean up
        if ( (errorcode = accdrv_wait_for_done()) == 0 )
        {      
#if 0
            if ( accdrv_get_int1_level() )
            {
                corona_print("T3:Err:INT1=1!\n");
                
                // Display FIFO level and status
                if ( accel_read_fifo_status( &fifo_status ) )
                {
                    corona_print("Error reading FIFO status register.\n");
                    return 1;
                }
                corona_print("T3: Fifo Level=%d\n", fifo_status & STLIS3DH_FIFO_LEVEL_MASK);                                                                                                                      
                return 10;
            }
#endif
            corona_print("T3:curptr=%p\n", accdrv_get_bufptr() );
        }
        else 
        {
            corona_print("T3:SemError! %x INT1=%d\n", errorcode, accdrv_get_int1_level() );  
            return 11;
        }
        _time_delay(1); // this is OK
    }
    
    // Wait for the last transfer
    accdrv_stop_bkgnd( FALSE );
 
// 3-APR-2013: this hardware test is no longer in sync with the driver - needs to be rewritten
    
    corona_print("T3:done:curptr=%p\n", accdrv_get_bufptr() );
				
// With fflush(): here: fifo level is 5   
#ifdef TEST3_DISPLAY        
	    corona_print("\nT3:P:%d\n", loop + 1);
#endif
// note: doing fflush() here won't work - can't get back to arm the DMA in time
	    //     fflush(stdout);     
// With fflush(): here fifo level is 31     		
    
    corona_print("\nTest 3 SUCCESS!\n\n");
		
	return 0;
}

// -- Public Functions --

// Toplevel for the Accelerometer SPIDMA driver test
// Return: None
void accel_spidma_test(void)
{
   uint_32                i=0, result;
   uint_32                param;
   SPI_READ_WRITE_STRUCT  rw;

   corona_print ("\n** Accelerometer Driver Unit Test **\n\n");
   
   // done in ACCMGR_init()
   accdrv_spidma_init(); // init our local driver
   
   result = accdrv_unit_test();   // run driver unit test
   if ( result )
   {
       corona_print("\nERROR! Unit test errorcode = %x\n\n", result );
   }
   display_statistics(); // display driver statistics
   
   corona_print("\nTest is done, INFINITE LOOPING");
   fflush(stdout);
   while(1){}

}

#endif  // ENABLE_WHISTLE_UNIT_TESTS

/* EOF */

