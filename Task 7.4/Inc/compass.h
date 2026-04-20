#ifndef COMPASS_H
#define COMPASS_H

#include <stdint.h>
#include "stm32f303xc.h"

typedef struct {
    int16_t  raw_x;
    int16_t  raw_y;
    int16_t  raw_z;
    float    heading_deg;
    uint32_t timestamp;
} MagData_t;

extern uint8_t compass_verify;

void             compass_init(void);
uint8_t          compass_read(void);
const MagData_t* compass_get_latest(void);

#endif
