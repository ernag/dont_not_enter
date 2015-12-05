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
//! \file lis3dh_accel.c
//! \brief Accelerometer driver.
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
//!
//! \detail This is the accel driver.  It handles talking to the accel and handling accel events.
//! \todo Accel interrupts.
//! \todo Accel low level SPI driver doesn't work properly, since CS doesn't stay asserted.  Need to fix.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "lis3dh_accel.h"
#include "SPI1_Accel.h"
#include "SPI_PDD.h"
#include "sleep_timer.h"
#include "pwr_mgr.h"
#include "corona_console.h"
extern cc_handle_t g_cc_handle;


////////////////////////////////////////////////////////////////////////////////
// Prototypes
////////////////////////////////////////////////////////////////////////////////
static corona_err_t lis3dh_init(void);

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
volatile uint8 g_SPI1_Accel_Wait_for_Send = TRUE;
volatile uint8 g_SPI1_Accel_Wait_for_Receive = TRUE;
static 	LDD_TDeviceData *masterDevData = NULL;

////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

corona_err_t lis3dh_comm_ok(void)
{
	LDD_TError err;
	uint8 tx[4];
	uint8 rx[4];
	uint32_t msTimeout = 1000;

	if( lis3dh_init() )
	{
		return CC_ACCEL_INIT;
	}

	tx[0] = SPI_STLIS3DH_READ_BIT | SPI_STLIS3DH_WHO_AM_I;
	memset(rx, 0, 4);

	err = SPI1_Accel_ReceiveBlock(masterDevData, rx, 2);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI2_Flash_ReceiveBlock error %x\n", err);
		return err;
	}

	err = SPI1_Accel_SendBlock(masterDevData, tx, 2);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
		return err;
	}

	while(g_SPI1_Accel_Wait_for_Send)
	{
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "Timeout waiting for Accel send!\n");
			return CC_ERROR_GENERIC;
		}
		sleep(1);
	}

	while(g_SPI1_Accel_Wait_for_Receive)
	{
		if(msTimeout-- == 0)
		{
			cc_print(&g_cc_handle, "Timeout waiting for Accel receive!\n");
			return CC_ERROR_GENERIC;
		}
		sleep(1);
	}
	g_SPI1_Accel_Wait_for_Send = TRUE;
	g_SPI1_Accel_Wait_for_Receive = TRUE;
	cc_print(&g_cc_handle, "rx[0]=0x%x\trx[1]=0x%x\n", rx[0], rx[1]); // 2nd byte should be 0x33, the Accel's identifier.

	if(rx[1] != 0x33)
	{
		cc_print(&g_cc_handle, "Accel ERROR:  Expected Identifier 0x33!\n");
		return CC_ERROR_GENERIC;
	}
	return SUCCESS;
}

static corona_err_t lis3dh_init(void)
{
	uint8_t data;

	if(!masterDevData)
	{
		masterDevData = SPI1_Accel_Init(NULL);
	}
	else
	{
		return SUCCESS; // assume device was already initialized.
	}

	// 1. Write CTRL_1
	data = 0b11100111; // ODR<3:0>=100Hz, LPEn=0, XYZEn=1 === 0b1110 0111
	//cc_print(&g_cc_handle, "Setting CTRL_REG1 to 0x%x\n", data);
	if ( lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG1, data) )
	{
	   cc_print(&g_cc_handle, "Error writing CTRL_REG1!\n");
	   return 1;
	}
	sleep(1);

