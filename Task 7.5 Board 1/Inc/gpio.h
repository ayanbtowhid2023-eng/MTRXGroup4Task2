/**
 * @file gpio.h
 * @brief Generic GPIO module — shared across all exercises.
 *
 * Works on any GPIO port and pin on the STM32F303.
 * Used by: Exercise 1 (LEDs/button), Exercise 2 (PWM pin),
 *          Exercise 3 (UART pins), Exercise 4 (I2C pins), Exercise 5 (all).
 *
 * Design principles:
 *   - Single responsibility: only handles GPIO pin config/read/write
 *   - No board-specific knowledge (no hardcoded pins)
 *   - Returns error codes so callers can handle failure
 */

#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include "stm32f303xc.h"

typedef enum {
    GPIO_DIRECTION_INPUT  = 0,
    GPIO_DIRECTION_OUTPUT = 1,
    GPIO_DIRECTION_AF     = 2    /* Alternate function — used by UART/I2C/PWM */
} GPIO_Direction;

typedef enum {
    GPIO_PIN_LOW  = 0,
    GPIO_PIN_HIGH = 1
} GPIO_PinState_t;

/**
 * @brief Initialise a GPIO pin.
 * @param port      GPIOx pointer (e.g. GPIOA)
 * @param pin       Pin number 0–15
 * @param direction Input, output, or alternate function
 * @return          0 on success, -1 on invalid args
 */
int gpio_init(GPIO_TypeDef *port, uint8_t pin, GPIO_Direction direction);

/**
 * @brief Initialise a GPIO pin in alternate function mode with a specific AF.
 * @param port  GPIOx pointer
 * @param pin   Pin number 0–15
 * @param af    Alternate function number 0–15 (see datasheet)
 * @return      0 on success, -1 on invalid args
 */
int gpio_init_af(GPIO_TypeDef *port, uint8_t pin, uint8_t af);

/**
 * @brief Write a value to an output GPIO pin.
 * @param port  GPIOx pointer
 * @param pin   Pin number 0–15
 * @param state GPIO_PIN_HIGH or GPIO_PIN_LOW
 * @return      0 on success, -1 on invalid args
 */
int gpio_write(GPIO_TypeDef *port, uint8_t pin, GPIO_PinState_t state);

/**
 * @brief Read the current state of a GPIO pin.
 * @param port   GPIOx pointer
 * @param pin    Pin number 0–15
 * @param state  Pointer to store result
 * @return       0 on success, -1 on invalid args
 */
int gpio_read(GPIO_TypeDef *port, uint8_t pin, GPIO_PinState_t *state);

#endif /* GPIO_H */
