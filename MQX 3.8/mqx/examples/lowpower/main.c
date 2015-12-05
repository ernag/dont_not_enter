/**HEADER********************************************************************
*
* Copyright (c) 2011 Freescale Semiconductor;
* All Rights Reserved
*
***************************************************************************
*
* THIS SOFTWARE IS PROVIDED BY FREESCALE "AS IS" AND ANY EXPRESSED OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL FREESCALE OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*
**************************************************************************
*
* $FileName: main.c$
* $Version : 3.8.B.2$
* $Date    : Mar-19-2012$
*
* Comments:
*
*END************************************************************************/

#include <mqx.h>
#include <bsp.h>


#if ! BSPCFG_ENABLE_IO_SUBSYSTEM
#error This application requires BSPCFG_ENABLE_IO_SUBSYSTEM defined non-zero in user_config.h. Please recompile BSP with this option.
#endif


#ifndef BSP_DEFAULT_IO_CHANNEL_DEFINED
#error This application requires BSP_DEFAULT_IO_CHANNEL to be not NULL. Please set corresponding BSPCFG_ENABLE_TTYx to non-zero in user_config.h and recompile BSP with this option.
#endif


#if ! MQX_ENABLE_LOW_POWER
#error This application requires MQX_ENABLE_LOW_POWER to be defined non-zero in user_config.h. Please recompile BSP with this option.
#endif


/* Task IDs */
enum task_ids {
    MAIN_TASK = 1,
    FOR_LOOP_LED_TASK
};


/* Task prototypes */
void main_task(uint_32);
void led_task(uint_32);
void for_loop_led_task(uint_32);


const TASK_TEMPLATE_STRUCT  MQX_template_list[] =
{
    {
        /* Task Index       */  MAIN_TASK,
        /* Function         */  main_task,
        /* Stack Size       */  2500,
        /* Priority Level   */  10,
        /* Name             */  "Main Task",
        /* Attributes       */  MQX_AUTO_START_TASK,
        /* Creation Params  */  0,
        /* Time Slice       */  0,
    },
    {
        /* Task Index       */  FOR_LOOP_LED_TASK,
        /* Function         */  for_loop_led_task,
        /* Stack Size       */  2500,
        /* Priority Level   */  12,
        /* Name             */  "FOR loop LED Task",
        /* Attributes       */  MQX_AUTO_START_TASK,
        /* Creation Params  */  0,
        /* Time Slice       */  0,
    },
    { 0 }
};


/* LWEVENT masks for app_event */
#define SW_EVENT_MASK          (1 << 0)
#define RTC_EVENT_MASK         (1 << 1)
#define ALL_EVENTS_MASK        (SW_EVENT_MASK | RTC_EVENT_MASK)


LWEVENT_STRUCT app_event;
LWGPIO_STRUCT  led1;


const char * kinetis_power_modes_names[] = {
    "LPM_CPU_POWER_MODE_KINETIS_RUN",
    "LPM_CPU_POWER_MODE_KINETIS_WAIT",
    "LPM_CPU_POWER_MODE_KINETIS_STOP",
    "LPM_CPU_POWER_MODE_KINETIS_VLPR",
    "LPM_CPU_POWER_MODE_KINETIS_VLPW",
    "LPM_CPU_POWER_MODE_KINETIS_VLPS",
    "LPM_CPU_POWER_MODE_KINETIS_LLS"
};


const char * predefined_power_modes_names[] = {
    "LPM_OPERATION_MODE_RUN",
    "LPM_OPERATION_MODE_WAIT",
    "LPM_OPERATION_MODE_SLEEP",
    "LPM_OPERATION_MODE_STOP"
};


/*FUNCTION*-----------------------------------------------------
*
* Task Name    : display_operation_mode_setting
* Comments     : Prints settings for given operation mode.
*
*END*-----------------------------------------------------------*/

