#include <stdint.h>
#include "demo_uart.h"
#include "serial_comms.h"
#include "serial.h"


static void delay_loop(volatile uint32_t count)
{
    while (count--) {}
}

static void on_message_received(uint8_t *data, uint8_t length, uint8_t msg_type)
{
    SerialComms_SendString((uint8_t *)"OK GOT IT\r\n");
}

void Demo_RunPartA(void)
{
    uint8_t tx_bytes[] = {0x41, 0x42, 0x43, 0x00};
    uint8_t rx_buffer[32];
    uint16_t received;

    SerialComms_SendBytes(tx_bytes, sizeof(tx_bytes));
    uint8_t string_to_receive[5];

    SerialReceiveString(string_to_receive, 5, &USART1_PORT);

    while (1) {} // pause to check variables
}





void Demo_RunPartB(void)
{
    while (1) {
        SerialComms_SendString((uint8_t *)"Debug: serial string output working\r\n");
        delay_loop(800000);
    }
}

void Demo_RunPartC(void)
{
    typedef struct {
        uint16_t heading;
        uint8_t  button_state;
    } SensorData;

    SensorData sensor;
    sensor.heading = 275;
    sensor.button_state = 1;

    uint8_t text_body[] = "HI";

    while (1) {
        SerialComms_SendMsg(MSG_TYPE_TEXT, text_body, sizeof(text_body) - 1);
        delay_loop(800000);
        SerialComms_SendMsg(MSG_TYPE_SENSOR, &sensor, sizeof(sensor));
        delay_loop(800000);
    }
}

void Demo_RunPartD(void)
{
    SerialComms_InitReceiver(&on_message_received);
    SerialComms_SendString((uint8_t *)"Part D: waiting for message...\r\n");

    while (1) {
        SerialComms_ReceiveMsg();
    }
}

void Demo_RunPartE(void)
{
    SerialComms_SendString((uint8_t *)"Part E: send packet now\r\n");

    while (1) {
        if (SerialComms_IsrFired()) {
            SerialComms_SendString((uint8_t *)"ISR FIRED!\r\n");
            break;
        }
    }

    while(1){}
}
