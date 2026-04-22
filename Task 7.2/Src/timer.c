/**
 * @file timer.c
 * @brief Generic repeating timer -- TIM3
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

// Inaccessible variables outside of this file - require get and set functions for outside access
static volatile uint32_t period_ms  = 0; // Period of timer - static as it is inaccessible, and volatile to signal that it is a changing value and must be read/written to from memory constantly
static          TimerCallback cb    = 0; // Represents an empty function pointer, when we jump to the pointer of a function we check that the pointer is not empty (actually points to an address)


// TIM3 Interrupt Service Routine - Function that runs when interrupt is fired
void TIM3_IRQHandler(void) // Interrupt request handler for TIM3
{
    TIM3->SR &= ~(1U << 0);   // SR is the status register, when an interrupt fires it clears bit 0, the UIF (Update Interrupt Flag), to 0 and allows for next interrupt to fire, otherwise the ISR enters an infinite loop
    if (cb != 0) { // Checks if the call-back function is NULL (0), if it is then it skips the call,
        cb(); // Calls the function fed into it
    }
}

// Accessible Functions (Global scope)

// Initialise Timer
void timer_init(uint32_t period_ms_arg, TimerCallback callback) // Two inputs: period_ms_arg (desired period in ms), callback (function pointer for callback)
{
    period_ms = (period_ms_arg >= 1) ? period_ms_arg : 1; // Sets the period to 1 if the argument passed into the function is less than 1
    cb        = callback; // Setting cb function from argument

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; // Enables the clock for TIM3 (APB1ENR is clock enable register for APB1 Peripherals)

    __asm__("cpsid i"); // Disables the interrupts globally, such that no interrupts may fire while we are configuring the timer

    TIM3->CR1  &= ~(1U << 0); // Clears bit 0 of CR1 (the Counter Enable bit - CEN). Stops the timer so it doesn't behave unexpectedly during configuration.
    TIM3->PSC   = 7999; // Sets the pre-scaler dividing the 8MHz clock for TIM3 by 8000, such that it is 1 kHz. This means that now each 'tick' is 1s. It is +1 as the pre-scaler automatically adds 1
    TIM3->ARR   = (uint32_t)(period_ms - 1); // ARR (Auto-Reload Register). This sets the number that the counter counts up to before overflowing and firing the interrupt. If period = 500, ARR = 499, thus counter counts up to 499 and then fires.
    TIM3->EGR  |= (1U << 0); // Writing bit 0 of the EGR (Event Generation Register), loads shadow registers to allow for immediate
    TIM3->SR   &= ~(1U << 0); // Writing to the EGR may have set the status register (interrupt flag), this prevents any undesired behaviour by clearing it
    TIM3->DIER |= (1U << 0); // Enables the DMA/Interrupt Enable Register. Tells TIM3 to generate an interrupt request when it overflows (i.e. finishes one round of counting)

    NVIC_SetPriority(TIM3_IRQn, 2); // Sets the priority of the interrupt to 2, such that higher priority interrupts can act first while still maintaining a relatively high priority
    NVIC_EnableIRQ(TIM3_IRQn); // Tells the interrupt controller (NVIC) to check the priority of the Interrupt Request (IRQ) then call the ISR (Interrupt Service Routine)

    TIM3->CR1  |= (1U << 0); // Sets bit 0 of CR1 (CEN - Counter Enable) --> Counter begins counting up from 0 to ARR, then fires the interrupt and then resets

    __asm__("cpsie i"); // Enables interrupts to occur since configuration has completed
}

// Set function to change the period, since the period is static (private to this file)
void timer_set_period_ms(uint32_t new_period_ms)
{
    period_ms  = new_period_ms; // Sets period_ms to the new period (as the period is private)
    TIM3->ARR  = (uint32_t)(period_ms - 1); // Writes the new period into the Auto-Reset Register
    // Note: EGR isn't updated as immediate action isn't required, and we can wait for the next cycle to begin
}

// Get function to return the period, which is private to this module
uint32_t timer_get_period_ms(void)
{
    return period_ms;
}

// Sets the callback function such that the timer is reusable and generic --> the function called on each tick is determined by the callback function inputted
void timer_set_callback(TimerCallback callback)
{
    cb = callback;
}
// However, when attempting to use this for the PWM there was difficulty as a second timer was required (TIM4) for the falling edge. This introduced additional errors which made it difficult to acquire the correct timing to allow for the servo to be operational.