#if 0
	// 2. Write CTRL_2
	data = 0b10000000; // HPM<1:0>= Normal Mode, HPF disabled
	//cc_print(&g_cc_handle, "Setting CTRL_REG2 to 0x%x\n", data);
	if (lis3dh_write_reg( SPI_STLIS3DH_CTRL_REG2, data) )
	{
	   cc_print(&g_cc_handle, "Error writing CTRL_REG2!\n");
	   return 1;
	}
	sleep(1);

	// 3. Write CTRL_3
	data = 0x00; // I1_AOI1 = enabled, drive interrupt to INT1 pad.
	//cc_print(&g_cc_handle, "Setting CTRL_REG3 to 0x%x\n", data);
	if ( lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG3, data) )
	{
	   cc_print(&g_cc_handle, "Error writing CTRL_REG3!\n");
	   return 1;
	}
	sleep(1);

	// 4. Write CTRL_4
	data = 0x28; // BDU=0, BLE=0, FullScale=8G, HiRes=0 (low power), SelfTest disabled, SIM=0
	//cc_print(&g_cc_handle, "Setting CTRL_REG4 to 0x%x\n", data);
	if ( lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG4, data) )
	{
	   cc_print(&g_cc_handle, "Error writing CTRL_REG4!\n");
	   return 1;
	}
	sleep(1);

	// 5. Write CTRL_5
	data = 0x08; // LIR_INT1 (latch interrupt, at least for now...)
	//cc_print(&g_cc_handle, "Setting CTRL_REG5 to 0x%x\n", data);
	if ( lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG5, data) )
	{
	   cc_print(&g_cc_handle, "Error writing CTRL_REG5!\n");
	   return 1;
	}
	sleep(1);

	// 6. Write CTRL_6
	data = 0x00; // H_LACTIVE=0, so that we are active high
	//cc_print(&g_cc_handle, "Setting CTRL_REG6 to 0x%x\n", data);
	if ( lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG6, data) )
	{
	   cc_print(&g_cc_handle, "Error writing CTRL_REG6!\n");
	   return 1;
	}
	sleep(1);

	// 7. Write Reference
	// TODO - What do I write into this register (SPI_STLIS3DH_REF_DATA_CAPTURE)?

	// 8. Write INT1_THS
	data = 0x07; // (INT1_THS / 128) * Full Scale (=8) ~~~ (7/128)*8=+/- 0.44 G
	//cc_print(&g_cc_handle, "Setting INT1_Threshold to 0x%x\n", data);
	if ( lis3dh_write_reg(SPI_STLIS3DH_INT1_THS, data) )
	{
	   cc_print(&g_cc_handle, "Error writing INT1_THS!\n");
	   return 1;
	}
	sleep(1);

	// 9. Write INT1_DUR
	data = 1; // (1/100) * X=1 = 10 ms
	//cc_print(&g_cc_handle, "Setting INT1_DURation to 0x%x\n", data);
	if ( lis3dh_write_reg(SPI_STLIS3DH_INT1_DUR, data) )
	{
	   cc_print(&g_cc_handle, "Error writing INT1_DUR!\n");
	   return 1;
	}
	sleep(1);

	// 10. Write INT1_CFG
	data = 0b10101010; // AOI=10=AND, ZH=1, ZL=0, YH=1, YL=0, ZH=1, ZL=0
	//cc_print(&g_cc_handle, "Setting INT1_CFG to 0x%x\n", data);
	if ( lis3dh_write_reg(SPI_STLIS3DH_INT1_CFG, data) )
	{
	   cc_print(&g_cc_handle, "Error writing INT1_CFG!\n");
	   return 1;
	}
	sleep(1);

	// 11. Write CTRL_REG5
	data = 0x08; // LIR_INT1 (latch interrupt, at least for now...)
	//cc_print(&g_cc_handle, "Setting CTRL_REG5 to 0x%x\n", data);
	if ( lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG5, data) )
	{
	   cc_print(&g_cc_handle, "Error writing CTRL_REG5!\n");
	   return 1;
	}
	sleep(1);
#endif
	return SUCCESS;
}

corona_err_t lis3dh_get_vals(int16_t *pX_axis, int16_t *pY_axis, int16_t *pZ_axis)
{
	uint8_t  x_l, x_h, y_l, y_h, z_l, z_h;

	if( lis3dh_init() )
	{
		return CC_ACCEL_INIT;
	}

	// X Axis
	if ( lis3dh_read_reg( SPI_STLIS3DH_OUT_X_L, &x_l) )
	{
	   cc_print(&g_cc_handle, "Error reading X AXIS!\n");
	   return CC_ACCEL_READ;
	}
	if ( lis3dh_read_reg( SPI_STLIS3DH_OUT_X_H, &x_h) )
	{
	   cc_print(&g_cc_handle, "Error reading X AXIS!\n");
	   return CC_ACCEL_READ;
	}
	*pX_axis = (x_h << 8) | x_l;
	//*pX_axis = x_h; // low power mode.
	//cc_print(&g_cc_handle, "X: 0x%x%x\n", x_h, x_l);

	// Y Axis
	if ( lis3dh_read_reg( SPI_STLIS3DH_OUT_Y_L, &y_l) )
	{
	   cc_print(&g_cc_handle, "Error reading Y AXIS!\n");
	   return CC_ACCEL_READ;
	}
	if ( lis3dh_read_reg( SPI_STLIS3DH_OUT_Y_H, &y_h) )
	{
	   cc_print(&g_cc_handle, "Error reading Y AXIS!\n");
	   return CC_ACCEL_READ;
	}
	*pY_axis = (y_h << 8) | y_l;
	//*pY_axis = y_h; // low power mode.
	//cc_print(&g_cc_handle, "y: 0x%x%x\n", y_h, y_l);

	// Z Axis
	if ( lis3dh_read_reg( SPI_STLIS3DH_OUT_Z_L, &z_l) )
	{
	   cc_print(&g_cc_handle, "Error reading Z AXIS!\n");
	   return CC_ACCEL_READ;
	}
	if ( lis3dh_read_reg( SPI_STLIS3DH_OUT_Z_H, &z_h) )
	{
	   cc_print(&g_cc_handle, "Error reading Z AXIS!\n");
	   return CC_ACCEL_READ;
	}
	*pZ_axis = (z_h << 8) | z_l;
	//*pZ_axis = z_h; // low power mode
	//cc_print(&g_cc_handle, "z: 0x%x%x\n", z_h, z_l);

	return SUCCESS;
}


