/**
 * @file servo.c
 * @brief Hardware PWM servo driver -- Exercise 2 Task c).
 *
 * Uses TIM3 Channel 1 hardware PWM output on PA6 (AF2).
 *
 * Clock: HSI = 8 MHz
 *   PSC = 7  ->  8MHz / 8 = 1 MHz (1 us per tick)
 *   ARR = 19999  ->  1MHz / 20000 = 50 Hz (20 ms period)
 *   CCR1 = pulse width in microseconds (1000-2000)
 *
 * Signal timing (as per assignment specification):
 *   CCR1 = 1000  ->  1.0 ms  ->  full clockwise
 *   CCR1 = 1500  ->  1.5 ms  ->  centre
 *   CCR1 = 2000  ->  2.0 ms  ->  full counter-clockwise
 *
 * Servo signal wire connects to PA6 on the STM32F303 Discovery board.
 *
 * Depends on: timer.c (for timer_set_period_ms / timer_get_period_ms),
 *             registers.h
 */

#include "servo.h"
#include "timer.h"
#include "gpio.h"
#include "registers.h"

// Inaccessible/private variables to this module - Requires get and set functions to access the pulse length in microseconds
static volatile uint32_t pulse_us = SERVO_POS_CENTRE_US; // Resets the pulse to the centre position

// Public API

// Initialising the servo
void servo_init(void) // Takes no inputs
{
    pulse_us = SERVO_POS_CENTRE_US; // Resets the pulse to the centre position every time the servo is initialised


    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; // Enabling the clock for TIM3 once more - However, here we are aiming to configure TIM3 for hardware PWM mode

    gpio_init_af(GPIOA, 6, 2); // Configure PA6 in alternative function mode (such that the TIM3 hardware can drive the pin directly)


    TIM3->CR1  &= ~(1U << 0); // Stops timer before configuring it
    TIM3->PSC   = 7; // Sets prescaler to 7 (8MHz/(7+1) = 1MHz), thus each tick corresponds to 1 microsecond
    TIM3->ARR   = 19999; // Sets the period to 50Hz (20 seconds) -> 1/(19999 + 2)


    TIM3->CCMR1 &= ~(0xFFU); // Wipes any previous configurations of the Capture Compare Mode Register 1 (CCMR1)  - We wipe the lowest 8 bits as they correspond to Channel 1
    TIM3->CCMR1 |=  (0x6U << 4)   // This toggles the bits corresponding to the Output Compare Mode 1 (OCM1) such that PWM Mode 1 is activated
                 |  (0x1U << 3);  // Sets Output Compare 1 Pre-load Enable, such that when we write a new position value, it takes place in the next 20ms cycle (rather than mid-cycle)


    TIM3->CCR1 = (uint32_t)pulse_us; // Setting the initial pulse width for the centre position


    TIM3->CCER |= (1U << 0); // Sets bit 0 of CCER (Capture Compare 1 Enable) to enable the CH1 output such that it reaches PA6


    TIM3->CR1 |= (1U << 7); // Sets ARPE (Auto Reload Preload Enable) which enables the pre-load register for ARR (Auto Reload Register), such that shadow registers aren't immediately written to, which may result in undesired behaviour


    TIM3->EGR |= (1U << 0); // Bit 0 of the Event Generation Reister (EGR) is Update Generation (UG). At runtime it copies everything into the shadow registers, such that there isn't one cycle of no action
    TIM3->SR  &= ~(1U << 0); // Clears the Update Interrupt Flag (UIF) such that no interrupts are fired when we enable the timer

    TIM3->CR1 |= (1U << 0); // Now that all configuration has occured we may start the counter. This line sets CEN (Counter Enable) of Control Register 1, which starts the timer.
}

void servo_set_position(uint32_t new_pulse_us) // Inputs the desired pulse width in us
{
    if (new_pulse_us < SERVO_POS_MIN_US) new_pulse_us = SERVO_POS_MIN_US; // ensures that the minimal position is 1000 us
    if (new_pulse_us > SERVO_POS_MAX_US) new_pulse_us = SERVO_POS_MAX_US; // ensures that the maximal position is 2000 us
    pulse_us   = new_pulse_us; // Sets new pulse as the pulse fed into the timer
    TIM3->CCR1 = (uint32_t)pulse_us; // Writes the new pulse width directly into CCR1. This value takes place when the next cycle begins.
}

uint32_t servo_get_position_us(void)
{
    return pulse_us; // returns the current pulse value
}
