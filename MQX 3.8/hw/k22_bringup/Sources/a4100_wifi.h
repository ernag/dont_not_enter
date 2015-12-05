/*
 * a4100_wifi.h
 *
 *  Created on: Jan 14, 2013
 *      Author: Ernie Aguilar
 */

#ifndef A4100_WIFI_H_
#define A4100_WIFI_H_

#include "PE_Types.h"
#include "k2x_errors.h"

// SPI Command Types for the AR4100
#define A4100_REG_READ_MASK			0xC000
#define A4100_REG_WRITE_MASK		0x4000
#define A4100_MAILBOX_READ_MASK		0x8000
#define A4100_MAILBOX_WRITE_MASK	0x0000

// SPI Register Offsets
#define A4100_SPI_CFG				0x0400

corona_err_t a4100_wifi_test(void);

#endif /* A4100_WIFI_H_ */
