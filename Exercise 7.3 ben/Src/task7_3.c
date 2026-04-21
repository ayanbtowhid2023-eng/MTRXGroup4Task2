#include "stm32f303xc.h"
#include "serial.h"

/*
 * Known-good baseline: just blinks PE8 and prints "UART OK" repeatedly.
 * Demonstrates Part a (byte TX) and Part b (sendString) of Exercise 7.3.
 */

static void delay(volatile uint32_t d)
{
    while (d--)
    {
        __asm__("nop");
    }
}

static void gpio_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOEEN;

    /* PA0 = user button input */
    GPIOA->MODER &= ~(3U << 0);

    /* PE8 = LED output */
    GPIOE->MODER &= ~(3U << (8U * 2U));
    GPIOE->MODER |=  (1U << (8U * 2U));
}

int main(void)
{
    gpio_init();
    serial_init();

    while (1)
    {
        GPIOE->ODR ^= (1U << 8);
        sendString("UART OK\r\n");
        delay(800000);
    }
}
