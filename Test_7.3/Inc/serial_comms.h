/*
 * serial_comms.h
 * MTRX2700 Lab 2 — Exercise 3: Serial Interface
 *
 * Higher-level wrapper around the raw UART functions in serial.h.
 * SerialComms owns one SerialPort and provides a clean API for the
 * rest of the application (send bytes, send strings, send/receive
 * framed messages with checksum verification).
 *
 * Packet format mirrored from serial.c:
 *   [ SERIAL_START_BYTE | SIZE | MSG_TYPE | BODY[SIZE] | SERIAL_STOP_BYTE | CHECKSUM ]
 *   Checksum = XOR of all preceding bytes in the packet.
 *
 * NOTE: This module manages its own static ISR + circular buffer.
 *       Do not mix SerialComms_ReceiveMsg() with serial.c's receiveMsg()
 *       on the same port — each has its own state.
 */

#ifndef SERIAL_COMMS_H
#define SERIAL_COMMS_H

#include <stdint.h>
#include "serial.h"

/* -----------------------------------------------------------------------
 * Protocol constants
 * ----------------------------------------------------------------------- */
#define SERIAL_START_BYTE   0xAA
#define SERIAL_STOP_BYTE    0x55

/* Maximum number of bytes in a message body.
 * Packet buffer on the stack = SERIAL_MAX_BODY + 6 bytes. */
#define SERIAL_MAX_BODY     64

/* Circular RX buffer size (must be a power of 2 for cheap modulo). */
#define RX_BUFFER_SIZE      128

/* -----------------------------------------------------------------------
 * Application-level message type identifiers
 * Add new types here as the integration task grows.
 * ----------------------------------------------------------------------- */
#define MSG_TYPE_TEXT       0x01
#define MSG_TYPE_SENSOR     0x02
#define MSG_TYPE_COMMAND    0x03

/* -----------------------------------------------------------------------
 * Initialisation
 * ----------------------------------------------------------------------- */

/*
 * SerialComms_Init
 * Configure the given port at 115200 baud and store it for all subsequent
 * SerialComms_* calls.  Must be called before any other function.
 *
 * port : e.g. &USART1_PORT (defined in serial.c / serial.h)
 */
void SerialComms_Init(SerialPort *port);

/* -----------------------------------------------------------------------
 * Transmit helpers
 * ----------------------------------------------------------------------- */

/*
 * SerialComms_SendBytes
 * Blocking: transmit `length` raw bytes from `data`.
 * Null pointer or zero length is silently ignored.
 */
void SerialComms_SendBytes(const uint8_t *data, uint16_t length);

/*
 * SerialComms_SendString
 * Blocking: transmit a null-terminated string.
 * Intended for debug output (Part B equivalent in the comms layer).
 */
void SerialComms_SendString(const uint8_t *text);

/*
 * SerialComms_SendMsg
 * Assemble and transmit a framed packet.
 *
 * msg_type  : one of the MSG_TYPE_* values above
 * body      : pointer to the payload (any struct or byte array)
 * body_size : number of payload bytes (use sizeof for structures)
 *
 * Silently returns if body is NULL, body_size is 0, or body_size exceeds
 * SERIAL_MAX_BODY.
 */
void SerialComms_SendMsg(uint8_t msg_type, const void *body, uint8_t body_size);

/* -----------------------------------------------------------------------
 * Receive helpers
 * ----------------------------------------------------------------------- */

/*
 * SerialComms_ReceiveBytes
 * Blocking: receive up to `max_length` bytes into `buffer`, stopping early
 * if a null byte (0x00) is encountered.
 * Returns the number of bytes actually stored.
 *
 * NOTE: internally calls SerialInputChar which is not implemented in the
 * current serial.c.  Replace with SerialReceiveByte if needed.
 */
uint16_t SerialComms_ReceiveBytes(uint8_t *buffer, uint16_t max_length);

/*
 * SerialComms_InitReceiver
 * Register the callback that SerialComms_ReceiveMsg() fires when a complete,
 * valid packet is received.  Must be called before SerialComms_ReceiveMsg().
 *
 * callback(data, length, msg_type)
 *   data     : pointer to received body bytes
 *   length   : number of body bytes
 *   msg_type : MSG_TYPE_* value from the packet header
 */
void SerialComms_InitReceiver(void (*callback)(uint8_t *data,
                                               uint8_t length,
                                               uint8_t msg_type));

/*
 * SerialComms_ReceiveMsg
 * Attempt to read one complete framed packet from the internal circular
 * buffer.  Returns immediately if no START byte is available.
 *
 * Call this repeatedly from main() after enabling the UART interrupt so
 * the ISR can fill the circular buffer concurrently.
 *
 * On success, verifies the checksum then fires the registered callback.
 * Sends an error string over the port if the checksum fails.
 *
 * Requires SerialComms_InitReceiver() to have been called first.
 */
void SerialComms_ReceiveMsg(void);

/* -----------------------------------------------------------------------
 * Utility / diagnostics
 * ----------------------------------------------------------------------- */

/*
 * SerialComms_Checksum
 * XOR of all bytes in `data[0..length-1]`.
 * Exposed for unit testing; used internally by SendMsg / ReceiveMsg.
 */
uint8_t SerialComms_Checksum(const uint8_t *data, uint8_t length);

/*
 * SerialComms_IsrFired
 * Returns 1 if the USART1 ISR has fired at least once since initialisation.
 * Useful for confirming that interrupt-driven receive is active (Part E demo).
 */
uint8_t SerialComms_IsrFired(void);

#endif /* SERIAL_COMMS_H */
