/**
 * @file discovery.c
 * @brief STM32F3 Discovery board-specific implementation.
 *
 * All pin numbers and port addresses for this board are defined HERE ONLY.
 * Porting to a new board = changing only this file.
 */

#include "discovery.h"
#include "stm32f303xc.h"
#include "gpio.h"
#include "led.h"
#include "button.h"

/* -----------------------------------------------------------------------
 * Private: board-specific LED descriptors
 * --------------------------------------------------------------------- */
static const LED_Descriptor discovery_leds[DISC_LED_COUNT] = {
    { GPIOE,  8 },   /* DISC_LED3  */
    { GPIOE,  9 },   /* DISC_LED4  */
    { GPIOE, 10 },   /* DISC_LED5  */
    { GPIOE, 11 },   /* DISC_LED6  */
    { GPIOE, 12 },   /* DISC_LED7  */
    { GPIOE, 13 },   /* DISC_LED8  */
    { GPIOE, 14 },   /* DISC_LED9  */
    { GPIOE, 15 },   /* DISC_LED10 */
};

/* -----------------------------------------------------------------------
 * Private: board-specific button descriptor
 * B1 = PA0, EXTI line 0, Port A (0), IRQ EXTI0
 * irq_number is IRQn_Type from stm32f303xc.h
 * --------------------------------------------------------------------- */
static const Button_Descriptor discovery_button = {
    .port       = GPIOA,
    .pin        = 0,
    .exti_line  = 0,
    .exti_port  = 0,
    .irq_number = EXTI0_IRQn
};

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void discovery_init(ButtonCallback button_callback)
{
    led_init(discovery_leds, DISC_LED_COUNT, 0);
    button_init(&discovery_button, button_callback);
}

void discovery_led_set(uint8_t led_id, LED_State state)
{
    led_set(led_id, state);
}

int discovery_led_get(uint8_t led_id)
{
    return led_get(led_id);
}

void discovery_led_clear_all(void)
{
    led_clear_all();
}

void discovery_led_set_single(uint8_t led_id)
{
    led_set_single(led_id);
}

int discovery_button_pressed(void)
{
    if (button_get_flag()) {
        button_clear_flag();
        return 1;
    }
    return 0;
}
