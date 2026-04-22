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
#include "stm32f303xc.h"

// Private call back function pointer, initialised to NULL (0)
static OneShotCallback stored_callback = 0;

// Interrupt Service Routine (ISR) for TIM4
void TIM4_IRQHandler(void) // This interrupt is triggered by the counter overflowing
{
    TIM4->SR  &= ~TIM_SR_UIF;   // Clears the update interrupt flag to prevent the ISR from firing again immediately
    TIM4->CR1 &= ~TIM_CR1_CEN;  // Stops TIM4 by clearing the CEN (Counter Enable) - This is what ensures that the interrupt is only fired once

    if (stored_callback != 0) {
        stored_callback(); // Calls the callback function after checking that it isn't empty
    }
}

// Public API

void one_shot_trigger(uint32_t delay_ms, OneShotCallback callback) // Two inputs: Delay in ms and the function pointer for the callback function
{
    if (delay_ms < 1) delay_ms = 1;

    // if (TIM4->CR1 & TIM_CR1_CEN) return;  // Reject if TIM4 is already running

    stored_callback = callback; // Saves callback function pointer so ISR can call it after the delay

    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; // Enables TIM4's clock

    TIM4->CR1  &= ~TIM_CR1_CEN;  // Stops TIM4 while configuration is occurring (just in case it was running)
    TIM4->PSC   = 7999; // Pre-scaler to scale it down from 8MHz by 8000 to 1KHz, thus one tick per millisecond
    TIM4->ARR   = (uint32_t)(delay_ms - 1); // Timer counts up from 0 to the Auto-Load Register value
    TIM4->CNT   = 0; // Resets counter explicitly to 0
    TIM4->EGR  |= TIM_EGR_UG;    // Sets the shadow registers
    TIM4->SR   &= ~TIM_SR_UIF;   // Clears status register flag
    TIM4->DIER |= TIM_DIER_UIE;  // Enables update interrupt so when TIM4 overflows it fires an interrupt

    NVIC_SetPriority(TIM4_IRQn, 2); // Sets interrupt priority as 2
    NVIC_EnableIRQ(TIM4_IRQn); // Tells the NVIC to check the interrupt priority and generate the interrupt

    TIM4->CR1  |= TIM_CR1_CEN;   // Sets CEN (Counter Enable) to begin the counter
}
