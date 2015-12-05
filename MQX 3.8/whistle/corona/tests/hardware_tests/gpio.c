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
//! \gpio.c
//! \brief GPIO tests.
//! \version version 0.1
//! \date Jan, 2013
//! \author chris@whistle.com
//!
//! \detail Driver for controlling the gpios.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Includes
////////////////////////////////////////////////////////////////////////////////
#include "unit_tests.h"
#ifdef ENABLE_WHISTLE_UNIT_TESTS

#include "gpio.h"
#include "button.h"

////////////////////////////////////////////////////////////////////////////////
// Variables
////////////////////////////////////////////////////////////////////////////////
LWEVENT_STRUCT gpio_int_lwevent;
#define BUTTON_EVENT      0x00000001
#define ACC_INT1_EVENT    0x00000002
#define ACC_INT2_EVENT    0x00000004
#define CHG_B_EVENT       0x00000008
#define PGOOD_B_EVENT     0x00000010

static LWGPIO_STRUCT led0_gpio, led1_gpio, led2_gpio, led3_gpio;
static LWGPIO_STRUCT sw_v1_gpio, sw_v2_gpio;
static LWGPIO_STRUCT clk26M_gpio, clk32K_gpio;
static LWGPIO_STRUCT acc_int1_gpio, acc_int2_gpio, chg_b_gpio, pgood_b_gpio;

////////////////////////////////////////////////////////////////////////////////
// Prototypes
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
// Code
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! \brief ACC_INT1 ISR.
//!
//! \fntype ISR
//!
//! \detail Accel Int1 ISR.
///////////////////////////////////////////////////////////////////////////////
void acc_int1_int_service_routine(void *pin)
{
    lwgpio_int_clear_flag((LWGPIO_STRUCT_PTR)pin);
    _lwevent_set(&gpio_int_lwevent, ACC_INT1_EVENT);
}


///////////////////////////////////////////////////////////////////////////////
//! \brief ACC_INT2 ISR.
//!
//! \fntype ISR
//!
//! \detail Accel Int2 ISR.
///////////////////////////////////////////////////////////////////////////////
void acc_int2_int_service_routine(void *pin)
{
    lwgpio_int_clear_flag((LWGPIO_STRUCT_PTR)pin);
    _lwevent_set(&gpio_int_lwevent, ACC_INT2_EVENT);
}


///////////////////////////////////////////////////////////////////////////////
//! \brief CHG_B ISR.
//!
//! \fntype ISR
//!
//! \detail Charger CHG_B ISR.
///////////////////////////////////////////////////////////////////////////////
void chg_b_int_service_routine(void *pin)
{
    lwgpio_int_clear_flag((LWGPIO_STRUCT_PTR)pin);
    _lwevent_set(&gpio_int_lwevent, CHG_B_EVENT);
}


