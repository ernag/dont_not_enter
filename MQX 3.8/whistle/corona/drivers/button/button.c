/*
 * button.c
 *
 *  Created on: Mar 8, 2013
 *      Author: Ernie
 */

#include "button.h"
#include "sys_event.h"
#include "whistle_lwgpio_isr_mux.h"
#include "wassert.h"
#include "ext_flash.h"
#include "pwr_mgr.h"
#include <watchdog.h>

//#define DOUBLE_PUSH

/*
 *   NOTE:  I accompanied this with a call to:
 *          "set_debug_led_code( dbg_led );" in main_task().
 */
#define D(x)

D(#include "debug_led.h")

/*
 *   Define delays related to button driver (milliseconds)
 */
#define BUTTON_REENTRY_DELAY          1

/*
 *   Divide by 10 to convert to ticks.
 */
#define BUTTON_DEBOUNCE_THRESH        	( 40   / 10 )
#define BUTTON_DOUBLEPUSH_THRESH      	( 700  / 10 )

#define LONG_PUSH_START_THRESH    		( 1000 / 10 )
#define LONG_PUSH_END_THRESH      		( 2500 / 10 )

#define BUTTON_DEBOUNCING_DELAY_MS   	( 40 )

static LWEVENT_STRUCT button_lwevent;
static LWGPIO_STRUCT  button_gpio;
static LWGPIO_VALUE   old_value = LWGPIO_VALUE_NOCHANGE;
D(int dbg_led = 0;)

static void _button_debounce(uint16_t delay)
{
#if 0 //DOUBLE_PUSH:  NOTE removed since double push not needed.
	  //              NOTE this code would break single click coming out of ship mode.
	  //              NOTE EA: believe debouncing is not necessary if double push not supported (and even then maybe not).
    _time_delay( delay );
    _lwevent_clear(&button_lwevent, BUTTON_UP | BUTTON_DOWN);
#endif
}

/*
 *   Returns the value of the button right now.
 */
button_event_t button_get_val(void)
{
    if(lwgpio_get_value( (LWGPIO_STRUCT_PTR) &button_gpio ) == LWGPIO_VALUE_HIGH)
    {
        return BUTTON_UP;  
    }
    else
    {
        return BUTTON_DOWN;
    }
}

/*
 *   Main task that runs periodically and broadcasts button events with help from hardware ISR.
 */

/* TBD: This begs to be rewritten as a looping state machine */
void BTNDRV_tsk(uint_32 dummy)
{
    _mqx_uint   mqx_rc;
    sys_event_t event;
    int i = 0;

    while (1)
    {
        /*
         *   First, wait for the initial push down and request a low power state.
         */
        PWRMGR_VoteForVLPR( PWRMGR_BTNMGR_VOTE );
        
        //corona_print(" *** TEMP:  button manager waiting on button down.\n", event);
        _lwevent_wait_ticks(&button_lwevent, BUTTON_DOWN, FALSE, 0);
        _button_debounce(BUTTON_DEBOUNCING_DELAY_MS);
        
        PWRMGR_VoteForRUNM( PWRMGR_BTNMGR_VOTE );
        PWRMGR_VoteForSleep( PWRMGR_BTNMGR_VOTE );   // we voted for RUNM, so we don't need the "awake" / "sleep" logic now.
        
        //corona_print("BUTTON_DOWN\n");  // comment out, this is an intrusive operation time-wise.

        /*
         *   Wait for a long-press or hold, otherwise timeout for single or double press.
         */
        mqx_rc = _lwevent_wait_ticks(&button_lwevent, BUTTON_UP, FALSE, LONG_PUSH_START_THRESH);
        _button_debounce(BUTTON_DEBOUNCING_DELAY_MS);
        
        if (mqx_rc == LWEVENT_WAIT_TIMEOUT)
        {
            /*
             *  Got long-press or hold. Wait to see which
             */
        	
        	/* We don't want to clear the event here. */
        	
            mqx_rc = _lwevent_wait_ticks(&button_lwevent, BUTTON_UP, FALSE, LONG_PUSH_END_THRESH - LONG_PUSH_START_THRESH);
            _button_debounce(BUTTON_DEBOUNCING_DELAY_MS);

            if (mqx_rc == LWEVENT_WAIT_TIMEOUT)
            {
                corona_print("BUTTON_HOLD\n");
                event = SYS_EVENT_BUTTON_HOLD;
                goto button_broadcast;
            }
            else if (mqx_rc == MQX_OK)
            {
                corona_print("BUTTON_LONG\n");
                event = SYS_EVENT_BUTTON_LONG_PUSH;
                goto button_broadcast;
            }
            else
            {
                wassert(0);
            }
        }
        else if (mqx_rc == MQX_OK)
        {
            _button_debounce(5);  // using a shorter than normal delay here, since double clicking can be quick!

#ifdef DOUBLE_PUSH
            /*
             *   Check for a double press, if not - broadcast a single press.
             */
            mqx_rc = _lwevent_wait_ticks(&button_lwevent, BUTTON_DOWN, FALSE, BUTTON_DOUBLEPUSH_THRESH);

            if (mqx_rc == LWEVENT_WAIT_TIMEOUT)
            {
#endif
                corona_print("BUTTON_SINGLE\n");
                event = SYS_EVENT_BUTTON_SINGLE_PUSH;
                goto button_broadcast;
#ifdef DOUBLE_PUSH
            }
            else if (mqx_rc == MQX_OK)
            {
                corona_print("BUTTON_DOUBLE\n");
                event = SYS_EVENT_BUTTON_DOUBLE_PUSH;
                goto button_broadcast;
            }
            else
            {
                wassert(0);
            }
#endif
        }
        else
        {
            wassert(0);
        }

button_broadcast:
        _watchdog_start(60*1000); // who knows what clients will do, so give plenty of time.
        sys_event_post(event);
        
        /*
         *   For the press and hold case, we want to make sure that SPI Flash is protected,
         *     in the event of the user pressing and holding for > 5 seconds.
         */
        if( SYS_EVENT_BUTTON_HOLD == event )
        {
        	wson_lock(MQX_NULL_TASK_ID);
        	
        	while( LWGPIO_VALUE_LOW == lwgpio_get_value( (LWGPIO_STRUCT_PTR) &button_gpio ) )
        	{
        		_time_delay( 100 );
        	}
        	
        	wson_unlock(MQX_NULL_TASK_ID);
        }
        
        /*
         *   Take one final, more extreme debouncing measure, to clear all of those old hounds.
         */
        _button_debounce(200);
        
        _watchdog_stop();
    }
}

/*
 * Button Interrupt Function Body
 * This routine is called by the interrupt handler if the button is 
 * pressed while the system is awake. It's also called by the LLWU 
 * interrupt handler if the button press was the source of a wakeup.
 */
void button_handler_action( void )
{
    LWGPIO_VALUE  new_value = lwgpio_get_value( (LWGPIO_STRUCT_PTR) &button_gpio );

    if (new_value == old_value)
    {
        return;
    }

    old_value = new_value;
    
    PWRMGR_VoteForAwake( PWRMGR_BTNMGR_VOTE );  // we really want RUNM, but that's not currently allowed inside an ISR so.

    if (LWGPIO_VALUE_HIGH == new_value)
    {
    	D(dbg_led |= 2;)
        _lwevent_set(&button_lwevent, BUTTON_UP);
    }
    else
    {
    	D(dbg_led |= 4;)
        _lwevent_set(&button_lwevent, BUTTON_DOWN);
    }    
}

/*
 *   PTE1 Hardware ISR for the button.
 */
static void button_int_service_routine(void *pointer)
{
    // Handle the button pin value change
    button_handler_action();

    // Clear the GPIO interrupt flag
    lwgpio_int_clear_flag(&button_gpio);
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Initialize button handler.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function initializes the button ISR and clears interrupt flags.
//!
//! \return void
//!
///////////////////////////////////////////////////////////////////////////////
void button_init(void)
{
    _lwevent_create(&button_lwevent, LWEVENT_AUTO_CLEAR);

    /*******************************************************************************
    *  Opening the pin for input, initialize interrupt on it and set interrupt handler.
    *******************************************************************************/
    /* opening pins for input */
    lwgpio_init(&button_gpio, BSP_GPIO_BUTTON_PIN, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE);
    
    /* disable in case it's already enabled */
    lwgpio_int_enable(&button_gpio, FALSE);

    lwgpio_set_functionality(&button_gpio, BSP_GPIO_BUTTON_MUX_GPIO);
    lwgpio_set_attribute(&button_gpio, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_ENABLE);
    lwgpio_set_attribute(&button_gpio, LWGPIO_ATTR_FILTER,  LWGPIO_AVAL_ENABLE);
    
    // Give time for the GPIO to be pulled up so we don't incorrectly detect button down below.
    _time_delay(1);
    
    /* enable gpio functionality for given pin, react on falling or rising edge */
    lwgpio_int_init(&button_gpio, LWGPIO_INT_MODE_FALLING | LWGPIO_INT_MODE_RISING);
    whistle_lwgpio_isr_install(&button_gpio, 3, button_int_service_routine);
    
    /*
     *   Check to see if the button is already pressed down (coming out of ship mode, etc...)
     */
    if( LWGPIO_VALUE_LOW == lwgpio_get_value( (LWGPIO_STRUCT_PTR) &button_gpio ) )
    {
    	D(dbg_led |= 1;)
    	_lwevent_set(&button_lwevent, BUTTON_DOWN);
    	old_value = LWGPIO_VALUE_LOW;
    }
}
