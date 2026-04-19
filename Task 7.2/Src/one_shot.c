/**
 * @file one_shot.c
 * @brief One-shot timer module -- TIM4, bare-metal STM32F303.
 *
 * HSI = 8 MHz.
 * PSC  = 7999  ->  1 kHz tick (1 ms per count)
 * ARR  = delay_ms - 1
 *
 * Timer fires once, calls callback, then stops. Does not repeat.
 *
 * Task d) -- one-shot event with delay in ms and callback function pointer.
 */

#include "one_shot.h"
#include "registers.h"

/* Private: stored callback */
static OneShotCallback stored_callback = 0;

/* -----------------------------------------------------------------------
 * TIM4 ISR -- fires once then stops
 * --------------------------------------------------------------------- */
void TIM4_IRQHandler(void)
{
    TIM4->SR  &= ~(1U << 0);    /* clear update interrupt flag */
    TIM4->CR1 &= ~(1U << 0);    /* stop timer -- one-shot done */

    if (stored_callback != 0) {
        stored_callback();
    }
}

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */

void one_shot_trigger(uint32_t delay_ms, OneShotCallback callback)
{
    if (delay_ms < 1) delay_ms = 1;

    stored_callback = callback;

    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;

    TIM4->CR1  &= ~(1U << 0);           /* stop while configuring */
    TIM4->PSC   = 7999;                  /* 8MHz / 8000 = 1 kHz */
    TIM4->ARR   = (uint32_t)(delay_ms - 1);
    TIM4->CNT   = 0;
    TIM4->EGR  |= (1U << 0);            /* load shadow registers */
    TIM4->SR   &= ~(1U << 0);           /* clear pending flag */
    TIM4->DIER |= (1U << 0);            /* enable update interrupt */

    NVIC_SetPriority(TIM4_IRQn, 2);
    NVIC_EnableIRQ(TIM4_IRQn);

    TIM4->CR1  |= (1U << 0);            /* start */
}
