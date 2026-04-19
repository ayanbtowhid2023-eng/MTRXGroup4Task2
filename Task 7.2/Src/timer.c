/**
 * @file timer.c
 * @brief Generic repeating timer -- TIM3, bare-metal STM32F303.
 *
 * HSI = 8 MHz.
 * PSC  = 7999  ->  TIM3 clock = 1 kHz  (1 tick = 1 ms)
 * ARR  = period_ms - 1
 *
 * Task a) -- periodic callback at configurable interval.
 * Task b) -- period private, only exposed via get/set functions.
 */

#include "timer.h"
#include "registers.h"

/* -----------------------------------------------------------------------
 * Private state -- not accessible outside this file
 * --------------------------------------------------------------------- */
static volatile uint32_t period_ms  = 0;
static          TimerCallback cb    = 0;

/* -----------------------------------------------------------------------
 * TIM3 ISR
 * --------------------------------------------------------------------- */
void TIM3_IRQHandler(void)
{
    TIM3->SR &= ~(1U << 0);   /* clear update interrupt flag */
    if (cb != 0) {
        cb();
    }
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void timer_init(uint32_t period_ms_arg, TimerCallback callback)
{
    period_ms = (period_ms_arg >= 1) ? period_ms_arg : 1;
    cb        = callback;

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    __asm__("cpsid i");

    TIM3->CR1  &= ~(1U << 0);          /* stop while configuring */
    TIM3->PSC   = 7999;                 /* 8MHz / 8000 = 1 kHz */
    TIM3->ARR   = (uint32_t)(period_ms - 1);
    TIM3->EGR  |= (1U << 0);           /* load shadow registers */
    TIM3->SR   &= ~(1U << 0);          /* clear pending flag */
    TIM3->DIER |= (1U << 0);           /* enable update interrupt */

    NVIC_SetPriority(TIM3_IRQn, 2);
    NVIC_EnableIRQ(TIM3_IRQn);

    TIM3->CR1  |= (1U << 0);           /* start */

    __asm__("cpsie i");
}

void timer_set_period_ms(uint32_t new_period_ms)
{
    if (new_period_ms < 1) new_period_ms = 1;
    period_ms  = new_period_ms;
    TIM3->ARR  = (uint32_t)(period_ms - 1);
}

uint32_t timer_get_period_ms(void)
{
    return period_ms;
}

void timer_set_callback(TimerCallback callback)
{
    cb = callback;
}
