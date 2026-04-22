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

# MTRX2700 - Exercise 7.1: Digital I/O
## STM32F3 Discovery Board


## Summary
On power-on LED3 lights. Each press of the blue button steps to the next LED (LED3 → LED4 → ... → LED10 → LED3). Nothing moves without a button press. All LED changes are driven by the button interrupt - the `while(1)` loop in `main.c` is empty.


---

## Usage

1. Create a new STM32CubeIDE project targeting `STM32F303VCTx`
2. Copy all `Inc/` files into the project `Inc/` folder
3. Copy all `Src/` files into the project `Src/` folder, replacing the generated `main.c`
4. Set include path to `../Inc` only
5. Build and flash

To demo Task C - in `on_button_press()` comment out `led_set_rate_limited()` and uncomment `discovery_led_set()`

To demo Task E - leave `led_set_rate_limited()` in (default as delivered)

---


## Functions and Modularity


### gpio.h / gpio.c
Generic GPIO module. Works on any port and pin. No board-specific knowledge.

- **`gpio_init(port, pin, direction)`** - enables peripheral clock, sets MODER register
- **`gpio_write(port, pin, state)`** - drives pin high or low via BSRR register
- **`gpio_read(port, pin, *state)`** - reads current pin state from IDR register
- **'gpio_init_af(port, pin, af)`** - sets alternate function mode for UART/I2C/PWM in Exercise 5
- All functions validate inputs and return -1 on error

### led.h / led.c
Generic LED module. Pin assignments injected at init time via `LED_Descriptor` array - no hardcoded pins in this file.

- **`led_init(descriptors, count, callback)`** - configures all LEDs as outputs, registers optional change callback
- **`led_set(id, state)`** / **`led_get(id)`** - only access points to private `led_state[]`
- **`led_clear_all()`** - turns all LEDs off
- **`led_set_single(id)`** - lights one LED, clears all others, used by Exercise 5 for compass heading display
- `led_state[]` is declared `static` - completely private to `led.c`, inaccessible from any other file

### button.h / button.c
Generic button module. All hardware details injected via `Button_Descriptor` at init time.

- **`button_init(descriptor, callback)`** - configures EXTI interrupt on given pin, registers callback
- **`button_read()`** - polls current pin state without using interrupts
- **`button_get_flag()`** / **`button_clear_flag()`** - non-ISR press detection used by Exercise 5
- **`EXTI0_IRQHandler`** - clears pending flag, sets press flag, calls registered callback

### discovery.h / discovery.c
The only file with board-specific knowledge. Knows PE8–PE15 are LEDs and PA0 is the button. All other modules are generic and work unchanged on any other board.

- **`discovery_init(callback)`** - single call sets up all 8 LEDs and the button with correct board pins
- **`discovery_led_set(id, state)`** / **`discovery_led_get(id)`** / **`discovery_led_clear_all()`** - LED access
- **`discovery_led_set_single(id)`** - lights one LED, clears all others, used by Exercise 5
- **`discovery_button_pressed()`** - one-shot flag read, auto-clears on read, used by Exercise 5

### led_timer.h / led_timer.c
TIM2 configured at 1ms tick (8MHz HSI, PSC=7999, ARR=1). Provides the system clock and LED rate limiter.

- **`led_timer_init(min_interval_ms)`** - starts TIM2, sets minimum allowed interval between LED changes
- **`led_set_rate_limited(id, state)`** - queues an LED change and returns immediately, never blocks
- **`get_tick()`** - returns ms since init, used by Exercise 4 for sensor data timestamps
- **`TIM2_IRQHandler`** - increments tick every 1ms, applies queued LED change once interval has elapsed

### main.c
Initialises all modules, lights LED3 on startup, then sits in an empty `while(1)`. All behaviour is interrupt driven.

- **`on_button_press()`** - ISR callback, reads current LED state via `discovery_led_get()`, steps to next LED

---

## Testing

| Test | Expected Result | Pass/Fail |
|------|----------------|-----------|
| Power on | LED3 lit, all others off | |
| Single button press | Steps to next LED in sequence | |
| Full sequence - 8 presses | Returns to LED3 after LED10 | |
| Rapid button presses | Steps at most once per 200ms | |
| Swap to `discovery_led_set` in callback | Responds instantly to every press | |
| Comment out `discovery_led_get` in callback | LEDs still step correctly | |
| `gpio_init` called with NULL port | Returns -1, no crash | |
| `gpio_init` called with pin > 15 | Returns -1, no crash | |
| `led_set` called with id > 7 | Returns -1, no crash | |
| `led_get` called with id > 7 | Returns -1, no crash | |

---

## Notes


What would happen if many different software modules can make changes to the LEDs?
All changes must go through led_set() - the single write path into the private led_state[] array. No module can corrupt another's LED state without going through this defined interface. Without this controlled access, multiple modules writing to the LEDs at the same time would produce unpredictable behaviour where one module silently overwrites another's changes.

What would happen if the button handling callback function takes a long time to complete?
The callback runs inside the ISR. A long callback blocks other interrupts at equal or lower priority from firing, can cause missed button presses, and stops the TIM2 tick from incrementing which breaks all timing across the system. The callback is kept minimal - it only reads the current LED state and queues a request via led_set_rate_limited(), which returns immediately.

