/**
 * @file led.h
 * @brief Generic LED module — no hardcoded pins.
 *
 * Pin/port assignments are injected at init time via LED_Descriptor.
 * This allows the module to be reused on any board.
 *
 * Used by: Exercise 1, Exercise 5 (Board 2 — display heading via LEDs).
 *
 * Design principles:
 *   - No board-specific knowledge
 *   - Private state (led_state[]) only via led_get/led_set
 *   - Accepts a function pointer callback for state-change notification
 *     (useful for Exercise 5 — notify UART module when LED changes)
 */

#ifndef LED_H
#define LED_H

#include <stdint.h>
#include "gpio.h"

#define LED_MAX_COUNT 8

typedef enum {
    LED_OFF = 0,
    LED_ON  = 1
} LED_State;

/* Descriptor: port and pin for one LED — supplied by board module */
typedef struct {
    GPIO_t  *port;
    uint8_t  pin;
} LED_Descriptor;

/* Optional callback: called after any LED state change */
typedef void (*LED_ChangeCallback)(uint8_t id, LED_State new_state);

/**
 * @brief Initialise LED module with board-supplied descriptors.
 * @param descriptors  Array of LED_Descriptor
 * @param count        Number of LEDs (max LED_MAX_COUNT)
 * @param callback     Optional callback on state change. NULL to disable.
 */
void led_init(const LED_Descriptor *descriptors, uint8_t count,
              LED_ChangeCallback callback);

/**
 * @brief Set the state of one LED.
 * @param id    LED index 0..(count-1)
 * @param state LED_ON or LED_OFF
 * @return      0 on success, -1 on invalid id
 */
int led_set(uint8_t id, LED_State state);

/**
 * @brief Get the cached state of one LED.
 * @param id  LED index
 * @return    LED_ON, LED_OFF, or -1 on invalid id
 */
int led_get(uint8_t id);

/** @brief Turn all LEDs off. */
void led_clear_all(void);

/**
 * @brief Toggle one LED.
 * @param id  LED index
 * @return    0 on success, -1 on invalid id
 */
int led_toggle(uint8_t id);

/**
 * @brief Light exactly one LED corresponding to an index 0–7.
 *        All others are turned off.
 *        Used by Exercise 5 to display compass heading.
 * @param id  LED index to light
 * @return    0 on success, -1 on invalid id
 */
int led_set_single(uint8_t id);

#endif /* LED_H */
