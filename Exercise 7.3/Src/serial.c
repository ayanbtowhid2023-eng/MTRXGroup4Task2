/**
 * @file serial.c
 * @brief Serial (UART) module -- bare-metal STM32F303.
 *
 * Uses registers.h only -- no HAL, no stm32f303xc.h.
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

#include "serial.h"
#include "registers.h"
#include <stddef.h>
#include <string.h>

#define SERIAL_START_BYTE  0x02
#define SERIAL_STOP_BYTE   0x03

/* -----------------------------------------------------------------------
 * Serial port struct (opaque to users)
 * --------------------------------------------------------------------- */
struct _SerialPort {
    USART_t  *UART;
    GPIO_t   *GPIO;
    volatile uint32_t MaskAPB2ENR;
    volatile uint32_t MaskAPB1ENR;
    volatile uint32_t MaskAHBENR;
    volatile uint32_t SerialPinModeValue;
    volatile uint32_t SerialPinSpeedValue;
    volatile uint32_t SerialPinAlternatePinValueLow;
    volatile uint32_t SerialPinAlternatePinValueHigh;
    void (*completion_function)(uint32_t);
};

/* -----------------------------------------------------------------------
 * USART1 port instance -- PC10=TX, PC11=RX, AF7
 * --------------------------------------------------------------------- */
SerialPort USART1_PORT = {
    USART1,
    GPIOC,
    RCC_APB2ENR_USART1EN,
    0x00,
    RCC_AHBENR_GPIOCEN,
    0xA00,
    0xF00,
    0x770000,
    0x00,
    0x00
};

/* -----------------------------------------------------------------------
 * Task e -- circular RX buffer (written by ISR, read by receiveMsg)
 * --------------------------------------------------------------------- */
static volatile uint8_t s_rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t s_write_pos = 0;
static volatile uint8_t s_read_pos  = 0;

/* -----------------------------------------------------------------------
 * Task f -- interrupt TX + double-buffer RX state
 * --------------------------------------------------------------------- */
#define TX_BUFFER_SIZE_F  128
#define RX_BODY_SIZE_F     64

static volatile uint8_t s_part_f_enabled     = 0;

/* TX */
static volatile uint8_t  s_tx_buffer[TX_BUFFER_SIZE_F];
static volatile uint8_t  s_tx_length          = 0;
static volatile uint8_t  s_tx_index           = 0;
static volatile uint8_t  s_tx_busy            = 0;

/* RX double buffer */
static volatile uint8_t  s_rx_body_buffer[2][RX_BODY_SIZE_F];
static volatile uint8_t  s_rx_body_length[2]  = {0, 0};
static volatile uint8_t  s_rx_msg_type[2]     = {0, 0};
static volatile uint8_t  s_rx_active_buffer   = 0;
static volatile int8_t   s_rx_ready_buffer    = -1;
static volatile uint8_t  s_rx_message_ready   = 0;
static volatile uint8_t  s_rx_dropped_message = 0;

/* RX packet state machine */
typedef enum {
    RX_WAIT_START,
    RX_READ_SIZE,
    RX_READ_TYPE,
    RX_READ_BODY,
    RX_READ_STOP,
    RX_READ_CHECKSUM
} RxState;

static volatile RxState  s_rx_state            = RX_WAIT_START;
static volatile uint8_t  s_rx_expected_size    = 0;
static volatile uint8_t  s_rx_body_index       = 0;
static volatile uint8_t  s_rx_running_checksum = 0;
static volatile uint8_t  s_rx_current_type     = 0;

/* -----------------------------------------------------------------------
 * Internal: circular buffer read
 * Returns 1 if a byte was available, 0 if empty.
 * --------------------------------------------------------------------- */
static uint8_t buffer_read(uint8_t *byte)
{
    if (s_read_pos == s_write_pos) return 0;
    *byte      = s_rx_buffer[s_read_pos];
    s_read_pos = (s_read_pos + 1) % RX_BUFFER_SIZE;
    return 1;
}

/* -----------------------------------------------------------------------
 * Internal: Task f packet state machine (called from ISR)
 * --------------------------------------------------------------------- */
