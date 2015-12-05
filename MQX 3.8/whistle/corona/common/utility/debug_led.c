#include <mqx.h>
#include <bsp.h>
#include <lwgpio.h>
#include "debug_led.h"
#include "wassert.h"

typedef struct _led_t {
	LWGPIO_STRUCT lwgpio;
	uint_32 pin;
	uint_32 mux;
	boolean is_initialized;
} led_t;

static led_t led[4] = {
		{{0}, BSP_GPIO_LED0_PIN, BSP_GPIO_LED0_MUX_GPIO, 0},
		{{0}, BSP_GPIO_LED1_PIN, BSP_GPIO_LED1_MUX_GPIO, 0},
		{{0}, BSP_GPIO_LED2_PIN, BSP_GPIO_LED2_MUX_GPIO, 0},
		{{0}, BSP_GPIO_LED3_PIN, BSP_GPIO_LED3_MUX_GPIO, 0},
};

void set_debug_led(int num, boolean on)
{
	if (!led[num].is_initialized)
	{
		wassert(lwgpio_init(&led[num].lwgpio, led[num].pin, LWGPIO_DIR_OUTPUT, LWGPIO_VALUE_NOCHANGE));

		/* switch pin functionality (MUX) to GPIO mode */
		lwgpio_set_functionality(&led[num].lwgpio, led[num].mux);
		
		led[num].is_initialized = TRUE;
	}
	
    lwgpio_set_value(&led[num].lwgpio, on ? LWGPIO_VALUE_LOW : LWGPIO_VALUE_HIGH);
}

void set_debug_led_code(int code)
{
	set_debug_led(0, 1 & code);
	set_debug_led(1, 2 & code);
	set_debug_led(2, 4 & code);
	set_debug_led(3, 8 & code);
}
