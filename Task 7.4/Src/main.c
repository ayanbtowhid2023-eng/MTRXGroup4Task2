#include "compass.h"
#include "accel.h"
#include "gyro.h"
#include "stm32f303xc.h"
#include <stddef.h>
#include <stdio.h>

MagData_t  debug_mag;
AccData_t  debug_acc;
GyroData_t debug_gyro;

static void led_init(void) {
    RCC->AHBENR  |= RCC_AHBENR_GPIOEEN;
    GPIOE->MODER |= (1U<<(8*2));
}

static void uart_init(void) {
    RCC->AHBENR  |= RCC_AHBENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    GPIOC->MODER  &= ~(3U<<(4*2));
    GPIOC->MODER  |=  (2U<<(4*2));
    GPIOC->AFR[0] &= ~(0xFU<<(4*4));
    GPIOC->AFR[0] |=  (7U <<(4*4));
    USART1->BRR = 69;
    USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart_send_string(const char *s) {
    while (*s) {
        while (!(USART1->ISR & USART_ISR_TXE));
        USART1->TDR = *s++;
    }
}

int main(void) {
    led_init();
    uart_init();

    compass_init();
    accel_init();
    gyro_init();

    for (volatile int i=0;i<5000000;i++);
    uart_send_string("READY\r\n");

    char buf[128];

    while (1) {
        compass_read();
        accel_read();
        gyro_read();

        const MagData_t  *m = compass_get_latest();
        const AccData_t  *a = accel_get_latest();
        const GyroData_t *g = gyro_get_latest();

        if (m && a && g) {
            debug_mag  = *m;
            debug_acc  = *a;
            debug_gyro = *g;

            GPIOE->ODR ^= (1U<<8);

            snprintf(buf, sizeof(buf),
                "MagX:%d MagY:%d MagZ:%d Heading:%d "
                "Roll:%d Pitch:%d "
                "GyroX:%d GyroY:%d GyroZ:%d dps\r\n",
                m->raw_x, m->raw_y, m->raw_z, (int)m->heading_deg,
                (int)a->roll_deg, (int)a->pitch_deg,
                (int)g->dps_x, (int)g->dps_y, (int)g->dps_z);
            uart_send_string(buf);
        }

        for (volatile int i=0;i<800000;i++);
    }
}
