/*
 * led_hal.h
 *
 *  Created on: Apr 11, 2013
 *      Author: Chris
 */

#ifndef LED_HAL_H_
#define LED_HAL_H_

#include "app_errors.h"

// LED Masks
#define LED1_ON   0x0001
#define LED2_ON   0x0002
#define LED3_ON   0x0004
#define LED4_ON   0x0008
#define LED5_ON   0x0010
#define LED6_ON   0x0020
#define LED7_ON   0x0040
#define LED8_ON   0x0080
#define LED9_ON   0x0100
#define LED10_ON  0x0200

// Scroll directions
#define   SCL_LEFT          0x01
#define   SCL_RIGHT         0x02
#define   SCL_DOWN          0x04
#define   SCL_UP            0x08

// Brightness: 0=off, 15=Max
#define DEFAULT_BRIGHTNESS  3
#define XXXX_BRIGHT         0x1f;       // use individual rgb levels
#define FULL_BRIGHT         0x0f;
#define HALF_BRIGHT         0x08;

#define SLP_CYC_MAX            0x03     // Max time, slowest cycle
#define SLP_CYC_MED            0x02
#define SLP_CYC_MIN            0x01     // Minimum time, fastest cycle

#define SLP_DLY_PRG            0x04     // Progressive delay, increases per LED num
#define SLP_DLY_MAX            0x03     // Max delay
#define SLP_DLY_MED            0x02
#define SLP_DLY_MIN            0x01     // Minimum delay

#define IS_SCROLL              TRUE
#define IS_SLOPE               FALSE

#define ALL_LEDS               0x03ff
#define ODD_LEDS               0x0155
#define EVEN_LEDS              0x02aa

typedef struct led_info_t
{
    uint_16 led_mask;   // 10-bit mask: b0=LED1..B9=LED10
    uint_8  color_mask; // xx1=B, x1x=G, 1xx=R
    uint_8  brightness; // current level: 0=off, 15=20mA
} led_info_t, *led_info_ptr_t;

// matrix structs
typedef struct matrix_info_s
{
    uint_16 led_mask;   // 10-bit mask: b0=LED1..B9=LED10
    uint_8  color_mask; // 0x01=G/B, 0x02=B/G, 0x04=R
    uint_8  brightness; // current level: 0=off, 15=20mA : >15 -> set rgb individual levels
    uint_8  g_level;    // individual green brightness level
    uint_8  b_level;    // individual blue brightness level
    uint_8  r_level;    // individual red brightness level
    uint_8  slope_dly;  // Slope delay
    uint_8  slope_cyc;  // Slope cycle
} matrix_info_t, *pmatrix_info_t;

typedef struct matrix_setup_s
{
    uint_16 a_led_mask;         // 10-bit mask: b0=LED1..B9=LED10
    uint_16 b_led_mask;
    uint_8  a_color_mask;       // xx1=G/B, x1x=B/G, 1xx=R
    uint_8  b_color_mask;
    uint_8  brightness;         // current level: 0=off, 15=20mA : >15 -> set rgb individual levels
    uint_8  g_level;            // individual green brightness level
    uint_8  b_level;            // individual blue brightness level
    uint_8  r_level;            // individual red brightness level
    uint_8  slope_dly;          // Slope delay
    uint_8  slope_cyc;          // Slope cycle
    uint_8  scroll_speed;       // 0 = fastest, 7 = slowest
    uint_8  scroll_direction;   // 1 = left, 2 = right, 4 = down, 8 = up
} matrix_setup_t;


typedef enum led_pattern_e
{
    LED_PATTERN_OFF,
    LED_PATTERN_1,
    LED_PATTERN_2,
    LED_PATTERN_3,
    LED_PATTERN_4,
    LED_PATTERN_5,
    LED_PATTERN_6,
    LED_PATTERN_7,
    LED_PATTERN_8,
    LED_PATTERN_9,
    LED_PATTERN_10,
    LED_PATTERN_11,
    LED_PATTERN_12,
    LED_PATTERN_13,
    LED_PATTERN_14,
    LED_PATTERN_15,
    LED_PATTERN_16,
    LED_PATTERN_17,
    LED_PATTERN_18,
    LED_PATTERN_MY3S,
    LED_PATTERN_MY3P,
    LED_PATTERN_MR1P,
    LED_PATTERN_CHGING,
    LED_PATTERN_CHGD,
    LED_PATTERN_BT_PAIR,
    LED_PATTERN_MW3S,
    LED_PATTERN_MA3S,
    LED_PATTERN_BA3S,
    LED_PATTERN_MW2S,
    LED_PATTERN_RGB,
//    LED_PATTERN_PWR_DRAIN_TEST,
    LED_PATTERN_FAIL
} led_pattern_t;

void  led_init(void);
void led_deinit(void);
err_t led_run_pattern(led_pattern_t pattern);
//err_t led_display_battery_level( uint_8 pct );

#endif /* LED_HAL_H_ */
