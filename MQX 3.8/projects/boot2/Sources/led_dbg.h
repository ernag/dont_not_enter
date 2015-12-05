/*
 * led_dbg.h
 *
 *  Created on: Jun 24, 2013
 *      Author: Ernie
 */

#ifndef LED_DBG_H_
#define LED_DBG_H_

#define   LED_MASK    (unsigned long)0xF000

/*
 *   Debug LED's are on PTD12 : PTD15
 */

typedef enum led_pattern_type
{
    LED_ALL_OFF = (unsigned long) 0x0,
    LED_P1      = (unsigned long) 0x1,
    LED_P2      = (unsigned long) 0x2,
    LED_P3      = (unsigned long) 0x3,
    LED_P4      = (unsigned long) 0x4,
    LED_P5      = (unsigned long) 0x5,
    LED_P6      = (unsigned long) 0x6,
    LED_P7      = (unsigned long) 0x7,
    LED_P8      = (unsigned long) 0x8,
    LED_P9      = (unsigned long) 0x9,
    LED_P10     = (unsigned long) 0xa,
    LED_P11     = (unsigned long) 0xb,
    LED_P12     = (unsigned long) 0xc,
    LED_P13     = (unsigned long) 0xd,
    LED_P14     = (unsigned long) 0xe,
    LED_ALL_ON  = (unsigned long) 0xf
}led_pat_t;

void led_dbg(led_pat_t pattern);

#endif /* LED_DBG_H_ */
