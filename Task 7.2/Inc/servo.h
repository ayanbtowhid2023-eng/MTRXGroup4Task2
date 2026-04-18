/**
 * @file servo.h
 * @brief Hardware PWM servo driver -- Exercise 2 Task c).
 *
 * Uses TIM3 Channel 1 hardware PWM on PA6 (AF2).
 *
 * Signal timing (as per assignment specification):
 *   1.0 ms  ->  full clockwise       (SERVO_POS_MIN_US)
 *   1.5 ms  ->  centre               (SERVO_POS_CENTRE_US)
 *   2.0 ms  ->  full counter-CW      (SERVO_POS_MAX_US)
 *
 * Connect servo signal wire to PA6 on the Discovery board.
 *
 * Depends on: timer.c, registers.h
 */

#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include "registers.h"

/** Pulse width limits in microseconds -- as per assignment specification */
#define SERVO_POS_MIN_US    1000U   /* full clockwise      */
#define SERVO_POS_CENTRE_US 1500U   /* centre              */
#define SERVO_POS_MAX_US    2000U   /* full counter-CW     */

/**
 * @brief  Initialise hardware PWM on TIM3 CH1 / PA6 for servo control.
 *         Servo starts at centre position (1.5 ms).
 */
void servo_init(void);

/**
 * @brief  Set servo position by pulse width in microseconds.
 *         Clamped to [SERVO_POS_MIN_US, SERVO_POS_MAX_US].
 * @param  pulse_us  1000 (full CW) to 2000 (full CCW)
 */
void servo_set_position(uint32_t pulse_us);

/**
 * @brief  Get current pulse width in microseconds.
 *         pulse_us is private -- this is the only external accessor.
 */
uint32_t servo_get_position_us(void);

#endif /* SERVO_H */
