/**
 * @file registers.h
 * @brief Bare-metal register definitions for STM32F303VCTx
 *
 * No HAL. No CMSIS. No external headers.
 * All addresses taken directly from the STM32F303 Reference Manual (RM0316).
 *
 * This file is shared across ALL exercises (1-5).
 * Any exercise that needs direct register access includes this file.
 */

#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Core types
 * --------------------------------------------------------------------- */
typedef volatile uint32_t reg32;
typedef volatile uint16_t reg16;
typedef volatile uint8_t  reg8;

/* -----------------------------------------------------------------------
 * GPIO registers (RM0316 Section 11)
 * --------------------------------------------------------------------- */
typedef struct {
    reg32 MODER;
    reg32 OTYPER;
    reg32 OSPEEDR;
    reg32 PUPDR;
    reg32 IDR;
    reg32 ODR;
    reg32 BSRR;
    reg32 LCKR;
    reg32 AFR[2];
    reg32 BRR;
} GPIO_t;

/* -----------------------------------------------------------------------
 * RCC registers (RM0316 Section 9)
 * --------------------------------------------------------------------- */
typedef struct {
    reg32 CR;
    reg32 CFGR;
    reg32 CIR;
    reg32 APB2RSTR;
    reg32 APB1RSTR;
    reg32 AHBENR;
    reg32 APB2ENR;
    reg32 APB1ENR;
    reg32 BDCR;
    reg32 CSR;
    reg32 AHBRSTR;
    reg32 CFGR2;
    reg32 CFGR3;
} RCC_t;

/* -----------------------------------------------------------------------
 * EXTI registers (RM0316 Section 14)
 * --------------------------------------------------------------------- */
typedef struct {
    reg32 IMR;
    reg32 EMR;
    reg32 RTSR;
    reg32 FTSR;
    reg32 SWIER;
    reg32 PR;
} EXTI_t;

/* -----------------------------------------------------------------------
 * SYSCFG registers (RM0316 Section 12)
 * --------------------------------------------------------------------- */
typedef struct {
    reg32 CFGR1;
    reg32 RCR;
    reg32 EXTICR[4];
    reg32 CFGR2;
    reg32 RESERVED[12];
    reg32 CFGR3;
} SYSCFG_t;

/* -----------------------------------------------------------------------
 * Timer registers (RM0316 Section 21) — used by Ex1 (rate limiter),
 * Ex2 (PWM), and Ex5 (integration)
 * --------------------------------------------------------------------- */
typedef struct {
    reg32 CR1;
    reg32 CR2;
    reg32 SMCR;
    reg32 DIER;
    reg32 SR;
    reg32 EGR;
    reg32 CCMR1;
    reg32 CCMR2;
    reg32 CCER;
    reg32 CNT;
    reg32 PSC;
    reg32 ARR;
    reg32 RCR;
    reg32 CCR1;
    reg32 CCR2;
    reg32 CCR3;
    reg32 CCR4;
} TIM_t;

/* -----------------------------------------------------------------------
 * USART registers (RM0316 Section 29) — used by Ex3 and Ex5
 * --------------------------------------------------------------------- */
typedef struct {
    reg32 CR1;
    reg32 CR2;
    reg32 CR3;
    reg32 BRR;
    reg32 GTPR;
    reg32 RTOR;
    reg32 RQR;
    reg32 ISR;
    reg32 ICR;
    reg16 RDR;
    reg16 RESERVED0;
    reg16 TDR;
    reg16 RESERVED1;
} USART_t;

/* -----------------------------------------------------------------------
 * I2C registers (RM0316 Section 28) — used by Ex4 and Ex5
 * --------------------------------------------------------------------- */
typedef struct {
    reg32 CR1;
    reg32 CR2;
    reg32 OAR1;
    reg32 OAR2;
    reg32 TIMINGR;
    reg32 TIMEOUTR;
    reg32 ISR;
    reg32 ICR;
    reg32 PECR;
    reg32 RXDR;
    reg32 TXDR;
} I2C_t;

