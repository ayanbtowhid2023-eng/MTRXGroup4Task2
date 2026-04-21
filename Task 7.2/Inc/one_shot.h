/**
 * @file one_shot.h
 * @brief One-shot timer module -- bare-metal, STM32F303.
 *
 * Uses TIM4. Fires a callback once after a given delay then stops.
 * TIM3 is left free for the repeating timer module (timer.c).
 *
 * Task d) -- one-shot event: timer resets, elapses, triggers callback,
 *            then does not continue.
 *
 * Clock assumption: HSI = 8 MHz.
 *   PSC = 7999  ->  1 kHz tick (1 ms per count)
 *   ARR = delay_ms - 1
 */

#ifndef ONE_SHOT_H
#define ONE_SHOT_H

#include <stdint.h>

/** Callback type invoked when the one-shot delay elapses. */
typedef void (*OneShotCallback)(void);

/**
 * @brief  Trigger a one-shot event after delay_ms milliseconds.
 *         The callback is called exactly once then the timer stops.
 *
 * @param  delay_ms  Delay in milliseconds (>= 1).
 * @param  callback  Function to call when delay elapses.
 */
void one_shot_trigger(uint32_t delay_ms, OneShotCallback callback);

#endif /* ONE_SHOT_H */
