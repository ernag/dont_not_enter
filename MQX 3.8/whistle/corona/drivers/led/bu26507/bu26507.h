/*
 * bu26507.h
 *
 *  Created on: Apr 11, 2013
 *      Author: Chris
 */

#ifndef BU26507_H_
#define BU26507_H_


////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include <mqx.h>
#include "app_errors.h"

#include "led_hal.h"


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
#define   BU_LED_MASK_A        0x07
#define   BU_LED_MASK_B        0x38
#define   BU_LED_MASK_AB       (BU_LED_MASK_A | BU_LED_MASK_B)

#define BU_PWM_DUTY_CYCLE      0x20
#define   BU_DUTY_PERC_MASK    0x1F

#define BU_CLOCK_SYNC          0x21
#define   BU_CLKIN             0x01
#define   BU_CLKOUT            0x02
#define   BU_SYNCON            0x04
#define   BU_SYNCACT           0x08

#define BU_SCROLL_EN           0x2D     // Also Slope & PWM, but just using scroll
#define   BU_SCLEN             0x01

#define BU_SCROLL_RESET        0x2E
#define   BU_SCLRST            0x01

#define BU_SCROLL_SETTING      0x2F
#define   BU_SCL_LEFT          0x01
#define   BU_SCL_RIGHT         0x02
#define   BU_SCL_DOWN          0x04
#define   BU_SCL_UP            0x08
#define   BU_SCLSPEED          0x70     // 0 = .1 sec ... 7 = .8 sec

#define BU_MATRIX_CONTROL      0x30
#define   BU_MTRX_START        0x01     // Lighting, slope and scroll sequence start

#define BU_MATRIX_DATA_CLEAR   0x31
#define   BU_MDC_CLRA          0x01
#define   BU_MDC_CLRB          0x02

#define BU_MAP_CHANGE          0x7F
#define   BU_MC_RMCG           0x01
#define   BU_MC_OAB            0x02
#define   BU_MC_IAB            0x04

////////////////////////////////////////////////////////////////////////////////
// Structures
////////////////////////////////////////////////////////////////////////////////

typedef struct display_cmd
{
    uint_8 reg;
    uint_8 data;
} display_cmd_t;

typedef enum bu26507_pattern_e
{
    BU26507_PATTERN_OFF,
    BU26507_PATTERN_FAIL,    
    BU26507_PATTERN1,
    BU26507_PATTERN2,
    BU26507_PATTERN3,
    BU26507_PATTERN18  =  18,
    BU26507_PATTERN54  =  54,
    BU26507_PATTERN55  =  55,
    BU26507_PATTERN101 = 101,
    BU26507_PATTERN102 = 102,
    BU26507_PATTERN103 = 103,
    BU26507_PATTERN118 = 118,
    BU26507_PATTERN154 = 154,
    BU26507_PATTERN155 = 155,
    BU26507_PATTERN201 = 201,
    BU26507_PATTERN202 = 202,
    BU26507_PATTERN203 = 203,
    BU26507_PATTERN218 = 218,
    BU26507_PATTERN254 = 254,
    BU26507_PATTERN255 = 255,
    BU26507_PATTERN_MY3S    = 301,
    BU26507_PATTERN_MY3P    = 302,
    BU26507_PATTERN_MR1P    = 303,
    BU26507_PATTERN_CHGING  = 304,
    BU26507_PATTERN_BT_PAIR = 305,
    BU26507_PATTERN_MW3S    = 306,
    BU26507_PATTERN_MA3S    = 307,
    BU26507_PATTERN_BA3S    = 308,
    BU26507_PATTERN_MW2S    = 309,
    BU26507_PATTERN_RGB     = 310,
    BU26507_PATTERN_CHGD    = 311
//    BU26507_PATTERN_PWR_DRAIN_TEST = 312
} bu26507_pattern_t;

/*
 *   LED Flex Type set at the factory.
 *   Everbright and ROHM have swapped Blue <---> Green.
 *   https://whistle.atlassian.net/wiki/display/COR/Corona+assembly+revisions+with+software+impacts
 */
typedef enum ledflex_type
{
    LEDFLEX_ROHM       = (uint32_t) 1,
    LEDFLEX_EVERBRIGHT = (uint32_t) 2
    
}ledflex_t;

////////////////////////////////////////////////////////////////////////////////
// Declarations
////////////////////////////////////////////////////////////////////////////////
err_t bu26507_run_pattern(bu26507_pattern_t pattern);
err_t bu26507_run_select( led_info_ptr_t pled_info );
err_t bu26507_init(void);
err_t bu26507_standby(void);
err_t bu26507_write(uint_8 reg, uint_8 data); // Deprecated
err_t bu26507_run_matrix(matrix_setup_t* mxs, boolean IS_XX);


#endif /* BU26507_H_ */
