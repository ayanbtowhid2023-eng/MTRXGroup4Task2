/**
 * @file led_timer.h
 * @brief LED rate-limiter using TIM2 — Exercise 1 Task e.
 *
 * NOTE: Exercise 2 will implement a full generic timer module.
 * This module uses TIM2 specifically for the LED rate-limit feature
 * and also exposes get_tick() which is used as a system millisecond
 * counter by main.c for delays.
 *
 * TIM3 and TIM4 are left free for Exercise 2's PWM/servo use.
 *
 * led_set_rate_limited() returns immediately — never blocks.
 * TIM2 ISR applies the queued change after the minimum interval.
 */

#ifndef LED_TIMER_H
#define LED_TIMER_H

#include <stdint.h>
#include "led.h"

/**
 * @brief Initialise TIM2 for 1 ms system tick + LED rate limiting.
 * @param min_interval_ms  Minimum ms between LED state changes.
 */
void led_timer_init(uint32_t min_interval_ms);

/**
 * @brief Queue an LED change — applied by ISR after rate-limit window.
 *        Returns immediately (non-blocking).
 * @param led_id  LED index (DISC_LED3 … DISC_LED10)
 * @param state   Desired state
 */
void led_set_rate_limited(uint8_t led_id, LED_State state);

/**
 * @brief Returns current millisecond tick count since led_timer_init().
 *        Used by main.c for non-blocking delays and by Exercise 4
 *        for magnetometer data timestamps.
 */
uint32_t get_tick(void);

#endif /* LED_TIMER_H */
