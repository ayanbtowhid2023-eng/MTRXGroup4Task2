#include "gyro.h"
#include <string.h>

#define GYRO_CTRL1   0x20
#define GYRO_CTRL4   0x23
#define GYRO_OUT_X_L 0x28

static GyroData_t latest_gyro;
static uint8_t    initialised = 0;
static int16_t    bias_x = 0;
static int16_t    bias_y = 0;
static int16_t    bias_z = 0;

static uint8_t spi_transfer(uint8_t data) {
    uint32_t timeout = 100000;
    while (!(SPI1->SR & SPI_SR_TXE)) if (--timeout == 0) return 0;
    *(volatile uint8_t*)&SPI1->DR = data;
    timeout = 100000;
    while (!(SPI1->SR & SPI_SR_RXNE)) if (--timeout == 0) return 0;
    return *(volatile uint8_t*)&SPI1->DR;
}

static void gyro_write_reg(uint8_t reg, uint8_t val) {
    GPIOE->BRR  = (1U<<3);
    spi_transfer(reg & 0x7F);
    spi_transfer(val);
    GPIOE->BSRR = (1U<<3);
}

static void gyro_read_regs(uint8_t reg, uint8_t *buf, uint8_t n) {
    GPIOE->BRR  = (1U<<3);
    spi_transfer(reg | 0xC0);
    for (uint8_t i = 0; i < n; i++) buf[i] = spi_transfer(0x00);
    GPIOE->BSRR = (1U<<3);
}

void gyro_init(void) {
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    RCC->AHBENR  |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOEEN;

    // PA5=SCK, PA6=MISO, PA7=MOSI AF5
    GPIOA->MODER   &= ~((3U<<(5*2))|(3U<<(6*2))|(3U<<(7*2)));
    GPIOA->MODER   |=  (2U<<(5*2))|(2U<<(6*2))|(2U<<(7*2));
    GPIOA->OSPEEDR |=  (3U<<(5*2))|(3U<<(6*2))|(3U<<(7*2));
    GPIOA->AFR[0]  &= ~((0xFU<<(5*4))|(0xFU<<(6*4))|(0xFU<<(7*4)));
    GPIOA->AFR[0]  |=  (5U<<(5*4))|(5U<<(6*4))|(5U<<(7*4));

    // PE3 CS high
    GPIOE->MODER &= ~(3U<<(3*2));
    GPIOE->MODER |=  (1U<<(3*2));
    GPIOE->BSRR   =  (1U<<3);

    SPI1->CR1 = 0;
    SPI1->CR2 = (7U<<8) | SPI_CR2_FRXTH;
    SPI1->CR1 = SPI_CR1_MSTR|(3U<<3)|SPI_CR1_SSM|SPI_CR1_SSI|SPI_CR1_SPE;

    for (volatile int i=0;i<10000;i++);

    gyro_write_reg(GYRO_CTRL1, 0x0F);  // 95Hz, normal, XYZ on
    gyro_write_reg(GYRO_CTRL4, 0x10);  // 500dps

    for (volatile int i=0;i<100000;i++);

    // Bias calibration — keep board still during startup
    int32_t bx = 0, by = 0, bz = 0;
    uint8_t gbuf[6];
    for (int i = 0; i < 200; i++) {
        gyro_read_regs(GYRO_OUT_X_L, gbuf, 6);
        bx += (int16_t)((gbuf[1]<<8)|gbuf[0]);
        by += (int16_t)((gbuf[3]<<8)|gbuf[2]);
        bz += (int16_t)((gbuf[5]<<8)|gbuf[4]);
        for (volatile int d = 0; d < 10000; d++);
    }
    bias_x = (int16_t)(bx / 200);
    bias_y = (int16_t)(by / 200);
    bias_z = (int16_t)(bz / 200);

    memset(&latest_gyro, 0, sizeof(GyroData_t));
    initialised = 1;
}

uint8_t gyro_read(void) {
    if (!initialised) return 0;
    uint8_t buf[6];
    gyro_read_regs(GYRO_OUT_X_L, buf, 6);
    latest_gyro.raw_x = (int16_t)((buf[1]<<8)|buf[0]) - bias_x;
    latest_gyro.raw_y = (int16_t)((buf[3]<<8)|buf[2]) - bias_y;
    latest_gyro.raw_z = (int16_t)((buf[5]<<8)|buf[4]) - bias_z;
    latest_gyro.dps_x = (float)latest_gyro.raw_x * 0.0175f;
    latest_gyro.dps_y = (float)latest_gyro.raw_y * 0.0175f;
    latest_gyro.dps_z = (float)latest_gyro.raw_z * 0.0175f;
    return 1;
}

const GyroData_t* gyro_get_latest(void) {
    if (!initialised) return NULL;
    return &latest_gyro;
}
