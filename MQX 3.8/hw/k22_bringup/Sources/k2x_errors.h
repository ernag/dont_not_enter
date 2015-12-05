////////////////////////////////////////////////////////////////////////////////
//! \addtogroup corona
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
//! \file corona_errors.h
//! \brief Global error definitions for Corona firmware.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
//!
//! \detail This is where all error definitions should be defined.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#ifndef CORONA_ERRORS_H_
#define CORONA_ERRORS_H_

////////////////////////////////////////////////////////////////////////////////
// Enums
////////////////////////////////////////////////////////////////////////////////
typedef enum k2x_error
{

	SUCCESS 	= 0,
	CC_ERROR_GENERIC = 1,
	CC_INVALID_CMD = 2,
	CC_NO_MAN = 3,
	CC_NOT_ENOUGH_ARGS = 4,
	CC_UNWRITABLE_GPIO = 5,
	CC_IRQ_UNSUPPORTED = 6,
	CC_I2C_UNINITIALIZED = 7,
	CC_ERR_ADC = 8,
	CC_ERR_INVALID_PWR_STATE = 9,
	CC_ERR_INVALID_WSON_CMD = 10,
	CC_ERR_SPI_RX_ERR = 11,
	CC_ERR_SPI_TX_ERR = 12,
	CC_ERR_MEM_ALLOC = 13,
	CC_ERR_WSON_OUTBOUNDS = 14,
	CC_ERR_WSON_GENERIC	= 15
	
}k2x_error_t, corona_err_t;


#endif /* CORONA_ERRORS_H_ */
////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
