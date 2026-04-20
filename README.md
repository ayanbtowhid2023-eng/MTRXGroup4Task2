# MTRX2700 Project 2

## Group Members
Ayan Towhid
Ben Innes
Noah Mattani
Remy Cameron

## Roles and Responbilities
Ayan:
- Exercise 7.2

Ben:
- Exercise 7.4
- Meeting Minutes

Noah:
- Exercise 7.3

Remy:
- Exercise 7.1


## Project Overview

## Exercise 7.1 - Digital I/O

### Summary

### Usage

### Valid input

### Functions and modularity

### Testing

### Notes

## Exercise 7.2 - Timer Interface

### Summary
This part of the project aims to create a module that can interface to the hardware timers of the STM32 Discovery Boards and its various functionalities. Functionality includes using TIM3 to trigger a callback function at a regular interval, using get/set functions to change said interval, using the PWM local to the TIM3 hardware to operate a servo in the full clockwise position, the centre position and the full anticlockwise position and the final functionality includes using TIM4 to trigger a single-shot event (defined by a callback function) after a given delay. 

### Usage
This part is separated into c files:
- Main.c - Where the final, high level operaiton occurs
- Gpio.c - From exercise 7.1 to control the general purpose input and output. Particularly for PA6 to output the PWM and for LED output.
- Timer.c - This is a generic, infintely repeating timer which fires a callback function at a configurable interval
- Servo.c - Drives a hobby servo motor using TIM3's hardware PWM output on PA6. The servo position is controlled by pulse width: 1ms for full clockwise, 1.5ms for centre, 2ms for full counter-clockwise, at 50Hz (every 20ms).
- One_shot.c - Triggers a single delayed callback using TIM4 which fires after a given delay and stops permanently.

This part also features the header files:
- Registers.h - From exercise 7.1. Defines the registers of the STM32 Discovery board, which may be accessed and interacted with to allow for specific functionality.
- Gpio.h - From exercise 7.1 to define the funcitons that allow for general purpose input and output
- Timer.h - Defines functions used in timer.c
- Servo.h - Defines functions used in servo.c
- One_shot.h - Defines funcitons used in one_shot.c

The main function is where the demonstrate parts A, B, C and D. The demonstrations may occur by 'un-commenting out' one of the following
'''
// #define TASK_A_DEMO
// #define TASK_C_DEMO
// #define TASK_D_DEMO
'''

### Valid input

### Functions and modularity

### Testing

### Notes

## Exercise 7.3 - Serial Interface

### Summary

### Usage

### Valid input

### Functions and modularity



### Testing

### Notes

## Exercise 7.4 - I2C Sensor Interface

### Summary

### Usage

### Valid input

### Functions and modularity

### Testing

### Notes

## Exercise 7.5 - Integration Task

### Summary

### Usage

### Valid input

### Functions and modularity

### Testing

### Notes
