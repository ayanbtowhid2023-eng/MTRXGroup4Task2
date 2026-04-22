/**
 * @file serial.h
 * @brief Serial (UART) module -- bare-metal STM32F303.
 *
 * Uses registers.h only -- no HAL, no stm32f303xc.h.
 *
 * ISR name: USART1_EXTI25_IRQHandler (confirmed working on this board)
 *
 * Task a) -- SerialOutputChar / SerialReceiveByte (raw bytes)
 * Task b) -- sendString / SerialOutputString (debug string)
 * Task c) -- sendMsg (structured framed packet TX)
 * Task d) -- receiveMsgPolling (polling RX with checksum + callback)
 * Task e) -- receiveMsg (interrupt-driven circular buffer RX)
 * Task f) -- sendMsgIT / receiveMsgDoubleBuffer (interrupt TX, double buffer RX)
 *
 * Packet format:
 *   [ START(0x02) | SIZE | TYPE | BODY... | STOP(0x03) | CHECKSUM ]
 *   CHECKSUM = XOR of all bytes from START through STOP inclusive
 */

#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>

/* -----------------------------------------------------------------------
 * Buffer / packet sizes
 * --------------------------------------------------------------------- */
#define RX_BUFFER_SIZE   128
#define TX_BUFFER_SIZE   128
#define RX_BODY_SIZE      64

/* -----------------------------------------------------------------------
 * Packet framing
 * --------------------------------------------------------------------- */
#define START_BYTE  0x02
#define STOP_BYTE   0x03

/* -----------------------------------------------------------------------
 * Baud rate constants
 * --------------------------------------------------------------------- */
enum {
    BAUD_9600   = 0,
    BAUD_19200  = 1,
    BAUD_38400  = 2,
    BAUD_57600  = 3,
    BAUD_115200 = 4
};

/* -----------------------------------------------------------------------
 * Opaque port handle
 * --------------------------------------------------------------------- */
struct _SerialPort;
typedef struct _SerialPort SerialPort;

extern SerialPort USART1_PORT;

/* -----------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------- */

/**
 * @brief Initialise a USART port (GPIO + clocks + baud rate).
 * @param baudRate           One of the BAUD_* constants.
 * @param serial_port        Port to initialise.
 * @param completion_function Called after TX string completes. NULL OK.
 */
void SerialInitialise(uint32_t baudRate,
                      SerialPort *serial_port,
                      void (*completion_function)(uint32_t));

/**
 * @brief Enable USART1 RX interrupt in NVIC (Task e).
 *        Call after SerialInitialise().
 */
void enable_interrupt(SerialPort *serial_port);

/**
 * @brief Enable USART1 interrupt with double-buffer RX + interrupt TX (Task f).
 *        Call after SerialInitialise().
 */
void enable_interrupt_part_f(SerialPort *serial_port);

/* -----------------------------------------------------------------------
 * Task a) -- raw byte I/O
 * --------------------------------------------------------------------- */

/** @brief Send one byte (polling). */
void SerialOutputChar(uint8_t data, SerialPort *serial_port);

/** @brief Receive one byte (polling, blocks until byte available). */
uint8_t SerialReceiveByte(SerialPort *serial_port);

/** @brief Receive exactly @p length bytes into @p buffer (polling). */
void SerialReceiveString(uint8_t *buffer, size_t length, SerialPort *serial_port);

/* -----------------------------------------------------------------------
 * Task b) -- string output
 * --------------------------------------------------------------------- */

/** @brief Send null-terminated string (polling). */
void SerialOutputString(uint8_t *pt, SerialPort *serial_port);

/** @brief Alias for SerialOutputString. */
void sendString(uint8_t *pt, SerialPort *serial_port);

/* -----------------------------------------------------------------------
 * Task c) -- structured packet TX (polling)
 * --------------------------------------------------------------------- */

/**
 * @brief Compute packet checksum.
 *        XOR of START_BYTE, size, type, all body bytes, STOP_BYTE.
 */
uint8_t calculateChecksum(void *data, uint8_t size, uint8_t type);

/**
 * @brief Assemble and send a framed packet (polling).
 * @param data        Pointer to body data.
 * @param size        Body size in bytes.
 * @param type        Message type identifier.
 * @param serial_port Port to send on.
 */
void sendMsg(void *data, uint8_t size, uint8_t type, SerialPort *serial_port);

/* -----------------------------------------------------------------------
 * Task d) -- polling RX (reads directly from hardware RXNE)
 * --------------------------------------------------------------------- */

/**
 * @brief Receive and validate a framed packet (pure polling).
 *        Blocks until a complete valid packet arrives.
 * @param serial_port Port to receive on.
 * @param callback    Called with (body, size, type) on valid packet.
 */
void receiveMsgPolling(SerialPort *serial_port,
                       void (*callback)(uint8_t *data, uint8_t size, uint8_t type));

/* -----------------------------------------------------------------------
 * Task e) -- interrupt-driven RX via circular buffer
 *
 * Call enable_interrupt() first to activate the ISR.
 * The ISR fills a circular buffer; receiveMsg() drains it.
 * --------------------------------------------------------------------- */

/**
 * @brief Receive and validate a framed packet from the ISR circular buffer.
 *        Non-blocking per byte -- uses timeouts internally.
 * @param serial_port Port (used for error messages only).
 * @param callback    Called with (body, size, type) on valid packet.
 */
void receiveMsg(SerialPort *serial_port,
                void (*callback)(uint8_t *data, uint8_t size, uint8_t type));

/* -----------------------------------------------------------------------
 * Task f) -- interrupt-driven TX + double-buffer RX (advanced)
 *
 * Call enable_interrupt_part_f() first.
 * --------------------------------------------------------------------- */

/**
 * @brief Send a null-terminated string using interrupt-driven TX.
 *        Returns immediately. Returns 0 if TX is busy.
 */
uint8_t sendStringIT(uint8_t *pt, SerialPort *serial_port);

/**
 * @brief Send a framed packet using interrupt-driven TX.
 *        Returns immediately. Returns 0 if TX is busy.
 */
uint8_t sendMsgIT(void *data, uint8_t size, uint8_t type, SerialPort *serial_port);

/**
 * @brief Check and deliver a completed packet from the double RX buffer.
 *        Call this from the main loop. Non-blocking.
 * @param serial_port Port (unused, for API consistency).
 * @param callback    Called with (body, size, type) if a packet is ready.
 */
void receiveMsgDoubleBuffer(SerialPort *serial_port,
                            void (*callback)(uint8_t *data, uint8_t size, uint8_t type));

/** @brief Returns 1 if interrupt TX is currently busy. */
uint8_t SerialTxBusy(void);

/** @brief Returns 1 if a message was dropped due to buffer not being read in time. */
uint8_t SerialRxDroppedMessage(void);

#endif /* SERIAL_H */
