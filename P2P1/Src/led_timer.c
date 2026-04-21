/**
 * @file led_timer.c
 * @brief LED rate-limiter + system tick using TIM2.
 *
 * HSI = 8 MHz. PSC=7999 → 1 kHz. ARR=1 → ISR every 1 ms.
 * TIM3/TIM4 left free for Exercise 2.
 */

#include "led_timer.h"
#include "led.h"
#include "registers.h"

static volatile uint32_t tick_ms      = 0;
static          uint32_t min_interval = 200;
static          uint32_t last_change  = 0;

static volatile int       pending_valid = 0;
static volatile uint8_t   pending_id;
static volatile LED_State  pending_state;

void led_timer_init(uint32_t min_interval_ms)
{
    min_interval = min_interval_ms;

    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->PSC   = 7999;
    TIM2->ARR   = 1;
    TIM2->CNT   = 0;
    TIM2->DIER |= (1U << 0);

    NVIC_SetPriority(TIM2_IRQn, 3);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= (1U << 0);
}

void led_set_rate_limited(uint8_t led_id, LED_State state)
{
    pending_id    = led_id;
    pending_state = state;
    pending_valid = 1;
}

uint32_t get_tick(void)
{
    return tick_ms;
}

void TIM2_IRQHandler(void)
{
    TIM2->SR &= ~(1U << 0);
    tick_ms++;

    if (!pending_valid) return;
    if ((tick_ms - last_change) >= min_interval) {
        led_set(pending_id, pending_state);
        last_change   = tick_ms;
        pending_valid = 0;
    }
}
