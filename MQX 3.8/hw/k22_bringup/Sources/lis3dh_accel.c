/*
 * lis3dh_accel.c
 *
 *  Created on: Jan 10, 2013
 *      Author: Ernie Aguilar
 */

#include "lis3dh_accel.h"
#include "SPI1_Accel.h"
#include "SPI_PDD.h"
#include "sleep_timer.h"
#include "pwr_mgr.h"

volatile uint8 g_SPI1_Accel_Wait_for_Send = TRUE;
volatile uint8 g_SPI1_Accel_Wait_for_Receive = TRUE;

// This is a stupid test that runs an infinite loop, sending the WHO_AM_I command to ST Micro.  Then just use a scope to view 0x33 CHIP ID on MISO pin.
k2x_error_t lis3dh_accel_test(void)
{
	LDD_TDeviceData *masterDevData;
	LDD_TError err;
	
	unsigned int i = 0;
	uint8 tx[32];
	uint8 rx[32];
	uint32_t spi1_pushr = 0;
	
	printf("lis3dh_accel_test\n");
	
	masterDevData = SPI1_Accel_Init(NULL);
	printf("SPI1 Handle: 0x%x\n", masterDevData);
	
	
	
	tx[0] = SPI_STLIS3DH_READ_BIT | SPI_STLIS3DH_WHO_AM_I;
	tx[1] = SPI_STLIS3DH_READ_BIT | SPI_STLIS3DH_WHO_AM_I;
	rx[0] = 0;
	rx[1] = 0;
	rx[2] = 0;
	rx[3] = 0;
	
	// The ST Micro accelerometer is SPI_CLK_POL_PHA_MODE0
        //case (SPI_CLK_POL_PHA_MODE0):
            /* Inactive state of SPI_CLK = logic 0 */
            //dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPOL_MASK);
            /* SPI_CLK transitions middle of bit timing */
            //dspi_ptr->CTAR[0] &= (~ DSPI_CTAR_CPHA_MASK);
	
	for(;;)
	{
		int retries = 5;
		
		err = SPI1_Accel_SendBlock(masterDevData, tx, 2); // hacking things to send 2 write commands, so I can keep CS asserted, and read 0x33 chip ID on a scope.
		//printf("SPI1_Accel_SendBlock Err: 0x%x\n", err);
		
		// Keep CS asserted for the read.
		//SPI_PUSHR_REG(SPI1_BASE_PTR) |= 0x10000; // This should ASSERT chip select for SPI1.
#if 0
		//while(SPI1_Accel_GetReceivedDataNum(masterDevData) != 1)
		{
			err = SPI1_Accel_ReceiveBlock(masterDevData, rx, 4);
			//printf("SPI1_Accel_ReceiveBlock Err: 0x%x\n", err);
		}
		
		//SPI_PUSHR_REG(SPI1_BASE_PTR) &= ~0x10000; // This should DE-assert chip select for SPI1.
		
		err = SPI1_Accel_SendBlock(masterDevData, tx, 2);
		//printf("SPI1_Accel_SendBlock Err: 0x%x\n", err);
#endif
		
#if 0
		
		do
		{
			err = SPI1_Accel_ReceiveBlock(masterDevData, rx, 1);
			//sleep(1);
		} while( (err != ERR_OK) /*&& (retries-- > 0)*/ );
		
		//printf("err: %d, retries: %d\n", err, retries);
		retries = 5;
		
		do
		{
			err = SPI1_Accel_SendBlock(masterDevData, tx, 2);
			//sleep(1);
		} while( (err != ERR_OK) /*&& (retries-- > 0)*/ );
		
		while(g_SPI1_Accel_Wait_for_Send){}
		
		//printf("err: %d, retries: %d\n", err, retries);
		
		g_SPI1_Accel_Wait_for_Send = TRUE;
		g_SPI1_Accel_Wait_for_Receive = TRUE;
		
#endif
		
		
#if 0
		/*
		 *    Initialize the top half of spi1_pushr with values of SPIx_PUSHR register want.
		 */
		spi1_pushr = 0;
		spi1_pushr |= SPI_PUSHR_CONT_MASK;  // continous chip select assertion.
		
		
		spi1_pushr |= SPI_STLIS3DH_WHO_AM_I;
		SPI_PDD_WriteMasterPushTxFIFOReg(SPI1_BASE_PTR, spi1_pushr);
		
		/* Deassert all chip selects (borrowed from MQX SPI Driver) */
		/*
		if (dspi_ptr->MCR & DSPI_MCR_MSTR_MASK)
		{
			for (num = 0; num < DSPI_CS_COUNT; num++)
			{
				if ((NULL != io_info_ptr->CS_CALLBACK[num]) && (0 != (io_info_ptr->CS_ACTIVE & (1 << num))))
				{
					io_info_ptr->CS_CALLBACK[num] (DSPI_PUSHR_PCS(1 << num), 1, io_info_ptr->CS_USERDATA[num]);
				}
			}
			io_info_ptr->CS_ACTIVE = 0;

			dspi_ptr->MCR |= DSPI_MCR_HALT_MASK;
			dspi_ptr->MCR &= (~ DSPI_MCR_MSTR_MASK);
			dspi_ptr->MCR |= DSPI_MCR_MSTR_MASK;
			dspi_ptr->MCR &= (~ DSPI_MCR_HALT_MASK);
		}
		*/
		
#endif
		printf("Data Read Back = 0x%x%x%x%x\n", rx[0], rx[1], rx[2], rx[3]);   // We expect the LIS3DH to be 0x33.

		sleep(1);
	}
	
	return SUCCESS;
}

/*
 *   Handle power management for the LIS3DH accelerometer.
 */
corona_err_t pwr_accel(pwr_state_t state)
{
	
	switch(state)
	{
		case PWR_STATE_SHIP:
			break;
			
		case PWR_STATE_STANDBY:
			break;
			
		case PWR_STATE_NORMAL:
			break;
			
		default:
			return CC_ERR_INVALID_PWR_STATE;
	}
	return SUCCESS;
}