What checks do you need to consider when setting up or using a GPIO pin for input or output?
gpio_init validates that the port pointer is not NULL, the pin number is between 0 and 15, and the port is one of the six known valid ports before enabling its clock. For input pins a pull-down is configured so the pin reads LOW when floating rather than returning an undefined value. All functions return -1 on any failure so the caller can detect and handle errors rather than silently proceeding with a misconfigured pin.
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

_Task A Demo_
- Uncomment `// #define TASK_A_DEMO`.
- The call back funciton is defined in the main to toggle LED's (LD4) on and off
- Every 500ms the LED should turn on and off.
- TIM3 is used.

_Task B Demo_
- This should decrease the period between toggling the LED (LD4) on and off to 100ms, such that the toggling occurs much faster.
- TIM3 is used.

_Task C Demo_
- Uncomment `// #define TASK_C_DEMO`
- The servo's maximum movement is restricted to 90 degrees.
- The servo begins in the full clock-wise position (defined by an input pulse width of 1ms), then goes to the centre position (1.5 ms) and then the full anticlockwise-position (2ms) before returning back to the full clock-wise position. This occurs at a period of 20ms.
- TIM3 and its PWM hardware is used.
- A DF9GMS servo is used, with brown wire connected to 5V, orange wire to PA6 and red wire to 5V. Due to concerns with stalling current the STM32 board should not power the servo, so only the orange wire is connected to PA6, while other powering modules (with shared ground) can be used to supply the servo with power.

_Task D Demo_
- Uncomment `// #define TASK_D_DEMO`
- After 5s LD4 is lit, and remains on.
- TIM4 is used.

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
| `new_period_ms` | 1 – 4,294,967,295 | 0 (sets ARR to 0xFFFFFFFF) | No clamping - caller must ensure ≥ 1 |
 
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
| None | - | Always resets to centre (1500 us). Safe to call multiple times. |
 
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
#### timer.c
First we define `period_ms` and `cb` which represent the period of the timer and the callback function pointer respectively. These are only accessible in this file, and must use get and set functions to access externally.

`TIM_IRQHandler`
- This is the interrupt request handler, which runs the callback function after the interrupt has been fired. This is also inaccessible outside of this file. The use of a callback function allows for greater modularity as any function may be called once the interrupt is fired. We use TIM3 for this functionality.

`timer_init`
- This initialises the timer, such that each 'tick' of the clock occurs every millisecond and the counter counts up to the given period before another interrupt is fired once configuration is completed.

`void timer_set_period_ms`
- Set function that allows for the period (set/inaccessible variable in file) to be changed. Writes the new period also into the Auto Reload Register, such that the counter may count up to this value before firing the next interrupt. 

`timer_get_period_ms`
- Allows us to read the inaccessible/private period value outside of this module file.

`timer_set_callback`
- Set function that allows for the callback function to be changed.

#### servo.c
`pulse_us = SERVO_POS_CENTRE_US` is statically defined to define the centre position of the servo.

`servo_init`
- Resets the pulse width to match the centre posiiton each time the servo is initialised
- We enable TIM3 as the clock and configuring the hardware for PWM mode. The clock and auto-reload register is scaled such that each 'tick' corresponds to 1ms and the period of each pulse is 20ms (5Hz frequency).
- CCER (Capture Compare Enable Register) is also enabled such that the PWM signal is outputted via pin PA6. 
- Once all configuration occurs, then the timer may begin.

`servo_set_position`
- Sets new pulse as the pulse fed into the timer and writes it directly into CCR1 for the new pulse width to be appended during the next cycle.
- This is bounded to 1000us to 2000us to ensure that the servo does not exceed its mechanical limits.

`servo_get_position`
- 'Get' function that allows us to read the position value/ the pulse width externally.

#### one_shot.c
`stored_callback` is privately initialised to NULL. This is the function pointer for the callback funciton.

`TIM4_IRQHandler`
- Handles the interrupt request. This interrupt is fired by the counter overflowing
- Once the counter overflows, we clear the counter enable bit such that the funciton is only called once with ` TIM4->CR1 &= ~TIM_CR1_CEN;`
- The callback function is then called

`one_shot_trigger`
- Enables the TIM4 clock and scales such that each tick is 1 ms.
- Once the timer and interrupts are properly configured the counter is enabled.

#### gpio.c
This is the same module implemented from task 7.1, for more details of its funciton refer above.

### Testing

_Task A_

| Test | Function Call | Expected Result | How to Verify |
|------|--------------|-----------------|---------------|
| Normal operation | `timer_init(500, on_timer_a)` | LD3 toggles every 500ms | Time 10 blinks with stopwatch - should take ~5 seconds |
| Different period | `timer_init(100, on_timer_a)` | LD3 toggles every 100ms | Time 10 blinks with stopwatch - should take ~1 second |
| NULL callback | `timer_init(500, NULL)` | No crash, timer runs silently, LED stays off | Board does not hang or reset |
| Zero period (boundary) | `timer_init(0, on_timer_a)` | Clamped to 1ms, LED toggles as fast as possible | LED appears constantly on due to speed of toggling |

