/**
 * @file button.c
 * @brief Generic button module — bare-metal, no hardcoded hardware.
 */

#include "button.h"
#include "gpio.h"
#include "stm32f303xc.h"

static ButtonCallback    stored_callback = 0;
static Button_Descriptor stored_desc;
static volatile int      press_flag = 0;

int button_init(const Button_Descriptor *desc, ButtonCallback callback)
{
    if (desc == 0) return -1;

    stored_callback = callback;
    stored_desc     = *desc;
    press_flag      = 0;

    /* Configure pin as input with pull-down */
    gpio_init(desc->port, desc->pin, GPIO_DIRECTION_INPUT);

    /* Enable SYSCFG clock */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* Route EXTI line to correct port */
    uint8_t reg_index  = desc->exti_line / 4;
    uint8_t bit_offset = (desc->exti_line % 4) * 4;
    SYSCFG->EXTICR[reg_index] &= ~(0xFU            << bit_offset);
    SYSCFG->EXTICR[reg_index] |=  (desc->exti_port << bit_offset);

    /* Unmask, rising edge only */
    EXTI->IMR  |=  (1U << desc->exti_line);
    EXTI->RTSR |=  (1U << desc->exti_line);
    EXTI->FTSR &= ~(1U << desc->exti_line);

    /* Use CMSIS NVIC functions — provided by stm32f303xc.h / core_cm4.h */
    NVIC_SetPriority(desc->irq_number, 2);
    NVIC_EnableIRQ(desc->irq_number);

    return 0;
}

int button_read(void)
{
    GPIO_PinState_t state;
    gpio_read(stored_desc.port, stored_desc.pin, &state);
    return (state == GPIO_PIN_HIGH) ? 1 : 0;
}

int button_get_flag(void)    { return press_flag; }
void button_clear_flag(void) { press_flag = 0; }

/* EXTI0 ISR */
void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & (1U << 0))
        EXTI->PR |= (1U << 0);   /* clear pending bit */

    press_flag = 1;

    if (stored_callback != 0)
        stored_callback();
}
