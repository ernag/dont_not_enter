/*
 * rf_switch.c
 *
 *  Created on: May 30, 2013
 *      Author: Chris
 */

#include <mqx.h>
#include <bsp.h>
#include "wassert.h"
#include "app_errors.h"
#include "rf_switch.h"
#include "cfg_mgr.h"
#include "cfg_factory.h"

// Bitfield Masks for configuring RF_SWitches
#define RF_SW_MASK 0x0Cu
#define RF_SW_V1   0x04u
#define RF_SW_V2   0x08u

//#define DEBUG_LEDS
#ifdef DEBUG_LEDS
#define SET_LED(led, on) set_debug_led(led, on)
#else
#define SET_LED(led, on)
#endif

static LWGPIO_STRUCT sw_v1, sw_v2;

void rf_switch_init(void)
{
    wassert(lwgpio_init(&sw_v1, BSP_GPIO_RF_SW_V1_PIN, LWGPIO_DIR_OUTPUT,
            LWGPIO_VALUE_NOCHANGE));

    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&sw_v1, BSP_GPIO_RF_SW_V1_MUX_GPIO);
    
    wassert(lwgpio_init(&sw_v2, BSP_GPIO_RF_SW_V2_PIN, LWGPIO_DIR_OUTPUT,
            LWGPIO_VALUE_NOCHANGE));

    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&sw_v2, BSP_GPIO_RF_SW_V2_MUX_GPIO);
}

/*
 *   Configure the RF Switch at PTB2 and PTB3 or Corona to be either for
 *     Bluetooth or WIFI.
 *     
 *     When setting on, must set one GPIO on and the other off atomically.
 *     When setting off, just turn off the desired side. The other side may already be off,
 *     but that's ok. It just means both sides will then be off.
 */
void rf_switch(rf_switch_e position, boolean on)
{
    uint32_t portb = GPIO_PDOR_REG( PTB_BASE_PTR );
    
    if (on)
    {
    	portb &= ~RF_SW_MASK;
    }
    
    if( FCFG_RFSWITCH_AS169 != g_st_cfg.fac_st.rfswitch )
    {
        /*
         *   AS-193
         *   Use this kind of strange logic so that we default to AS-193.
         */
        if(RF_SWITCH_BT == position)
        {
            /*
             *   Configure RF switch for BT.
             */
        	if (on)
        	{
            	SET_LED(0,1);
            	SET_LED(3,0);
        		portb |= RF_SW_V1;
        	}
        	else
        	{
            	SET_LED(0,0);
        		portb &= ~RF_SW_V1;
        	}
        }
        else
        {
            /*
             *   Configure RF switch for Wi-Fi.
             */
        	if (on)
        	{
            	SET_LED(3,1);
            	SET_LED(0,0);
        		portb |= RF_SW_V2;
        	}
        	else
        	{
            	SET_LED(3,0);
        		portb &= ~RF_SW_V2;
        	}
        }
    }
    else
    {
        /*
         *   AS-169 (inverted).
         */
        if(RF_SWITCH_BT == position)
        {
            /*
             *   Configure RF switch for BT.
             */
        	if (on)
        	{
            	SET_LED(0,1);
            	SET_LED(3,0);
        		portb |= RF_SW_V2;
        	}
        	else
        	{
            	SET_LED(0,0);
        		portb &= ~RF_SW_V2;
        	}
        }
        else
        {
            /*
             *   Configure RF switch for Wi-Fi.
             */
        	if (on)
        	{
            	SET_LED(3,1);
            	SET_LED(0,0);
        		portb |= RF_SW_V1;
        	}
        	else
        	{
            	SET_LED(3,0);
        		portb &= ~RF_SW_V1;
        	}
        }
    }
    
    GPIO_PDOR_REG( PTB_BASE_PTR ) = portb;
    
    _time_delay(1);  // give a bit of time to let the RF switch state "settle".
}
