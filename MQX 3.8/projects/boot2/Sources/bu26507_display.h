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
#include "PE_Types.h"
#include "corona_errors.h"

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
#define BU26507_0_SLAVE_ADDR			0x74
#define BU26507_0_SLAVE_ADDR_SHIFTED	0xE8 // 7-bit address needs to be shifted to the left
#if CORONA_HAS_2_ROHM_CHIPS_ONBOARD     // Proto2 onwards won't.
#define BU26507_1_SLAVE_ADDR			0x75
#define BU26507_1_SLAVE_ADDR_SHIFTED	0xEA // 7-bit address needs to be shifted to the left
#endif

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

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

typedef struct display_cmd
{
	uint8 reg;
	uint8 data;
} display_cmd_t;

typedef enum bu_color_type
{
    BU_COLOR_RED    = 0,
    BU_COLOR_WHITE  = 1,
    BU_COLOR_GREEN  = 2,
    BU_COLOR_BLUE   = 3,
    BU_COLOR_YELLOW = 4,
    BU_COLOR_PURPLE = 5,
    BU_COLOR_OFF    = 6,
    
    BU_NUM_COLORS   = 7
    
}bu_color_t;

typedef enum bu_pattern_type
{
    BU_PATTERN_SOLID_PURPLE,
    BU_PATTERN_ABORT,
    BU_PATTERN_BOOTLDR_MENU,
    BU_PATTERN_BOOTLDR_JUMP_TO_APP,
    BU_PATTERN_BOOTLDR_STAY_IN_BOOT,
    BU_PATTERN_BOOTLDR_RESET_FACTORY_DEFAULTS,
    BU_PATTERN_BOOTLDR_INITIAL_PRESS,
    BU_PATTERN_BOOTLDR_WAIT_FOR_CLICK,
    
    BU_PATTERN_OFF
    
}bu_pattern_t;

extern bu_color_t g_cstate;

#if 0
/*
 *   LED Flex Type set at the factory.
 *   Everbright and ROHM have swapped Blue <---> Green.
 *   https://whistle.atlassian.net/wiki/display/COR/Corona+assembly+revisions+with+software+impacts
 */
typedef enum ledflex_type
{
    LEDFLEX_ROHM       = (uint32_t) 1,
    LEDFLEX_EVERBRIGHT = (uint32_t) 2,
    
    LEDFLEX_INVALID    = 0xFEEDBEEF
    
}ledflex_t;
#endif

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
corona_err_t bu26507_display_pattern(bu_pattern_t pattern);
corona_err_t bu26507_init(void);
corona_err_t bu26507_write(uint8_t reg, uint8_t data);
corona_err_t bu26507_color(bu_color_t c);
corona_err_t bu26507_pwm(uint16_t percentage);


#endif /* BU26507_DISPLAY_H_ */
////////////////////////////////////////////////////////////////////////////////
// End of file
////////////////////////////////////////////////////////////////////////////////
//! @}
