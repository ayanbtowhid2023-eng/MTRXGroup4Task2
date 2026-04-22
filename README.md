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

Summary
On power-on LED3 lights. Each press of the blue button steps to the next LED (LED3 → LED4 → ... → LED10 → LED3). Nothing moves without a button press. All LED changes are driven entirely by the button interrupt — the while(1) loop in main.c is empty.
main.c
    ↓
discovery.c       ← only file that knows PE8–PE15, PA0
    ↓         ↓
led.c       button.c    ← generic, no hardcoded pins
       ↓   ↓
       gpio.c           ← direct register access
          ↓
      registers.h       ← raw hardware addresses
      led_timer.c       ← TIM2 system tick + rate limiter

Usage

Create a new STM32CubeIDE project targeting STM32F303VCTx
Copy all Inc/ files into the project Inc/ folder
Copy all Src/ files into the project Src/ folder, replacing the generated main.c
Set include path to ../Inc only
Build and flash

To demo Task C — in on_button_press() comment out led_set_rate_limited() and uncomment discovery_led_set()
To demo Task E — leave led_set_rate_limited() in (default as delivered)

Valid Input
FunctionValid InputsReturnsgpio_init(port, pin, dir)port: GPIOA–GPIOF, pin: 0–15, dir: INPUT/OUTPUT/AF0 success, -1 errorgpio_write(port, pin, state)port: GPIOA–GPIOF, pin: 0–15, state: HIGH/LOW0 success, -1 errorgpio_read(port, pin, *state)port: GPIOA–GPIOF, pin: 0–15, state: non-NULL pointer0 success, -1 errorgpio_init_af(port, pin, af)port: GPIOA–GPIOF, pin: 0–15, af: 0–150 success, -1 errorled_set(id, state)id: 0–7, state: LED_ON/LED_OFF0 success, -1 errorled_get(id)id: 0–7LED_ON, LED_OFF, or -1led_set_single(id)id: 0–70 success, -1 errordiscovery_led_set(id, state)id: DISC_LED3–DISC_LED10, state: LED_ON/LED_OFF—discovery_led_get(id)id: DISC_LED3–DISC_LED10LED_ON, LED_OFF, or -1discovery_button_pressed()none1 if pressed, 0 if notled_set_rate_limited(id, state)id: 0–7, state: LED_ON/LED_OFF—led_timer_init(interval_ms)any uint32_t, recommended: 200—get_tick()noneuint32_t ms since init

Functions and Modularity
registers.h
Defines all STM32F303 peripheral structs (GPIO, RCC, EXTI, TIM2, USART, I2C) and base addresses directly from the reference manual. No external headers required. Every module in the project includes only this file.
gpio.h / gpio.c
Generic GPIO module. Works on any port and pin. No board-specific knowledge.

gpio_init(port, pin, direction) — enables peripheral clock, sets MODER register
gpio_write(port, pin, state) — drives pin high or low via BSRR register
gpio_read(port, pin, *state) — reads current pin state from IDR register
gpio_init_af(port, pin, af) — sets alternate function mode, used by UART/I2C/PWM in Exercise 5
All functions validate inputs and return -1 on error

led.h / led.c
Generic LED module. Pin assignments are injected at init time via LED_Descriptor array — no hardcoded pins anywhere in this file.

led_init(descriptors, count, callback) — configures all LEDs as outputs, registers optional change callback
led_set(id, state) / led_get(id) — the only access points to private led_state[]
led_clear_all() — turns all LEDs off
led_set_single(id) — lights one LED, clears all others (used by Exercise 5 for compass heading display)
led_state[] is declared static — completely private to led.c, inaccessible from any other file

button.h / button.c
Generic button module. All hardware details injected via Button_Descriptor at init time.

button_init(descriptor, callback) — configures EXTI interrupt on given pin, registers callback
button_read() — polls current pin state without using interrupts
button_get_flag() / button_clear_flag() — non-ISR press detection used by Exercise 5 to package button state into UART messages
EXTI0_IRQHandler — clears pending flag, sets press flag, calls registered callback

discovery.h / discovery.c
The only file in the project with board-specific knowledge. Knows PE8–PE15 are LEDs and PA0 is the button. All other modules are generic and would work unchanged on any other board.

discovery_init(callback) — single call sets up all 8 LEDs and the button with correct board pins
discovery_led_set(id, state) / discovery_led_get(id) / discovery_led_clear_all()
discovery_led_set_single(id) — lights one LED, clears all others (Exercise 5 heading display)
discovery_button_pressed() — one-shot flag read, auto-clears on read (Exercise 5)

