/**
 * @file gpio.c
 * @brief Generic GPIO module — bare-metal, shared across all exercises.
 */

#include "gpio.h"

static int validate(GPIO_TypeDef *port, uint8_t pin)
{
    if (port == 0) return -1;
    if (pin > 15)  return -1;
    return 0;
}

static int enable_clock(GPIO_TypeDef *port)
{
    if      (port == GPIOA) RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    else if (port == GPIOB) RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    else if (port == GPIOC) RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
    else if (port == GPIOD) RCC->AHBENR |= RCC_AHBENR_GPIODEN;
    else if (port == GPIOE) RCC->AHBENR |= RCC_AHBENR_GPIOEEN;
    else if (port == GPIOF) RCC->AHBENR |= RCC_AHBENR_GPIOFEN;
    else return -1;
    return 0;
}

int gpio_init(GPIO_TypeDef *port, uint8_t pin, GPIO_Direction direction)
{
    if (validate(port, pin) != 0) return -1;
    if (enable_clock(port)  != 0) return -1;

    /* Clear mode bits */
    port->MODER &= ~(0x3U << (pin * 2));

    switch (direction) {
        case GPIO_DIRECTION_OUTPUT:
            port->MODER |=  (0x1U << (pin * 2));  /* 01 = output */
            port->PUPDR &= ~(0x3U << (pin * 2));   /* no pull */
            break;
        case GPIO_DIRECTION_AF:
            port->MODER |=  (0x2U << (pin * 2));  /* 10 = alternate function */
            break;
        case GPIO_DIRECTION_INPUT:
        default:
            /* 00 = input, pull-down */
            port->PUPDR &= ~(0x3U << (pin * 2));
            port->PUPDR |=  (0x2U << (pin * 2));
            break;
    }
    return 0;
}

int gpio_init_af(GPIO_TypeDef *port, uint8_t pin, uint8_t af)
{
    if (validate(port, pin) != 0) return -1;
    if (af > 15)                  return -1;

    /* Set mode to alternate function */
    gpio_init(port, pin, GPIO_DIRECTION_AF);

    /* Set the AF number in AFR[0] (pins 0-7) or AFR[1] (pins 8-15) */
    uint8_t reg    = pin >> 3;
    uint8_t offset = (pin & 0x7) * 4;
    port->AFR[reg] &= ~(0xFU << offset);
    port->AFR[reg] |=  ((uint32_t)af << offset);

    return 0;
}

int gpio_write(GPIO_TypeDef *port, uint8_t pin, GPIO_PinState_t state)
{
    if (validate(port, pin) != 0) return -1;

    if (state == GPIO_PIN_HIGH)
        port->BSRR = (1U << pin);
    else
        port->BSRR = (1U << (pin + 16));
    return 0;
}

int gpio_read(GPIO_TypeDef *port, uint8_t pin, GPIO_PinState_t *state)
{
    if (validate(port, pin) != 0) return -1;
    if (state == 0)                return -1;

    *state = ((port->IDR >> pin) & 1U) ? GPIO_PIN_HIGH : GPIO_PIN_LOW;
    return 0;
}
