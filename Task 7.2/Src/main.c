#include <stm32f303xc.h>
#include "timer.h"

void chase_led(void) {
    uint8_t *led_register = ((uint8_t*)&(GPIOE->ODR)) + 1;
    *led_register <<= 1;
    if (*led_register == 0) {
        *led_register = 1;
    }
}

int main(void) {
    // Enable clocks
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOCEN | RCC_AHBENR_GPIOEEN;

    // Set PE8-15 as outputs
    uint16_t *led_output_registers = ((uint16_t *)&(GPIOE->MODER)) + 1;
    *led_output_registers = 0x5555;

    // Set initial LED state
    uint8_t *led_register = ((uint8_t*)&(GPIOE->ODR)) + 1;
    *led_register = 1;

    // --- Task a) basic timer ---
    timer_init(500, &chase_led);

    // --- Task b) get/set period ---
    //setperiod(100);                      // speed up
   //setperiod(getperiod() * 2);          // double current period

    for (;;) {}
}
