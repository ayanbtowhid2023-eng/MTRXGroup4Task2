/**
 ******************************************************************************
 * @file    serial.h
 * @brief   Serial (UART) module for MTRX2700 C Lab - Exercise 3
 *
 * USART1_PORT: USART1 on PC4(TX)/PC5(RX) -> ST-Link VCP -> screen terminal
 * USART3_PORT: USART3 on PC10(TX)/PC11(RX) -> loopback test (jumper PC10->PC11)
 *
 * Packet format:
 *   [ START(0x02) | SIZE | TYPE | BODY... | STOP(0x03) | CHECKSUM ]
 ******************************************************************************
 */

#ifndef SERIAL_PORT_HEADER
#define SERIAL_PORT_HEADER

#include <stdint.h>

#define MAX_BUFFER_LENGTH  128
#define RX_BUFFER_SIZE     128
#define RX_BODY_SIZE        64

#define SERIAL_START_BYTE  0x02
#define SERIAL_STOP_BYTE   0x03

struct _SerialPort;
typedef struct _SerialPort SerialPort;

extern SerialPort USART1_PORT;  /* debug output -> screen */
extern SerialPort USART3_PORT;  /* packet loopback test   */

enum {
    BAUD_9600,
    BAUD_19200,
    BAUD_38400,
    BAUD_57600,
    BAUD_115200
};

void SerialInitialise(uint32_t baudRate,
                      SerialPort *serial_port,
                      void (*completion_function)(uint32_t));

/* Call after SerialInitialise for USART3_PORT to enable RX interrupt */
void enable_interrupt(SerialPort *serial_port);

void SerialOutputChar(uint8_t data, SerialPort *serial_port);
void SerialOutputString(uint8_t *pt, SerialPort *serial_port);
void sendString(uint8_t *pt, SerialPort *serial_port);

void sendMsg(void *data, uint8_t size, uint8_t type, SerialPort *serial_port);

void receiveMsg(SerialPort *serial_port,
                void (*callback)(uint8_t *data, uint8_t size, uint8_t type));

uint8_t calculateChecksum(void *data, uint8_t size, uint8_t type);
uint8_t SerialTxBusy(void);

#endif /* SERIAL_PORT_HEADER */
