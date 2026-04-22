/**
 ******************************************************************************
 * @file    main.c
 * @brief   Exercise 3 - Serial Interface loopback test
 *
 * Hardware setup:
 *   - USB cable from board to Mac
 *   - Jumper wire from PC10 (USART3 TX) to PC11 (USART3 RX) for loopback
 *
 * USART1 (PC4/PC5) -> ST-Link -> screen: debug strings
 * USART3 (PC10/PC11) -> loopback jumper: sendMsg / receiveMsg test
 *
 * View output: screen /dev/tty.usbmodem103 115200
 ******************************************************************************
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "stm32f303xc.h"
#include "serial.h"

#define MSG_TYPE_SENSOR  0x01

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint32_t timestamp_ms;
} SensorData;

static const SensorData TX_SENSOR_DATA = {
    .x            = 100,
    .y            = 200,
    .z            = 300,
    .timestamp_ms = 12345
};

void finished_transmission(uint32_t bytes_sent) { (void)bytes_sent; }

static char debug_buf[64];

void on_message_received(uint8_t *data, uint8_t size, uint8_t type)
{
    if (type == MSG_TYPE_SENSOR && size == sizeof(SensorData)) {
        SensorData received;
        memcpy(&received, data, sizeof(SensorData));

        if (received.x            == TX_SENSOR_DATA.x &&
            received.y            == TX_SENSOR_DATA.y &&
            received.z            == TX_SENSOR_DATA.z &&
            received.timestamp_ms == TX_SENSOR_DATA.timestamp_ms)
        {
            sendString((uint8_t*)"Loopback test PASSED!\r\n", &USART1_PORT);
            sprintf(debug_buf, "  x=%d y=%d z=%d ts=%lu\r\n",
                    received.x, received.y, received.z, received.timestamp_ms);
            sendString((uint8_t*)debug_buf, &USART1_PORT);
        } else {
            sendString((uint8_t*)"Loopback test FAILED - data mismatch!\r\n", &USART1_PORT);
        }
    } else {
        sendString((uint8_t*)"Received unknown message type\r\n", &USART1_PORT);
    }
}

int main(void)
{
    /* USART1: debug output to screen via ST-Link */
    SerialInitialise(BAUD_115200, &USART1_PORT, &finished_transmission);

    /* USART3: packet loopback test - PC10 jumpered to PC11 */
    SerialInitialise(BAUD_115200, &USART3_PORT, 0);
    enable_interrupt(&USART3_PORT);

    sendString((uint8_t*)"================================\r\n", &USART1_PORT);
    sendString((uint8_t*)"MTRX2700 Exercise 3 - Serial Module\r\n", &USART1_PORT);
    sendString((uint8_t*)"Debug: USART1 PC4/PC5 -> screen\r\n", &USART1_PORT);
    sendString((uint8_t*)"Packet: USART3 PC10/PC11 loopback\r\n", &USART1_PORT);
    sendString((uint8_t*)"================================\r\n", &USART1_PORT);

    sendString((uint8_t*)"Sending SensorData packet...\r\n", &USART1_PORT);
    sendMsg((void*)&TX_SENSOR_DATA, sizeof(TX_SENSOR_DATA),
            MSG_TYPE_SENSOR, &USART3_PORT);

    while (1) {
        receiveMsg(&USART3_PORT, &on_message_received);
    }
}