static void part_f_rx_process_byte(uint8_t byte)
{
    uint8_t active = s_rx_active_buffer;

    switch (s_rx_state) {

        case RX_WAIT_START:
            if (byte == SERIAL_START_BYTE) {
                s_rx_running_checksum = SERIAL_START_BYTE;
                s_rx_body_index       = 0;
                s_rx_expected_size    = 0;
                s_rx_state            = RX_READ_SIZE;
            }
            break;

        case RX_READ_SIZE:
            if (byte == 0 || byte > RX_BODY_SIZE_F) {
                s_rx_state = RX_WAIT_START;
            } else {
                s_rx_expected_size     = byte;
                s_rx_running_checksum ^= byte;
                s_rx_state             = RX_READ_TYPE;
            }
            break;

        case RX_READ_TYPE:
            s_rx_current_type      = byte;
            s_rx_running_checksum ^= byte;
            s_rx_state             = RX_READ_BODY;
            break;

        case RX_READ_BODY:
            s_rx_body_buffer[active][s_rx_body_index] = byte;
            s_rx_body_index++;
            s_rx_running_checksum ^= byte;
            if (s_rx_body_index >= s_rx_expected_size) {
                s_rx_state = RX_READ_STOP;
            }
            break;

        case RX_READ_STOP:
            if (byte == SERIAL_STOP_BYTE) {
                s_rx_running_checksum ^= SERIAL_STOP_BYTE;
                s_rx_state = RX_READ_CHECKSUM;
            } else {
                s_rx_state = RX_WAIT_START;
            }
            break;

        case RX_READ_CHECKSUM:
            if (byte == s_rx_running_checksum) {
                if (s_rx_message_ready) {
                    s_rx_dropped_message = 1;
                } else {
                    s_rx_body_length[active] = s_rx_expected_size;
                    s_rx_msg_type[active]    = s_rx_current_type;
                    s_rx_ready_buffer        = active;
                    s_rx_message_ready       = 1;
                    s_rx_active_buffer      ^= 1;
                    s_rx_body_length[s_rx_active_buffer] = 0;
                }
            }
            s_rx_state = RX_WAIT_START;
            break;

        default:
            s_rx_state = RX_WAIT_START;
            break;
    }
}

/* -----------------------------------------------------------------------
 * USART1 ISR
 * Name confirmed correct: USART1_EXTI25_IRQHandler
 * --------------------------------------------------------------------- */
void USART1_EXTI25_IRQHandler(void)
{
    /* Clear overrun */
    if (USART1->ISR & USART_ISR_ORE) {
        USART1->ICR |= USART_ICR_ORECF;
    }

    /* RX byte available */
    if (USART1->ISR & USART_ISR_RXNE) {
        uint8_t byte = (uint8_t)(USART1->RDR & 0xFF);

        if (s_part_f_enabled) {
            part_f_rx_process_byte(byte);
        } else {
            uint8_t next_write = (s_write_pos + 1) % RX_BUFFER_SIZE;
            if (next_write != s_read_pos) {
                s_rx_buffer[s_write_pos] = byte;
                s_write_pos = next_write;
            }
        }
    }

    /* TX register empty -- send next byte (Task f) */
    if ((USART1->ISR & USART_ISR_TXE) && (USART1->CR1 & USART_CR1_TXEIE)) {
        if (s_tx_index < s_tx_length) {
            USART1->TDR = s_tx_buffer[s_tx_index];
            s_tx_index++;
        } else {
            USART1->CR1 &= ~USART_CR1_TXEIE;
            USART1->CR1 |= (1U << 6);   /* TCIE -- transmission complete interrupt */
        }
    }

    /* Transmission complete (Task f) */
    if ((USART1->ISR & USART_ISR_TC) && (USART1->CR1 & (1U << 6))) {
        USART1->ICR |= (1U << 6);       /* TCCF -- clear TC flag */
        USART1->CR1 &= ~(1U << 6);      /* disable TCIE */
        s_tx_busy = 0;
        if (USART1_PORT.completion_function != 0) {
            USART1_PORT.completion_function(s_tx_length);
        }
    }
}

