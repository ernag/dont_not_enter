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
* $FileName: bma280_int.c$
* $Version : 3.7.10.0$
* $Date    : Feb-7-2011$
*
* Comments:
*
*   This file contains the functions which write and read the SPI memories
*   using the SPI driver in interrupt mode.
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>
#include "bma280.h"

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : stlis3dh_print_ctrl_regs
* Comments  : Prints contents of all the control registers and returns.
* Return: 
*         0 for success, non-zero otherwise.           
*
*END*----------------------------------------------------------------------*/
uint_8 stlis3dh_print_ctrl_regs(MQX_FILE_PTR spifd)
{
	uint8_t data;
	
	printf("LIS3DH Control Registers\n");
	if ( bma280_read_register(spifd, SPI_STLIS3DH_CTRL_REG1, &data, 1) )
	{
	   printf("Error reading SPI_STLIS3DH_CTRL_REG1!\n");
	   return 1;
	}
	printf("CTRL_REG1: 0x%x\n", data);
	_time_delay(1);
	
	if ( bma280_read_register(spifd, SPI_STLIS3DH_CTRL_REG2, &data, 1) )
	{
	   printf("Error reading SPI_STLIS3DH_CTRL_REG2!\n");
	   return 1;
	}
	printf("CTRL_REG2: 0x%x\n", data);
	_time_delay(1);
	
	if ( bma280_read_register(spifd, SPI_STLIS3DH_CTRL_REG3, &data, 1) )
	{
	   printf("Error reading SPI_STLIS3DH_CTRL_REG3!\n");
	   return 1;
	}
	printf("CTRL_REG3: 0x%x\n", data);
	_time_delay(1);
	
	if ( bma280_read_register(spifd, SPI_STLIS3DH_CTRL_REG4, &data, 1) )
	{
	   printf("Error reading SPI_STLIS3DH_CTRL_REG4!\n");
	   return 1;
	}
	printf("CTRL_REG4: 0x%x\n", data);
	_time_delay(1);
	
	if ( bma280_read_register(spifd, SPI_STLIS3DH_CTRL_REG5, &data, 1) )
	{
	   printf("Error reading SPI_STLIS3DH_CTRL_REG5!\n");
	   return 1;
	}
	printf("CTRL_REG5: 0x%x\n", data);
	_time_delay(1);
	
	if ( bma280_read_register(spifd, SPI_STLIS3DH_CTRL_REG6, &data, 1) )
	{
	   printf("Error reading SPI_STLIS3DH_CTRL_REG6!\n");
	   return 1;
	}
	printf("CTRL_REG6: 0x%x\n", data);
	_time_delay(1);

	return 0;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : stlis3dh_enable_any_motion
* Comments  : Enables any motion interrupt on INT1 pad.
* Return: 
*         0 for success, non-zero otherwise.           
*
*END*----------------------------------------------------------------------*/
uint_8 stlis3dh_enable_any_motion(MQX_FILE_PTR spifd)
{
	uint_8 data;
	uint_8 dummy;
/*
	6.3.2 HP filter bypassed
	This paragraph provides a basic algorithm which shows the practical use of the inertial
	wake-up feature. In particular, with the code below, the device is configured to recognize
	when the absolute acceleration along either the X or Y axis exceeds a preset threshold (250
	mg used in the example). The event which triggers the interrupt is latched inside the device
	and its occurrence is signalled through the use of the INT1 pin.

	1 Write 57h into CTRL_REG1    // EA:  Going to write 0xAF, to get low power enable...
	// Turn on the sensor and enable X, Y, and Z // 1010 0111
	// ODR = 100 Hz
	2 Write 00h into CTRL_REG2 // High-pass filter disabled
	3 Write 40h into CTRL_REG3 // Interrupt driven to INT1 pad
	4 Write 88h into CTRL_REG4 // FS = 2 g, does [3] High Rez bit need to be '1' for Normal Mode?
	5 Write 08h into CTRL_REG5 // Interrupt latched
	6 Write10h into INT1_THS // Threshold = 250 mg
	7 Write 00h into INT1_DURATION // Duration = 0
	8 Write 0Ah into INT1_CFG // Enable XH and YH interrupt generation
	9 Poll INT1 pad; if INT1=0 then go to 8
	// Poll RDY/INT pin waiting for the
	// wake-up event
	10 Read INT1_SRC
	// Return the event that has triggered the
	// interrupt
	11 (Wake-up event has occurred; insert
	your code here)
	// Event handling
	12 Go to 8
	
>>> Sorry for the typos in AN3308. 
In Section 6.3.2 and 6.3.3, the first line of the sample code should be:
 “Write 0x57 into CTRL_REG1” for 100Hz ODR normal mode with X/Y/Z enabled. 
 The fourth line should be “Write 0x88 into CTRL_REG4” for +/-2g FS range 
 normal mode with HR and BDU bits enabled. If you want to use LIS3DH low power mode to save power, 
 then you can write 0x5F and 0x80 to CTRL_REG1 and CTRL_REG4 respectively. For wake-up detection, 
 you can lower down the ODR to 25Hz or lower to save more power.
*/
	data = 0x27; //0x57;
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG1, &data, 1) )
	{
	   printf("Error writing CTRL_REG1!\n");
	   return 1;
	}
	_time_delay(1);
	
	data = 0x01; //0x00;
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG2, &data, 1) )
	{
	   printf("Error writing CTRL_REG2!\n");
	   return 1;
	}
	_time_delay(1);
	
	data = 0x40;
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG3, &data, 1) )
	{
	   printf("Error writing CTRL_REG3!\n");
	   return 1;
	}
	_time_delay(1);
	
	data = 0x38;
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG4, &data, 1) )
	{
	   printf("Error writing CTRL_REG4!\n");
	   return 1;
	}
	_time_delay(1);
	
	data = 0x08; // 8 means LATCHED interrupt.
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG5, &data, 1) )
	{
	   printf("Error writing CTRL_REG5!\n");
	   return 1;
	}
	_time_delay(1);

	// TODO - Need to write something to CTRL_REG6 ?  (based on Section 3 of App Note)
	// TODO - Need to "write reference" (REGISTER 0x26)?  (based on Section 3 of App Note)

	data = 0x00;
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG6, &data, 1) )
	{
	   printf("Error writing CTRL_REG6!\n");
	   return 1;
	}
	_time_delay(1);
	
	data = 0x08;//0x10;
	if ( bma280_write_register(spifd, SPI_STLIS3DH_INT1_THS, &data, 1) )
	{
	   printf("Error writing INT1_THS!\n");
	   return 1;
	}
	_time_delay(1);
	
	data = 0x00;
	if ( bma280_write_register(spifd, SPI_STLIS3DH_INT1_DUR, &data, 1) )
	{
	   printf("Error writing INT1_DURATION!\n");
	   return 1;
	}
	_time_delay(1);
	
	data = 0x2A; //0x95; //0xAA; //0x0A; // ZH, YH, or XH.
	if ( bma280_write_register(spifd, SPI_STLIS3DH_INT1_CFG, &data, 1) )
	{
	   printf("Error writing INT1_CFG!\n");
	   return 1;
	}
	_time_delay(1);
	
	// TODO - Need to write CTRL_REG5 AGAIN?  (based on Section 3 of App Note)
	/*
	data = 0x08;
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG5, &data, 1) )
	{
	   printf("Error writing CTRL_REG5!\n");
	   return 1;
	}
	_time_delay(1);
	*/