static void display_operation_mode_setting
    (
        LPM_OPERATION_MODE operation_mode
    )
{
    LPM_CPU_OPERATION_MODE kinetis_core_bsp_setting = LPM_CPU_OPERATION_MODES[operation_mode];
    
    printf("Kinetis Core Setting:\n");
    printf("  Mode:       %s \n",kinetis_power_modes_names[kinetis_core_bsp_setting.MODE_INDEX]);
    printf("  Flags:\n");
    if(kinetis_core_bsp_setting.FLAGS & LPM_CPU_POWER_MODE_FLAG_SLEEP_ON_EXIT)
    {
        printf("              %s \n","LPM_CPU_POWER_MODE_FLAG_SLEEP_ON_EXIT");
    }
    printf("Wake up events:\n");
    printf(" LLWU_PE1 = 0x%02x Mode wake up events from pins 0..3\n",kinetis_core_bsp_setting.PE1);
    printf(" LLWU_PE2 = 0x%02x Mode wake up events from pins 8..11\n",kinetis_core_bsp_setting.PE2);
    printf(" LLWU_PE3 = 0x%02x Mode wake up events from pins 12..15\n",kinetis_core_bsp_setting.PE3);
    printf(" LLWU_ME  = 0x%02x Mode wake up events from internal sources\n\n",kinetis_core_bsp_setting.ME);
}


/*FUNCTION*-----------------------------------------------------
*
* Task Name    : my_rtc_isr
* Comments     : RTC interrupt isr used to catch RTC alarm
*                and escape from "interrupts only" low power mode.
*
*END*-----------------------------------------------------------*/

static void my_rtc_isr
    (
        pointer rtc_registers_ptr
    )
{
    uint_32 state = _rtc_get_status ();

    if (state & RTC_RTCISR_ALM)
    {
        /* Do not return to sleep after isr again */
        _lpm_wakeup_core ();

        _lwevent_set(&app_event, RTC_EVENT_MASK);
    }
    _rtc_clear_requests (state);
}


/*FUNCTION*-----------------------------------------------------
*
* Task Name    : install_rtc_interrupt
* Comments     : Installs RTC interrupt and enables alarm.
*
*END*-----------------------------------------------------------*/

static void install_rtc_interrupt
    (
    )
{
    if (MQX_OK != _rtc_int_install ((pointer)my_rtc_isr))
    {
        printf ("\nError installing RTC interrupt!\n");
        _task_block();
    }

    _rtc_clear_requests (RTC_RTCISR_ALM);

    if (0 == _rtc_int_enable (TRUE, RTC_RTCIENR_ALM))
    {
        printf ("\nError enabling RTC interrupt!\n");
        _task_block();
    }
}


/*FUNCTION*-----------------------------------------------------
*
* Task Name    : set_rtc_alarm
* Comments     : Setup RTC alarm to given number of seconds from now.
*
*END*-----------------------------------------------------------*/

static void set_rtc_alarm
    (
        uint_32 seconds
    )
{
    DATE_STRUCT     time_rtc;
    TIME_STRUCT     time_mqx;

    _rtc_get_time_mqxd (&time_rtc);
    _time_from_date (&time_rtc, &time_mqx);
    time_mqx.SECONDS += seconds;
    _time_to_date (&time_mqx, &time_rtc);
    _rtc_set_alarm_mqxd (&time_rtc);
    printf ("\nRTC alarm set to +%d seconds.\n", seconds);
}


/*FUNCTION*-----------------------------------------------------
*
* Task Name    : my_button_isr
* Comments     : SW interrupt callback used to catch SW press
*                and trigger SW_EVENT
*
*END*-----------------------------------------------------------*/

static void my_button_isr(void *pin)
{
    _lwevent_set(&app_event, SW_EVENT_MASK);
    lwgpio_toggle_value(&led1);
    lwgpio_int_clear_flag(pin);
}


/*FUNCTION*-----------------------------------------------------
*
* Task Name    : button_led_init
* Comments     : Setup the button to trigger interrupt on each button press.
*
*END*-----------------------------------------------------------*/

