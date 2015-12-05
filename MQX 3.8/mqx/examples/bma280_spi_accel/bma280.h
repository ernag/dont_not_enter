#ifndef __bma280_h__
#define __bma280_h__
/**HEADER********************************************************************
* 
* Copyright (c) WHISTLE
* All Rights Reserved                       
*
*************************************************************************** 
*
* THIS SOFTWARE IS PROVIDED BY WHISTLE (FREESCALE) "AS IS" AND ANY EXPRESSED OR 
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
* $FileName: bma280.h$
* $Version : 3.8.27.0$
* $Date    : Dec-08-2012$
*
* Comments:
*
*   This file contains definitions for the SPI bma280 example.
*
*END************************************************************************/

typedef enum accel_axis
{
	ACCEL_AXIS_X = 0,
	ACCEL_AXIS_Y = 1,
	ACCEL_AXIS_Z = 2
}accel_axis_t;

/* The SPI serial memory test addresses */
#define SPI_MEMORY_ADDR1               0x0000F0 /* test address 1 */
#define SPI_MEMORY_ADDR2               0x0001F0 /* test address 2 */
#define SPI_MEMORY_ADDR3               0x0002F0 /* test address 3 */

/* Number of bytes used for addressing within memory */
#define SPI_MEMORY_ADDRESS_BYTES       3        

/* Memory page size - maximum bytes per write */
#define SPI_MEMORY_PAGE_SIZE           0x0100   

/* The SPI memory instructions */
#define SPI_MEMORY_WRITE_STATUS        0x01
#define SPI_MEMORY_WRITE_DATA          0x02
#define SPI_MEMORY_READ_DATA           0x03
#define SPI_MEMORY_WRITE_LATCH_DISABLE 0x04
#define SPI_MEMORY_READ_STATUS         0x05
#define SPI_MEMORY_WRITE_LATCH_ENABLE  0x06
#define SPI_MEMORY_CHIP_ERASE          0xC7

/* The SPI BMA280 Registers */
// Bit0: Read/Write bit. When 0, the data SDI is written into the chip. 
//                       When 1, the data SDO from the chip is read.
#define SPI_BMA280_READ_BIT				0x80
#define SPI_BMA280_WRITE_BIT			0x00

#define NEG_16_SIGN_MASK				0x8000  // used to make a negative number out of a 13-bit number.
#define NEG_14_SIGN_MASK                0x2000  // used to remove sign bit from converted 16-bit number.
#define NEG_MSB_MASK					0x80	// used to determine if the MSB of 13-bit number would produce a negative result.
#define OFC_RESET_ALL_MASK				0x80    // used to reset all axis offset to 0.
#define BMA280_FRESH_DATA				0x01    // used to determine if data is fresh from accel.
#define BMA280_RANGE_MASK				0x0F	// used to determine which G range we are currently set to.
#define BMA280_UNFILTERED				0x0F
#define BMA280_BW_MASK					0x1F

#define BMA280_FRESH_DATA_TIMEOUT_MS	1000    // number of milliseconds to delay waiting for fresh data from accelerometer.

#define BMA280_MSB_IS_NEG(X)	(X & 0x80) ? (1) : (0) // used to determine if a number is negative coming from MSB reg.

#define BMA280_RANGE_2G			0x03
#define BMA280_RANGE_4G			0x05
#define BMA280_RANGE_8G			0x08
#define BMA280_RANGE_16G		0x0C

// Bit1-7: Address AD(6:0).  (These are the registers)
// Bit8-15: When in write mode, these are the data SDI, which will be written into the address.
//          When in read mode, these are the data SDO, which are read from the address.
#define SPI_BMA280_FIFO_OUTPUT			0x3F
#define SPI_BMA280_OFC_CTRL				0x36
#define SPI_BMA280_PMU_BW				0x10
#define SPI_BMA280_ACC_MEAS_RANGE		0x0F
#define SPI_BMA280_FIFO_FRAME_CNT		0x0E
#define SPI_BMA280_TEMPERATURE			0x08
#define SPI_BMA280_ACC_Z_MSB			0x07
#define SPI_BMA280_ACC_Z_LSB			0x06
#define SPI_BMA280_ACC_Y_MSB			0x05
#define SPI_BMA280_ACC_Y_LSB			0x04
#define SPI_BMA280_ACC_X_MSB			0x03
#define SPI_BMA280_ACC_X_LSB			0x02
#define SPI_BMA280_CHIP_ID				0x00

