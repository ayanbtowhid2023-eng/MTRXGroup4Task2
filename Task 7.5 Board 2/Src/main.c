/**
 * @file board2_main.c
 * @brief Exercise 5 — Board 2 (UART RX + Servo + LEDs)
 *
 * Board 2 responsibilities:
 *   - Receive MagMessage_t packets over USART3 RX (serial.c from Ex3)
 *   - If button_flag == 0: move servo to heading position (servo.c from Ex2)
 *   - If button_flag == 1: light one LED corresponding to heading (discovery.c from Ex1)
 *   - Print debug info to USART1 (PC4/PC5 -> ST-Link -> screen terminal)
 *
 * Wiring:
 *   Board 1 PC10 (USART3 TX) ----> Board 2 PC11 (USART3 RX)
 *   Board 1 GND              ----> Board 2 GND
 *   Servo signal             ----> PA6
 *   Servo VCC                ----> 5V
 *   Servo GND                ----> GND
 *
 * Debug:
 *   screen /dev/tty.usbmodemXXX 115200
 *
 * Heading -> servo mapping:
 *   0 deg   -> 1000 us (full CW)
 *   180 deg -> 1500 us (centre)
 *   360 deg -> 2000 us (full CCW)
 *   Formula: pulse_us = 1000 + (heading / 360.0) * 1000
 *
 * Heading -> LED mapping (8 LEDs = 8 x 45 deg sectors):
 *   0-44 deg   -> LED 0 (North)
 *   45-89 deg  -> LED 1 (NE)
 *   90-134 deg -> LED 2 (East)
 *   ... and so on
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f303xc.h"

/* Ex2 — servo */
#include "servo.h"

/* Ex1 — LEDs + discovery board */
#include "discovery.h"
#include "led_timer.h"

/* Ex3 — serial */
#include "serial.h"

/* Ex5 — shared message definition */
#include "msg.h"

/* -----------------------------------------------------------------------
 * Private state
 * --------------------------------------------------------------------- */
static char debug_buf[128];

/* -----------------------------------------------------------------------
 * Heading -> servo pulse width
 *
 * Servo range: 1000us (0 deg) to 2000us (360 deg)
 * But servo only physically moves ~90 deg, so we scale:
 *   pulse_us = 1000 + (heading / 360.0) * 1000
 * This maps the full compass range across the full servo range.
 * --------------------------------------------------------------------- */
static uint32_t heading_to_pulse_us(float heading_deg)
{
    if (heading_deg < 0.0f)   heading_deg = 0.0f;
    if (heading_deg > 360.0f) heading_deg = 360.0f;

    uint32_t pulse = (uint32_t)(1000.0f + (heading_deg / 360.0f) * 1000.0f);

    /* Clamp to valid servo range */
    if (pulse < SERVO_POS_MIN_US) pulse = SERVO_POS_MIN_US;
    if (pulse > SERVO_POS_MAX_US) pulse = SERVO_POS_MAX_US;

    return pulse;
}

/* -----------------------------------------------------------------------
 * Heading -> LED index (0-7)
 *
 * Divides 360 degrees into 8 sectors of 45 degrees each.
 * LED 0 = North (0-44), LED 1 = NE (45-89), ..., LED 7 = NW (315-359)
 * --------------------------------------------------------------------- */
static uint8_t heading_to_led(float heading_deg)
{
    if (heading_deg < 0.0f)   heading_deg = 0.0f;
    if (heading_deg > 360.0f) heading_deg = 360.0f;

    /* Each sector = 45 degrees, 8 sectors total */
    uint8_t sector = (uint8_t)(heading_deg / 45.0f);
    if (sector > 7) sector = 7;
    return sector;
}

/* -----------------------------------------------------------------------
 * Message received callback — called by receiveMsg() when a valid
 * packet arrives with correct checksum.
 *
 * data : pointer to message body bytes
 * size : number of body bytes received
 * type : message type code
 * --------------------------------------------------------------------- */
static void on_message_received(uint8_t *data, uint8_t size, uint8_t type)
{
    /* Validate type and size */
    if (type != MSG_TYPE_MAG || size != sizeof(MagMessage_t))
    {
        sendString((uint8_t *)"ERR: unexpected message type/size\r\n", &USART1_PORT);
        return;
    }

    /* Unpack message body */
    MagMessage_t msg;
    memcpy(&msg, data, sizeof(MagMessage_t));

    if (msg.button_flag == 0)
    {
        /* --- SERVO MODE --- */
        uint32_t pulse = heading_to_pulse_us(msg.heading_deg);
        servo_set_position(pulse);

        /* Turn all LEDs off in servo mode */
        discovery_led_clear_all();

        snprintf(debug_buf, sizeof(debug_buf),
                 "SERVO | Heading: %3d deg | Pulse: %lu us\r\n",
                 (int)msg.heading_deg, (unsigned long)pulse);
        sendString((uint8_t *)debug_buf, &USART1_PORT);
    }
    else
    {
        /* --- LED MODE --- */
        uint8_t led_idx = heading_to_led(msg.heading_deg);
        discovery_led_set_single(led_idx);

        /* Centre servo while in LED mode */
        servo_set_position(SERVO_POS_CENTRE_US);

        snprintf(debug_buf, sizeof(debug_buf),
                 "LED   | Heading: %3d deg | LED: %d\r\n",
                 (int)msg.heading_deg, led_idx);
        sendString((uint8_t *)debug_buf, &USART1_PORT);
    }
}

/* -----------------------------------------------------------------------
 * Completion callback for USART1 debug strings
 * --------------------------------------------------------------------- */
static void on_tx_complete(uint32_t bytes_sent)
{
    (void)bytes_sent;
}

/* -----------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------- */
int main(void)
{
    /* Enable FPU */
    SCB->CPACR |= ((3UL << (10*2)) | (3UL << (11*2)));

    /* --- USART1: debug output to screen via ST-Link --- */
    SerialInitialise(BAUD_115200, &USART1_PORT, on_tx_complete);

    /* --- USART3: packet RX from Board 1 (PC11=RX), enable interrupt --- */
    SerialInitialise(BAUD_115200, &USART3_PORT, 0);
    enable_interrupt(&USART3_PORT);

    /* --- Discovery board: LEDs (no button needed on Board 2) --- */
    led_timer_init(30);
    discovery_init(0);   /* NULL = no button callback */

    /* --- Servo: TIM3 hardware PWM on PA6 --- */
    servo_init();

    /* --- Startup message --- */
    sendString((uint8_t *)"================================\r\n", &USART1_PORT);
    sendString((uint8_t *)"MTRX2700 Ex5 -- Board 2\r\n",        &USART1_PORT);
    sendString((uint8_t *)"UART RX -> Servo + LEDs\r\n",        &USART1_PORT);
    sendString((uint8_t *)"================================\r\n", &USART1_PORT);

    /* --- Main loop: poll for incoming packets --- */
    while (1)
    {
        receiveMsg(&USART3_PORT, on_message_received);
    }
}
