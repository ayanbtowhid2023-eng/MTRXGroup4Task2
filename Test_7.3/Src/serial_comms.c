#include "serial_comms.h"
#include "stm32f303xc.h"

static SerialPort *s_port = 0;
static void (*s_rx_callback)(uint8_t *data, uint8_t length, uint8_t msg_type) = 0;

static volatile uint8_t s_rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t s_write_pos = 0;
static volatile uint8_t s_read_pos  = 0;
static volatile uint8_t s_isr_fired = 0;

// -------------------------------------------------------
// USART1_IRQHandler  (Part E)
// Fires automatically when a byte arrives on USART1
// -------------------------------------------------------



/*
void USART1_IRQHandler(void)
{
    s_isr_fired = 1;

    // Clear overrun error if set — otherwise UART stops receiving
    if (USART1->ISR & USART_ISR_ORE) {
        USART1->ICR |= USART_ICR_ORECF;
    }

    if (USART1->ISR & USART_ISR_RXNE) {
        uint8_t byte = (uint8_t)(USART1->RDR & 0xFF);
        uint8_t next_write = (s_write_pos + 1) % RX_BUFFER_SIZE;
        if (next_write != s_read_pos) {
            s_rx_buffer[s_write_pos] = byte;
            s_write_pos = next_write;
        }
    }
}
*/

static uint8_t buffer_read(uint8_t *byte)
{
    if (s_read_pos == s_write_pos) {
        return 0;
    }
    *byte = s_rx_buffer[s_read_pos];
    s_read_pos = (s_read_pos + 1) % RX_BUFFER_SIZE;
    return 1;
}

static void on_tx_complete(uint32_t bytes_sent)
{
    (void)bytes_sent;
}

void SerialComms_Init(SerialPort *port)
{
    if (port == 0) {
        return;
    }
    s_port = port;
    SerialInitialise(BAUD_115200, s_port, &on_tx_complete);
}

void SerialComms_SendBytes(const uint8_t *data, uint16_t length)
{
    uint16_t i;
    if (data == 0 || length == 0) {
        return;
    }
    for (i = 0; i < length; i++) {
        SerialOutputChar(data[i], s_port);
    }
}

uint16_t SerialComms_ReceiveBytes(uint8_t *buffer, uint16_t max_length)
{
    uint16_t count = 0;
    uint8_t byte;

    if (buffer == 0 || max_length == 0) {
        return 0;
    }

    while (count < max_length) {
        if (!SerialInputChar(&byte, s_port, 0xFFFFFF)) {
            break;
        }
        buffer[count] = byte;
        count++;
        if (byte == 0x00) {
            break;
        }
    }
    return count;
}

void SerialComms_SendString(const uint8_t *text)
{
    if (text == 0) {
        return;
    }
    SerialOutputString((uint8_t *)text, s_port);
}

uint8_t SerialComms_Checksum(const uint8_t *data, uint8_t length)
{
    uint8_t i;
    uint8_t result = 0;
    for (i = 0; i < length; i++) {
        result ^= data[i];
    }
    return result;
}

void SerialComms_SendMsg(uint8_t msg_type, const void *body, uint8_t body_size)
{
    const uint8_t *body_bytes = (const uint8_t *)body;
    uint8_t packet[SERIAL_MAX_BODY + 6];
    uint8_t index = 0;
    uint8_t i;

    if (body == 0 || body_size == 0 || body_size > SERIAL_MAX_BODY) {
        return;
    }

    packet[index++] = SERIAL_START_BYTE;
    packet[index++] = body_size;
    packet[index++] = msg_type;

    for (i = 0; i < body_size; i++) {
        packet[index++] = body_bytes[i];
    }

    packet[index++] = SERIAL_STOP_BYTE;

    uint8_t checksum = SerialComms_Checksum(packet, index);
    packet[index++] = checksum;

    SerialComms_SendBytes(packet, index);
}

void SerialComms_InitReceiver(void (*callback)(uint8_t *data, uint8_t length, uint8_t msg_type))
{
    if (callback == 0) {
        return;
    }
    s_rx_callback = callback;
}

void SerialComms_ReceiveMsg(void)
{
    uint8_t byte;
    uint8_t body_size;
    uint8_t msg_type;
    uint8_t body[SERIAL_MAX_BODY];
    uint8_t packet[SERIAL_MAX_BODY + 6];
    uint8_t packet_index = 0;
    uint8_t received_checksum;
    uint8_t calculated_checksum;
    uint8_t i;
    uint32_t timeout;

    if (s_rx_callback == 0) {
        return;
    }

    // Step 1: Wait for START byte
    timeout = 0xFFFFFF;
    do {
        if (buffer_read(&byte)) {
            if (byte == SERIAL_START_BYTE) break;
        }
        timeout--;
        if (timeout == 0) return;
    } while (1);

    packet[packet_index++] = byte;

    // Step 2: Read SIZE
    timeout = 0xFFFFFF;
    while (!buffer_read(&body_size)) {
        if (--timeout == 0) return;
    }
    if (body_size == 0 || body_size > SERIAL_MAX_BODY) return;
    packet[packet_index++] = body_size;

    // Step 3: Read TYPE
    timeout = 0xFFFFFF;
    while (!buffer_read(&msg_type)) {
        if (--timeout == 0) return;
    }
    packet[packet_index++] = msg_type;

    // Step 4: Read BODY
    for (i = 0; i < body_size; i++) {
        timeout = 0xFFFFFF;
        while (!buffer_read(&byte)) {
            if (--timeout == 0) return;
        }
        body[i] = byte;
        packet[packet_index++] = byte;
    }

    // Step 5: Check STOP byte
    timeout = 0xFFFFFF;
    while (!buffer_read(&byte)) {
        if (--timeout == 0) return;
    }
    if (byte != SERIAL_STOP_BYTE) return;
    packet[packet_index++] = byte;

    // Step 6: Verify CHECKSUM
    timeout = 0xFFFFFF;
    while (!buffer_read(&received_checksum)) {
        if (--timeout == 0) return;
    }

    calculated_checksum = SerialComms_Checksum(packet, packet_index);
    if (calculated_checksum != received_checksum) {
        SerialComms_SendString((uint8_t *)"ERR: bad checksum\r\n");
        return;
    }

    // Step 7: Fire callback
    s_rx_callback(body, body_size, msg_type);
}

uint8_t SerialComms_IsrFired(void) {
    return s_isr_fired;
}