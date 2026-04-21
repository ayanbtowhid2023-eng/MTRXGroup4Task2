#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include "stm32f303xc.h"

/* Packet framing bytes */
#define SERIAL_STX            0x02
#define SERIAL_ETX            0x03

/* Buffer sizes (internal). SERIAL_MAX_PACKET must be >= 255 so a uint8_t
 * LEN field can address any valid packet. */
#define SERIAL_MAX_PACKET     256
#define SERIAL_TX_BUFFER_SIZE 256

/* Protocol-level maximum body length.
 * Packet = STX + LEN + TYPE + BODY + ETX + CHECKSUM  -> 5 overhead bytes.
 * LEN is a uint8_t field, so total packet must fit in 255 bytes.
 * Therefore max body = 255 - 5 = 250. */
#define SERIAL_MAX_BODY       250U

typedef void (*SerialRxCallback)(const uint8_t *data, uint16_t len);

void serial_init(void);

/* Blocking transmit */
void serial_send_byte(uint8_t byte);
void serial_send_array(const uint8_t *data, uint16_t len);
void sendString(const char *str);
void sendMsg(uint8_t msgType, const void *body, uint8_t bodyLen);

/* Interrupt-driven transmit */
int serial_send_array_it(const uint8_t *data, uint16_t len);
int serial_send_msg_it(uint8_t msgType, const void *body, uint8_t bodyLen);
uint8_t serial_tx_busy(void);

/* Blocking receive */
int receiveMsg(uint8_t *buffer, uint16_t *len);

/* Interrupt-driven receive.
 *
 * Threading model:
 *   - The ISR parses bytes into a double buffer and sets a "ready" flag.
 *   - It does NOT invoke the user callback directly (kept short).
 *   - Call serial_process_rx() from the main loop. It fires the registered
 *     callback in thread context if a new packet is ready.
 *   - As an alternative to the callback, poll serial_get_latest_packet() to
 *     copy the most recent valid packet into a user buffer.
 */
void serial_set_callback(SerialRxCallback cb);
void serial_enable_rx_interrupt(void);
void serial_disable_rx_interrupt(void);
void serial_process_rx(void);
int  serial_get_latest_packet(uint8_t *buffer, uint16_t *len);

#endif