---

_Task B_

| Test | Function Call | Expected Result | How to Verify |
|------|--------------|-----------------|---------------|
| Get period before set | `timer_get_period_ms()` after `timer_init(500, on_timer_a)` | Returns 500 | Set breakpoint after `timer_init`, inspect return value in debugger - should read 500 |
| Set new period | `timer_set_period_ms(100)` | LED visibly speeds up from 500ms to 100ms blink rate | Observe LED blink rate change immediately after call |
| Get period after set | `timer_get_period_ms()` after `timer_set_period_ms(100)` | Returns 100 | Set breakpoint after `timer_set_period_ms`, inspect return value in debugger - should read 100 |
| Set zero period (boundary) | `timer_set_period_ms(0)` | ARR set to 0xFFFFFFFF - very long period, LED effectively stops | LED stops toggling |

---

_Task C_

| Test | Function Call | Expected Result | How to Verify |
|------|--------------|-----------------|---------------|
| Initialisation | `servo_init()` | Servo moves to centre position | Physically observe servo arm at centre (1500us) |
| Full clockwise | `servo_set_position(SERVO_POS_MIN_US)` | Servo moves to full CW position | Physically observe servo arm at full CW - visually distinct from centre |
| Centre position | `servo_set_position(SERVO_POS_CENTRE_US)` | Servo moves to centre position | Physically observe servo arm returns to centre |
| Full counter-clockwise | `servo_set_position(SERVO_POS_MAX_US)` | Servo moves to full CCW position | Physically observe servo arm at full CCW - visually distinct from centre |
| Below minimum (boundary) | `servo_set_position(500)` | Clamped to 1000us, servo moves to full CW | Call `servo_get_position_us()` immediately after - debugger should show 1000 |
| Above maximum (boundary) | `servo_set_position(3000)` | Clamped to 2000us, servo moves to full CCW | Call `servo_get_position_us()` immediately after - debugger should show 2000 |
| Get position | `servo_get_position_us()` after `servo_set_position(1200)` | Returns 1200 | Set breakpoint after `servo_set_position`, inspect return value in debugger - should read 1200 |
| Get position before init | `servo_get_position_us()` before `servo_init()` | Returns `SERVO_POS_CENTRE_US` (1500) | Inspect return value in debugger - should read 1500 |
| 50Hz period verification | Observe servo sweep loop | Servo sweeps CW → centre → CCW repeatedly | Each sweep step held for ~800000 cycles - physically verify three distinct positions are reached |

---

Testing may also occur by connecting an oscilloscope to PA6 to show the pulse widths of 1ms, 1.5ms and 2ms every 20ms.

_Task D_

| Test | Function Call | Expected Result | How to Verify |
|------|--------------|-----------------|---------------|
| Normal operation | `one_shot_trigger(1000, one_shot_callback)` | LD3 turns on exactly once after 1 second | Time delay with stopwatch - should be ~1 second. LED stays on permanently after firing |
| Fires only once | `one_shot_trigger(1000, one_shot_callback)` | LD3 turns on and never turns off | Observe LED stays on indefinitely - timer does not repeat |
| Short delay (boundary) | `one_shot_trigger(1, one_shot_callback)` | LD3 turns on almost immediately | LED turns on as fast as possible after call |
| Zero delay (boundary) | `one_shot_trigger(0, one_shot_callback)` | Clamped to 1ms, LED turns on almost immediately | LED turns on as fast as possible - same as 1ms test |
| NULL callback | `one_shot_trigger(1000, NULL)` | No crash, timer fires and stops, nothing happens | Board does not hang or reset after 1 second |
| Board reset re-trigger | Reset board, observe behaviour | LD3 stays off for 1 second then turns on again | Proves one-shot fires reliably on each power cycle, not random behaviour |

### Notes
1. How much would be required to make this software module able to use different timers - as in, could you have multiple regular intervals, or multiple one-shot delays at the same time?
Currently TIM3 and TIM4 are hardcoded to handle timer.c and one_shot.c respectively, however different timers could be used by replacing the mentions of TIM3 to TIM4 and vice versa. This can be done manually, for example in timer.c:
- `RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;` becomes `RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;`
- `TIM3->CR1 &= ~TIM_CR1_CEN;` becomes `TIM4->CR1 &= ~TIM_CR1_CEN;`
- `TIM3->PSC = 7999;` becomes `TIM4->PSC = 7999;`
- `TIM3->ARR = (uint32_t)(period_ms - 1);` becomes `TIM4->ARR = (uint32_t)(period_ms - 1);`
- `TIM3->EGR |= TIM_EGR_UG;` becomes `TIM4->EGR |= TIM_EGR_UG;`
- `TIM3->SR &= ~TIM_SR_UIF;` becomes `TIM4->SR &= ~TIM_SR_UIF;`
- `TIM3->DIER |= TIM_DIER_UIE;` becomes `TIM4->DIER |= TIM_DIER_UIE;`
- `TIM3->CR1 |= TIM_CR1_CEN;` becomes `TIM4->CR1 |= TIM_CR1_CEN;`
- `NVIC_SetPriority(TIM3_IRQn, 2);` becomes `NVIC_SetPriority(TIM4_IRQn, 2);`
- `NVIC_EnableIRQ(TIM3_IRQn);` becomes `NVIC_EnableIRQ(TIM4_IRQn);`
- `void TIM3_IRQHandler(void)` becomes `void TIM4_IRQHandler(void)`
- `TIM3->SR &= ~TIM_SR_UIF;` becomes `TIM4->SR &= ~TIM_SR_UIF;`