// ST Micro Accel Commands
#define SPI_STLIS3DH_WHO_AM_I			0x0F
#define SPI_STLIS3DH_CTRL_REG1			0x20
#define SPI_STLIS3DH_CTRL_REG2			0x21
#define SPI_STLIS3DH_CTRL_REG3			0x22
#define SPI_STLIS3DH_CTRL_REG4			0x23
#define SPI_STLIS3DH_CTRL_REG5			0x24
#define SPI_STLIS3DH_CTRL_REG6			0x25
#define SPI_STLIS3DH_REF_DATA_CAPTURE	0x26 // AKA "HP_RESET_FILTER" in the app note.
#define SPI_STLIS3DH_OUT_X_L			0x28
#define SPI_STLIS3DH_OUT_X_H			0x29
#define SPI_STLIS3DH_OUT_Y_L			0x2A
#define SPI_STLIS3DH_OUT_Y_H			0x2B
#define SPI_STLIS3DH_OUT_Z_L			0x2C
#define SPI_STLIS3DH_OUT_Z_H			0x2D
#define SPI_STLIS3DH_INT1_CFG			0x30
#define SPI_STLIS3DH_INT1_SRC			0x31
#define SPI_STLIS3DH_INT1_THS			0x32
#define SPI_STLIS3DH_INT1_DUR			0x33

/* Common data variables */
extern uchar   send_buffer[];
extern uchar   recv_buffer[];

/* Funtion prototypes */
extern uint_8  bma280_test (MQX_FILE_PTR spifd);
extern uint_8  bma280_read_register (MQX_FILE_PTR spifd, uint_8 cmd, uint_8 *pData, uint_16 numBytes);
extern uint_8  bma280_write_register (MQX_FILE_PTR spifd, uint_8 cmd, uint_8 *pData, uint_16 numBytes);
extern uint_8  bma280_read_axis(MQX_FILE_PTR spifd, accel_axis_t axis, int16_t *pValue, uint16_t timeout_ms);

extern uint_8  stlis3dh_test(MQX_FILE_PTR spifd);
extern uint_8  stlis3dh_init(MQX_FILE_PTR spifd);
extern uint_8  stlis3dh_enable_any_motion(MQX_FILE_PTR spifd);
extern uint_8  stlis3dh_print_ctrl_regs(MQX_FILE_PTR spifd);

extern void    set_CS (uint_32 cs_mask, uint_32 logic_level, pointer user_data);

extern void    memory_chip_erase (MQX_FILE_PTR spifd);
extern void    memory_set_write_latch (MQX_FILE_PTR spifd, boolean enable);
extern void    memory_set_protection (MQX_FILE_PTR spifd, boolean protect);
extern uint_8  memory_read_status (MQX_FILE_PTR spifd);
extern void    memory_write_byte (MQX_FILE_PTR spifd, uint_32 addr, uchar data);
extern uint_8  memory_read_byte (MQX_FILE_PTR spifd, uint_32 addr);
extern uint_32 memory_write_data (MQX_FILE_PTR spifd, uint_32 addr, uint_32 size, uchar_ptr data);
extern uint_32 memory_read_data (MQX_FILE_PTR spifd, uint_32 addr, uint_32 size, uchar_ptr data);

extern void    memory_chip_erase_interrupt (MQX_FILE_PTR spifd);
extern void    memory_set_write_latch_interrupt (MQX_FILE_PTR spifd, boolean enable);
extern void    memory_set_protection_interrupt (MQX_FILE_PTR spifd, boolean protect);
extern uint_8  memory_read_status_interrupt (MQX_FILE_PTR spifd);
extern void    memory_write_byte_interrupt (MQX_FILE_PTR spifd, uint_32 addr, uchar data);
extern uint_8  memory_read_byte_interrupt (MQX_FILE_PTR spifd, uint_32 addr);
extern uint_32 memory_write_data_interrupt (MQX_FILE_PTR spifd, uint_32 addr, uint_32 size, uchar_ptr data);
extern uint_32 memory_read_data_interrupt (MQX_FILE_PTR spifd, uint_32 addr, uint_32 size, uchar_ptr data);

#endif
/* EOF */
