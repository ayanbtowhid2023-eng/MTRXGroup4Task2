/**
 * @file main.c
 * @brief Exercise 3 -- Serial Interface demo.
 *
 * HOW TO DEMO -- uncomment ONE define at a time:
 *
 * TASK_A_DEMO: Sends a raw byte array over USART1.
 *              Connect PC to PC10(TX)/PC11(RX) via USB-UART adapter.
 *              Open terminal at 115200 baud to see output.
 *
 * TASK_B_DEMO: Sends a debug string via sendString().
 *
 * TASK_C_DEMO: Sends a structured SensorData packet via sendMsg().
 *
 * TASK_D_DEMO: Polling receiveMsg() -- loopback wire PC10 -> PC11.
 *
 * TASK_E_DEMO: Interrupt-driven RX via SerialSetRxCallback().
 *              Loopback wire PC10 -> PC11.
 *
 * Wiring:
 *   PC10 (TX) -> USB-UART adapter RX  (or loopback to PC11 for RX tests)
 *   PC11 (RX) -> USB-UART adapter TX
 *   GND       -> USB-UART adapter GND
 */

#include <stdint.h>
#include <string.h>
#include "serial.h"

/* -----------------------------------------------------------------------
 * Uncomment ONE at a time
 * --------------------------------------------------------------------- */
#define TASK_A_DEMO
// #define TASK_B_DEMO
// #define TASK_C_DEMO
// #define TASK_D_DEMO
// #define TASK_E_DEMO

/* -----------------------------------------------------------------------
 * TX completion callback
 * --------------------------------------------------------------------- */
static void on_tx_complete(uint32_t bytes_sent)
{
    (void)bytes_sent;
}

/* -----------------------------------------------------------------------
 * Shared sensor data structure for tasks c, d, e
 * --------------------------------------------------------------------- */
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint32_t timestamp_ms;
} SensorData;

#define MSG_TYPE_SENSOR  0x01

/* -----------------------------------------------------------------------
 * TASK A: send and receive raw bytes
 * --------------------------------------------------------------------- */
#ifdef TASK_A_DEMO

int main(void)
{
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);

    /* TASK A: send a raw byte array -- pointer to array + length */
    uint8_t tx_data[] = { 0x41, 0x42, 0x43, 0x0D, 0x0A };  /* "ABC\r\n" */
    sendBytes(tx_data, sizeof(tx_data), &USART1_PORT);

    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK B: debug string
 * --------------------------------------------------------------------- */
#elif defined(TASK_B_DEMO)

int main(void)
{
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);

    /* TASK B: pointer to string passed in, transmitted over USART1 */
    sendString((uint8_t *)"MTRX2700 Exercise 3 - Serial Module\r\n",
               &USART1_PORT);

    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK C: structured packet TX
 * --------------------------------------------------------------------- */
#elif defined(TASK_C_DEMO)

int main(void)
{
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);

    sendString((uint8_t *)"Sending SensorData packet...\r\n", &USART1_PORT);

    SensorData sensor = { .x = 100, .y = 200, .z = 300, .timestamp_ms = 12345 };

    /* TASK C: package struct into framed packet and send
     * Packet: [ STX | LEN | TYPE | BODY... | ETX | BCC ] */
    sendMsg(&sensor, sizeof(sensor), MSG_TYPE_SENSOR, &USART1_PORT);

    sendString((uint8_t *)"Packet sent.\r\n", &USART1_PORT);

    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK D: polling receiveMsg()
 * Requires loopback wire: PC10 -> PC11
 * --------------------------------------------------------------------- */
#elif defined(TASK_D_DEMO)

static SensorData received_data = {0};

static void on_packet_received(uint8_t *data, uint32_t num_bytes)
{
    if (num_bytes == sizeof(SensorData)) {
        memcpy(&received_data, data, sizeof(SensorData));
    }
}

int main(void)
{
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);

    sendString((uint8_t *)"Task D: polling receiveMsg test...\r\n", &USART1_PORT);

    /* Send a packet -- loopback wire sends it back to RX */
    SensorData sensor = { .x = 100, .y = 200, .z = 300, .timestamp_ms = 12345 };
    sendMsg(&sensor, sizeof(sensor), MSG_TYPE_SENSOR, &USART1_PORT);

    /* TASK D: poll for incoming packet -- blocks until received */
    receiveMsg(&USART1_PORT, &on_packet_received);

    /* Check data matches */
    if (received_data.x            == sensor.x &&
        received_data.y            == sensor.y &&
        received_data.z            == sensor.z &&
        received_data.timestamp_ms == sensor.timestamp_ms)
    {
        sendString((uint8_t *)"Loopback test PASSED!\r\n", &USART1_PORT);
    } else {
        sendString((uint8_t *)"Loopback test FAILED!\r\n", &USART1_PORT);
    }

    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK E: interrupt-driven RX via SerialSetRxCallback()
 * Requires loopback wire: PC10 -> PC11
 * --------------------------------------------------------------------- */
#elif defined(TASK_E_DEMO)

static volatile uint8_t packet_received = 0;
static SensorData received_data = {0};

static void on_packet_received(uint8_t *data, uint32_t num_bytes)
{
    /* Called from ISR -- keep short, just copy and flag */
    if (num_bytes == sizeof(SensorData)) {
        memcpy(&received_data, data, sizeof(SensorData));
        packet_received = 1;
    }
}

int main(void)
{
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);

    /* TASK E: register callback and enable NVIC -- RX now interrupt-driven */
    SerialSetRxCallback(&USART1_PORT, &on_packet_received);
    SerialEnableNVIC();

    sendString((uint8_t *)"Task E: interrupt RX test...\r\n", &USART1_PORT);

    /* Send a packet -- loopback wire sends it back via ISR */
    SensorData sensor = { .x = 100, .y = 200, .z = 300, .timestamp_ms = 12345 };
    sendMsg(&sensor, sizeof(sensor), MSG_TYPE_SENSOR, &USART1_PORT);

    /* Wait for ISR to set flag */
    while (!packet_received) {}

    if (received_data.x            == sensor.x &&
        received_data.y            == sensor.y &&
        received_data.z            == sensor.z &&
        received_data.timestamp_ms == sensor.timestamp_ms)
    {
        sendString((uint8_t *)"Loopback test PASSED!\r\n", &USART1_PORT);
    } else {
        sendString((uint8_t *)"Loopback test FAILED!\r\n", &USART1_PORT);
    }

    while (1) {}
}

#endif