/*   ST Micro comment regarding the "HP_FILTER_RESET" part...
 * >>> You can ignore the eighth line “Read HP_FILTER_RESET”. This register is actually 
 *     the REFERENCE(26h) register which is used to shift the X/Y/Z measurements with the 
 *     value in this register.
 */
	
/*
	if ( bma280_read_register(spifd, SPI_STLIS3DH_REF_DATA_CAPTURE, &dummy, 1) )
	{
	   printf("Error reading SPI_STLIS3DH_WHO_AM_I!\n");
	   return 1;
	}
	_time_delay(1);
*/
	
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : stlis3dh_test
* Comments  : This function does unit testing on the ST Micro LIS3dh accelerometer.
* Return: 
*         0 for success, non-zero otherwise.           
*
*END*----------------------------------------------------------------------*/
uint_8  stlis3dh_test(MQX_FILE_PTR spifd)
{
	uint_8 whoami = 0, data;
	int_8  x_l, x_h, y_l, y_h, z_l, z_h;
	int16_t x, y, z;
	uint_32 num_chars;
	
	printf("\n\n      ** ST LIS3DH Accelerometer Test **\n\n");
	
	stlis3dh_init(spifd);
	
	//stlis3dh_enable_any_motion(spifd);
	stlis3dh_print_ctrl_regs(spifd);
	
	//while(1){} // Use this for "enable_any_motion"
	
	if ( bma280_read_register(spifd, SPI_STLIS3DH_WHO_AM_I, &whoami, 1) )
	{
	   printf("Error reading SPI_STLIS3DH_WHO_AM_I!\n");
	   return 1;
	}
	printf("Who Am I: 0x%x\n", whoami); // We expect the LIS3DH to be 0x33.
	//stlis3dh_print_ctrl_regs(spifd);
	
	while(1)
	{
		/*
		 *   TODO - Make a "read_axis" function like we have for BMA280.
		 */
		
		// X Axis
		if ( bma280_read_register(spifd, SPI_STLIS3DH_OUT_X_L, (uint8_t *)&x_l, 1) )
		{
		   printf("Error reading X AXIS!\n");
		   return 1;
		}
		if ( bma280_read_register(spifd, SPI_STLIS3DH_OUT_X_H, (uint8_t *)&x_h, 1) )
		{
		   printf("Error reading X AXIS!\n");
		   return 1;
		}
		//x = (x_h << 8) | x_l;
		x = x_h; // low power mode.
		
		// Y Axis
		if ( bma280_read_register(spifd, SPI_STLIS3DH_OUT_Y_L, (uint8_t *)&y_l, 1) )
		{
		   printf("Error reading Y AXIS!\n");
		   return 1;
		}
		if ( bma280_read_register(spifd, SPI_STLIS3DH_OUT_Y_H, (uint8_t *)&y_h, 1) )
		{
		   printf("Error reading Y AXIS!\n");
		   return 1;
		}
		//y = (y_h << 8) | y_l;
		y = y_h; // low power mode.
		
		// Z Axis
		if ( bma280_read_register(spifd, SPI_STLIS3DH_OUT_Z_L, (uint8_t *)&z_l, 1) )
		{
		   printf("Error reading Z AXIS!\n");
		   return 1;
		}
		if ( bma280_read_register(spifd, SPI_STLIS3DH_OUT_Z_H, (uint8_t *)&z_h, 1) )
		{
		   printf("Error reading Z AXIS!\n");
		   return 1;
		}
		//z = (z_h << 8) | z_l;
		z = z_h; // low power mode
		
/*
		if ( bma280_read_register(spifd, SPI_STLIS3DH_INT1_SRC, &data, 1) )
		{
		   printf("Error reading Z AXIS!\n");
		   return 1;
		}
*/
		
		/* Print everything out to console for user. */
		printf("  0x%x        %hi        %hi       %hi         ", whoami, x, y, z); // STREAMING MODE
		//printf("%u,%hi,%hi,%hi,\n", (time.SECONDS*1000) + time.MILLISECONDS, x_accel, y_accel, z_accel);   // CSV MODE
		
		
		/* Use backspace to re-adjust print output */

		num_chars = 100; // number of characters to backspace to give impression of non-moving accel data on console.
		while(num_chars-- > 0)
		{
		   printf("\b");
		}

		_time_delay(333);

	}
	
	return 0;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : stlis3dh_init
* Comments  : This function initializes the ST Micro LIS3dh accelerometer.
* Return: 
*         0 for success, non-zero otherwise.           
*
*END*----------------------------------------------------------------------*/
uint_8  stlis3dh_init(MQX_FILE_PTR spifd)
{
	uint8_t data;
	
	// 1. Write CTRL_1
	data = 0b01010111; // ODR<3:0>=100Hz, LPEn=0, XYZEn=1 === 0b1110 0111
	printf("Setting CTRL_REG1 to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG1, &data, 1) )
	{
	   printf("Error writing CTRL_REG1!\n");
	   return 1;
	}
	_time_delay(1);
	   
	// 2. Write CTRL_2
	
	data = 0b10000000; // HPM<1:0>= Normal Mode, HPF disabled
	printf("Setting CTRL_REG2 to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG2, &data, 1) )
	{
	   printf("Error writing CTRL_REG2!\n");
	   return 1;
	}
	_time_delay(1);
	
	// 3. Write CTRL_3
	data = 0x40; // I1_AOI1 = enabled, drive interrupt to INT1 pad.
	printf("Setting CTRL_REG3 to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG3, &data, 1) )
	{
	   printf("Error writing CTRL_REG3!\n");
	   return 1;
	}
	_time_delay(1);

	// 4. Write CTRL_4 
	data = 0x20; // BDU=0, BLE=0, FullScale=8G, HiRes=0 (low power), SelfTest disabled, SIM=0
	printf("Setting CTRL_REG4 to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG4, &data, 1) )
	{
	   printf("Error writing CTRL_REG4!\n");
	   return 1;
	}
	_time_delay(1);

	// 5. Write CTRL_5
	data = 0x08; // LIR_INT1 (latch interrupt, at least for now...)
	printf("Setting CTRL_REG5 to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG5, &data, 1) )
	{
	   printf("Error writing CTRL_REG5!\n");
	   return 1;
	}
	_time_delay(1);
	
	// 6. Write CTRL_6
	data = 0x00; // H_LACTIVE=0, so that we are active high
	printf("Setting CTRL_REG6 to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG6, &data, 1) )
	{
	   printf("Error writing CTRL_REG6!\n");
	   return 1;
	}
	_time_delay(1);
	
	// 7. Write Reference
	// TODO - What do I write into this register (SPI_STLIS3DH_REF_DATA_CAPTURE)?
	
	// 8. Write INT1_THS
	data = 0x07; // (INT1_THS / 128) * Full Scale (=8) ~~~ (7/128)*8=+/- 0.44 G
	printf("Setting INT1_Threshold to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_INT1_THS, &data, 1) )
	{
	   printf("Error writing INT1_THS!\n");
	   return 1;
	}
	_time_delay(1);
	
	// 9. Write INT1_DUR
	data = 1; // (1/100) * X=1 = 10 ms
	printf("Setting INT1_DURation to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_INT1_DUR, &data, 1) )
	{
	   printf("Error writing INT1_DUR!\n");
	   return 1;
	}
	_time_delay(1);
	
	// 10. Write INT1_CFG
	data = 0b10101010; // AOI=10=AND, ZH=1, ZL=0, YH=1, YL=0, ZH=1, ZL=0
	printf("Setting INT1_CFG to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_INT1_CFG, &data, 1) )
	{
	   printf("Error writing INT1_CFG!\n");
	   return 1;
	}
	_time_delay(1);
	
	// 11. Write CTRL_REG5
	data = 0x08; // LIR_INT1 (latch interrupt, at least for now...)
	printf("Setting CTRL_REG5 to 0x%x\n", data);
	if ( bma280_write_register(spifd, SPI_STLIS3DH_CTRL_REG5, &data, 1) )
	{
	   printf("Error writing CTRL_REG5!\n");
	   return 1;
	}
	_time_delay(1);

	return 0;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : bma280_test
* Comments  : This function does unit testing on the BMA280 accelerometer.
* Return: 
*         0 for success, non-zero otherwise.           
*
*END*----------------------------------------------------------------------*/
uint_8 bma280_test (MQX_FILE_PTR spifd)
{
   int_16 z_accel, x_accel, y_accel, num_chars;
   uint_8 data;
   uint_8 ret;
   uint_8 chip_id;
   uint_8 range;
   TIME_STRUCT time;
   
   printf("\n\n      ** BMA280 Accelerometer Test **\n\n");
   
   if ( bma280_read_register(spifd, SPI_BMA280_ACC_MEAS_RANGE, &data, 1) )
   {
	   printf("Error reading SPI_BMA280_ACC_MEAS_RANGE!\n");
	   return 1;
   }
   
   _time_delay(10);
   
   range = data & BMA280_RANGE_MASK;
   printf("Default G Range: 0x%x\n", range);

   range = BMA280_RANGE_16G;
   printf("Setting range to: 0x%x\n", range);
   if ( bma280_write_register(spifd, SPI_BMA280_ACC_MEAS_RANGE, &range, 1) )
   {
	   printf("Error writing SPI_BMA280_ACC_MEAS_RANGE!\n");
	   return 1;
   }
   _time_delay(1);
   
   printf("Reading back range to see if I wrote it properly.\n");
   if ( bma280_read_register(spifd, SPI_BMA280_ACC_MEAS_RANGE, &data, 1) )
   {
	   printf("Error reading SPI_BMA280_ACC_MEAS_RANGE!\n");
	   return 1;
   }
   
   printf("Range is now: 0x%x\n", data & BMA280_RANGE_MASK);
   if ( range != (data & BMA280_RANGE_MASK) )
   {
	   printf("ERROR: Ranges don't match!\n");
   }
   
   if ( bma280_read_register(spifd, SPI_BMA280_PMU_BW, &data, 1) )
   {
	   printf("Error reading SPI_BMA280_ACC_MEAS_RANGE!\n");
	   return 1;
   }
   data &= BMA280_BW_MASK;
   printf("Default Bandwidth: 0x%x\n", data);
   _time_delay(1);
   
   data = BMA280_UNFILTERED;
   printf("Setting Bandwidth to: 0x%x\n", data);
   if ( bma280_write_register(spifd, SPI_BMA280_PMU_BW, &data, 1) )
   {
	   printf("Error writing SPI_BMA280_ACC_MEAS_RANGE!\n");
	   return 1;
   }
   _time_delay(1);
   
   printf("Reading back bandwidth to see if I wrote it properly.\n");
   if ( bma280_read_register(spifd, SPI_BMA280_PMU_BW, &data, 1) )
   {
	   printf("Error reading SPI_BMA280_ACC_MEAS_RANGE!\n");
	   return 1;
   }
   data &= BMA280_BW_MASK;
   printf("Bandwidth: 0x%x\n", data);
   
   printf("Baselining the accelerometer offsets\n");
   if ( bma280_read_register(spifd, SPI_BMA280_OFC_CTRL, &data, 1) )
   {
	   printf("Error reading SPI_BMA280_OFC_CTRL!\n");
	   return 1;
   }
   _time_delay(1);
   printf("Default value for SPI_BMA280_OFC_CTRL is: 0x%x\n", data);
   printf("Clearing the offset_reset bit to reset all offsets.\n");
   data |= OFC_RESET_ALL_MASK;
   
   if ( bma280_write_register(spifd, SPI_BMA280_OFC_CTRL, &data, 1) )
   {
	   printf("Error writing SPI_BMA280_OFC_CTRL!\n");
	   return 1;
   }
   
   printf("\n CHIP ID        X           Y          Z\n");

   /*  Application loops forever, printing ACCEL DATA to screen   */
   
   while(1)
   {
	   x_accel = 0;
	   y_accel = 0;
	   z_accel = 0;
	   
	   _time_get(&time);
	   
	   /*    CHIP  ID    */
	   
/*
	   ret = bma280_read_register(spifd, SPI_BMA280_CHIP_ID, &data, 1);
	   if ( (ret) || (data != 0xFB) ) // Chip ID is always 0xFB for the BMA280.
	   {
		   printf("Error reading SPI_BMA280_CHIP_ID! Read 0x%x, Expected 0xFB\n", data);
		   return 1;
	   }
	   chip_id = data;
*/
	   
	   /*    X AXIS    */
	   
	   if ( bma280_read_axis(spifd, ACCEL_AXIS_X, &x_accel, BMA280_FRESH_DATA_TIMEOUT_MS) )
	   {
		   printf("ERROR READING X AXIS!\n");
		   return 1;
	   }
	   
	   /*    Y AXIS    */
	   
	   if ( bma280_read_axis(spifd, ACCEL_AXIS_Y, &y_accel, BMA280_FRESH_DATA_TIMEOUT_MS) )
	   {
		   printf("ERROR READING Y AXIS!\n");
		   return 1;
	   }
	   
	   /*    Z AXIS    */
	   
	   if ( bma280_read_axis(spifd, ACCEL_AXIS_Z, &z_accel, BMA280_FRESH_DATA_TIMEOUT_MS) )
	   {
		   printf("ERROR READING Z AXIS!\n");
		   return 1;
	   }
	   
	   /* Print everything out to console for user. */
	   //printf("  0x%x        %hi        %hi       %hi         ", chip_id, x_accel, y_accel, z_accel); // STREAMING MODE
	   printf("%u,%hi,%hi,%hi,\n", (time.SECONDS*1000) + time.MILLISECONDS, x_accel, y_accel, z_accel);   // CSV MODE
	   
	   
	   /* Use backspace to re-adjust print output */

	   /*
	   num_chars = 100; // number of characters to backspace to give impression of non-moving accel data on console.
	   while(num_chars-- > 0)
	   {
		   printf("\b");
	   }
	   */

	   //_time_delay(333);
   }
   return 0;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : bma280_read_axis
* Comments  : This function reads a 13-bit BMA280 axis value.
* Return: 
*         Status read (0 success).         
*
*END*----------------------------------------------------------------------*/
uint_8 bma280_read_axis(MQX_FILE_PTR spifd, accel_axis_t axis, int16_t *pValue, uint16_t timeout_ms)
{
	uint8_t cmd_lsb;
	uint8_t cmd_msb;
	uint8_t data_lsb = 0, data_msb = 0;
	
	*pValue = 0;
	
	switch(axis)
	{
		case ACCEL_AXIS_X:
			cmd_lsb = SPI_BMA280_ACC_X_LSB;
			cmd_msb = SPI_BMA280_ACC_X_MSB;
			break;
			
		case ACCEL_AXIS_Y:
			cmd_lsb = SPI_BMA280_ACC_Y_LSB;
			cmd_msb = SPI_BMA280_ACC_Y_MSB;
			break;
			
		case ACCEL_AXIS_Z:
			cmd_lsb = SPI_BMA280_ACC_Z_LSB;
			cmd_msb = SPI_BMA280_ACC_Z_MSB;
			break;
			
		default:
			printf("ERROR: Unsupported Axis!\n");
			return 2;
	}
	
	/* wait for fresh data and make sure we haven't timed out. */
	
	while( ((data_lsb & BMA280_FRESH_DATA) == 0) && (timeout_ms != 0) )
	{	
		if ( bma280_read_register(spifd, cmd_lsb, &data_lsb, 1) )
		{
		   printf("Error reading Axis %i LSB!\n", axis);
		   return 1;
		}
		*pValue = (int_16)(data_lsb >> 2); // last 2 bits of LSB is a DON'T-CARE and a FRESH-DATA (discard)
		
		if ( bma280_read_register(spifd, cmd_msb, &data_msb, 1) )
		{
			printf("Error reading Axis %i MSB!\n", axis);
		   return 1;
		}
		
		/* convert 14-bit 2's complement to 16-bit 2's complement */
		
		*pValue |= (data_msb << 6);
		*pValue = (*pValue ^ NEG_14_SIGN_MASK) - NEG_14_SIGN_MASK;
		
		timeout_ms--;
		_time_delay(1); // loop 1ms at a time, make sure we never timeout.
	}
	
	if ( data_lsb & BMA280_FRESH_DATA )
	{
		return 0;
	}
	return 3; // handle case where timeout was 0 and we never got real data.
}


/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : bma280_read_register
* Comments  : This function reads a BMA280 register.
* Return: 
*         Status read (0 success).         
*
*END*----------------------------------------------------------------------*/
uint_8 bma280_read_register (MQX_FILE_PTR spifd, uint_8 cmd, uint_8 *pData, uint_16 numBytes)
{
   uint_32 result;
   
   cmd |= SPI_BMA280_READ_BIT; // read commands have a '1' for MSBit.
	
   do 
   {
	 result = fwrite (&cmd, 1, 1, spifd);
   } while (result < 1);
   
   if (result != 1)
   {
	 /* Stop transfer */
	  printf ("ERROR Writing BMA280, wrote wrong number of bytes!\n");
	  return 1;
   }
   
   do 
   {
	  result = fread (pData, 1, numBytes, spifd);
   } while (result < 1);

   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   if (result != numBytes) 
   {
	  printf ("ERROR Reading BMA280, read wrong number of bytes!\n");
	  return 1;
   }
   
   return 0;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : bma280_writes_register
* Comments  : This function writes a BMA280 register.
* Return: 
*         Status read (0 success).         
*
*END*----------------------------------------------------------------------*/
uint_8 bma280_write_register (MQX_FILE_PTR spifd, uint_8 cmd, uint_8 *pData, uint_16 numBytes)
{
   uint_32 result;
   uint_8  to_send[2];
   
   cmd &= ~SPI_BMA280_READ_BIT; // write commands have a '0' for MSBit, so clear that part of the command.
   
   to_send[0] = cmd;
   to_send[1] = *pData; // TODO - Need to support writing more than once.
	
   do 
   {
	 result = fwrite (to_send, 1, 1+numBytes, spifd);
   } while (result < (1+numBytes));
   
   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);
   
   if ( result != (1+numBytes) )
   {
	 /* Stop transfer */
	  printf ("ERROR Writing BMA280, wrote wrong number of bytes!\n");
	  return 1;
   }
   
   return 0;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_chip_erase
* Comments  : This function erases the whole memory SPI chip
*
*END*----------------------------------------------------------------------*/
void memory_chip_erase_interrupt (MQX_FILE_PTR spifd)
{
   uint_32 result;
   
   /* This operation must be write-enabled */
   memory_set_write_latch_interrupt (spifd, TRUE);

   memory_read_status_interrupt (spifd);

   printf ("Erase whole memory chip:\n");
   send_buffer[0] = SPI_MEMORY_CHIP_ERASE;
     
   /* Write instruction */
   do 
   {
      result = fwrite (send_buffer, 1, 1, spifd);
   } while (result < 1);
   
   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   while (memory_read_status_interrupt (spifd) & 1)
   {
      _time_delay (1000);
   }

   printf ("Erase chip ... ");
   if (result != 1) 
   {
      printf ("ERROR\n");
   }
   else
   {
      printf ("OK\n");
   }
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_set_write_latch_interrupt
* Comments  : This function sets latch to enable/disable memory write 
*             operation
*
*END*----------------------------------------------------------------------*/
void memory_set_write_latch_interrupt (MQX_FILE_PTR spifd, boolean enable)
{
   uint_32 result;
   
   if (enable)
   {
      printf ("Enable write latch in memory ... ");
      send_buffer[0] = SPI_MEMORY_WRITE_LATCH_ENABLE;
   } else {
      printf ("Disable write latch in memory ... ");
      send_buffer[0] = SPI_MEMORY_WRITE_LATCH_DISABLE;
   }
     
   /* Write instruction */
   do 
   {
      result = fwrite (send_buffer, 1, 1, spifd);
   } while (result < 1);
   
   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   if (result != 1) 
   {
      printf ("ERROR\n");
   }
   else
   {
      printf ("OK\n");
   }
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_set_protection_interrupt
* Comments  : This function sets write protection in memory status register
*
*END*----------------------------------------------------------------------*/
void memory_set_protection_interrupt (MQX_FILE_PTR spifd, boolean protect)
{
   uint_32 result, i;
   uint_8 protection;
   
   /* Must do it twice to ensure right transitions in protection state */
   for (i = 0; i < 2; i++)
   {
      /* Each write operation must be enabled in memory */
      memory_set_write_latch_interrupt (spifd, TRUE);
   
      memory_read_status_interrupt (spifd);
   
      if (protect)
      {
         printf ("Write protect memory ... ");
         protection = 0xFF;
      } else {
         printf ("Write unprotect memory ... ");
         protection = 0x00;
      }
     
      send_buffer[0] = SPI_MEMORY_WRITE_STATUS;
      send_buffer[1] = protection;
   
      /* Write instruction */
      do 
      {
         result = fwrite (send_buffer, 1, 2, spifd);
      } while (result < 2);
   
      /* Wait till transfer end (and deactivate CS) */
      fflush (spifd);

      if (result != 2) 
      {
         printf ("ERROR\n");
      }
      else
      {
         printf ("OK\n");
      }
   }
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_read_status_interrupt
* Comments  : This function reads memory status register 
* Return: 
*         Status read.             
*
*END*----------------------------------------------------------------------*/
uint_8 memory_read_status_interrupt (MQX_FILE_PTR spifd)
{
   uint_32 result;
   uint_8 state = 0xFF;

   printf ("Read memory status ... ");

   send_buffer[0] = SPI_MEMORY_READ_STATUS;

   /* Write instruction */
   do 
   {
     result = fwrite (send_buffer, 1, 1, spifd);
   } while (result < 1);
   
   if (result != 1)
   {
     /* Stop transfer */
      printf ("ERROR (1)\n");
      return state;
   }
  
   /* Read memory status */
   do 
   {
      result = fread (&state, 1, 1, spifd);
   } while (result < 1);

   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   if (result != 1) 
   {
      printf ("ERROR (2)\n");
   }
   else
   {
      printf ("0x%02x\n", state);
   }
   
   return state;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_write_byte_interrupt
* Comments  : This function writes a data byte to memory  
*     
*
*END*----------------------------------------------------------------------*/
void memory_write_byte_interrupt (MQX_FILE_PTR spifd, uint_32 addr, uchar data)
{
   uint_32 result;

   /* Each write operation must be enabled in memory */
   memory_set_write_latch_interrupt (spifd, TRUE);

   memory_read_status_interrupt (spifd);

   send_buffer[0] = SPI_MEMORY_WRITE_DATA;                                               // Write instruction
   for (result = SPI_MEMORY_ADDRESS_BYTES; result != 0; result--)
   {
      send_buffer[result] = (addr >> ((SPI_MEMORY_ADDRESS_BYTES - result) << 3)) & 0xFF; // Address
   }
   send_buffer[1 + SPI_MEMORY_ADDRESS_BYTES] = data;                                     // Data
   
   printf ("Write byte to location 0x%08x in memory ... ", addr);
   
   /* Write instruction, address and byte */
   result = 0;
   do 
   {
      result += fwrite (send_buffer + result, 1, 1 + SPI_MEMORY_ADDRESS_BYTES + 1 - result, spifd);
   } while (result < 1 + SPI_MEMORY_ADDRESS_BYTES + 1);
   
   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   if (result != 1 + SPI_MEMORY_ADDRESS_BYTES + 1) 
   {
      printf ("ERROR\n");
   }
   else 
   {
      printf ("0x%02x\n", data);
   }
   
   /* There is 5 ms internal write cycle needed for memory */
   _time_delay (5);
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_read_byte_interrupt
* Comments  : This function reads a data byte from memory
* Return: 
*         Byte read.             
*
*END*----------------------------------------------------------------------*/
uint_8 memory_read_byte_interrupt (MQX_FILE_PTR spifd, uint_32 addr)
{
   uint_32 result;
   uint_8 data = 0;    
   
   send_buffer[0] = SPI_MEMORY_READ_DATA;                                                // Read instruction
   for (result = SPI_MEMORY_ADDRESS_BYTES; result != 0; result--)
   {
      send_buffer[result] = (addr >> ((SPI_MEMORY_ADDRESS_BYTES - result) << 3)) & 0xFF; // Address
   }

   printf ("Read byte from location 0x%08x in memory ... ", addr);

   /* Write instruction and address */
   result = 0;
   do 
   {
      result += fwrite (send_buffer + result, 1, 1 + SPI_MEMORY_ADDRESS_BYTES - result, spifd);
   } while (result < 1 + SPI_MEMORY_ADDRESS_BYTES);

   if (result != 1 + SPI_MEMORY_ADDRESS_BYTES)
   {
      /* Stop transfer */
      printf ("ERROR (1)\n");
      return data;
   }

   /* Read data from memory */
   do 
   {
      result = fread (&data, 1, 1, spifd);
   } while (result < 1);
   
   /* Wait till transfer end (and deactivate CS) */
   fflush (spifd);

   if (result != 1) 
   {
      printf ("ERROR (2)\n");
   }
   else
   {
      printf ("0x%02x\n", data);
   }
   
   return data;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_write_data_interrupt
* Comments  : This function writes data buffer to memory using page write
* Return: 
*         Number of bytes written.             
*
*END*----------------------------------------------------------------------*/
uint_32 memory_write_data_interrupt (MQX_FILE_PTR spifd, uint_32 addr, uint_32 size, uchar_ptr data)
{
   uint_32 i, len;
   uint_32 result = size;

   while (result > 0) 
   {
      /* Each write operation must be enabled in memory */
      memory_set_write_latch_interrupt (spifd, TRUE);

      memory_read_status_interrupt (spifd);

      len = result;
      if (len > SPI_MEMORY_PAGE_SIZE - (addr & (SPI_MEMORY_PAGE_SIZE - 1))) len = SPI_MEMORY_PAGE_SIZE - (addr & (SPI_MEMORY_PAGE_SIZE - 1));
      result -= len;

      printf ("Page write %d bytes to location 0x%08x in memory:\n", len, addr);

      send_buffer[0] = SPI_MEMORY_WRITE_DATA;                                     // Write instruction
      for (i = SPI_MEMORY_ADDRESS_BYTES; i != 0; i--)
      {
         send_buffer[i] = (addr >> ((SPI_MEMORY_ADDRESS_BYTES - i) << 3)) & 0xFF; // Address
      }
      for (i = 0; i < len; i++)
      {
         send_buffer[1 + SPI_MEMORY_ADDRESS_BYTES + i] = data[i];                 // Data
      }

      /* Write instruction, address and data */
      i = 0;
      do 
      {
         i += fwrite (send_buffer + i, 1, 1 + SPI_MEMORY_ADDRESS_BYTES + len - i, spifd);
      } while (i < 1 + SPI_MEMORY_ADDRESS_BYTES + len);
     
      /* Wait till transfer end (and deactivate CS) */
      fflush (spifd);

      if (i != 1 + SPI_MEMORY_ADDRESS_BYTES + len) 
      {
         printf ("ERROR\n"); 
         return size - result;
      }
      else
      {
         for (i = 0; i < len; i++)
            printf ("%c", data[i]);
         printf ("\n"); 
      }
     
      /* Move to next block */
      addr += len;
      data += len;

      /* There is 5 ms internal write cycle needed for memory */
      _time_delay (5);
   }
   
   return size;
}

/*FUNCTION*---------------------------------------------------------------
* 
* Function Name : memory_read_data_interrupt
* Comments  : This function reads data from memory into given buffer
* Return: 
*         Number of bytes read.             
*
*END*----------------------------------------------------------------------*/
uint_32 memory_read_data_interrupt (MQX_FILE_PTR spifd, uint_32 addr, uint_32 size, uchar_ptr data)
{
   uint_32 result, i, param;
   
   printf ("Reading %d bytes from location 0x%08x in memory: ", size, addr);

   send_buffer[0] = SPI_MEMORY_READ_DATA;                                                // Read instruction
   for (result = SPI_MEMORY_ADDRESS_BYTES; result != 0; result--)
   {
      send_buffer[result] = (addr >> ((SPI_MEMORY_ADDRESS_BYTES - result) << 3)) & 0xFF; // Address
   }

   /* Write instruction and address */
   result = 0;
   do 
   {
      result += fwrite (send_buffer + result, 1, 1 + SPI_MEMORY_ADDRESS_BYTES - result, spifd);
   } while (result < 1 + SPI_MEMORY_ADDRESS_BYTES);

   if (result != 1 + SPI_MEMORY_ADDRESS_BYTES)
   {
      /* Stop transfer */
      printf ("ERROR (1)\n");
      return 0;
   }

   /* Get actual flags */
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_GET_FLAGS, &param)) 
   {
      printf ("ERROR (2)\n");
      return 0;
   }

   /* Flush but do not de-assert CS */
   param |= SPI_FLAG_NO_DEASSERT_ON_FLUSH;
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_SET_FLAGS, &param)) 
   {
      printf ("ERROR (3)\n");
      return 0;
   }
   fflush (spifd);
  
   /* Read size bytes of data */
   result = 0;
   do 
   {
      result += fread (data + result, 1, size - result, spifd);
   } while (result < size);

   /* Wait till transfer end and de-assert CS */
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_FLUSH_DEASSERT_CS, &param)) 
   {
      printf ("ERROR (4)\n");
      return 0;
   }

   /* Get actual flags */
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_GET_FLAGS, &param)) 
   {
      printf ("ERROR (5)\n");
      return 0;
   }

   /* Revert opening flags */
   param &= (~ SPI_FLAG_NO_DEASSERT_ON_FLUSH);
   if (SPI_OK != ioctl (spifd, IO_IOCTL_SPI_SET_FLAGS, &param)) 
   {
      printf ("ERROR (6)\n");
      return 0;
   }

   if (result != size) 
   {
      printf ("ERROR (7)\n"); 
      return 0;
   } 
   else
   {
      printf ("\n"); 
      for (i = 0; i < result; i++)
         printf ("%c", data[i]);
      printf ("\n"); 
   }
   
   return result;
}

/* EOF */

