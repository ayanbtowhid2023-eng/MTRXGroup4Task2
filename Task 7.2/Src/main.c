/*
 @file main.c
 @brief Exercise 2 -- Timer Interface demo.

 HOW TO DEMO -- uncomment ONE define at a time:

 TASK_A_DEMO: LD3 blinks at 500ms interval via timer_init() callback.
              Shows periodic callback at a configurable interval (task a).
              Uncomment the 3 lines inside main() to also demo task b --
              timer_get/set_period_ms() changes blink to 100ms.

  TASK_C_DEMO: Servo sweeps CW -> centre -> CCW repeatedly on PA6.
              servo_init() configures TIM3 hardware PWM directly.

  TASK_D_DEMO: LD3 turns on exactly once after 1000ms via
               one_shot_trigger(). Proves it fires once and stops.

  Wiring:
   Servo brown/black -> GND
   Servo red         -> 5V
   Servo orange      -> PA6
 */

#include <stdint.h>
#include "stm32f303xc.h"
#include "gpio.h"
#include "timer.h"
#include "servo.h"
#include "one_shot.h"

// Un-comment 1 at a time for each demo

//#define TASK_A_DEMO
//#define TASK_C_DEMO
 #define TASK_D_DEMO


// Tasks A and B
#ifdef TASK_A_DEMO

static void on_timer_a(void) // Call-back function for toggling LED's
{
    static int state = 0; // initialises state as off
    if (state == 0) {
        gpio_write(GPIOE, 8, GPIO_PIN_HIGH); // Turns LED on
        state = 1;
    } else {
        gpio_write(GPIOE, 8, GPIO_PIN_LOW);  // Turns the LED off
        state = 0;
    }
}

int main(void)
{
    gpio_init(GPIOE, 8, GPIO_DIRECTION_OUTPUT);  // Initialises GPIOE clock by enabling it and sets MODER bits for pin 8 to output mode
    gpio_write(GPIOE, 8, GPIO_PIN_LOW); // Drives low to ensure the LED starts off before the timer starts

    // TASK A: interval (500ms) and callback passed at init
    timer_init(500, on_timer_a);

    // TASK B: un-comment these three lines to demo get/set.
    // uint32_t current = timer_get_period_ms();  // Gets the current period
    //(void)current; // Removes memory warning
    // timer_set_period_ms(100); // Sets the new period

    while (1) {}
}

// Task C
#elif defined(TASK_C_DEMO)

int main(void)
{
    /* TASK C: servo_init() configures TIM3 hardware PWM on PA6 (AF2).
     * servo_set_position() writes directly to TIM3->CCR1.
     * servo_get_position_us() is the only way to read back pulse width. */
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

// Task D
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
    one_shot_trigger(5000, one_shot_callback);

    while (1) {}
}

#endif
