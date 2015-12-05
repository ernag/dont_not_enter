/*
 * led_dbg.c
 * 
 * __VERY__ sophisticated driver for debug LED's.
 *
 *  Created on: Jun 24, 2013
 *      Author: Ernie
 */

#include "led_dbg.h"
#include "derivative.h" /* include peripheral declarations */
#include "Cpu.h"
#include "corona_gpio.h"

/*
 *   Set an LED pattern on the board.
 */
void led_dbg(led_pat_t pattern)
{
    uint32_t led_bits = 0;
    uint32_t psor;
    
    psor = GPIOD_PDOR;
    psor &= ~LED_MASK;
    
    led_bits = pattern << 12;
    psor |= led_bits;
    
    GPIOD_PDOR = psor;
}
