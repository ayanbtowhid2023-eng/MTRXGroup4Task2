/**
 ******************************************************************************
 * @file    main.c
 * @brief   Exercise 3 - Serial Interface loopback test
 *
 * Hardware setup:
 *   - USB cable from board to Mac
 *   - Jumper wire from PC10 (TX) to PC11 (RX) for loopback
 *
 * View output: screen /dev/tty.usbmodem103 115200
 *
 * Expected output:
 *   MTRX2700 Exercise 3 - Serial Module
 *   Sending SensorData packet...
 *   Loopback test PASSED!
 *     x=100 y=200 z=300 ts=12345
 ******************************************************************************
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "stm32f303xc.h"
#include "serial.h"

/* =========================================================================
 * Message type identifiers
 * ====================================================================== */
#define MSG_TYPE_SENSOR  0x01
#define MSG_TYPE_COMMAND 0x02

/* =========================================================================
 * Example message structures
 * ====================================================================== */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint32_t timestamp_ms;
} SensorData;

typedef struct {
    uint8_t command_id;
    uint8_t param;
} CommandMsg;

/* =========================================================================
 * Global state
 * ====================================================================== */
static volatile uint8_t  g_new_data_available  = 0;
static SensorData        g_last_sensor_data     = {0};
static volatile uint8_t  g_loopback_test_passed = 0;
static volatile uint32_t g_packets_received     = 0;

static const SensorData TX_SENSOR_DATA = {
    .x            = 100,
    .y            = 200,
    .z            = 300,
    .timestamp_ms = 12345
};

/* =========================================================================
 * TX completion callback
 * ====================================================================== */
void finished_transmission(uint32_t bytes_sent)
{
    (void)bytes_sent;
}

/* =========================================================================
 * RX complete callback - called from IRQ when a valid packet arrives
 * ====================================================================== */
void on_rx_complete(uint8_t *data, uint32_t num_bytes)
{
    /* Running in interrupt context - keep short, copy and flag */
    g_packets_received++;
    if (num_bytes == sizeof(SensorData)) {
        memcpy(&g_last_sensor_data, data, sizeof(SensorData));
        g_new_data_available = 1;
    }
}

/* =========================================================================
 * main
 * ====================================================================== */
int main(void)
{
    char debug_buf[64];

    /* Initialise USART1 and enable NVIC interrupt */
    SerialInitialise(BAUD_115200, &USART1_PORT, &finished_transmission);
    setupNVIC();

    /* Register RX callback for packet reception */
    SerialSetRxCallback(&on_rx_complete);

    /* 7.3b: debug strings */
    SerialOutputStringDB((uint8_t*)"================================\r\n", &USART1_PORT);
    SerialOutputStringDB((uint8_t*)"MTRX2700 Exercise 3 - Serial Module\r\n", &USART1_PORT);
    SerialOutputStringDB((uint8_t*)"Loopback: PC10 jumpered to PC11\r\n", &USART1_PORT);
    SerialOutputStringDB((uint8_t*)"================================\r\n", &USART1_PORT);

    /* Wait for debug strings to finish transmitting */
    while (is_transmitting) {}

    /* 7.3c + 7.3a: send structured packet - loops back via jumper wire */
    SerialOutputStringDB((uint8_t*)"Sending SensorData packet...\r\n", &USART1_PORT);
    while (is_transmitting) {}

    sendMsg((void*)&TX_SENSOR_DATA, sizeof(TX_SENSOR_DATA),
            MSG_TYPE_SENSOR, &USART1_PORT);

    /* Main loop - use WFI to wait for interrupts efficiently */
    while (1) {
        __WFI();

        if (g_new_data_available) {
            g_new_data_available = 0;

            if (g_last_sensor_data.x            == TX_SENSOR_DATA.x &&
                g_last_sensor_data.y            == TX_SENSOR_DATA.y &&
                g_last_sensor_data.z            == TX_SENSOR_DATA.z &&
                g_last_sensor_data.timestamp_ms == TX_SENSOR_DATA.timestamp_ms)
            {
                g_loopback_test_passed = 1;
                SerialOutputStringDB((uint8_t*)"Loopback test PASSED!\r\n", &USART1_PORT);

                sprintf(debug_buf, "  x=%d y=%d z=%d ts=%lu\r\n",
                        g_last_sensor_data.x,
                        g_last_sensor_data.y,
                        g_last_sensor_data.z,
                        g_last_sensor_data.timestamp_ms);
                SerialOutputStringDB((uint8_t*)debug_buf, &USART1_PORT);
            } else {
                g_loopback_test_passed = 0;
                SerialOutputStringDB((uint8_t*)"Loopback test FAILED!\r\n", &USART1_PORT);
            }
        }
    }
}