corona_err_t lis3dh_read_reg(const uint8_t reg, uint8_t *pVal)
{
	LDD_TError err;
	uint8_t rx[2], tx[2];

	if( lis3dh_init() )
	{
		return CC_ACCEL_INIT;
	}

	tx[0] = reg | SPI_STLIS3DH_READ_BIT;

	err = SPI1_Accel_ReceiveBlock(masterDevData, rx, 2);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI2_Flash_ReceiveBlock error %x\n", err);
		return err;
	}

	err = SPI1_Accel_SendBlock(masterDevData, tx, 2);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
		return err;
	}
	while(g_SPI1_Accel_Wait_for_Send){/* sleep */}
	while(g_SPI1_Accel_Wait_for_Receive){/* sleep */}
	g_SPI1_Accel_Wait_for_Send = TRUE;
	g_SPI1_Accel_Wait_for_Receive = TRUE;

	*pVal = rx[1];
	return SUCCESS;
}

corona_err_t lis3dh_write_reg(const uint8_t reg, const uint8_t val)
{
	LDD_TError err;
	uint8_t tx[2];

	if( lis3dh_init() )
	{
		return CC_ACCEL_INIT;
	}

	tx[0] = reg;
	tx[1] = val;

	err = SPI1_Accel_SendBlock(masterDevData, tx, 2);
	if (err != ERR_OK) {
		cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
		return err;
	}
	while(g_SPI1_Accel_Wait_for_Send){/* sleep */}
	g_SPI1_Accel_Wait_for_Send = TRUE;

	return SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//! \brief Execute a very simple LIS3DH Accelerometer Test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function  runs an infinite loop, sending the WHO_AM_I command to ST Micro.
//!
//! \return corona_err_t error code
//!
///////////////////////////////////////////////////////////////////////////////
corona_err_t lis3dh_accel_test(void)
{

	LDD_TError err;
	uint8 tx[32];
	uint8 rx[32];

	cc_print(&g_cc_handle, "lis3dh_accel_test\n");

	if( lis3dh_init() )
	{
		return CC_ACCEL_INIT;
	}

	tx[0] = SPI_STLIS3DH_READ_BIT | SPI_STLIS3DH_WHO_AM_I;
	memset(rx, 0, 32);

	for(;;)
	{
		err = SPI1_Accel_ReceiveBlock(masterDevData, rx, 2);
		if (err != ERR_OK) {
			cc_print(&g_cc_handle, "SPI2_Flash_ReceiveBlock error %x\n", err);
			return err;
		}

		err = SPI1_Accel_SendBlock(masterDevData, tx, 2);
		if (err != ERR_OK) {
			cc_print(&g_cc_handle, "SPI2_Flash_SendBlock error %x\n", err);
			return err;
		}
		while(g_SPI1_Accel_Wait_for_Send){/* sleep */}
		while(g_SPI1_Accel_Wait_for_Receive){/* sleep */}
		g_SPI1_Accel_Wait_for_Send = TRUE;
		g_SPI1_Accel_Wait_for_Receive = TRUE;
		cc_print(&g_cc_handle, "rx[0]=0x%x\trx[1]=0x%x\n", rx[0], rx[1]); // 2nd byte should be 0x33, the Accel's identifier.
		//cc_print(&g_cc_handle, "tx[0]=0x%x\ttx[1]=0x%x\ttx[2]=0x%x\ttx[3]=0x%x\n", tx[0], tx[1], tx[2], tx[3]);
	}

	return SUCCESS;
}

/*
 *   Handle power management for the LIS3DH accelerometer.
 */
corona_err_t pwr_accel(pwr_state_t state)
{
	uint8_t value;

	switch(state)
	{
		case PWR_STATE_SHIP:
			// ODR=PowerDown, LPen=1, X/Y/Z disabled.
			lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG1, 0x08);
			lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG4, 0x00);

#ifdef CHECK_ACCEL_SHIP_MODE

			sleep(500);

			lis3dh_read_reg(SPI_STLIS3DH_CTRL_REG1, &value);
			cc_print(&g_cc_handle, "Accel CTRL_REG1 = 0x%x\n", value);

			lis3dh_read_reg(SPI_STLIS3DH_CTRL_REG2, &value);
			cc_print(&g_cc_handle, "Accel CTRL_REG2 = 0x%x\n", value);
#endif
			break;

		case PWR_STATE_STANDBY:
			lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG1, 0x2F);
			lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG4, 0x28);
			break;

		case PWR_STATE_NORMAL:
			lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG1, 0x47);
			lis3dh_write_reg(SPI_STLIS3DH_CTRL_REG4, 0x28);
			break;

		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
