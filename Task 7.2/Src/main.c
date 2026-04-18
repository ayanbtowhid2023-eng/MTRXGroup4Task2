/**
 * @file main.c
 * @brief Exercise 2 -- Hardware PWM servo demo.
 *
 * Sweeps servo CW -> centre -> CCW repeatedly.
 *
 * Wiring:
 *   Servo brown/black -> GND (Discovery board GND)
 *   Servo red         -> 5V  (Discovery board 5V)
 *   Servo orange      -> PA6 (TIM3 CH1 hardware PWM output)
 *
 * NOTE: signal wire must be on PA6, NOT PA1.
 */

#include <stdint.h>
#include "registers.h"
#include "timer.h"
#include "servo.h"

int main(void)
{
    servo_init();

    while (1)
    {
        servo_set_position(SERVO_POS_MIN_US);       /* 1 ms -- full CW */
        for (volatile int i = 0; i < 800000; i++);

        servo_set_position(SERVO_POS_CENTRE_US);    /* 1.5 ms -- centre */
        for (volatile int i = 0; i < 800000; i++);

        servo_set_position(SERVO_POS_MAX_US);       /* 2 ms -- full CCW */
        for (volatile int i = 0; i < 800000; i++);
    }
}
