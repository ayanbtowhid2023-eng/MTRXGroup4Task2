#include "timer.h"
#include <stm32f303xc.h>
#include <stddef.h>

// Hidden from outside - only accessible within this file
static void (*timer_callback)(void) = NULL;
static uint32_t current_period_ms = 0;

void timer_init(uint32_t period_ms, void (*callback)(void)) {

    // Store the callback and period
    timer_callback = callback;
    current_period_ms = period_ms;

    __disable_irq();

    // Enable TIM2 clock
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    // Prescaler: 8MHz / 8000 = 1000 Hz (1 tick = 1ms)
    TIM2->PSC = 7999;

    // Auto-reload: fires every period_ms ticks = period_ms milliseconds
    TIM2->ARR = period_ms - 1;

    // Enable update interrupt
    TIM2->DIER |= TIM_DIER_UIE;

    // Enable in NVIC
    NVIC_EnableIRQ(TIM2_IRQn);

    // Start the timer
    TIM2->CR1 |= TIM_CR1_CEN;

    __enable_irq();
}

void setperiod(uint32_t period_ms) {
    current_period_ms = period_ms;

    // Update the hardware register directly - no need to reinitialise
    TIM2->ARR = period_ms - 1;
}

uint32_t getperiod(void) {
    return current_period_ms;
}

// ISR - called by hardware when timer fires
void TIM2_IRQHandler(void) {
    if (TIM2->SR & TIM_SR_UIF) {
        if (timer_callback != NULL) {
            timer_callback();
        }
        TIM2->SR &= ~TIM_SR_UIF;  // clear flag AFTER calling callback
    }
}
