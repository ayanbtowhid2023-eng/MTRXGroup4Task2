/**
 * @file led.c
 * @brief Generic LED module — no hardcoded pins, no board knowledge.
 */

#include "led.h"
#include "gpio.h"

/* Private state */
static LED_State          led_state[LED_MAX_COUNT];
static LED_Descriptor     led_desc[LED_MAX_COUNT];
static uint8_t            led_count    = 0;
static LED_ChangeCallback led_callback = 0;

void led_init(const LED_Descriptor *descriptors, uint8_t count,
              LED_ChangeCallback callback)
{
    if (descriptors == 0) return;
    if (count > LED_MAX_COUNT) count = LED_MAX_COUNT;

    led_count    = count;
    led_callback = callback;

    for (int i = 0; i < led_count; i++) {
        led_desc[i]  = descriptors[i];
        led_state[i] = LED_OFF;
        gpio_init(led_desc[i].port, led_desc[i].pin, GPIO_DIRECTION_OUTPUT);
        gpio_write(led_desc[i].port, led_desc[i].pin, GPIO_PIN_LOW);
    }
}

int led_set(uint8_t id, LED_State state)
{
    if (id >= led_count) return -1;
    led_state[id] = state;
    gpio_write(led_desc[id].port, led_desc[id].pin,
               (state == LED_ON) ? GPIO_PIN_HIGH : GPIO_PIN_LOW);

    if (led_callback != 0)
        led_callback(id, state);

    return 0;
}

int led_get(uint8_t id)
{
    if (id >= led_count) return -1;
    return (int)led_state[id];
}

void led_clear_all(void)
{
    for (int i = 0; i < led_count; i++)
        led_set(i, LED_OFF);
}

int led_toggle(uint8_t id)
{
    if (id >= led_count) return -1;
    LED_State s = (led_state[id] == LED_ON) ? LED_OFF : LED_ON;
    return led_set(id, s);
}

int led_set_single(uint8_t id)
{
    if (id >= led_count) return -1;
    led_clear_all();
    return led_set(id, LED_ON);
}
