/**
 * @file main.c
 * @brief Exercise 2 -- Timer Interface demo.
 *
 * HOW TO DEMO -- uncomment ONE define at a time:
 *
 * TASK_A_DEMO: LD3 blinks at 500ms interval via timer_init() callback.
 *              Shows periodic callback at a configurable interval (task a).
 *              Uncomment the 3 lines inside main() to also demo task b --
 *              timer_get/set_period_ms() changes blink to 100ms.
 *
 * TASK_C_DEMO: Servo sweeps CW -> centre -> CCW repeatedly on PA6.
 *              servo_init() calls timer_init(20, on_period) internally.
 *
 * TASK_D_DEMO: LD3 turns on exactly once after 1000ms via
 *              one_shot_trigger(). Proves it fires once and stops.
 *
 * Wiring:
 *   Servo brown/black -> GND
 *   Servo red         -> 5V
 *   Servo orange      -> PA6
 */

#include <stdint.h>
#include "registers.h"
#include "gpio.h"
#include "timer.h"
#include "servo.h"
#include "one_shot.h"

/* -----------------------------------------------------------------------
 * Uncomment ONE at a time for each demo
 * --------------------------------------------------------------------- */
#define TASK_A_DEMO
// #define TASK_C_DEMO
// #define TASK_D_DEMO

/* -----------------------------------------------------------------------
 * TASK A: periodic callback at configurable interval
 * TASK B: comment in the three lines below on_timer to demo get/set
 * --------------------------------------------------------------------- */
#ifdef TASK_A_DEMO

static void on_timer_a(void)
{
    /* Called every 500ms by TIM3 -- toggles LD3 */
    static int state = 0;
    if (state == 0) {
        gpio_write(GPIOE, 8, GPIO_PIN_HIGH);
        state = 1;
    } else {
        gpio_write(GPIOE, 8, GPIO_PIN_LOW);
        state = 0;
    }
}

int main(void)
{
    gpio_init(GPIOE, 8, GPIO_DIRECTION_OUTPUT);
    gpio_write(GPIOE, 8, GPIO_PIN_LOW);

    /* TASK A: interval (500ms) and callback passed at init */
    timer_init(500, on_timer_a);

    /* TASK B: uncomment these three lines to demo get/set.
     * Period is private to timer.c -- only accessible via these functions.
     * Comment them out again to go back to task a demo.              */
    // uint32_t current = timer_get_period_ms();  /* read private period */
    // (void)current;
    // timer_set_period_ms(100);                  /* change to 100ms -- faster */

    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK C: PWM servo sweep
 * --------------------------------------------------------------------- */
#elif defined(TASK_C_DEMO)

int main(void)
{
    /* TASK C: servo_init() calls timer_init(20, on_period) internally.
     * Note to tutor: timer_get/set_period_ms() used inside servo module */
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

/* -----------------------------------------------------------------------
 * TASK D: one-shot fires once then stops
 * --------------------------------------------------------------------- */
#elif defined(TASK_D_DEMO)

static void one_shot_callback(void)
{
    /* Fires exactly once after 1000ms then stops -- LD3 stays on */
    gpio_write(GPIOE, 8, GPIO_PIN_HIGH);
}

int main(void)
{
    gpio_init(GPIOE, 8, GPIO_DIRECTION_OUTPUT);
    gpio_write(GPIOE, 8, GPIO_PIN_LOW);

    /* TASK D: delay in ms and callback pointer passed as parameters */
    one_shot_trigger(1000, one_shot_callback);

    while (1) {}
}

#endif