Alternatively, an easier solution could be to have an alternate funciton for handling if a given timer requested is TIM3 or TIM4 - this allows for 2 timers working at the same time. This funciton would have the requested timer to be used as an input. In this case only 2 timers (timer only no one-shot), 2 one-shots (no timer) or 1 one-shot and 1-timer code to be run at a time due to TIM3 and TIM4. However, TIM2 can also be used if the task 1 code is not being run, such that 3 different timers can be used.

2. What happens if one software module sets a delay, and before it elapses a different software module also tries to set a delay.
If the same timer is tried to be used then the line `if (TIM4->CR1 & TIM_CR1_CEN) return;` rejects the request to use TIM4 as well in the shot_back demo. Otherwise, without this the callback never fires and after the given delay the next callback run is fired instead.

3. How would you drive multiple servo motors using your timer and PWM modules?
   TIM3 has 4 channels (CH1 to CH4) corresponding to pins PA6, PA7, PB0 and PB1. We can append our configuration to account for all channels:
```
//C MR1 for CH1 and CH2
TIM3->CCMR1 &= ~(TIM_CCMR1_OC1M | TIM_CCMR1_OC1PE |
                  TIM_CCMR1_OC2M | TIM_CCMR1_OC2PE);
TIM3->CCMR1 |=  (TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE)  // CH1 PWM mode 1
             |  (TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE); // CH2 PWM mode 1

// CCMR2 covers CH3 and CH4
TIM3->CCMR2 &= ~(TIM_CCMR2_OC3M | TIM_CCMR2_OC3PE |
                  TIM_CCMR2_OC4M | TIM_CCMR2_OC4PE);
TIM3->CCMR2 |=  (TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1 | TIM_CCMR2_OC3PE)  // CH3 PWM mode 1 
             |  (TIM_CCMR2_OC4M_2 | TIM_CCMR2_OC4M_1 | TIM_CCMR2_OC4PE); // CH4 PWM mode 1 

// Enable all 4 channel outputs in CCER
TIM3->CCER |= TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E | TIM_CCER_CC4E;

// Set all channels to centre position 
TIM3->CCR1 = SERVO_POS_CENTRE_US;
TIM3->CCR2 = SERVO_POS_CENTRE_US;
TIM3->CCR3 = SERVO_POS_CENTRE_US;
TIM3->CCR4 = SERVO_POS_CENTRE_US;
```
After this we would define the GPIO pins:
```
gpio_init_af(GPIOA, 6, 2);  // PA6 - TIM3 CH1 AF2
gpio_init_af(GPIOA, 7, 2);  // PA7 - TIM3 CH2 AF2
gpio_init_af(GPIOB, 0, 2);  // PB0 - TIM3 CH3 AF2
gpio_init_af(GPIOB, 1, 2);  // PB1 - TIM3 CH4 AF2
```


## Exercise 7.3 - Serial Interface
 
### Summary
This exercise implements a complete serial (UART) communication module for the STM32F3 Discovery board. It provides debug string output, structured packet transmission with framing and checksums, and interrupt-driven packet reception with both circular-buffer and double-buffer modes.
 
Two UART ports are used:
- **USART1 (PC4/PC5)** - connected to the ST-Link VCP, debug strings appear directly in `screen` on your Mac via USB
- **USART3 (PC10/PC11)** - used for structured packet transmission and reception, tested with a loopback jumper wire
The packet format is: `[ START(0x02) | SIZE | TYPE | BODY... | STOP(0x03) | CHECKSUM ]` where the checksum is the XOR of all fields including the start and stop bytes.
 
---
 
### Usage
 
**Hardware setup:**
- USB cable from board to Mac (powers the board and provides the USART1 debug channel)
- Jumper wire from **PC10 (USART3 TX)** to **PC11 (USART3 RX)** for the loopback test
**To view debug output:**
```bash
screen /dev/tty.usbmodem103 115200
```
 
**To select which task to demonstrate**, set `DEMO_PART` in `main.c`:
```c
#define DEMO_PART 'e'   // tasks a-e: polling TX, interrupt circular-buffer RX
#define DEMO_PART 'f'   // task f:    interrupt TX, double-buffer RX
```
 
**File structure:**
- `serial.h` - public interface: port handles, packet constants, all function declarations
- `serial.c` - full implementation: both port instances, IRQ handlers, all functions
- `main.c` - demonstration: sends a `SensorData` struct as a packet, receives it via loopback, verifies data matches
---
 
### Valid input
 
#### `SerialInitialise(uint32_t baudRate, SerialPort *serial_port, void (*completion_function)(uint32_t))`
 