/* -----------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------- */
void SerialInitialise(uint32_t baudRate,
                      SerialPort *serial_port,
                      void (*completion_function)(uint32_t))
{
    serial_port->completion_function = completion_function;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    RCC->AHBENR  |= serial_port->MaskAHBENR;

    serial_port->GPIO->MODER   = serial_port->SerialPinModeValue;
    serial_port->GPIO->OSPEEDR |= serial_port->SerialPinSpeedValue;
    serial_port->GPIO->AFR[0]  |= serial_port->SerialPinAlternatePinValueLow;
    serial_port->GPIO->AFR[1]  |= serial_port->SerialPinAlternatePinValueHigh;

    RCC->APB1ENR |= serial_port->MaskAPB1ENR;
    RCC->APB2ENR |= serial_port->MaskAPB2ENR;

    /* Ensure TXEIE disabled at init */
    serial_port->UART->CR1 &= ~USART_CR1_TXEIE;

    uint16_t *baud_rate_config = (uint16_t *)&serial_port->UART->BRR;
    switch (baudRate) {
        case BAUD_9600:   *baud_rate_config = 0x341; break;
        case BAUD_19200:  *baud_rate_config = 0x1A1; break;
        case BAUD_38400:  *baud_rate_config = 0x0D0; break;
        case BAUD_57600:  *baud_rate_config = 0x08B; break;
        case BAUD_115200: *baud_rate_config = 0x046; break;
        default:          *baud_rate_config = 0x046; break;
    }

    serial_port->UART->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void enable_interrupt(SerialPort *serial_port)
{
    __asm__("cpsid i");

    /* Enable RXNE interrupt in USART CR1 */
    serial_port->UART->CR1 |= USART_CR1_RXNEIE;

    /* Enable USART1 IRQ in NVIC directly -- IRQ 37, ISER[1] bit 5 */
    NVIC->ISER[USART1_IRQn >> 5] = (1U << (USART1_IRQn & 0x1F));

    /* Set priority -- IP[37] */
    NVIC->IP[USART1_IRQn] = (uint8_t)(1U << 4);

    __asm__("cpsie i");
}

/* -----------------------------------------------------------------------
 * Task a) -- raw byte I/O
 * --------------------------------------------------------------------- */
void SerialOutputChar(uint8_t data, SerialPort *serial_port)
{
    while ((serial_port->UART->ISR & USART_ISR_TXE) == 0) {}
    serial_port->UART->TDR = data;
}

uint8_t SerialReceiveByte(SerialPort *serial_port)
{
    while ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {}
    return (uint8_t)(serial_port->UART->RDR);
}

void SerialReceiveString(uint8_t *buffer, size_t length, SerialPort *serial_port)
{
    for (size_t i = 0; i < length; i++) {
        buffer[i] = SerialReceiveByte(serial_port);
    }
}

/* -----------------------------------------------------------------------
 * Task b) -- string output
 * --------------------------------------------------------------------- */
void SerialOutputString(uint8_t *pt, SerialPort *serial_port)
{
    uint32_t counter = 0;
    while (*pt) {
        SerialOutputChar(*pt, serial_port);
        counter++;
        pt++;
    }
    if (serial_port->completion_function != 0) {
        serial_port->completion_function(counter);
    }
}

void sendString(uint8_t *pt, SerialPort *serial_port)
{
    SerialOutputString(pt, serial_port);
}

/* -----------------------------------------------------------------------
 * Task c) -- structured packet TX
 * --------------------------------------------------------------------- */
uint8_t calculateChecksum(void *data, uint8_t size, uint8_t type)
{
    uint8_t *body    = (uint8_t *)data;
    uint8_t checksum = 0;
    checksum ^= SERIAL_START_BYTE;
    checksum ^= size;
    checksum ^= type;
    for (uint8_t i = 0; i < size; i++) {
        checksum ^= body[i];
    }
    checksum ^= SERIAL_STOP_BYTE;
    return checksum;
}

void sendMsg(void *data, uint8_t size, uint8_t type, SerialPort *serial_port)
{
    uint8_t *body    = (uint8_t *)data;
    uint8_t checksum = calculateChecksum(data, size, type);

    SerialOutputChar(SERIAL_START_BYTE, serial_port);
    SerialOutputChar(size,              serial_port);
    SerialOutputChar(type,              serial_port);
    for (uint8_t i = 0; i < size; i++) {
        SerialOutputChar(body[i], serial_port);
    }
    SerialOutputChar(SERIAL_STOP_BYTE, serial_port);
    SerialOutputChar(checksum,         serial_port);
}

/* -----------------------------------------------------------------------
 * Task d) -- polling RX
 * --------------------------------------------------------------------- */
void receiveMsgPolling(SerialPort *serial_port,
                       void (*callback)(uint8_t *data, uint8_t size, uint8_t type))
{
    uint8_t byte, body_size, msg_type;
    uint8_t body[64];
    uint8_t received_checksum, calculated_checksum;

    if (callback == 0) return;

    do { byte = SerialReceiveByte(serial_port); } while (byte != SERIAL_START_BYTE);

    body_size = SerialReceiveByte(serial_port);
    if (body_size == 0 || body_size > 64) return;

    msg_type = SerialReceiveByte(serial_port);

    for (uint8_t i = 0; i < body_size; i++) {
        body[i] = SerialReceiveByte(serial_port);
    }

    byte = SerialReceiveByte(serial_port);
    if (byte != SERIAL_STOP_BYTE) return;

    received_checksum   = SerialReceiveByte(serial_port);
    calculated_checksum = calculateChecksum(body, body_size, msg_type);
    if (calculated_checksum != received_checksum) return;

    callback(body, body_size, msg_type);
}

/* -----------------------------------------------------------------------
 * Task e) -- interrupt RX via circular buffer
 * --------------------------------------------------------------------- */
void receiveMsg(SerialPort *serial_port,
                void (*callback)(uint8_t *data, uint8_t size, uint8_t type))
{
    uint8_t byte, body_size, msg_type;
    uint8_t body[64];
    uint8_t packet[64 + 6];
    uint8_t packet_index        = 0;
    uint8_t received_checksum   = 0;
    uint8_t calculated_checksum = 0;

    if (callback == 0) return;

    /* Wait for START byte */
    do { } while (!buffer_read(&byte) || byte != SERIAL_START_BYTE);
    packet[packet_index++] = byte;

    /* Read SIZE */
    while (!buffer_read(&body_size)) {}
    if (body_size == 0 || body_size > 64) return;
    packet[packet_index++] = body_size;

    /* Read TYPE */
    while (!buffer_read(&msg_type)) {}
    packet[packet_index++] = msg_type;

    /* Read BODY */
    for (uint8_t i = 0; i < body_size; i++) {
        while (!buffer_read(&byte)) {}
        body[i] = byte;
        packet[packet_index++] = byte;
    }

    /* Read STOP */
    while (!buffer_read(&byte)) {}
    if (byte != SERIAL_STOP_BYTE) return;
    packet[packet_index++] = byte;

    /* Read CHECKSUM */
    while (!buffer_read(&received_checksum)) {}

    /* Verify */
    for (uint8_t i = 0; i < packet_index; i++) {
        calculated_checksum ^= packet[i];
    }
    if (calculated_checksum != received_checksum) return;

    callback(body, body_size, msg_type);
}

/* -----------------------------------------------------------------------
 * Task f) -- interrupt TX + double-buffer RX
 * --------------------------------------------------------------------- */
void enable_interrupt_part_f(SerialPort *serial_port)
{
    __asm__("cpsid i");

    s_part_f_enabled     = 1;
    s_rx_state           = RX_WAIT_START;
    s_rx_active_buffer   = 0;
    s_rx_ready_buffer    = -1;
    s_rx_message_ready   = 0;
    s_rx_dropped_message = 0;

    serial_port->UART->CR1 |= USART_CR1_RXNEIE;

    NVIC->ISER[USART1_IRQn >> 5] = (1U << (USART1_IRQn & 0x1F));
    NVIC->IP[USART1_IRQn]        = (uint8_t)(1U << 4);

    __asm__("cpsie i");
}

uint8_t sendStringIT(uint8_t *pt, SerialPort *serial_port)
{
    uint8_t length = 0;
    if (pt == 0) return 0;
    while (pt[length] != 0) {
        length++;
        if (length >= TX_BUFFER_SIZE) return 0;
    }
    __asm__("cpsid i");
    if (s_tx_busy) { __asm__("cpsie i"); return 0; }
    for (uint8_t i = 0; i < length; i++) s_tx_buffer[i] = pt[i];
    s_tx_length = length;
    s_tx_index  = 0;
    s_tx_busy   = 1;
    serial_port->UART->CR1 |= USART_CR1_TXEIE;
    __asm__("cpsie i");
    return 1;
}

uint8_t sendMsgIT(void *data, uint8_t size, uint8_t type, SerialPort *serial_port)
{
    uint8_t *body  = (uint8_t *)data;
    uint8_t  index = 0;

    if (data == 0 || size == 0 || size > RX_BODY_SIZE_F) return 0;
    if ((size + 5) > TX_BUFFER_SIZE) return 0;

    __asm__("cpsid i");
    if (s_tx_busy) { __asm__("cpsie i"); return 0; }

    uint8_t checksum = calculateChecksum(data, size, type);
    s_tx_buffer[index++] = SERIAL_START_BYTE;
    s_tx_buffer[index++] = size;
    s_tx_buffer[index++] = type;
    for (uint8_t i = 0; i < size; i++) s_tx_buffer[index++] = body[i];
    s_tx_buffer[index++] = SERIAL_STOP_BYTE;
    s_tx_buffer[index++] = checksum;

    s_tx_length = index;
    s_tx_index  = 0;
    s_tx_busy   = 1;
    serial_port->UART->CR1 |= USART_CR1_TXEIE;
    __asm__("cpsie i");
    return 1;
}

void receiveMsgDoubleBuffer(SerialPort *serial_port,
                            void (*callback)(uint8_t *data, uint8_t size, uint8_t type))
{
    int8_t  ready;
    uint8_t size, type;
    (void)serial_port;

    if (callback == 0) return;

    __asm__("cpsid i");
    if (!s_rx_message_ready) { __asm__("cpsie i"); return; }
    ready = s_rx_ready_buffer;
    size  = s_rx_body_length[ready];
    type  = s_rx_msg_type[ready];
    __asm__("cpsie i");

    callback((uint8_t *)s_rx_body_buffer[ready], size, type);

    __asm__("cpsid i");
    if (s_rx_ready_buffer == ready) {
        s_rx_message_ready = 0;
        s_rx_ready_buffer  = -1;
    }
    __asm__("cpsie i");
}

uint8_t SerialTxBusy(void)           { return s_tx_busy; }
uint8_t SerialRxDroppedMessage(void) { return s_rx_dropped_message; }
