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
 * Task d) -- one-shot: fires once then stops.
 */

#include "timer.h"
#include "registers.h"

/* -----------------------------------------------------------------------
 * Private state -- not accessible outside this file
 * --------------------------------------------------------------------- */
static volatile uint32_t  period_ms     = 0;
static          TimerCallback cb        = 0;
static volatile int       one_shot_mode = 0;

/* -----------------------------------------------------------------------
 * TIM3 ISR
 * --------------------------------------------------------------------- */
void TIM3_IRQHandler(void)
{
    /* Clear update-interrupt flag */
    TIM3->SR &= ~(1U << 0);

    if (one_shot_mode) {
        /* Stop the timer immediately */
        TIM3->CR1 &= ~(1U << 0);
        one_shot_mode = 0;
    }

    if (cb != 0) {
        cb();
    }
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void timer_init(uint32_t period_ms_arg, TimerCallback callback)
{
    period_ms     = (period_ms_arg >= 1) ? period_ms_arg : 1;
    cb            = callback;
    one_shot_mode = 0;

    /* Enable TIM3 clock */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    __asm__("cpsid i");

    /* Stop timer while configuring */
    TIM3->CR1 &= ~(1U << 0);

    /* 8 MHz / (7999 + 1) = 1 kHz -- one tick per ms */
    TIM3->PSC = 7999;

    /* Period: ARR = period_ms - 1 */
    TIM3->ARR = (uint32_t)(period_ms - 1);

    /* Force an update to load PSC/ARR into shadow registers */
    TIM3->EGR |= (1U << 0);

    /* Clear any pending interrupt that the EGR write may have set */
    TIM3->SR &= ~(1U << 0);

    /* Enable update interrupt */
    TIM3->DIER |= (1U << 0);

    NVIC_SetPriority(TIM3_IRQn, 2);
    NVIC_EnableIRQ(TIM3_IRQn);

    /* Start timer */
    TIM3->CR1 |= (1U << 0);

    __asm__("cpsie i");
}

void timer_set_period_ms(uint32_t new_period_ms)
{
    if (new_period_ms < 1) new_period_ms = 1;
    period_ms = new_period_ms;
    TIM3->ARR = (uint32_t)(period_ms - 1);
}

uint32_t timer_get_period_ms(void)
{
    return period_ms;
}

void timer_set_callback(TimerCallback callback)
{
    cb = callback;
}

void timer_one_shot(uint32_t delay_ms, TimerCallback callback)
{
    if (delay_ms < 1) delay_ms = 1;
    cb            = callback;
    one_shot_mode = 1;
    period_ms     = delay_ms;

    TIM3->CR1 &= ~(1U << 0);

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    TIM3->PSC  = 7999;
    TIM3->ARR  = (uint32_t)(delay_ms - 1);
    TIM3->CNT  = 0;
    TIM3->EGR |= (1U << 0);
    TIM3->SR  &= ~(1U << 0);
    TIM3->DIER |= (1U << 0);

    NVIC_SetPriority(TIM3_IRQn, 2);
    NVIC_EnableIRQ(TIM3_IRQn);

    TIM3->CR1 |= (1U << 0);
}
