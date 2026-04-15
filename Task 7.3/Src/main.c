/**
 ******************************************************************************
 * @file    main.c
 * @brief   Exercise 3 - Serial Interface demonstration
 *
 * Demonstrates:
 *   a) SerialOutputString  - null-terminated debug strings over USART1
 *   b) sendMsg             - structured packet transmission over USART2
 *   c) Interrupt-driven receive via rx_complete callback
 *
 * Wiring assumptions
 *   USART1 (PC10/PC11) -> USB-Serial adapter -> PC terminal (debug console)
 *   USART2 (PA2 TX / PA3 RX) -> loopback or second STM32 board
 ******************************************************************************
 */

#include <stdint.h>
#include <string.h>
#include "serial.h"
#include "stm32f303xc.h"

/* =========================================================================
 * Example message structures
 * ====================================================================== */

/** Application-defined message type identifiers */
#define MSG_TYPE_SENSOR  0x01
#define MSG_TYPE_COMMAND 0x02

/** Example sensor data structure */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint32_t timestamp_ms;
} SensorData;

/** Example command structure */
typedef struct {
    uint8_t  command_id;
    uint8_t  param;
} CommandMsg;

/* =========================================================================
 * Received data storage
 *   We copy data out of the rx_buffer inside the callback so the buffer
 *   is free for the next incoming packet immediately.
 * ====================================================================== */
static volatile uint8_t  g_new_data_available = 0;
static SensorData        g_last_sensor_data   = {0};

/* =========================================================================
 * Callbacks
 * ====================================================================== */

/**
 * @brief  TX complete callback - called after SerialOutputString finishes.
 */
void on_tx_complete(uint32_t bytes_sent)
{
    /* Optional: toggle a debug LED, update a counter, etc. */
    (void)bytes_sent;
}

/**
 * @brief  RX complete callback - called from the USART2 ISR when a valid
 *         packet has been received and its checksum verified.
 *
 * @param  data       Pointer to the internal receive buffer (body bytes only).
 * @param  num_bytes  Number of body bytes.
 *
 * IMPORTANT: This runs in interrupt context - keep it short.
 *            Copy data out, set a flag, return quickly.
 */
void on_rx_complete(uint8_t *data, uint32_t num_bytes)
{
    if (num_bytes == sizeof(SensorData)) {
        /* Copy received bytes into our local structure */
        memcpy(&g_last_sensor_data, data, sizeof(SensorData));
        g_new_data_available = 1;
    }
}

/* =========================================================================
 * IRQ Handlers
 *   These forward to SerialReceiveHandler so the implementation stays in
 *   serial.c and does not spread across multiple files.
 * ====================================================================== */

void USART1_IRQHandler(void)
{
    SerialReceiveHandler(&USART1_PORT);
}

void USART2_IRQHandler(void)
{
    SerialReceiveHandler(&USART2_PORT);
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void)
{
    /* ------------------------------------------------------------------
     * Initialise serial ports
     * ------------------------------------------------------------------ */

    /* USART1: debug console output to PC. No rx callback needed here. */
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete, NULL);

    /* USART2: inter-board structured messages. Rx callback fires on arrival. */
    SerialInitialise(BAUD_115200, &USART2_PORT, NULL, &on_rx_complete);

    /* ------------------------------------------------------------------
     * (a) Debug string output
     * ------------------------------------------------------------------ */
    SerialOutputString((uint8_t*)"MTRX2700 Exercise 3 - Serial Module\r\n",
                       &USART1_PORT);
    SerialOutputString((uint8_t*)"Sending structured sensor packet...\r\n",
                       &USART1_PORT);

    /* ------------------------------------------------------------------
     * (b) Send a structured packet
     * ------------------------------------------------------------------ */
    SensorData tx_data = {
        .x            = 100,
        .y            = 200,
        .z            = 300,
        .timestamp_ms = 12345
    };

    sendMsg(&tx_data, sizeof(tx_data), MSG_TYPE_SENSOR, &USART2_PORT);

    /* Also send a command message to show different message types work */
    CommandMsg cmd = { .command_id = 0x01, .param = 0xFF };
    sendMsg(&cmd, sizeof(cmd), MSG_TYPE_COMMAND, &USART2_PORT);

    /* ------------------------------------------------------------------
     * (c) Main loop - process received packets when flagged by ISR
     * ------------------------------------------------------------------ */
    uint8_t debug_buf[64];

    for (;;) {
        if (g_new_data_available) {
            /* Clear flag first to avoid missing a packet that arrives
               while we are processing this one */
            g_new_data_available = 0;

            /* Use the debug port to print what we received */
            SerialOutputString((uint8_t*)"Received sensor packet!\r\n",
                               &USART1_PORT);

            /*
             * In a real application you would format g_last_sensor_data
             * here and send it over USART1 using SerialOutputString.
             * sprintf is available if you link newlib (syscalls.c present).
             */
            (void)debug_buf; /* suppress unused warning in this skeleton */
        }
    }
}
