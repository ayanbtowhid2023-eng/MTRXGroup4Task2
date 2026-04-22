/**
 * @file main.c
 * @brief Exercise 3 -- Serial Interface demo.
 *
 * Uncomment EXACTLY ONE task define at a time.
 *
 * TASK_A_DEMO -- raw bytes, prints "ABC" in terminal
 * TASK_B_DEMO -- sendString
 * TASK_C_DEMO -- sendMsg, raw packet visible in terminal
 * TASK_D_DEMO -- polling receiveMsgPolling, needs loopback PC10->PC11
 * TASK_E_DEMO -- interrupt receiveMsg, needs loopback PC10->PC11
 *                NO USB-UART adapter for this test -- loopback only
 *                LD3 (PE8) on = PASSED
 *                LD4 (PE9) on = FAILED / timed out
 * TASK_F_DEMO -- interrupt TX + double buffer RX, needs loopback PC10->PC11
 */

#include <stdint.h>
#include <string.h>
#include "serial.h"

/* -----------------------------------------------------------------------
 * Uncomment EXACTLY ONE
 * --------------------------------------------------------------------- */
// #define TASK_A_DEMO
// #define TASK_B_DEMO
// #define TASK_C_DEMO
// #define TASK_D_DEMO
#define TASK_E_DEMO
// #define TASK_F_DEMO

/* -----------------------------------------------------------------------
 * Shared
 * --------------------------------------------------------------------- */
static void on_tx_complete(uint32_t bytes_sent) { (void)bytes_sent; }

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint32_t timestamp_ms;
} SensorData;

#define MSG_TYPE_SENSOR  0x01

/* -----------------------------------------------------------------------
 * LED helpers -- PE8 = LD3 (PASSED), PE9 = LD4 (FAILED)
 * --------------------------------------------------------------------- */
static void leds_init(void)
{
    /* Enable GPIOE clock */
    *((volatile uint32_t *)0x40021014) |= (1U << 21);  /* RCC->AHBENR GPIOEEN */
    /* PE8, PE9, PE10 as outputs */
    volatile uint32_t *moder = (volatile uint32_t *)0x48001000;
    *moder &= ~(0xFU << 16);   /* clear PE8 and PE9 mode bits */
    *moder |=  (0x5U << 16);   /* PE8 = output, PE9 = output */
}

static void led_on(uint8_t pin)
{
    volatile uint32_t *bsrr = (volatile uint32_t *)0x48001018;
    *bsrr = (1U << pin);
}

/* -----------------------------------------------------------------------
 * TASK A
 * --------------------------------------------------------------------- */
#ifdef TASK_A_DEMO
int main(void)
{
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);
    for (volatile int i = 0; i < 500000; i++) {}
    uint8_t tx_data[] = { 0x41, 0x42, 0x43, 0x0D, 0x0A };
    for (uint8_t i = 0; i < sizeof(tx_data); i++) {
        SerialOutputChar(tx_data[i], &USART1_PORT);
    }
    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK B
 * --------------------------------------------------------------------- */
#elif defined(TASK_B_DEMO)
int main(void)
{
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);
    sendString((uint8_t *)"MTRX2700 Exercise 3 - Serial Module\r\n", &USART1_PORT);
    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK C
 * --------------------------------------------------------------------- */
#elif defined(TASK_C_DEMO)
int main(void)
{
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);
    sendString((uint8_t *)"Sending SensorData packet...\r\n", &USART1_PORT);
    SensorData sensor = { .x = 100, .y = 200, .z = 300, .timestamp_ms = 12345 };
    sendMsg(&sensor, sizeof(sensor), MSG_TYPE_SENSOR, &USART1_PORT);
    sendString((uint8_t *)"Packet sent.\r\n", &USART1_PORT);
    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK D
 * --------------------------------------------------------------------- */
#elif defined(TASK_D_DEMO)

static SensorData received_d = {0};