/* -----------------------------------------------------------------------
 * NVIC (ARM Cortex-M4 — fixed address)
 * --------------------------------------------------------------------- */
typedef struct {
    reg32 ISER[8];
    reg32 RESERVED0[24];
    reg32 ICER[8];
    reg32 RESERVED1[24];
    reg32 ISPR[8];
    reg32 RESERVED2[24];
    reg32 ICPR[8];
    reg32 RESERVED3[24];
    reg32 IABR[8];
    reg32 RESERVED4[56];
    uint8_t IP[240];
} NVIC_t;

/* -----------------------------------------------------------------------
 * Peripheral base addresses
 * --------------------------------------------------------------------- */
#define GPIOA    ((GPIO_t *)   0x48000000UL)
#define GPIOB    ((GPIO_t *)   0x48000400UL)
#define GPIOC    ((GPIO_t *)   0x48000800UL)
#define GPIOD    ((GPIO_t *)   0x48000C00UL)
#define GPIOE    ((GPIO_t *)   0x48001000UL)
#define GPIOF    ((GPIO_t *)   0x48001400UL)

#define RCC      ((RCC_t *)    0x40021000UL)
#define EXTI     ((EXTI_t *)   0x40010400UL)
#define SYSCFG   ((SYSCFG_t *) 0x40010000UL)

#define TIM2     ((TIM_t *)    0x40000000UL)
#define TIM3     ((TIM_t *)    0x40000400UL)
#define TIM4     ((TIM_t *)    0x40000800UL)

#define USART1   ((USART_t *)  0x40013800UL)
#define USART2   ((USART_t *)  0x40004400UL)
#define USART3   ((USART_t *)  0x40004800UL)

#define I2C1     ((I2C_t *)    0x40005400UL)
#define I2C2     ((I2C_t *)    0x40005800UL)

#define NVIC     ((NVIC_t *)   0xE000E100UL)

/* -----------------------------------------------------------------------
 * RCC clock enable bits
 * --------------------------------------------------------------------- */
#define RCC_AHBENR_GPIOAEN    (1U << 17)
#define RCC_AHBENR_GPIOBEN    (1U << 18)
#define RCC_AHBENR_GPIOCEN    (1U << 19)
#define RCC_AHBENR_GPIODEN    (1U << 20)
#define RCC_AHBENR_GPIOEEN    (1U << 21)
#define RCC_AHBENR_GPIOFEN    (1U << 22)

#define RCC_APB2ENR_SYSCFGEN  (1U << 0)
#define RCC_APB2ENR_USART1EN  (1U << 14)

#define RCC_APB1ENR_TIM2EN    (1U << 0)
#define RCC_APB1ENR_TIM3EN    (1U << 1)
#define RCC_APB1ENR_TIM4EN    (1U << 2)
#define RCC_APB1ENR_USART2EN  (1U << 17)
#define RCC_APB1ENR_USART3EN  (1U << 18)
#define RCC_APB1ENR_I2C1EN    (1U << 21)
#define RCC_APB1ENR_I2C2EN    (1U << 22)

/* -----------------------------------------------------------------------
 * NVIC helpers
 * --------------------------------------------------------------------- */
#define NVIC_EnableIRQ(irq)          (NVIC->ISER[(irq) >> 5] = (1U << ((irq) & 0x1F)))
#define NVIC_SetPriority(irq, pri)   (NVIC->IP[irq] = (uint8_t)((pri) << 4))

/* -----------------------------------------------------------------------
 * IRQ numbers for STM32F303VCTx
 * --------------------------------------------------------------------- */
#define EXTI0_IRQn       6
#define TIM2_IRQn       28
#define TIM3_IRQn       29
#define TIM4_IRQn       30
#define USART1_IRQn     37
#define USART2_IRQn     38
#define USART3_IRQn     39
#define I2C1_EV_IRQn    31
#define I2C2_EV_IRQn    33

#endif /* REGISTERS_H */
