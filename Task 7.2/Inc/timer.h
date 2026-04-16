#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Initialise the timer with a period (in ms) and a callback function
void timer_init(uint32_t period_ms, void (*callback)(void));

// Set the timer period (in ms)
void setperiod(uint32_t period_ms);

// Get the current timer period (in ms)
uint32_t getperiod(void);

#endif
