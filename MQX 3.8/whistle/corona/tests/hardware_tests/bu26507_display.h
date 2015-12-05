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
//! \file bu26507_display.h
//! \brief Display header
//! \version version 0.1
//! \date Jan, 2013
//! \author ernie@whistle.com
////////////////////////////////////////////////////////////////////////////////

#ifndef BU26507_DISPLAY_H_
#define BU26507_DISPLAY_H_

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
int bu26507_display_test(int stayOn);
MQX_FILE_PTR bu26507_display_init(void);

#if 0

/*
 *     READING IS NOT SUPPORTED WITH THE DISPLAY DEVICE !
 */
corona_err_t bu26507_read(uint8_t reg, uint8_t *pData);
#endif

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////

// There are 2 BU26507 slave devices, one with address 0x74 (device 0) and one with address 0x75 (device 1)
#define BU26507_0_SLAVE_ADDR            0x74
#define BU26507_0_SLAVE_ADDR_SHIFTED    0xE8 // 7-bit address needs to be shifted to the left
#if CORONA_HAS_2_ROHM_CHIPS_ONBOARD          // Proto2 onwards won't.
#define BU26507_1_SLAVE_ADDR            0x75
#define BU26507_1_SLAVE_ADDR_SHIFTED    0xEA // 7-bit address needs to be shifted to the left
#endif

// REGISTER AND MASK DEFINITIONS

#define BU_SW_RST              0x00
#define   BU_SFTRST            0x01

#define BU_OSC_CTRL            0x01
#define   BU_OSCEN             0x08

#define BU_LED_EN_ADDR         0x11     // LED enable register and 6 LED's in the list.
#define   BU_LED1_MASK         0x01
#define   BU_LED2_MASK         0x02
#define   BU_LED3_MASK         0x04
#define   BU_LED4_MASK         0x08
#define   BU_LED5_MASK         0x10
#define   BU_LED6_MASK         0x20

#define BU_PWM_DUTY_CYCLE      0x20
#define   BU_DUTY_PERC_MASK    0x1F

#define BU_CLOCK_SYNC          0x21
#define   BU_CLKIN             0x01
#define   BU_CLKOUT            0x02
#define   BU_SYNCON            0x04
#define   BU_SYNCACT           0x08

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

typedef struct display_cmd
{
    uint_8 reg;
    uint_8 data;
} display_cmd_t;
#endif /* BU26507_DISPLAY_H_ */
////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
