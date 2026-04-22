/**
 * @file button.h
 * @brief Generic button module — hardware-independent.
 *
 * Port, pin, and EXTI config supplied by board module at init time.
 *
 * Used by: Exercise 1, Exercise 5 (Board 1 — button triggers LED/servo mode).
 *
 * Key addition for Exercise 5:
 *   button_get_flag() / button_clear_flag() allow other modules (e.g. UART)
 *   to check if the button was pressed and package it into a message,
 *   without needing direct access to the callback.
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include "stm32f303xc.h"
#include "gpio.h"

/* Callback type: called on button press (from ISR — keep short) */
typedef void (*ButtonCallback)(void);

/* Descriptor: all hardware details supplied by board module */
typedef struct {
    GPIO_TypeDef *port;
    uint8_t       pin;
    uint8_t       exti_line;
    uint8_t       exti_port;   /* 0=PA, 1=PB, 2=PC, 3=PD, 4=PE */
    IRQn_Type     irq_number;  /* IRQn_Type from stm32f303xc.h  */
} Button_Descriptor;

/**
 * @brief Initialise button from board-supplied descriptor.
 * @param desc      Hardware descriptor
 * @param callback  Called on press (from ISR). NULL to disable.
 * @return          0 on success, -1 on error
 */
int button_init(const Button_Descriptor *desc, ButtonCallback callback);

/**
 * @brief Poll current button state (non-interrupt).
 * @return 1 if pressed, 0 if released
 */
int button_read(void);

/**
 * @brief Returns 1 if button has been pressed since last clear.
 *        Used by Exercise 5 to check if button was pressed before
 *        packaging a UART message.
 */
int button_get_flag(void);

/**
 * @brief Clear the button press flag.
 *        Call after reading the flag to reset it.
 */
void button_clear_flag(void);

#endif /* BUTTON_H */