led_timer.h / led_timer.c
TIM2 configured at 1ms tick (8MHz HSI, PSC=7999, ARR=1). Provides both the system clock and the LED rate limiter.

led_timer_init(min_interval_ms) — starts TIM2, sets minimum allowed interval between LED changes
led_set_rate_limited(id, state) — queues an LED change and returns immediately, never blocks
get_tick() — returns milliseconds since init, used by Exercise 4 for sensor data timestamps
TIM2_IRQHandler — increments tick every 1ms, applies queued LED change once interval has elapsed

main.c
Initialises all modules, lights LED3 on startup, then sits in an empty while(1). All behaviour is interrupt driven — no polling.

on_button_press() — ISR callback, reads current LED state via discovery_led_get(), steps to next LED in sequence


Testing
TestExpected ResultPass/FailPower onLED3 lit, all others offSingle button pressSteps to next LED in sequenceFull sequence — 8 pressesReturns to LED3 after LED10Rapid button pressesSteps at most once per 200msSwap to discovery_led_set in callbackResponds instantly to every pressComment out discovery_led_get in callbackLEDs still step correctlygpio_init called with NULL portReturns -1, no crashgpio_init called with pin > 15Returns -1, no crashled_set called with id > 7Returns -1, no crashled_get called with id > 7Returns -1, no crash

Notes

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
- Gpio.h - From exercise 7.1 to define the funcitons that allow for general purpose input and output
- Timer.h - Defines functions used in timer.c
- Servo.h - Defines functions used in servo.c
- One_shot.h - Defines funcitons used in one_shot.c
- Stm32f303xc.h - Contains all information about registers and GPIO

The main function is where we  may demonstrate parts A, B, C and D. The demonstrations may occur by 'un-commenting out' one of the following:
```c
// #define TASK_A_DEMO
// #define TASK_C_DEMO
// #define TASK_D_DEMO
```
Task B is demonstrated by uncommening `#define TASK_A_DEMO` and in the main code for the Task A demoing section uncommenting:
```
// uint32_t current = timer_get_period_ms();  // Gets the current period
// (void)current; // Removes memory warning
// timer_set_period_ms(100); // Sets the new period
```

### Valid input

## `timer_init(uint32_t period_ms, TimerCallback callback)`
 
| Parameter | Valid Range | Invalid | Notes |
|-----------|-------------|---------|-------|
| `period_ms` | 1 – 4,294,967,295 | 0 (clamped to 1) | Sets ARR = period_ms - 1 |
| `callback` | Any valid function pointer | `NULL` | NULL disables callback, timer still runs |
 
---
 
## `timer_set_period_ms(uint32_t new_period_ms)`
 
| Parameter | Valid Range | Invalid | Notes |
|-----------|-------------|---------|-------|
| `new_period_ms` | 1 – 4,294,967,295 | 0 (sets ARR to 0xFFFFFFFF) | No clamping — caller must ensure ≥ 1 |
 
---
 
## `timer_get_period_ms(void)`
 
| Return Value | Range | Notes |
|--------------|-------|-------|
| `uint32_t` | 1 – 4,294,967,295 | Returns 0 if called before `timer_init()` |
 
---
 
## `timer_set_callback(TimerCallback callback)`
 
| Parameter | Valid Range | Invalid | Notes |
|-----------|-------------|---------|-------|
| `callback` | Any valid function pointer | `NULL` | NULL disables callback without stopping timer |
 
---
 
## `one_shot_trigger(uint32_t delay_ms, OneShotCallback callback)`
 
| Parameter | Valid Range | Invalid | Notes |
|-----------|-------------|---------|-------|
| `delay_ms` | 1 – 4,294,967,295 | 0 (clamped to 1) | Sets ARR = delay_ms - 1 |
| `callback` | Any valid function pointer | `NULL` | NULL means ISR fires but does nothing |
 
---
 
## `servo_init(void)`
 
| Parameter | Valid Range | Notes |
|-----------|-------------|-------|
| None | — | Always resets to centre (1500 us). Safe to call multiple times. |
 
---
 
## `servo_set_position(uint32_t pulse_us)`
 
| Parameter | Valid Range | Clamped Range | Notes |
|-----------|-------------|---------------|-------|
| `pulse_us` | 1000 – 2000 | < 1000 → 1000, > 2000 → 2000 | 1000 = full CW, 1500 = centre, 2000 = full CCW |
 
---
 
## `servo_get_position_us(void)`
 
| Return Value | Range | Notes |
|--------------|-------|-------|
| `uint32_t` | 1000 – 2000 | Returns `SERVO_POS_CENTRE_US` (1500) if called before `servo_init()` |


### Functions and modularity
#### 

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
