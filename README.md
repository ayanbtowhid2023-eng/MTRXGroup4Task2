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
```c
// #define TASK_A_DEMO
// #define TASK_C_DEMO
// #define TASK_D_DEMO
```

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
This part of the project creates modules that read the STM32F3 Discovery board's onboard motion sensors to give a full attitude reference: heading from the magnetometer, roll and pitch from the accelerometer, and angular rates from the gyroscope. The magnetometer and accelerometer are both on the LSM303AGR chip and are accessed over I2C1. The gyroscope is an L3GD20 on SPI1. Each sensor has its own module exposing an init/read/get_latest interface, and a shared data structure for raw values and decoded outputs. All three are read in the main loop and the results are streamed over USART1 for verification.

### Usage
This part is separated into c files:
- `main.c` - Top level loop that initialises all three sensors, reads them every iteration, toggles PE8 to indicate a successful read, and sends the results over UART at 115200 baud
- `compass.c` - Sets up I2C1 (including bus recovery and FPU enable), configures the LSM303AGR magnetometer, reads x/y/z, and computes heading from atan2(y, x)
- `accel.c` - Configures the LSM303AGR accelerometer. I2C1 is already up from `compass_init()`, so this only writes the sensor registers and decodes roll and pitch
- `gyro.c` - Sets up SPI1, configures the L3GD20 at 500 dps, runs a 200-sample bias calibration on startup, reads x/y/z, and converts to degrees per second

This part also features the header files:
- `compass.h` - Defines `MagData_t` (raw x/y/z, heading, timestamp) and the public interface
- `accel.h` - Defines `AccData_t` (raw x/y/z, roll, pitch) and the public interface
- `gyro.h` - Defines `GyroData_t` (raw x/y/z, dps x/y/z) and the public interface

Typical call order in `main`:
1. `compass_init()` (brings up I2C1 and the FPU)
2. `accel_init()` (reuses I2C1)
3. `gyro_init()` (brings up SPI1 and calibrates gyro bias, keep board still)
4. `compass_read()`, `accel_read()`, `gyro_read()` in the loop
5. `compass_get_latest()`, `accel_get_latest()`, `gyro_get_latest()` to access the decoded values

### Valid input
- Accelerometer: ±2g full scale, 50 Hz output data rate, high-resolution mode
- Gyroscope: ±500 dps full scale, 95 Hz ODR, 0.0175 dps/LSB
- Magnetometer: continuous mode, default 10 Hz ODR, heading in the 0 to 360 degree range
- Roll: ±180 degrees, Pitch: ±90 degrees
- Board must be stationary during `gyro_init()` for the bias calibration to be valid

### Functions and modularity
Each sensor module is self-contained and only exposes three functions through its header:
- `init()` sets up the bus (where needed), configures the sensor, and optionally verifies WHO_AM_I or runs calibration
- `read()` reads raw bytes, decodes them into the module's data structure, and returns 1 on success
- `get_latest()` returns a const pointer to the latest data, or NULL if the module is not initialised

The raw I2C and SPI transfer routines are `static` inside the `.c` files, so external code cannot touch the bus directly. Other modules only see the sensor-level interface. The data structures are defined in the headers so they can be passed straight into the serial module for the Exercise 5 integration task.

`accel_init()` deliberately does not re-initialise I2C1. It relies on `compass_init()` having brought the bus up first. This keeps the dependency order explicit and avoids clobbering a working peripheral.

### Testing
Three verification methods are used since debugger breakpoints corrupt I2C transfers and cannot be trusted for step-through testing:
1. **LED blink on PE8** toggles each time all three reads return valid data. This gives a quick visual indication that the main loop is running and every sensor is responding
2. **UART output on PC4** is read on the host with `screen /dev/tty.usbmodem1203 115200`. Each line prints raw x/y/z, heading, roll, pitch, and gyro dps so behaviour can be checked against known orientations
3. **WHO_AM_I register read** populates the global `compass_verify` during init. The expected value can be confirmed in the debugger after startup to prove the correct sensor is on the bus

Physical sanity checks during development:
- Hold board flat: roll and pitch near zero
- Rotate 90 degrees about the z-axis: heading changes by roughly 90 degrees
- Leave board still after calibration: gyro dps values sit near zero

### Notes
- I2C GPIO pins must be push-pull with pull-down resistors on this board for the sensor to respond reliably
- The FPU is explicitly enabled in `compass_init()` via `SCB->CPACR` before any float math (`atan2f`, `sqrtf`) runs. Skipping this causes a hard fault
- `compass_init()` runs a manual bus recovery (9 SCL clocks) before enabling the peripheral. This fixes the case where the sensor is holding SDA low after an unclean reset
- Debugger breakpoints stall I2C transfers and return corrupted data, which is why UART and LED verification are used instead of single-stepping
- Hard-iron magnetometer calibration is not currently applied. The standard method is to rotate the board through all orientations, record the min and max of each axis, compute the midpoint offsets, and subtract them from the raw readings before running atan2 for heading

## Exercise 7.5 - Integration Task

### Summary

### Usage

### Valid input

### Functions and modularity

### Testing

### Notes