| Parameter | Valid Range | Notes |
|-----------|-------------|-------|
| `baudRate` | `BAUD_9600` to `BAUD_115200` | Use the enum constants defined in serial.h |
| `serial_port` | `&USART1_PORT` or `&USART3_PORT` | Must not be NULL |
| `completion_function` | Any valid function pointer | May be NULL - called after each TX buffer completes |
 
#### `sendMsg(void *data, uint8_t size, uint8_t type, SerialPort *serial_port)`
 
| Parameter | Valid Range | Notes |
|-----------|-------------|-------|
| `data` | Any valid pointer | Points to the struct/buffer to transmit |
| `size` | 1 – 64 | Use `sizeof(your_struct)`. Must not exceed `RX_BODY_SIZE` |
| `type` | 0x00 – 0xFF | Application-defined message type identifier |
| `serial_port` | `&USART1_PORT` or `&USART3_PORT` | Target port |
 
#### `sendMsgIT(void *data, uint8_t size, uint8_t type, SerialPort *serial_port)` (task f)
 
| Parameter | Valid Range | Notes |
|-----------|-------------|-------|
| `data` | Any valid pointer | Same as `sendMsg` |
| `size` | 1 – 64 | Must not exceed `RX_BODY_SIZE` |
| Return value | 0 or 1 | Returns 0 if TX is already busy, 1 on success |
 
#### `receiveMsg(SerialPort *serial_port, void (*callback)(uint8_t*, uint8_t, uint8_t))`
 
| Parameter | Valid Range | Notes |
|-----------|-------------|-------|
| `serial_port` | `&USART3_PORT` | Port that has had `enable_interrupt()` called |
| `callback` | Any valid function pointer | Called with `(body, size, type)` on successful packet receipt. May be NULL |
 
---
 
### Functions and Modularity
 
#### serial.h / serial.c
 
The full struct definition of `_SerialPort` is hidden inside `serial.c` - callers only see the opaque `SerialPort` typedef. All hardware register details are encapsulated in the two pre-instantiated port objects `USART1_PORT` and `USART3_PORT`. No other file needs to know which GPIO pins, clock masks, or alternate function values are involved.
 
**`SerialInitialise(baudRate, serial_port, completion_function)`**
- Enables clocks for the GPIO port and UART peripheral
- Configures GPIO pins to alternate function mode using `|=` on MODER and AFR registers to avoid disturbing other pins on the same port
- Sets the baud rate register and enables TX, RX, and the UART peripheral
- Does not enable interrupts - call `enable_interrupt()` or `enable_interrupt_part_f()` separately
**`enable_interrupt(serial_port)`**
- Enables the RXNE interrupt on the UART and registers the USART3 IRQ in the NVIC
- Disables global interrupts during setup to prevent a race condition on the NVIC registers
- Used for tasks a–e (circular buffer receive)
**`sendString(pt, serial_port)`** / **`SerialOutputString(pt, serial_port)`**
- Blocking polling TX of a null-terminated string
- Polls the TXE flag before writing each byte to TDR
- Calls the completion callback with the byte count when done
**`calculateChecksum(data, size, type)`**
- XOR of: `START_BYTE`, `size`, `type`, all body bytes, `STOP_BYTE`
- Computed before transmission and verified after reception
**`sendMsg(data, size, type, serial_port)`**
- Assembles and transmits a complete framed packet using polling TX
- Packet layout: `[ 0x02 | size | type | body[0..size-1] | 0x03 | checksum ]`
- Blocking - returns only after the last byte is written to TDR
**`receiveMsg(serial_port, callback)`**
- Reads bytes from the circular buffer filled by the RXNE ISR
- Parses the packet format step by step with per-byte timeouts
- Verifies the checksum and fires the callback with `(body, size, type)` on success
- Prints an error string to USART1 on stop-byte or checksum failure
- Non-blocking on timeout - returns immediately if no packet arrives within the timeout window
**`USART3_EXTI28_IRQHandler`**
- In normal mode (tasks a–e): reads each incoming byte from RDR and stores it in the circular buffer `s_rx_buffer` using a write pointer. Clears overrun errors to prevent the UART from locking up.
- In part-f mode (task f): routes incoming bytes to `part_f_rx_process_byte()` for the double-buffer state machine, and also handles TX interrupts to send bytes from `s_tx_buffer` one at a time.
**`enable_interrupt_part_f(serial_port)`** (task f)
- Resets all TX and RX double-buffer state before enabling interrupts
- Enables RXNEIE and the NVIC - from this point the IRQ handles both TX and RX
**`sendMsgIT(data, size, type, serial_port)`** (task f)
- Loads the framed packet into `s_tx_buffer`, sets `s_tx_busy = 1`, and enables TXEIE
- Returns immediately - the IRQ handler sends one byte per TXE interrupt
- Returns 0 if TX is already busy, 1 on success
**`sendStringIT(pt, serial_port)`** (task f)
- Same pattern as `sendMsgIT` but for a null-terminated debug string
**`receiveMsgDoubleBuffer(serial_port, callback)`** (task f)
- Checks `s_rx_msg_ready` flag set by the ISR
- Fires the callback with the completed buffer while the ISR continues receiving into the other buffer
- Uses `__disable_irq()` / `__enable_irq()` guards around shared state access
**`part_f_rx_process_byte(byte)`** (internal, task f)
- Six-state machine: `WAIT_START → READ_SIZE → READ_TYPE → READ_BODY → READ_STOP → READ_CHECKSUM`
- On valid checksum: marks the active buffer as ready and switches the ISR to the other buffer
- On dropped message (previous packet not yet consumed): sets `s_rx_dropped_msg` flag
---
 