void button_led_init
    (
        void
    )
{
    static LWGPIO_STRUCT                   sw;
    /* Set the pin to input */
    if (!lwgpio_init(&sw, BSP_SW2, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE))
    {
        printf("\nSW initialization failed.\n");
        _task_block();
    }

    /* Set functionality to GPIO mode */
    lwgpio_set_functionality(&sw, BSP_SW2_MUX_GPIO);
    
    /* Enable pull up */
    lwgpio_set_attribute(&sw, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_ENABLE);

    /* Setup the pin interrupt mode */
    if (!lwgpio_int_init(&sw, LWGPIO_INT_MODE_FALLING))
    {
        printf("Initializing SW for interrupt failed.\n");
        _task_block();
    }

    /* Install gpio interrupt service routine */
    _int_install_isr(lwgpio_int_get_vector(&sw), my_button_isr, (void *) &sw);
    
    /* Set interrupt priority and enable interrupt source in the interrupt controller */
    _bsp_int_init(lwgpio_int_get_vector(&sw), 3, 0, TRUE);
    
    /* Enable interrupt for pin */
    lwgpio_int_enable(&sw, TRUE);

    /* Initialize LED pin for output */
    if (!lwgpio_init(&led1, BSP_LED1, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_HIGH))
    {
        printf("\nLED1 initialization failed.\n");
        _task_block();
    }
    /* Set LED pin to GPIO functionality */
    lwgpio_set_functionality(&led1, BSP_LED1_MUX_GPIO);
}


/*TASK*-----------------------------------------------------
*
* Task Name    : main_task
* Comments     : Low power modes switching.
*
*END*-----------------------------------------------------*/

