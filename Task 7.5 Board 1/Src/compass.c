#include "compass.h"
#include <math.h>
#include <string.h>

#define MAG_ADDR     0x1E
#define MAG_CFG_A    0x60
#define MAG_CFG_B    0x61
#define MAG_CFG_C    0x62
#define MAG_OUTX_L   0x68
#define MAG_WHO_AM_I 0x4F

static MagData_t latest_mag;
static uint8_t   initialised = 0;
static uint32_t  read_count  = 0;
uint8_t          compass_verify = 0;

static uint8_t i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    uint32_t timeout;
    timeout = 100000;
    while (I2C1->ISR & I2C_ISR_BUSY) if (--timeout == 0) return 0;
    I2C1->CR2 = ((uint32_t)(addr<<1) & I2C_CR2_SADD)|(2UL<<16)|I2C_CR2_AUTOEND|I2C_CR2_START;
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS)) if (--timeout == 0) return 0;
    I2C1->TXDR = reg;
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS)) if (--timeout == 0) return 0;
    I2C1->TXDR = val;
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_STOPF)) if (--timeout == 0) return 0;
    I2C1->ICR |= I2C_ICR_STOPCF;
    return 1;
}

static uint8_t i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
    uint32_t timeout;
    timeout = 100000;
    while (I2C1->ISR & I2C_ISR_BUSY) if (--timeout == 0) return 0;
    I2C1->CR2 = ((uint32_t)(addr<<1) & I2C_CR2_SADD)|(1UL<<16)|I2C_CR2_START;
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TXIS)) if (--timeout == 0) return 0;
    I2C1->TXDR = reg;
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_TC)) if (--timeout == 0) return 0;
    I2C1->CR2 = ((uint32_t)(addr<<1) & I2C_CR2_SADD)|((uint32_t)n<<16)|I2C_CR2_RD_WRN|I2C_CR2_AUTOEND|I2C_CR2_START;
    for (uint8_t i = 0; i < n; i++) {
        timeout = 100000;
        while (!(I2C1->ISR & I2C_ISR_RXNE)) if (--timeout == 0) return 0;
        buf[i] = (uint8_t)I2C1->RXDR;
    }
    timeout = 100000;
    while (!(I2C1->ISR & I2C_ISR_STOPF)) if (--timeout == 0) return 0;
    I2C1->ICR |= I2C_ICR_STOPCF;
    return 1;
}

void compass_init(void) {
    SCB->CPACR |= ((3UL<<(10*2))|(3UL<<(11*2)));

    // I2C bus recovery
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    GPIOB->MODER &= ~((3U<<(6*2))|(3U<<(7*2)));
    GPIOB->MODER |=  (1U<<(6*2));
    for (int i = 0; i < 9; i++) {
        GPIOB->BSRR = (1U<<6); for (volatile int d=0;d<1000;d++);
        GPIOB->BRR  = (1U<<6); for (volatile int d=0;d<1000;d++);
    }
    GPIOB->BRR  = (1U<<7);
    GPIOB->BSRR = (1U<<6); for (volatile int d=0;d<1000;d++);
    GPIOB->BSRR = (1U<<7);

    // I2C1
    RCC->AHBENR  |= RCC_AHBENR_GPIOBEN;
    RCC->CFGR3   &= ~RCC_CFGR3_I2C1SW;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    GPIOB->MODER   &= ~((3U<<(6*2))|(3U<<(7*2)));
    GPIOB->MODER   |=  (2U<<(6*2))|(2U<<(7*2));
    GPIOB->OTYPER  &= ~((1U<<6)|(1U<<7));
    GPIOB->OSPEEDR |=  (3U<<(6*2))|(3U<<(7*2));
    GPIOB->PUPDR   &= ~((3U<<(6*2))|(3U<<(7*2)));
    GPIOB->PUPDR   |=  (2U<<(6*2))|(2U<<(7*2));
    GPIOB->AFR[0]  &= ~((0xFU<<(6*4))|(0xFU<<(7*4)));
    GPIOB->AFR[0]  |=  (4U<<(6*4))|(4U<<(7*4));
    I2C1->CR1    &= ~I2C_CR1_PE;
    I2C1->TIMINGR = 0x2000090E;
    I2C1->CR1    |= I2C_CR1_PE;

    for (volatile int i=0;i<100000;i++);

    if (!i2c_write_reg(MAG_ADDR, MAG_CFG_A, 0x00)) return;
    if (!i2c_write_reg(MAG_ADDR, MAG_CFG_B, 0x00)) return;
    if (!i2c_write_reg(MAG_ADDR, MAG_CFG_C, 0x00)) return;

    for (volatile int i=0;i<100000;i++);

    i2c_read_regs(MAG_ADDR, MAG_WHO_AM_I, &compass_verify, 1);

    memset(&latest_mag, 0, sizeof(MagData_t));
    initialised = 1;
}

uint8_t compass_read(void) {
    if (!initialised) return 0;
    uint8_t buf[6];
    if (!i2c_read_regs(MAG_ADDR, MAG_OUTX_L, buf, 6)) return 0;
    latest_mag.raw_x = (int16_t)((buf[1]<<8)|buf[0]);
    latest_mag.raw_y = (int16_t)((buf[3]<<8)|buf[2]);
    latest_mag.raw_z = (int16_t)((buf[5]<<8)|buf[4]);
    float heading = atan2f((float)latest_mag.raw_y,
                           (float)latest_mag.raw_x) * (180.0f/3.14159265f);
    if (heading < 0.0f) heading += 360.0f;
    latest_mag.heading_deg = heading;
    latest_mag.timestamp   = read_count++;
    return 1;
}

const MagData_t* compass_get_latest(void) {
    if (!initialised) return NULL;
    return &latest_mag;
}