### Testing
 
_Tasks a–e_ (set `DEMO_PART 'e'`, jumper PC10→PC11)
 
| Test | Expected Result | How to Verify |
|------|----------------|---------------|
| Power on | Debug strings appear in `screen` immediately | `screen /dev/tty.usbmodem103 115200`, press reset |
| `sendString` output | `"MTRX2700 Exercise 3 - Serial Module"` printed | Visible in screen terminal |
| `sendMsg` packet format | Correct framing bytes visible | Set breakpoint in `sendMsg`, step through and inspect each byte written to TDR |
| Loopback test passes | `"Loopback test PASSED! x=100 y=200 z=300 ts=12345"` | Printed in screen terminal after reset |
| Checksum error injection | `"ERR: bad checksum"` printed | Temporarily corrupt a body byte in `TX_SENSOR_DATA` and observe error output |
| Stop byte error injection | `"ERR: no stop byte"` printed | Temporarily change `SERIAL_STOP_BYTE` on TX side only |
| Interrupt-driven RX | ISR is called from interrupt vector, not main loop | Set breakpoint in `USART3_EXTI28_IRQHandler`, check Call Stack panel shows interrupt context |
| Callback fires with correct data | `on_message_received` called with `size=12`, `type=0x01` | Set breakpoint in callback, inspect `data` and `size` in Variables panel |
| NULL callback | No crash | Pass `NULL` to `receiveMsg` - board should not hang or reset |
| Oversized packet | Silently discarded, no crash | Send a packet with `size > 64` from external tool |
 
_Task f_ (set `DEMO_PART 'f'`, jumper PC10→PC11)
 
| Test | Expected Result | How to Verify |
|------|----------------|---------------|
| Interrupt TX sends packet | `"TX complete, waiting for packet..."` printed | Visible in screen terminal |
| Double-buffer RX receives packet | `"Loopback test PASSED!"` printed | Visible in screen terminal |
| `sendMsgIT` is non-blocking | Returns before packet is fully sent | Set breakpoint at line after `sendMsgIT` call - hits immediately while IRQ sends in background |
| TX ISR fires | Bytes sent one per TXE interrupt | Set breakpoint in `USART3_EXTI28_IRQHandler` TX section, observe `s_tx_index` incrementing |
| Double-buffer swap | ISR writes to one buffer while callback processes other | Inspect `s_rx_active_buf` and `s_rx_ready_buf` in Expressions panel |
| TX busy guard | `sendMsgIT` returns 0 if called while TX in progress | Call `sendMsgIT` twice in quick succession, check return value of second call |
 
---
 
### Notes
 
**How do you handle the case when there is more data received than will fit in the buffer?**
The `size` field in the packet header is checked against `RX_BODY_SIZE` (64 bytes) before any body bytes are read. If the declared size exceeds the buffer, the packet is discarded immediately and the state machine resets to `WAIT_START`. For the circular buffer, if the write pointer would lap the read pointer the incoming byte is silently dropped and the overrun error flag is cleared so the UART does not lock up.
 
**How do you determine when the incoming data has finished being received? Do you use a terminating character? What if this byte is missed or if the same byte appears elsewhere in the received data?**
The packet uses both a STOP byte (0x03) and an explicit SIZE field in the header. Reception is primarily length-driven - the receiver reads exactly `size` body bytes before expecting the STOP byte, so a 0x03 value appearing inside the body is not treated as termination. The STOP byte is only checked after all `size` body bytes have been read. If the STOP byte is missed or corrupted, `receiveMsg` prints an error and discards the packet. A final XOR checksum over all fields provides a further integrity check - even if a 0x03 body byte happened to coincide with the stop byte position, the checksum would catch any resulting data corruption.
 
**What are some potential advantages and disadvantages of passing structures and raw bytes rather than strings?**
Passing structures is more compact and efficient - a `SensorData` struct sends exactly 12 bytes rather than a formatted ASCII string that could be 40+ characters for the same data. It also removes ambiguity since the receiver knows the exact byte layout from the struct definition. The disadvantage is that both sides must agree on the same struct layout, and endianness matters if the boards have different architectures. Strings are easier to debug in a terminal but wasteful in bandwidth and parsing overhead.
 
**How do other software modules interact with the received data? Can they request the latest data? What happens with the incoming memory buffer after someone has requested the data? Do you clear the buffer? Do you have more than one buffer?**
Other modules interact with received data entirely through the callback function registered with `receiveMsg` or `receiveMsgDoubleBuffer`. When a complete packet arrives, the callback is fired with a pointer to the body bytes, the size, and the message type. It is the callback's responsibility to copy the data out immediately - the internal buffer is not cleared after the callback returns, but it may be overwritten by the next incoming packet as soon as the ISR receives more data. In circular buffer mode there is one shared buffer and no protection against the next packet overwriting an unread one. In double-buffer mode (task f) there are two body buffers - the ISR switches to the second buffer as soon as the first is marked ready, so the callback can safely process the first buffer while new data fills the second. The `s_rx_dropped_msg` flag is set if a new packet arrives before the previous one has been consumed.
 