void main_task
    (
        uint_32 initial_data
    )
{
    LPM_OPERATION_MODE power_mode;

    /* Initialize switches */
    button_led_init();

    /* Install interrupt for RTC alarm */
    install_rtc_interrupt();

    /* Create global event */
    if (_lwevent_create(&app_event, 0) != MQX_OK)
    {
        printf("\nCreating app_event failed.\n");
        _task_block();
    }

    printf("\nMQX Low Power Modes Demo\n");

    while (1)
    {
        /* Find out current mode setting */
        power_mode = _lpm_get_operation_mode();

        printf("\n******************************************************************************\n");
        printf("**************** Current Mode : %s ***********************\n", predefined_power_modes_names[power_mode]);
        printf("******************************************************************************\n");

        display_operation_mode_setting(power_mode);
        
        /* Wait for button press */
        printf ("Press button to move to next operation mode.\n");
        _lwevent_wait_ticks (&app_event, SW_EVENT_MASK, FALSE, 0);
        _lwevent_clear (&app_event, ALL_EVENTS_MASK);
        printf("\nButton pressed. Moving to next operation mode.\n");

        power_mode = LPM_OPERATION_MODE_WAIT;
		
        printf("\n******************************************************************************\n");
        printf("**************** Current Mode : %s **********************\n", predefined_power_modes_names[power_mode]);
        printf("******************************************************************************\n");

        display_operation_mode_setting(power_mode);

        printf(
        "Info: WAIT mode is mapped on Kinets VLPR mode by default.\n"
        "      It requires 2 MHz clock and bypassed pll.\n"
        "      Core continues the execution after entering the mode.\n");

        /* The LPM_OPERATION_MODE_WAIT is mapped on  LPM_CPU_POWER_MODE_KINETIS_VLPR by default,
        this mode requires 2 MHz, bypassed PLL clock setting. Change clocks to appropriate mode */
        printf("\nChanging frequency to 2 MHz.\n");
        if (CM_ERR_OK != _lpm_set_clock_configuration(BSP_CLOCK_CONFIGURATION_2MHZ))
        {
            printf("Cannot change clock configuration");
            _task_block();
        }
        /* Change the operation mode */
        printf ("\nSetting operation mode to %s ... ", predefined_power_modes_names[power_mode]);
        printf ("%s\n", _lpm_set_operation_mode (LPM_OPERATION_MODE_WAIT) == 0 ? "OK" : "ERROR");

        /* Wait for button press */
        printf ("\nPress button to move to next operation mode.\n");
        _lwevent_wait_ticks (&app_event, SW_EVENT_MASK, FALSE, 0);
        _lwevent_clear (&app_event, ALL_EVENTS_MASK);
        printf("\nButton pressed.\n");

        /* Return to RUN mode */
        printf ("\nSetting operation mode back to %s ... ", predefined_power_modes_names[LPM_OPERATION_MODE_RUN]);
        printf ("%s\n", _lpm_set_operation_mode (LPM_OPERATION_MODE_RUN) == 0 ? "OK" : "ERROR");

        /* Return default clock configuration */
        printf("\nChanging frequency back to the default one.\n");
        if (CM_ERR_OK != _lpm_set_clock_configuration(BSP_CLOCK_CONFIGURATION_DEFAULT))
        {
            printf("Cannot change clock configuration");
            _task_block();
        }

        printf("\nMoving to next operation mode.\n");

        power_mode = LPM_OPERATION_MODE_SLEEP;

        printf("\n******************************************************************************\n");
        printf("**************** Current Mode : %s *********************\n", predefined_power_modes_names[power_mode]);
        printf("******************************************************************************\n");

        display_operation_mode_setting(power_mode);

        printf(
        "Info: SLEEP mode is mapped on Kinetis WAIT mode by default. Core is inactive\n"
        "      in this mode, reacting only to interrupts.\n"
        "      The LPM_CPU_POWER_MODE_FLAG_SLEEP_ON_EXIT is set, therefore core goes\n"
        "      to sleep again after any isr finishes. The core will stay awake after\n"
        "      call to _lpm_wakeup_core() from RTC or serial line interrupt.\n");

        /* Wake up in 10 seconds */
        set_rtc_alarm(10);

        /* Change the operation mode */
        printf ("\nSetting operation mode to %s ... ", predefined_power_modes_names[power_mode]);
        printf ("%s\n", _lpm_set_operation_mode (power_mode) == 0 ? "OK" : "ERROR");
        
        if (LWEVENT_WAIT_TIMEOUT == _lwevent_wait_ticks (&app_event, RTC_EVENT_MASK, FALSE, 1))
        {
            printf("\nCore woke up by serial interrupt. Waiting for RTC alarm ... ");
            _lwevent_wait_ticks (&app_event, RTC_EVENT_MASK, FALSE, 0);
            printf("OK\n");
        }
        else
        {
            printf("\nCore woke up by RTC interrupt.\n");
        }
        _lwevent_clear (&app_event, ALL_EVENTS_MASK);
        
        /* Wait for button press */
        printf ("\nPress button to move to next operation mode.\n");
        _lwevent_wait_ticks (&app_event, SW_EVENT_MASK, FALSE, 0);
        _lwevent_clear (&app_event, ALL_EVENTS_MASK);
        printf("\nButton pressed. Moving to next operation mode.\n");

        power_mode = LPM_OPERATION_MODE_STOP;

        printf("\n******************************************************************************\n");
        printf("**************** Current Mode : %s **********************\n", predefined_power_modes_names[power_mode]);
        printf("******************************************************************************\n");

        display_operation_mode_setting(power_mode);

        printf(
        "Info: STOP mode is mapped to Kinets LLS mode by default.\n"
        "      Core and most peripherals are inactive in this mode, reacting only to\n"
        "      specified wake up events. The events can be changed in BSP (init_lpm.c).\n"
        "      Serial line is turned off in this mode. The core will wake up from\n"
        "      RTC interrupt.\n");

        /* Wake up in 10 seconds */
        set_rtc_alarm(10);

        /* Change the operation mode */
        printf ("\nSetting operation mode to %s ... \n", predefined_power_modes_names[power_mode]);
        _lpm_set_operation_mode (power_mode);

        /**************************************************************************************************/
        /* SCI HW MODULE IS DISABLED AT THIS POINT - SERIAL DRIVER MUST NOT BE USED UNTIL MODE IS CHANGED */
        /**************************************************************************************************/
        
        /* Return to RUN mode */
        _lpm_set_operation_mode (LPM_OPERATION_MODE_RUN);
        
        printf("\nCore is awake. Moved to next operation mode.\n");
    }
}


/*TASK*-----------------------------------------------------
*
* Task Name    : for_loop_led_task
* Comments     : LEDs setup and blinking, blinking frequency depends on core speed
*
*END*-----------------------------------------------------*/

void for_loop_led_task
    (
        uint_32 initial_data
    )
{
    volatile uint_32 i = 0;
    LWGPIO_STRUCT led2;

    /* Initialize LED pin for output */
    if (!lwgpio_init(&led2, BSP_LED2, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_HIGH))
    {
        printf("\nLED2 initialization failed.\n");
        _task_block();
    }
    /* Set LED pin to GPIO functionality */
    lwgpio_set_functionality(&led2, BSP_LED2_MUX_GPIO);

    while(1)
    {
        for (i = 0; i < 800000; i++) {};
        
        /* Toggle led 2 */
        lwgpio_toggle_value(&led2);
    }
}
