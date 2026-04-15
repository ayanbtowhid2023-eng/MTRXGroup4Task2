/**
 ******************************************************************************
 * @file    serial.h
 * @brief   Serial (UART) module interface for MTRX2700 C Lab - Exercise 3
 *
 * Provides:
 *   - Debug string transmission (sendString / SerialOutputString)
 *   - Structured message transmission with framing and XOR checksum (sendMsg)
 *   - Interrupt-driven structured message reception (receiveMsg)
 *
 * Packet format used by sendMsg / receiveMsg:
 *
 *   [ START_BYTE | SIZE (1B) | MSG_TYPE (1B) | ...BODY... | CHECKSUM (1B) | STOP_BYTE ]
 *
 *   - START_BYTE  : 0xAA
 *   - SIZE        : number of bytes in the body only
 *   - MSG_TYPE    : caller-defined identifier for the message structure
 *   - BODY        : raw bytes of the user's structure (up to SERIAL_RX_BUFFER_SIZE)
 *   - CHECKSUM    : XOR of SIZE, MSG_TYPE, and all BODY bytes
 *   - STOP_BYTE   : 0x55  (also acts as the terminating character for receiveMsg)
 *
 * Usage example:
 *
 *   // Initialise
 *   SerialInitialise(BAUD_115200, &USART1_PORT, NULL);
 *   SerialInitialise(BAUD_115200, &USART2_PORT, on_receive_callback);
 *
 *   // Send a debug string
 *   SerialOutputString((uint8_t*)"Hello\r\n", &USART1_PORT);
 *
 *   // Send a structured message
 *   struct MyData d = { .x = 1, .y = 2 };
 *   sendMsg(&d, sizeof(d), MSG_TYPE_DATA, &USART2_PORT);
 *
 *   // Receiving is interrupt-driven; the callback set in SerialInitialise
 *   // is called automatically when a complete packet arrives.
 ******************************************************************************
 */

#ifndef SERIAL_H_
#define SERIAL_H_

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Constants
 * ---------------------------------------------------------------------- */

/** Maximum number of body bytes that can be received in one message. */
#define SERIAL_RX_BUFFER_SIZE  128

/** Framing bytes */
#define SERIAL_START_BYTE  0xAA
#define SERIAL_STOP_BYTE   0x55

/* -------------------------------------------------------------------------
 * Baud rate enumeration
 * ---------------------------------------------------------------------- */
#define BAUD_9600   0x00
#define BAUD_19200  0x01
#define BAUD_38400  0x02
#define BAUD_57600  0x03
#define BAUD_115200 0x04

/* -------------------------------------------------------------------------
 * Opaque serial port handle
 *   The full struct definition lives in serial.c to hide hardware details.
 * ---------------------------------------------------------------------- */
typedef struct _SerialPort SerialPort;

/** Pre-instantiated port handles - declare extern so other modules can use them. */
extern SerialPort USART1_PORT;
extern SerialPort USART2_PORT;

/* -------------------------------------------------------------------------
 * Callback types
 * ---------------------------------------------------------------------- */

/**
 * @brief Called after SerialOutputString completes.
 * @param bytes_sent  Number of characters transmitted.
 */
typedef void (*SerialTxCompleteCallback)(uint32_t bytes_sent);

/**
 * @brief Called after a complete, valid packet is received.
 * @param data        Pointer to the received body bytes (internal buffer).
 * @param num_bytes   Number of body bytes received.
 *
 * IMPORTANT: Copy data out of this buffer before returning; the buffer
 * may be overwritten by the next incoming packet.
 */
typedef void (*SerialRxCompleteCallback)(uint8_t *data, uint32_t num_bytes);

/* -------------------------------------------------------------------------
 * Initialisation
 * ---------------------------------------------------------------------- */

/**
 * @brief  Initialise a UART peripheral and register callbacks.
 *
 * @param  baudRate          One of the BAUD_* constants.
 * @param  serial_port       Pointer to the port to initialise (e.g. &USART1_PORT).
 * @param  tx_complete       Called when SerialOutputString finishes. May be NULL.
 * @param  rx_complete       Called when a complete valid packet is received. May be NULL.
 *
 * Enables the RXNE interrupt so that receiving is fully interrupt-driven.
 * Call this once at startup before using any other functions on the port.
 */
void SerialInitialise(uint32_t baudRate,
                      SerialPort *serial_port,
                      SerialTxCompleteCallback tx_complete,
                      SerialRxCompleteCallback rx_complete);

/* -------------------------------------------------------------------------
 * Transmission
 * ---------------------------------------------------------------------- */

/**
 * @brief  Transmit a single byte (blocking until TXE).
 * @param  data         Byte to send.
 * @param  serial_port  Target port.
 */
void SerialOutputChar(uint8_t data, SerialPort *serial_port);

/**
 * @brief  Transmit a null-terminated string (debug / console output).
 *
 * Sends bytes until the null terminator is reached, then invokes the
 * tx_complete callback with the number of bytes sent.
 *
 * @param  pt           Pointer to null-terminated string.
 * @param  serial_port  Target port.
 */
void SerialOutputString(uint8_t *pt, SerialPort *serial_port);

/**
 * @brief  Package a data structure into a framed packet and transmit it.
 *
 * Packet layout:
 *   START_BYTE | size(1B) | msg_type(1B) | body(size B) | checksum(1B) | STOP_BYTE
 *
 * The checksum is the XOR of: size, msg_type, and every byte of the body.
 *
 * @param  data         Pointer to the structure / memory block to send.
 * @param  size         Number of bytes to send (use sizeof(your_struct)).
 * @param  msg_type     Application-defined message type identifier.
 * @param  serial_port  Target port.
 */
void sendMsg(void *data, uint8_t size, uint8_t msg_type, SerialPort *serial_port);

/* -------------------------------------------------------------------------
 * Reception  (interrupt-driven - no explicit "call to receive" needed)
 * ---------------------------------------------------------------------- */

/**
 * @brief  The USART1 interrupt handler - must be called from the IRQ vector.
 *
 * Routes incoming bytes through the packet state machine.  When a complete,
 * checksum-valid packet arrives the rx_complete callback is invoked.
 *
 * Place the following in your stm32f3xx_it.c (or equivalent):
 *
 *   void USART1_IRQHandler(void) { SerialReceiveHandler(&USART1_PORT); }
 *   void USART2_IRQHandler(void) { SerialReceiveHandler(&USART2_PORT); }
 */
void SerialReceiveHandler(SerialPort *serial_port);

#endif /* SERIAL_H_ */
