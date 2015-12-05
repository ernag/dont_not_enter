/*
 * apple_mfi.h
 *   Driver for controlling the Apple MFI Bluetooth chip via I2C
 *
 *  Created on: Jan 16, 2013
 *      Author: Ernie Aguilar
 */

#ifndef APPLE_MFI_H_
#define APPLE_MFI_H_

#include "PE_Types.h"
#include "k2x_errors.h"

// The Apple MFI chips have different slave _FINAL_ addresses depending on the circumstances, given below.
//               ***   FORMAT   ***
// A7     A6    A5    A4    A3    A2    A1    A0
// 0      0     1     0     0     0     RST   R/W
// RST = State of the MFI_RST_B pin.
// R/W = Reading or Writing
// NOTE:  The K22 I2C driver will add the R/W bit, so we will put the 7-bit address first in this format.
// XX     A6    A5    A4    A3    A2    A1    A0
// XX     0     0     1     0     0     0     RST   =  ( 0x10 | MFI_RST_B )

#define AAPL_MFI_ADDR_RST0	0x10 //0x20   // I2C SlaveAddress when ((MFI_RST_B == 0) && WRITING_TO_SLAVE)
#define AAPL_MFI_ADDR_RST1	0x11 //0x22   // I2C SlaveAddress when ((MFI_RST_B == 1) && WRITING_TO_SLAVE)

// MFI REGISTER DEFINITIONS
#define MFI_DEV_VERSION			0x00	// Dev Version should always be 0x05
#define MFI_FW_VERSION			0x01	// FW  Version should always be 0x01
#define MFI_PROCTL_MAJOR_VERSION	0x02 // value = 0x02
#define MFI_PROCTL_MINOR_VERSION	0x03 // value = 0x00
#define MFI_DEVICE_ID				0x04 // value = 0x200
#define MFI_ERR_CODE				0x05

#define MFI_SELFTEST_CTRL_STATUS	0x40


k2x_error_t apple_mfi_test(void);

#endif /* APPLE_MFI_H_ */
