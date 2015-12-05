/*
 * bu26507_display.h
 *
 *  Created on: Jan 15, 2013
 *      Author: Ernie Aguilar
 */

#ifndef BU26507_DISPLAY_H_
#define BU26507_DISPLAY_H_

#include "PE_Types.h"
#include "k2x_errors.h"

corona_err_t bu26507_display_test(void);
corona_err_t bu26507_init(void);
corona_err_t bu26507_write(uint8_t reg, uint8_t data);
#if 0
/*
 *     READING IS NOT SUPPORTED WITH THE DISPLAY DEVICE !
 */
corona_err_t bu26507_read(uint8_t reg, uint8_t *pData);
#endif


// There are 2 BU26507 slave devices, one with address 0x74 (device 0) and one with address 0x75 (device 1)
#define BU26507_0_SLAVE_ADDR			0x74
#define BU26507_0_SLAVE_ADDR_SHIFTED	0xE8 // 7-bit address needs to be shifted to the left
#define BU26507_1_SLAVE_ADDR			0x75
#define BU26507_1_SLAVE_ADDR_SHIFTED	0xEA // 7-bit address needs to be shifted to the left

// REGISTER AND MASK DEFINITIONS

#define BU_SW_RST		0x00
#define   BU_SFTRST		0x01

#define BU_OSC_CTRL		0x01
#define   BU_OSCEN		0x08

#define BU_LED_EN_ADDR			0x11    // LED enable register and 6 LED's in the list.
#define   BU_LED1_MASK			0x01
#define	  BU_LED2_MASK			0x02
#define	  BU_LED3_MASK			0x04
#define	  BU_LED4_MASK			0x08
#define	  BU_LED5_MASK			0x10
#define   BU_LED6_MASK			0x20

#define BU_PWM_DUTY_CYCLE		0x20
#define   BU_DUTY_PERC_MASK		0x1F

#define BU_CLOCK_SYNC			0x21
#define   BU_CLKIN				0x01
#define   BU_CLKOUT				0x02
#define   BU_SYNCON				0x04
#define   BU_SYNCACT			0x08

typedef struct display_cmd
{
	uint8 reg;
	uint8 data;
} display_cmd_t;


#endif /* BU26507_DISPLAY_H_ */
