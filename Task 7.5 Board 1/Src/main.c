/**
 * @file board1_main.c
 * @brief Exercise 5 — Board 1 (Compass + Button + UART TX)
 *
 * Board 1 responsibilities:
 *   - Read magnetometer heading via I2C (compass.c from Ex4)
 *   - Detect button press via interrupt (button.c / discovery.c from Ex1)
 *   - Package heading + button_flag into MagMessage_t
 *   - Transmit packet over USART3 TX (serial.c from Ex3) every ~100ms
 *   - Print debug info to USART1 (PC4/PC5 -> ST-Link -> screen terminal)
 *
 * Wiring:
 *   Board 1 PC10 (USART3 TX) ----> Board 2 PC11 (USART3 RX)
 *   Board 1 GND              ----> Board 2 GND
 *
 * Debug:
 *   screen /dev/tty.usbmodemXXX 115200
 *
 * Button behaviour:
 *   Each press toggles button_flag between 0 (servo mode) and 1 (LED mode).
 *   Board 2 reads this flag on every received packet to decide display mode.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f303xc.h"

/* Ex4 — compass */
#include "compass.h"

/* Ex1 — button + discovery board */
#include "discovery.h"
#include "led_timer.h"

/* Ex3 — serial */
#include "serial.h"

/* Ex5 — shared message definition */
#include "msg.h"

/* -----------------------------------------------------------------------
 * Private state
 * --------------------------------------------------------------------- */
static volatile uint8_t button_flag = 0;   /* 0 = servo mode, 1 = LED mode */
static char debug_buf[128];

// Each button press toggles the button flag from 0 to 1
static void on_button_press(void)
{
    button_flag ^= 1;   /* toggle between 0 and 1 */
}

// Callback once transmission has occurred over USART1
static void on_tx_complete(uint32_t bytes_sent)
{
    (void)bytes_sent;
}

// Rougly 100ms delay loop, not entirely accurate, but sufficient for our purposes
static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
        for (volatile uint32_t j = 0; j < 8000; j++);
}

// main
int main(void)
{
    // USART1: debug output to screen
    SerialInitialise(BAUD_115200, &USART1_PORT, on_tx_complete);

    // USART3: packet TX to Board 2 (PC10=TX)
    SerialInitialise(BAUD_115200, &USART3_PORT, 0);

    //Discovery board: LEDs + button (button toggles mode)
    led_timer_init(30);
    discovery_init(on_button_press); // Initialises

    // Compass: I2C1 on PB6/PB7
    compass_init();

    // Startup message for testing
    sendString((uint8_t *)"Board 1\r\n",        &USART1_PORT);

    // Main loop: read compass, send packet, debug print
    while (1)
    {
        // Reads magnetometer
        if (compass_read())
        {
            const MagData_t *mag = compass_get_latest();

            // Builds message body
            MagMessage_t msg;
            msg.heading_deg = mag->heading_deg;
            msg.button_flag = button_flag;

            // Send packet over USART3 to Board 2
            sendMsg((void *)&msg, sizeof(MagMessage_t),
                    MSG_TYPE_MAG, &USART3_PORT);

            // Debug print to screen
            snprintf(debug_buf, sizeof(debug_buf),
                     "Heading: %3d deg | Mode: %s\r\n",
                     (int)msg.heading_deg,
                     msg.button_flag ? "LED" : "SERVO");
            sendString((uint8_t *)debug_buf, &USART1_PORT);
        }
        else
        {
            sendString((uint8_t *)"ERR: compass read failed\r\n", &USART1_PORT);
        }

        delay_ms(100);   // send at ~10 Hz
    }
}