**What happens if someone requests new serial port data before the current stream of data is complete (i.e. before the stream is terminated)?**
In circular buffer mode, `receiveMsg` reads from the buffer up to the point it has been filled by the ISR. If called before a complete packet has arrived, each per-step timeout expires and the function returns without firing the callback. The partial bytes remain in the circular buffer and the next call to `receiveMsg` will start from the START byte search. In double-buffer mode, `receiveMsgDoubleBuffer` simply returns immediately if `s_rx_msg_ready` is not set, so calling it before a packet is complete has no effect and no data is lost.
 
**What if someone keeps working on the serial RX buffer at the same time as new data comes in?**
In circular buffer mode, the ISR writes to `s_write_pos` and the main loop reads from `s_read_pos`. These are separate indices on a fixed-size circular buffer so they can operate concurrently without corruption, as long as the buffer does not overflow. In double-buffer mode, the ISR always writes to `s_rx_body_buf[s_rx_active_buf]` while the callback reads from `s_rx_body_buf[s_rx_ready_buf]`. After a successful packet is received these are guaranteed to be different buffers, and the index swap is protected by `__disable_irq()` / `__enable_irq()` guards to prevent a race condition if the IRQ fires mid-swap.



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
- **Do the readings correspond to cardinal directions?** Not directly. The raw `atan2(y, x)` output rotates correctly with the board (a 90 degree rotation produces roughly a 90 degree change in heading), but the zero reference does not line up with magnetic north and each axis carries a constant bias. This is caused by nearby magnetic interference from components on the board itself which add a fixed offset to every sample. The raw heading is reliable for relative rotation but not for absolute compass direction
- **How to calibrate the sensor?** The usual approach is to slowly rotate the board through a full circle while logging the x and y readings, then look at the highest and lowest value each axis hits. If the sensor was perfectly centred, those ranges would be symmetrical around zero, but because of the offsets mentioned above they aren't. Taking the midpoint of the min and max for each axis gives you a rough offset, and subtracting that from the raw readings before calculating the heading brings the output closer to a true angle. There are more involved versions that also account for the shape of the readings being stretched rather than a clean circle, and for the board not being level, but the basic idea is just to figure out the offset and subtract it



## Exercise 7.5 - Integration Task
 
### Summary
This exercise integrates all modules developed in Exercises 7.1 to 7.4 into a two-board system. Board 1 reads the magnetometer heading via I2C and monitors a button press via interrupt. It packages this data into a structured UART message and transmits it to Board 2 every 100ms. Board 2 receives the message and either drives a hobby servo to the corresponding heading position, or lights one of the 8 Discovery board LEDs corresponding to the compass direction, depending on the current display mode. The display mode is toggled each time the button on Board 1 is pressed.
 
---
 
### Usage
 
**Hardware setup:**
- Connect Board 1 PC10 (USART3 TX) to Board 2 PC11 (USART3 RX) with a jumper wire
- Connect Board 1 GND to Board 2 GND
- Connect servo signal wire to PA6 on Board 2, servo VCC to 5V, servo GND to GND
- USB cable to each board for power and debug output
**Wiring diagram:**
```
Board 1 PC10 (USART3 TX)  ------>  Board 2 PC11 (USART3 RX)
Board 1 GND               ------>  Board 2 GND
                                   Board 2 PA6  ------>  Servo signal
```
 
**To view debug output:**
```bash
screen /dev/tty.usbmodemXXX 115200
```
Open two terminals, one for each board.
 
**Order of operations - Board 1 main.c:**
1. `SerialInitialise(BAUD_115200, &USART1_PORT, on_tx_complete)` - sets up debug output to screen via ST-Link
2. `SerialInitialise(BAUD_115200, &USART3_PORT, 0)` - sets up USART3 TX for packet transmission to Board 2
3. `led_timer_init(30)` - starts TIM2 for system tick and LED rate limiting
4. `discovery_init(on_button_press)` - initialises all 8 LEDs and button interrupt on PA0
5. `compass_init()` - brings up I2C1 on PB6/PB7, enables FPU, configures magnetometer
6. Main loop: calls `compass_read()` every ~100ms, packages heading and button flag into `MagMessage_t`, transmits via `sendMsg()` over USART3
**Order of operations - Board 2 main.c:**
1. `SCB->CPACR` - enables FPU before any float operations
2. `SerialInitialise(BAUD_115200, &USART1_PORT, on_tx_complete)` - sets up debug output to screen via ST-Link
3. `SerialInitialise(BAUD_115200, &USART3_PORT, 0)` - sets up USART3 RX for packet reception from Board 1
4. `enable_interrupt(&USART3_PORT)` - enables RXNE interrupt on USART3 so incoming bytes are stored in circular buffer
5. `led_timer_init(30)` - starts TIM2 for system tick and LED rate limiting
6. `discovery_init(0)` - initialises all 8 LEDs, no button callback needed on Board 2
7. `servo_init()` - configures TIM3 hardware PWM on PA6, servo starts at centre position (1500us)
8. Main loop: calls `receiveMsg()` continuously, on valid packet calls `on_message_received()` callback which drives servo or LEDs depending on `button_flag`
---
 