///////////////////////////////////////////////////////////////////////////////
//! \brief PGOOD_B ISR.
//!
//! \fntype ISR
//!
//! \detail Charger PGOOD_B ISR.
///////////////////////////////////////////////////////////////////////////////
void pgood_b_int_service_routine(void *pin)
{
    lwgpio_int_clear_flag((LWGPIO_STRUCT_PTR)pin);
    _lwevent_set(&gpio_int_lwevent, PGOOD_B_EVENT);
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Initialize ACC_INT1 handler.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function initializes the ACC_INT1 ISR and clears interrupt flags.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int acc_int1_init(void)
{
    _mqx_uint result;

    /*******************************************************************************
    *  Opening the pin for input, initialize interrupt on it and set interrupt handler.
    *******************************************************************************/
    /* opening pins for input */
    if (!lwgpio_init(&acc_int1_gpio, BSP_GPIO_ACC_INT1_PIN, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE))
    {
        return -1;
    }
    lwgpio_set_functionality(&acc_int1_gpio, BSP_GPIO_ACC_INT1_MUX_GPIO);

    /* enable acc_int1_gpio functionality for given pin, react on rising edge */
    if (!lwgpio_int_init(&acc_int1_gpio, LWGPIO_INT_MODE_RISING))
    {
        return -2;
    }

    /* install acc_int1_gpio interrupt service routine */
    _int_install_isr(lwgpio_int_get_vector(&acc_int1_gpio), acc_int1_int_service_routine, (void *)&acc_int1_gpio);
    /* set the interrupt level, and unmask the interrupt in interrupt controller*/
    _bsp_int_init(lwgpio_int_get_vector(&acc_int1_gpio), 3, 0, TRUE);
    /* enable interrupt on GPIO peripheral module*/
    lwgpio_int_enable(&acc_int1_gpio, TRUE);

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Initialize ACC_INT2 handler.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function initializes the ACC_INT2 ISR and clears interrupt flags.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int acc_int2_init(void)
{
    _mqx_uint result;

    /*******************************************************************************
    *  Opening the pin for input, initialize interrupt on it and set interrupt handler.
    *******************************************************************************/
    /* opening pins for input */
    if (!lwgpio_init(&acc_int2_gpio, BSP_GPIO_ACC_INT2_PIN, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE))
    {
        return -1;
    }
    lwgpio_set_functionality(&acc_int2_gpio, BSP_GPIO_ACC_INT2_MUX_GPIO);

    /* enable gpio functionality for given pin, react on rising edge */
    if (!lwgpio_int_init(&acc_int2_gpio, LWGPIO_INT_MODE_RISING))
    {
        return -2;
    }

    /* install gpio interrupt service routine */
    _int_install_isr(lwgpio_int_get_vector(&acc_int2_gpio), acc_int2_int_service_routine, (void *)&acc_int2_gpio);
    /* set the interrupt level, and unmask the interrupt in interrupt controller*/
    _bsp_int_init(lwgpio_int_get_vector(&acc_int2_gpio), 3, 0, TRUE);
    /* enable interrupt on GPIO peripheral module*/
    lwgpio_int_enable(&acc_int2_gpio, TRUE);

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Initialize CHG_B handler.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function initializes the CHG_B ISR and clears interrupt flags.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int chg_b_init(void)
{
    _mqx_uint result;

    /*******************************************************************************
    *  Opening the pin for input, initialize interrupt on it and set interrupt handler.
    *******************************************************************************/
    /* opening pins for input */
    if (!lwgpio_init(&chg_b_gpio, BSP_GPIO_CHG_B_PIN, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE))
    {
        return -1;
    }
    lwgpio_set_functionality(&chg_b_gpio, BSP_GPIO_CHG_B_MUX_GPIO);
    lwgpio_set_attribute(&chg_b_gpio, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_ENABLE);

    /* enable gpio functionality for given pin, react on falling edge */
    if (!lwgpio_int_init(&chg_b_gpio, LWGPIO_INT_MODE_FALLING | LWGPIO_INT_MODE_RISING))
    {
        return -2;
    }

    /* install gpio interrupt service routine */
    _int_install_isr(lwgpio_int_get_vector(&chg_b_gpio), chg_b_int_service_routine, (void *)&chg_b_gpio);
    /* set the interrupt level, and unmask the interrupt in interrupt controller*/
    _bsp_int_init(lwgpio_int_get_vector(&chg_b_gpio), 3, 0, TRUE);
    /* enable interrupt on GPIO peripheral module*/
    lwgpio_int_enable(&chg_b_gpio, TRUE);

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Initialize PGOOD_B handler.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function initializes the PGOOD_B ISR and clears interrupt flags.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int pgood_b_init(void)
{
    _mqx_uint result;

    /*******************************************************************************
    *  Opening the pin for input, initialize interrupt on it and set interrupt handler.
    *******************************************************************************/
    /* opening pins for input */
    if (!lwgpio_init(&pgood_b_gpio, BSP_GPIO_PGOOD_B_PIN, LWGPIO_DIR_INPUT, LWGPIO_VALUE_NOCHANGE))
    {
        return -1;
    }
    lwgpio_set_functionality(&pgood_b_gpio, BSP_GPIO_PGOOD_B_MUX_GPIO);
    lwgpio_set_attribute(&pgood_b_gpio, LWGPIO_ATTR_PULL_UP, LWGPIO_AVAL_ENABLE);

    /* enable gpio functionality for given pin, react on falling edge */
    if (!lwgpio_int_init(&pgood_b_gpio, LWGPIO_INT_MODE_FALLING | LWGPIO_INT_MODE_RISING))
    {
        return -2;
    }

    /* install gpio interrupt service routine */
    _int_install_isr(lwgpio_int_get_vector(&pgood_b_gpio), pgood_b_int_service_routine, (void *)&pgood_b_gpio);
    /* set the interrupt level, and unmask the interrupt in interrupt controller*/
    _bsp_int_init(lwgpio_int_get_vector(&pgood_b_gpio), 3, 0, TRUE);
    /* enable interrupt on GPIO peripheral module*/
    lwgpio_int_enable(&pgood_b_gpio, TRUE);

    return 0;
}


///////////////////////////////////////////////////////////////////////////////
//! \brief Simple GPIO Unit test.
//!
//! \fntype Non-Reentrant Function
//!
//! \detail This function runs infinite loop, changing LED values.
//!
//! \return int error code
//!
///////////////////////////////////////////////////////////////////////////////
int gpio_test(void)
{
    int       rc;
    _mqx_uint result, mask;

    corona_print("GPIO  Test...\n");

    /* Create the lightweight semaphore */
    result = _lwevent_create(&gpio_int_lwevent, LWEVENT_AUTO_CLEAR);
    if (result != MQX_OK)
    {
        return -1;
    }

    button_init();

    rc = acc_int1_init();
    if (0 != rc)
    {
        corona_print("acc_int1_init failed with code %d\n", rc);
        _task_block();
    }

    rc = acc_int2_init();
    if (0 != rc)
    {
        corona_print("acc_int2_init failed with code %d\n", rc);
        _task_block();
    }

    rc = chg_b_init();
    if (0 != rc)
    {
        corona_print("chg_b_init failed with code %d\n", rc);
        _task_block();
    }

    rc = pgood_b_init();
    if (0 != rc)
    {
        corona_print("pgood_b_init failed with code %d\n", rc);
        _task_block();
    }

    /* RF Switch test */
    if (!lwgpio_init(&sw_v1_gpio, BSP_GPIO_RF_SW_V1_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_RF_SW_V1_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&sw_v1_gpio, BSP_GPIO_RF_SW_V1_MUX_GPIO);

    lwgpio_set_value(&sw_v1_gpio, LWGPIO_VALUE_LOW);
    lwgpio_set_value(&sw_v1_gpio, LWGPIO_VALUE_HIGH);
    lwgpio_set_value(&sw_v1_gpio, LWGPIO_VALUE_LOW);

    if (!lwgpio_init(&sw_v2_gpio, BSP_GPIO_RF_SW_V2_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_RF_SW_V2_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&sw_v2_gpio, BSP_GPIO_RF_SW_V2_MUX_GPIO);

    lwgpio_set_value(&sw_v2_gpio, LWGPIO_VALUE_LOW);
    lwgpio_set_value(&sw_v2_gpio, LWGPIO_VALUE_HIGH);
    lwgpio_set_value(&sw_v2_gpio, LWGPIO_VALUE_LOW);

    /* 26 MHz clock test - set high */
    if (!lwgpio_init(&clk26M_gpio, BSP_GPIO_MCU_26MHz_EN_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_MCU_26MHz_EN_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&clk26M_gpio, BSP_GPIO_MCU_26MHz_EN_MUX_GPIO);

    lwgpio_set_value(&clk26M_gpio, LWGPIO_VALUE_LOW);
    lwgpio_set_value(&clk26M_gpio, LWGPIO_VALUE_HIGH);
    lwgpio_set_value(&clk26M_gpio, LWGPIO_VALUE_LOW);

    /* 32 KHz clock test - set low */
    if (!lwgpio_init(&clk32K_gpio, BSP_GPIO_BT_32KHz_EN_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_BT_32KHz_EN_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* swich pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&clk32K_gpio, BSP_GPIO_BT_32KHz_EN_MUX_GPIO);

    lwgpio_set_value(&clk32K_gpio, LWGPIO_VALUE_LOW);
    lwgpio_set_value(&clk32K_gpio, LWGPIO_VALUE_HIGH);
    lwgpio_set_value(&clk32K_gpio, LWGPIO_VALUE_LOW);

    /* Debug LED test */
    if (!lwgpio_init(&led0_gpio, BSP_GPIO_LED0_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_LED0_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* switch pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&led0_gpio, BSP_GPIO_LED0_MUX_GPIO);


    if (!lwgpio_init(&led1_gpio, BSP_GPIO_LED1_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_LED1_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* switch pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&led1_gpio, BSP_GPIO_LED1_MUX_GPIO);

    if (!lwgpio_init(&led2_gpio, BSP_GPIO_LED2_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_LED2_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* switch pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&led2_gpio, BSP_GPIO_LED2_MUX_GPIO);

    if (!lwgpio_init(&led3_gpio, BSP_GPIO_LED3_PIN, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE))
    {
        corona_print("Initializing BSP_GPIO_LED3_PIN GPIO as output failed.\n");
        _task_block();
    }

    /* switch pin functionality (MUX) to GPIO mode */
    lwgpio_set_functionality(&led3_gpio, BSP_GPIO_LED3_MUX_GPIO);

    lwgpio_set_value(&led0_gpio, LWGPIO_VALUE_HIGH);
    _time_delay(500);
    lwgpio_set_value(&led0_gpio, LWGPIO_VALUE_LOW);

    lwgpio_set_value(&led1_gpio, LWGPIO_VALUE_HIGH);
    _time_delay(500);
    lwgpio_set_value(&led1_gpio, LWGPIO_VALUE_LOW);

    lwgpio_set_value(&led2_gpio, LWGPIO_VALUE_HIGH);
    _time_delay(500);
    lwgpio_set_value(&led2_gpio, LWGPIO_VALUE_LOW);

    lwgpio_set_value(&led3_gpio, LWGPIO_VALUE_HIGH);
    _time_delay(500);
    lwgpio_set_value(&led3_gpio, LWGPIO_VALUE_LOW);

    /* wait for button press, lwevent is set in isrs */
    while (1)
    {
        fflush(stdout);

        result = _lwevent_wait_for(&gpio_int_lwevent, BUTTON_EVENT | ACC_INT1_EVENT | ACC_INT2_EVENT | CHG_B_EVENT | PGOOD_B_EVENT, FALSE, 0);

        switch (result)
        {
        case MQX_OK:
            /* Don't get value using legacy my_event.VALUE, obsolete */
            mask = _lwevent_get_signalled();
            if (mask & BUTTON_EVENT)
            {
                corona_print("BUTTON_EVENT unblocked this task.\n");
            }
            if (mask & ACC_INT1_EVENT)
            {
                corona_print("ACC_INT1_EVENT unblocked this task.\n");
            }
            if (mask & ACC_INT2_EVENT)
            {
                corona_print("ACC_INT2_EVENT unblocked this task.\n");
            }
            if (mask & CHG_B_EVENT)
            {
                corona_print("CHG_B_EVENT unblocked this task.\n");
            }
            if (mask & PGOOD_B_EVENT)
            {
                corona_print("PGOOD_B_EVENT unblocked this task.\n");
            }
            break;

        default:
            corona_print("An error %d on lwevent.\n", result);
            break;
        }
    }
}
#endif
