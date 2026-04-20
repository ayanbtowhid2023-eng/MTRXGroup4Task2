#ifndef GYRO_H
#define GYRO_H

#include <stdint.h>
#include "stm32f303xc.h"

typedef struct {
    int16_t raw_x;
    int16_t raw_y;
    int16_t raw_z;
    float   dps_x;
    float   dps_y;
    float   dps_z;
} GyroData_t;

void              gyro_init(void);
uint8_t           gyro_read(void);
const GyroData_t* gyro_get_latest(void);

#endif
