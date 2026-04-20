#include "accel.h"
#include <math.h>
#include <string.h>

#define ACC_ADDR    0x19
#define ACC_CTRL1   0x20
#define ACC_CTRL4   0x23
#define ACC_OUT_X_L 0x28

static AccData_t latest_acc;
static uint8_t   initialised = 0;

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

void accel_init(void) {
    // I2C1 already initialised by compass_init
    // Just configure the accelerometer registers
    if (!i2c_write_reg(ACC_ADDR, ACC_CTRL1, 0x47)) return;  // 50Hz, XYZ
    if (!i2c_write_reg(ACC_ADDR, ACC_CTRL4, 0x08)) return;  // HR, ±2g
    memset(&latest_acc, 0, sizeof(AccData_t));
    initialised = 1;
}

uint8_t accel_read(void) {
    if (!initialised) return 0;
    uint8_t buf[6];
    if (!i2c_read_regs(ACC_ADDR, ACC_OUT_X_L | 0x80, buf, 6)) return 0;
    latest_acc.raw_x = (int16_t)((buf[1]<<8)|buf[0]);
    latest_acc.raw_y = (int16_t)((buf[3]<<8)|buf[2]);
    latest_acc.raw_z = (int16_t)((buf[5]<<8)|buf[4]);
    float ax = (float)latest_acc.raw_x;
    float ay = (float)latest_acc.raw_y;
    float az = (float)latest_acc.raw_z;
    latest_acc.roll_deg  = atan2f(ay, az) * (180.0f/3.14159265f);
    latest_acc.pitch_deg = atan2f(-ax, sqrtf(ay*ay+az*az)) * (180.0f/3.14159265f);
    return 1;
}

const AccData_t* accel_get_latest(void) {
    if (!initialised) return NULL;
    return &latest_acc;
}
