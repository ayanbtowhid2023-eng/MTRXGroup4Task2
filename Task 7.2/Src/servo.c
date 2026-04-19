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

/* Private: current pulse width in microseconds */
static volatile uint32_t pulse_us = SERVO_POS_CENTRE_US;

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void servo_init(void)
{
    pulse_us = SERVO_POS_CENTRE_US;

    /* 1. Enable TIM3 clock */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* 2. Configure PA6 as AF2 (TIM3_CH1) using gpio module from Ex1 */
    gpio_init_af(GPIOA, 6, 2);

    /* 3. Configure TIM3 timebase
     *    PSC = 7  ->  8MHz / 8 = 1 MHz (1 tick = 1 us)
     *    ARR = 19999  ->  50 Hz (20 ms period)
     */
    TIM3->CR1  &= ~(1U << 0);     /* stop while configuring */
    TIM3->PSC   = 7;
    TIM3->ARR   = 19999;

    /* 4. Configure TIM3 CH1 in PWM mode 1
     *    CCMR1 bits 6:4 = 110 (PWM mode 1) + bit 3 = preload enable
     */
    TIM3->CCMR1 &= ~(0xFFU);
    TIM3->CCMR1 |=  (0x6U << 4)   /* OC1M = PWM mode 1 */
                 |  (0x1U << 3);   /* OC1PE = preload enable */

    /* 5. Set initial pulse width */
    TIM3->CCR1 = (uint32_t)pulse_us;

    /* 6. Enable CH1 output (CCER bit 0) */
    TIM3->CCER |= (1U << 0);

    /* 7. Enable auto-reload preload */
    TIM3->CR1 |= (1U << 7);       /* ARPE */

    /* 8. Force update to load shadow registers */
    TIM3->EGR |= (1U << 0);
    TIM3->SR  &= ~(1U << 0);

    /* 9. Start timer */
    TIM3->CR1 |= (1U << 0);
}

void servo_set_position(uint32_t new_pulse_us)
{
    if (new_pulse_us < SERVO_POS_MIN_US) new_pulse_us = SERVO_POS_MIN_US;
    if (new_pulse_us > SERVO_POS_MAX_US) new_pulse_us = SERVO_POS_MAX_US;
    pulse_us   = new_pulse_us;
    TIM3->CCR1 = (uint32_t)pulse_us;
}

uint32_t servo_get_position_us(void)
{
    return pulse_us;
}
