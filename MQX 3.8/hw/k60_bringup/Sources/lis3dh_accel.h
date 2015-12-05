////////////////////////////////////////////////////////////////////////////////
//! \addtogroup drivers
//! \ingroup corona
//! @{
//
// Copyright (c) 2013 Whistle Labs
//
// Whistle Labs
// Proprietary and Confidential
//
// This source code and the algorithms implemented therein constitute
// confidential information and may comprise trade secrets of Whistle Labs
// or its associates.
//
//! \file lis3dh_accel.h
//! \brief Accelerometer driver header.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
////////////////////////////////////////////////////////////////////////////////

#ifndef LIS3DH_H_
#define LIS3DH_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "PE_Types.h"
#include "corona_errors.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

// MSBit determines read or write to the accelerometer
#define SPI_STLIS3DH_READ_BIT			0x80
#define SPI_STLIS3DH_WRITE_BIT			0x00

// ST Micro Accel Commands
#define SPI_STLIS3DH_STATUS_AUX			0x07
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

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
corona_err_t lis3dh_accel_test(void);
corona_err_t lis3dh_comm_ok(void);
corona_err_t lis3dh_read_reg(const uint8_t reg, uint8_t *pVal);
corona_err_t lis3dh_write_reg(const uint8_t reg, const uint8_t val);
corona_err_t lis3dh_get_vals(int16_t *pX_axis, int16_t *pY_axis, int16_t *pZ_axis);

#endif /* LIS3DH_H_ */

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
