/**
 * @file discovery.h
 * @brief STM32F3 Discovery board-specific module.
 *
 * THE ONLY FILE that knows the physical hardware layout:
 *   LEDs  LD3–LD10  → GPIOE, PE8–PE15
 *   Button B1       → GPIOA, PA0, EXTI0
 *
 * All other modules (gpio, led, button) are generic and reusable.
 * This module wires them to the Discovery board hardware.
 *
 * Used by:
 *   Exercise 1 — primary user
 *   Exercise 5 — Board 2 calls discovery_led_set_single() to show heading
 *              — Board 1 calls discovery_button_pressed() to check for press
 *
 * Design principles demonstrated:
 *   Abstraction    — hardware hidden behind discovery_init()
 *   Encapsulation  — pin maps private to discovery.c
 *   Modularity     — one module, one board
 *   Reuse          — led/button/gpio modules unchanged when board changes
 */

#ifndef DISCOVERY_H
#define DISCOVERY_H

#include "led.h"
#include "button.h"

/* LED index names matching Discovery board silkscreen */
#define DISC_LED3   0   /* PE8  — orange */
#define DISC_LED4   1   /* PE9  — green  */
#define DISC_LED5   2   /* PE10 — orange */
#define DISC_LED6   3   /* PE11 — green  */
#define DISC_LED7   4   /* PE12 — orange */
#define DISC_LED8   5   /* PE13 — green  */
#define DISC_LED9   6   /* PE14 — orange */
#define DISC_LED10  7   /* PE15 — green  */
#define DISC_LED_COUNT 8

/**
 * @brief Initialise all Discovery board peripherals (LEDs + button).
 *        Single entry point — replaces separate led_init / button_init calls.
 *
 * @param button_callback  Called on button press (from ISR). NULL to disable.
 */
void discovery_init(ButtonCallback button_callback);

/**
 * @brief Set a Discovery LED on or off.
 * @param led_id  DISC_LED3 … DISC_LED10
 * @param state   LED_ON or LED_OFF
 */
void discovery_led_set(uint8_t led_id, LED_State state);

/**
 * @brief Get the current state of a Discovery LED.
 * @param led_id  DISC_LED3 … DISC_LED10
 * @return        LED_ON, LED_OFF, or -1 on error
 */
int discovery_led_get(uint8_t led_id);

/** @brief Turn all Discovery LEDs off. */
void discovery_led_clear_all(void);

/**
 * @brief Light one LED, turn all others off.
 *        Used by Exercise 5 to display compass heading direction.
 * @param led_id  DISC_LED3 … DISC_LED10
 */
void discovery_led_set_single(uint8_t led_id);

/**
 * @brief Returns 1 if the button has been pressed since last check.
 *        Used by Exercise 5 (Board 1) to flag mode change in UART message.
 *        Automatically clears the flag after reading.
 */
int discovery_button_pressed(void);

#endif /* DISCOVERY_H */
