/*
 * lis3dh.h
 * Driver for the ST Micro LIS3DH Accelerometer hooked up to SPI1 PORTB of K22.
 *
 *  Created on: Jan 10, 2013
 *      Author: Ernie Aguilar
 */

#ifndef LIS3DH_H_
#define LIS3DH_H_

#include "PE_Types.h"
#include "k2x_errors.h"

// MSBit determines read or write to the accelerometer
#define SPI_STLIS3DH_READ_BIT			0x80
#define SPI_STLIS3DH_WRITE_BIT			0x00

// ST Micro Accel Commands
#define SPI_STLIS3DH_WHO_AM_I			0x0F  // We expect the LIS3DH to be CHIP_ID = 0x33.
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


k2x_error_t lis3dh_accel_test(void);

#endif /* LIS3DH_H_ */
