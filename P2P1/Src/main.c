/**

 *
 * TASK A: Point to gpio_init/gpio_write on PA5 — generic GPIO
 *         working on a pin that has nothing to do with the LEDs.
 *         Comment both lines out after showing.
 *
 * TASK B: Press the button — LED steps to the next one each press.
 *         Proves LED and button modules work independently.
 *
 * TASK C: Every LED step is driven purely by the button interrupt.
 *         There is no polling in the main loop — point to the empty
 *         while(1) to show this.
 *
 * TASK D: Point to discovery_led_get() in the callback — the only
 *         way to read LED state. Comment it out to show the stepping
 *         still works — state is managed privately inside led.c.
 *
 * TASK E: Press the button rapidly — LEDs can only step once every
 *         200ms no matter how fast you press.
 *         For task c demo: swap led_set_rate_limited() for
 *         discovery_led_set() in the callback to show instant response,
 *         then swap back to show the rate limiting effect.
 */

#include <stdint.h>
#include "registers.h"
#include "gpio.h"
#include "discovery.h"
#include "led_timer.h"


static uint8_t current_led = DISC_LED3;


static void on_button_press(void)
{
    /* TASK D — only way to read LED state is via discovery_led_get() */
    int state = discovery_led_get(current_led);
    (void)state;

    /* Move to next LED */
    current_led = (current_led + 1) % DISC_LED_COUNT;

    /* Turn off all LEDs then light the next one */
    discovery_led_clear_all();

    /* TASK E: rate-limited — leave in for task e demo */
    led_set_rate_limited(current_led, LED_ON);

    /* TASK C: direct set — uncomment and comment out line above for task c demo
    discovery_led_set(current_led, LED_ON);
    */
}

int main(void)
{
    /* Always leave in — required for get_tick() and task e rate limiter */
    led_timer_init(30);

    /* TASK B — initialises LED and button modules on correct board pins */
    discovery_init(on_button_press);

    /* TASK A — generic GPIO called directly on PA5, independent of LEDs.
     * Comment both lines out after demonstrating task a. */
    gpio_init(GPIOA, 5, GPIO_DIRECTION_OUTPUT);
    gpio_write(GPIOA, 5, GPIO_PIN_HIGH);

    /* Light first LED on startup so board shows something immediately */
    discovery_led_set(current_led, LED_ON);

    /* TASK C — nothing in here. Every LED change is driven by the
     * button interrupt callback above, not by this loop. */
    while (1)
    {
    }
}
