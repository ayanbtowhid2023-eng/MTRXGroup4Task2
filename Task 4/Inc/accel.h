#ifndef ACCEL_H
#define ACCEL_H

#include <stdint.h>
#include "stm32f303xc.h"

typedef struct {
    int16_t raw_x;
    int16_t raw_y;
    int16_t raw_z;
    float   roll_deg;
    float   pitch_deg;
} AccData_t;

void            accel_init(void);
uint8_t         accel_read(void);
const AccData_t* accel_get_latest(void);

#endif
