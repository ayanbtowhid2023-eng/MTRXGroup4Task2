/**
 * @file serial.h
 * @brief Serial (UART) module -- bare-metal STM32F303.
 *
 * Two port instances:
 *   USART1_PORT -- debug output to PC   (PC10=TX, PC11=RX, AF7)
 *   USART3_PORT -- board-to-board Ex5   (PC10=TX, PC11=RX, AF7)
 *
 * Task a) -- sendBytes / receiveBytes (raw byte array send/receive)
 * Task b) -- sendString (debug string)
 * Task c) -- sendMsg (structured framed packet TX)
 * Task d) -- receiveMsg (polling RX with checksum + callback)
 * Task e) -- interrupt-driven RX (replaces polling receiveMsg)
 * Task f) -- interrupt-driven TX with double buffer (advanced)
 *
 * Packet format (tasks c/d/e):
 *   [ STX(0x02) | LEN | TYPE | BODY... | ETX(0x03) | BCC ]
 *   BCC = XOR of LEN, TYPE, and all BODY bytes
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Buffer sizes
 * --------------------------------------------------------------------- */
#define MAX_BUFFER_LENGTH      128
#define DB_BUFFER_SIZE         256
#define SERIAL_RX_BUFFER_SIZE  128

/* -----------------------------------------------------------------------
 * Packet framing constants
 * --------------------------------------------------------------------- */
#define SERIAL_STX  0x02
#define SERIAL_ETX  0x03

/* -----------------------------------------------------------------------
 * Baud rates
 * --------------------------------------------------------------------- */
enum {
    BAUD_9600,
    BAUD_19200,
    BAUD_38400,
    BAUD_57600,
    BAUD_115200
};

/* -----------------------------------------------------------------------
 * Opaque serial port handle
 * --------------------------------------------------------------------- */
struct _SerialPort;
typedef struct _SerialPort SerialPort;

/* Pre-defined port instances */
extern SerialPort USART1_PORT;
extern SerialPort USART3_PORT;

/* -----------------------------------------------------------------------
 * Callback types
 * --------------------------------------------------------------------- */

/**
 * @brief Called when TX completes.
 * @param bytes_sent  Number of bytes transmitted.
 */
typedef void (*SerialTxCallback)(uint32_t bytes_sent);

/**
 * @brief Called when a complete valid packet is received.
 * @param data       Pointer to received body bytes.
 * @param num_bytes  Number of body bytes.
 * NOTE: copy data out before returning -- buffer may be overwritten.
 */
typedef void (*SerialRxCallback)(uint8_t *data, uint32_t num_bytes);

/* -----------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------- */

/**
 * @brief  Initialise a USART port.
 * @param  baudRate            One of the BAUD_* constants.
 * @param  serial_port         Pointer to port instance.
 * @param  completion_function Called after TX completes. NULL to disable.
 */
void SerialInitialise(uint32_t baudRate,
                      SerialPort *serial_port,
                      void (*completion_function)(uint32_t));

/**
 * @brief  Enable USART1 interrupt in NVIC.
 *         Call after SerialInitialise() for interrupt-driven operation.
 */
void SerialEnableNVIC(void);

/* -----------------------------------------------------------------------
 * Task a) -- raw byte send/receive
 * --------------------------------------------------------------------- */

/**
 * @brief  Send an array of bytes over a serial port (polling).
 * @param  data         Pointer to byte array.
 * @param  len          Number of bytes to send.
 * @param  serial_port  Port to use.
 */
void sendBytes(uint8_t *data, uint32_t len, SerialPort *serial_port);

/**
 * @brief  Receive an array of bytes (polling, blocks until len bytes received).
 * @param  buf          Buffer to store received bytes.
 * @param  len          Number of bytes to receive.
 * @param  serial_port  Port to use.
 */
void receiveBytes(uint8_t *buf, uint32_t len, SerialPort *serial_port);

/* -----------------------------------------------------------------------
 * Task b) -- debug string
 * --------------------------------------------------------------------- */

/**
 * @brief  Send a null-terminated string (polling).
 * @param  pt           Pointer to null-terminated string.
 * @param  serial_port  Port to use.
 */
void sendString(uint8_t *pt, SerialPort *serial_port);

/* -----------------------------------------------------------------------
 * Task c) -- structured packet TX
 * --------------------------------------------------------------------- */

/**
 * @brief  Package and send a structured message as a framed packet.
 *         Packet: [ STX | LEN | TYPE | BODY... | ETX | BCC ]
 *
 * @param  data         Pointer to data structure.
 * @param  size         Size in bytes (use sizeof).
 * @param  msg_type     Message type identifier.
 * @param  serial_port  Port to use.
 */
void sendMsg(void *data, uint8_t size, uint8_t msg_type,
             SerialPort *serial_port);

/* -----------------------------------------------------------------------
 * Task d) -- polling RX with checksum + callback
 * --------------------------------------------------------------------- */

/**
 * @brief  Receive a framed packet (polling).
 *         Reads bytes until ETX received, verifies BCC checksum,
 *         then calls callback with received body bytes.
 *
 * @param  serial_port  Port to receive on.
 * @param  callback     Called with (data, num_bytes) on valid packet.
 */
void receiveMsg(SerialPort *serial_port, SerialRxCallback callback);

/* -----------------------------------------------------------------------
 * Task e) -- interrupt-driven RX (replaces polling receiveMsg)
 * --------------------------------------------------------------------- */

/**
 * @brief  Register a callback for interrupt-driven packet reception.
 *         Once registered, packets are received automatically via ISR.
 *         Call SerialEnableNVIC() to activate.
 *
 * @param  serial_port  Port to register on.
 * @param  callback     Called on valid packet receipt. NULL to disable.
 */
void SerialSetRxCallback(SerialPort *serial_port, SerialRxCallback callback);

/* -----------------------------------------------------------------------
 * Task f) -- interrupt-driven TX with double buffer (advanced)
 * --------------------------------------------------------------------- */

/**
 * @brief  Send a string using interrupt-driven double-buffered TX.
 *         Returns immediately -- does not block.
 * @param  pt           Pointer to null-terminated string.
 * @param  serial_port  Port to use.
 */
void sendStringDB(uint8_t *pt, SerialPort *serial_port);

/* Exposed so main can wait for TX to finish */
extern volatile uint8_t is_transmitting;

#endif /* SERIAL_H */