static void on_received_d(uint8_t *data, uint8_t size, uint8_t type)
{
    (void)type;
    if (size == sizeof(SensorData)) memcpy(&received_d, data, sizeof(SensorData));
}

int main(void)
{
    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);
    sendString((uint8_t *)"Task D: polling RX...\r\n", &USART1_PORT);
    SensorData sensor = { .x = 100, .y = 200, .z = 300, .timestamp_ms = 12345 };
    sendMsg(&sensor, sizeof(sensor), MSG_TYPE_SENSOR, &USART1_PORT);
    receiveMsgPolling(&USART1_PORT, &on_received_d);
    if (received_d.x == sensor.x && received_d.y == sensor.y &&
        received_d.z == sensor.z && received_d.timestamp_ms == sensor.timestamp_ms) {
        sendString((uint8_t *)"Loopback PASSED!\r\n", &USART1_PORT);
    } else {
        sendString((uint8_t *)"Loopback FAILED!\r\n", &USART1_PORT);
    }
    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK E -- interrupt RX via circular buffer
 * Disconnect USB-UART adapter from PC10/PC11
 * Connect loopback wire: PC10 -> PC11
 * LD3 (PE8) lights up = PASSED
 * LD4 (PE9) lights up = FAILED or timed out
 * --------------------------------------------------------------------- */
#elif defined(TASK_E_DEMO)

static SensorData received_e = {0};
static volatile uint8_t packet_received = 0;

static void on_received_e(uint8_t *data, uint8_t size, uint8_t type)
{
    (void)type;
    if (size == sizeof(SensorData)) {
        memcpy(&received_e, data, sizeof(SensorData));
        packet_received = 1;
    }
}

int main(void)
{
    leds_init();

    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);
    enable_interrupt(&USART1_PORT);

    SensorData sensor = { .x = 100, .y = 200, .z = 300, .timestamp_ms = 12345 };
    sendMsg(&sensor, sizeof(sensor), MSG_TYPE_SENSOR, &USART1_PORT);

    /* receiveMsg reads from circular buffer filled by ISR */
    receiveMsg(&USART1_PORT, &on_received_e);

    if (packet_received &&
        received_e.x            == sensor.x &&
        received_e.y            == sensor.y &&
        received_e.z            == sensor.z &&
        received_e.timestamp_ms == sensor.timestamp_ms)
    {
        led_on(8);   /* PE8 = LD3 = PASSED */
    } else {
        led_on(9);   /* PE9 = LD4 = FAILED */
    }

    while (1) {}
}

/* -----------------------------------------------------------------------
 * TASK F -- interrupt TX + double buffer RX
 * --------------------------------------------------------------------- */
#elif defined(TASK_F_DEMO)

static SensorData received_f = {0};
static volatile uint8_t packet_received_f = 0;

static void on_received_f(uint8_t *data, uint8_t size, uint8_t type)
{
    (void)type;
    if (size == sizeof(SensorData)) {
        memcpy(&received_f, data, sizeof(SensorData));
        packet_received_f = 1;
    }
}

int main(void)
{
    leds_init();

    SerialInitialise(BAUD_115200, &USART1_PORT, &on_tx_complete);
    enable_interrupt_part_f(&USART1_PORT);

    SensorData sensor = { .x = 42, .y = 84, .z = 126, .timestamp_ms = 99999 };
    sendMsgIT(&sensor, sizeof(sensor), MSG_TYPE_SENSOR, &USART1_PORT);

    uint32_t timeout = 0;
    while (!packet_received_f && timeout < 5000000) {
        receiveMsgDoubleBuffer(&USART1_PORT, &on_received_f);
        timeout++;
    }

    if (packet_received_f &&
        received_f.x            == sensor.x &&
        received_f.y            == sensor.y &&
        received_f.z            == sensor.z &&
        received_f.timestamp_ms == sensor.timestamp_ms)
    {
        led_on(8);   /* PE8 = LD3 = PASSED */
    } else {
        led_on(9);   /* PE9 = LD4 = FAILED */
    }

    while (1) {}
}

#endif