### Valid Input
 
#### `MagMessage_t` (defined in msg.h)
 
| Field | Type | Valid Range | Notes |
|-------|------|-------------|-------|
| `heading_deg` | `float` | 0.0 – 360.0 | Compass heading from `compass_get_latest()` |
| `button_flag` | `uint8_t` | 0 or 1 | 0 = servo mode, 1 = LED mode. Toggled on each button press |
 
#### `heading_to_pulse_us(float heading_deg)` - Board 2 internal
 
| Parameter | Valid Range | Clamped Range | Notes |
|-----------|-------------|---------------|-------|
| `heading_deg` | 0.0 – 360.0 | < 0 → 0, > 360 → 360 | Maps full compass range to 1000–2000us servo pulse |
 
#### `heading_to_led(float heading_deg)` - Board 2 internal
 
| Parameter | Valid Range | Clamped Range | Notes |
|-----------|-------------|---------------|-------|
| `heading_deg` | 0.0 – 360.0 | < 0 → 0, > 360 → 360 | Divides 360 degrees into 8 sectors of 45 degrees each, returns LED index 0–7 |
 
---
 
### Functions and Modularity
 
#### msg.h
Shared header included by both Board 1 and Board 2 projects. Defines the message body structure and type code used in the UART packet.
 
- **`MagMessage_t`** - body struct containing `heading_deg` (float) and `button_flag` (uint8_t)
- **`MSG_TYPE_MAG`** - message type code `0x01` used in the TYPE field of the serial packet
#### Board 1 - board1_main.c
 
**`on_button_press()`**
- ISR callback registered with `discovery_init()`, called from `EXTI0_IRQHandler` on each button press
- Toggles `button_flag` between 0 and 1 using XOR: `button_flag ^= 1`
- Kept minimal as it runs inside the ISR - only modifies one variable
**`on_tx_complete(uint32_t bytes_sent)`**
- Completion callback required by `SerialInitialise()` for USART1
- Does nothing - included to satisfy the serial module interface
**`delay_ms(uint32_t ms)`**
- Blocking busy-loop delay used to pace the main loop at ~100ms per packet
- Uses nested loops calibrated to HSI 8MHz: outer loop runs `ms` times, inner loop runs 8000 cycles
**Main loop**
- Calls `compass_read()` to update the magnetometer data structure
- Retrieves latest data via `compass_get_latest()`
- Packages `heading_deg` and `button_flag` into a `MagMessage_t` struct
- Calls `sendMsg()` to transmit the packet over USART3 to Board 2
- Prints heading and current mode to USART1 debug console
#### Board 2 - board2_main.c
 
**`heading_to_pulse_us(float heading_deg)`**
- Maps compass heading (0–360 degrees) to servo pulse width (1000–2000us)
- Formula: `pulse_us = 1000 + (heading / 360.0) * 1000`
- Clamps output to `[SERVO_POS_MIN_US, SERVO_POS_MAX_US]`
**`heading_to_led(float heading_deg)`**
- Divides 360 degrees into 8 sectors of 45 degrees, returns LED index 0–7
- LED 0 = North (0–44°), LED 1 = NE (45–89°), LED 2 = East (90–134°), LED 3 = SE (135–179°), LED 4 = South (180–224°), LED 5 = SW (225–269°), LED 6 = West (270–314°), LED 7 = NW (315–359°)
**`on_message_received(uint8_t *data, uint8_t size, uint8_t type)`**
- Callback passed to `receiveMsg()`, called on each valid received packet
- Validates message type (`MSG_TYPE_MAG`) and size (`sizeof(MagMessage_t)`)
- If `button_flag == 0`: calls `heading_to_pulse_us()` and `servo_set_position()`, clears all LEDs
- If `button_flag == 1`: calls `heading_to_led()` and `discovery_led_set_single()`, centres servo at 1500us
- Prints current mode, heading, and output value to USART1 debug console
**`on_tx_complete(uint32_t bytes_sent)`**
- Completion callback required by `SerialInitialise()` for USART1
- Does nothing - included to satisfy the serial module interface
#### Reused modules (unchanged from earlier exercises)
 
| Module | Source | Role in Ex5 |
|--------|--------|-------------|
| `compass.c` / `compass.h` | Ex4 | Board 1 - reads magnetometer heading via I2C1 |
| `serial.c` / `serial.h` | Ex3 | Both boards - UART packet TX and RX |
| `discovery.c` / `discovery.h` | Ex1 | Both boards - LED and button interface |
| `button.c` / `button.h` | Ex1 | Board 1 - button interrupt on PA0 |
| `led.c` / `led.h` | Ex1 - | Both boards - LED state management |
| `gpio.c` / `gpio.h` | Ex1 | Both boards - pin configuration |
| `led_timer.c` / `led_timer.h` | Ex1 | Both boards - TIM2 system tick |
| `servo.c` / `servo.h` | Ex2 | Board 2 - TIM3 hardware PWM on PA6 |
 
---

## Testing
