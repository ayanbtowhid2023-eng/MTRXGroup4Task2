/**
 * @file timer.h
 * @brief Generic repeating timer module -- bare-metal, STM32F303.
 *
 * Uses TIM3. TIM2 reserved for Ex1 led_timer. TIM4 used by one_shot.c.
 *
 * Task a) -- periodic callback at configurable interval passed at init.
 * Task b) -- period private, only accessible via get/set functions.
 *
 * Clock assumption: HSI = 8 MHz.
 *   PSC = 7999  ->  1 kHz tick (1 ms per count)
 *   ARR = period_ms - 1
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include "stm32f303xc.h"

/** Callback type invoked on each timer period expiry (from ISR). */
typedef void (*TimerCallback)(void);

/**
 * @brief  Initialise TIM3 for a repeating interrupt at the given period.
 * @param  period_ms  Desired period in milliseconds (>= 1).
 * @param  callback   Function called on each expiry. NULL to disable.
 */
void timer_init(uint32_t period_ms, TimerCallback callback);

/**
 * @brief  Change the repeating period while the timer is running.
 *         Takes effect on the next cycle.
 * @param  period_ms  New period in milliseconds (>= 1).
 */
void timer_set_period_ms(uint32_t period_ms);

/**
 * @brief  Return the current period in milliseconds.
 *         period_ms is private -- this is the only external accessor.
 */
uint32_t timer_get_period_ms(void);

/**
 * @brief  Replace the periodic callback at run-time.
 *         Pass NULL to disable without stopping the timer.
 */
void timer_set_callback(TimerCallback callback);

#endif /* TIMER_H */
