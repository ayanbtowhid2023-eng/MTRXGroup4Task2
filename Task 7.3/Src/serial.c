/**
 ******************************************************************************
 * @file    serial.c
 * @brief   Serial (UART) module for MTRX2700 C Lab - Exercise 3
 *
 * USART1_PORT: USART1 on PC4(TX)/PC5(RX) - ST-Link VCP -> screen terminal
 * USART3_PORT: USART3 on PC10(TX)/PC11(RX) - packet loopback / inter-board
 *
 * Packet format:
 *   [ START(0x02) | SIZE | TYPE | BODY... | STOP(0x03) | CHECKSUM ]
 *   Checksum = XOR of START, SIZE, TYPE, all BODY bytes, STOP.
 ******************************************************************************
 */

#include "serial.h"
#include "stm32f303xc.h"
#include <stddef.h>
#include <string.h>

/* =========================================================================
 * Serial port struct
 * ====================================================================== */
struct _SerialPort {
    USART_TypeDef   *UART;
    GPIO_TypeDef    *GPIO;
    volatile uint32_t MaskAPB2ENR;
    volatile uint32_t MaskAPB1ENR;
    volatile uint32_t MaskAHBENR;
    volatile uint32_t SerialPinModeValue;
    volatile uint32_t SerialPinSpeedValue;
    volatile uint32_t SerialPinAlternatePinValueLow;
    volatile uint32_t SerialPinAlternatePinValueHigh;
    void (*completion_function)(uint32_t);
};

/* USART1 on PC4(TX)/PC5(RX), AF7 - connected to ST-Link VCP */
SerialPort USART1_PORT = {
    USART1, GPIOC,
    RCC_APB2ENR_USART1EN, 0x00, RCC_AHBENR_GPIOCEN,
    0x00000A00, 0x00000F00, 0x00770000, 0x00000000, 0x00
};

/* USART3 on PC10(TX)/PC11(RX), AF7 - packet comms / loopback */
SerialPort USART3_PORT = {
    USART3, GPIOC,
    0x00, RCC_APB1ENR_USART3EN, RCC_AHBENR_GPIOCEN,
    0x00A00000, 0x00F00000, 0x00000000, 0x00007700, 0x00
};

/* =========================================================================
 * Circular RX buffer (tasks a-e) - written by ISR, read by receiveMsg
 * ====================================================================== */
static volatile uint8_t  s_rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t  s_write_pos = 0;
static volatile uint8_t  s_read_pos  = 0;

/* =========================================================================
 * TX interrupt buffer (task f)
 * ====================================================================== */
static volatile uint8_t  s_tx_buffer[TX_BUFFER_SIZE];
static volatile uint8_t  s_tx_length   = 0;
static volatile uint8_t  s_tx_index    = 0;
static volatile uint8_t  s_tx_busy     = 0;

/* =========================================================================
 * RX double buffer (task f)
 * ====================================================================== */
static volatile uint8_t  s_rx_body_buf[2][RX_BODY_SIZE];
static volatile uint8_t  s_rx_body_len[2]  = {0, 0};
static volatile uint8_t  s_rx_msg_type[2]  = {0, 0};
static volatile uint8_t  s_rx_active_buf   = 0;
static volatile int8_t   s_rx_ready_buf    = -1;
static volatile uint8_t  s_rx_msg_ready    = 0;
static volatile uint8_t  s_rx_dropped_msg  = 0;
static volatile uint8_t  s_part_f_enabled  = 0;

typedef enum {
    RX_DB_WAIT_START,
    RX_DB_READ_SIZE,
    RX_DB_READ_TYPE,
    RX_DB_READ_BODY,
    RX_DB_READ_STOP,
    RX_DB_READ_CHECKSUM
} RxDbState;

static volatile RxDbState s_rx_db_state        = RX_DB_WAIT_START;
static volatile uint8_t   s_rx_db_expected_size = 0;
static volatile uint8_t   s_rx_db_body_index    = 0;
static volatile uint8_t   s_rx_db_running_chk   = 0;
static volatile uint8_t   s_rx_db_current_type  = 0;

/* =========================================================================
 * part_f_rx_process_byte - double-buffer state machine (declared before use)
 * ====================================================================== */
static void part_f_rx_process_byte(uint8_t byte)
{
    uint8_t active = s_rx_active_buf;

    switch (s_rx_db_state) {
        case RX_DB_WAIT_START:
            if (byte == SERIAL_START_BYTE) {
                s_rx_db_running_chk   = SERIAL_START_BYTE;
                s_rx_db_body_index    = 0;
                s_rx_db_expected_size = 0;
                s_rx_db_state         = RX_DB_READ_SIZE;
            }
            break;

        case RX_DB_READ_SIZE:
            if (byte == 0 || byte > RX_BODY_SIZE) {
                s_rx_db_state = RX_DB_WAIT_START;
            } else {
                s_rx_db_expected_size  = byte;
                s_rx_db_running_chk   ^= byte;
                s_rx_db_state          = RX_DB_READ_TYPE;
            }
            break;

        case RX_DB_READ_TYPE:
            s_rx_db_current_type  = byte;
            s_rx_db_running_chk  ^= byte;
            s_rx_db_state         = RX_DB_READ_BODY;
            break;

        case RX_DB_READ_BODY:
            s_rx_body_buf[active][s_rx_db_body_index] = byte;
            s_rx_db_body_index++;
            s_rx_db_running_chk ^= byte;
            if (s_rx_db_body_index >= s_rx_db_expected_size)
                s_rx_db_state = RX_DB_READ_STOP;
            break;

        case RX_DB_READ_STOP:
            if (byte == SERIAL_STOP_BYTE) {
                s_rx_db_running_chk ^= SERIAL_STOP_BYTE;
                s_rx_db_state        = RX_DB_READ_CHECKSUM;
            } else {
                s_rx_db_state = RX_DB_WAIT_START;
            }
            break;

        case RX_DB_READ_CHECKSUM:
            if (byte == s_rx_db_running_chk) {
                if (s_rx_msg_ready) {
                    s_rx_dropped_msg = 1;
                } else {
                    s_rx_body_len[active]  = s_rx_db_expected_size;
                    s_rx_msg_type[active]  = s_rx_db_current_type;
                    s_rx_ready_buf         = active;
                    s_rx_msg_ready         = 1;
                    s_rx_active_buf       ^= 1;
                    s_rx_body_len[s_rx_active_buf] = 0;
                }
            }
            s_rx_db_state = RX_DB_WAIT_START;
            break;

        default:
            s_rx_db_state = RX_DB_WAIT_START;
            break;
    }
}

/* =========================================================================
 * USART1_EXTI25_IRQHandler - not used for RX in this setup
 * ====================================================================== */
void USART1_EXTI25_IRQHandler(void)
{
    if (USART1->ISR & USART_ISR_ORE)
        USART1->ICR |= USART_ICR_ORECF;
}

/* =========================================================================
 * USART3_EXTI28_IRQHandler
 *   Normal mode  (tasks a-e): stores bytes in circular buffer
 *   Part F mode  (task f):    double-buffer state machine + TX interrupt
 * ====================================================================== */
void USART3_EXTI28_IRQHandler(void)
{
    /* Clear overrun error */
    if (USART3->ISR & USART_ISR_ORE)
        USART3->ICR |= USART_ICR_ORECF;

    /* RX */
    if (USART3->ISR & USART_ISR_RXNE) {
        uint8_t byte = (uint8_t)(USART3->RDR & 0xFF);
        if (s_part_f_enabled) {
            part_f_rx_process_byte(byte);
        } else {
            uint8_t next = (s_write_pos + 1) % RX_BUFFER_SIZE;
            if (next != s_read_pos) {
                s_rx_buffer[s_write_pos] = byte;
                s_write_pos = next;
            }
        }
    }

    /* TX - send next byte from buffer */
    if ((USART3->ISR & USART_ISR_TXE) && (USART3->CR1 & USART_CR1_TXEIE)) {
        if (s_tx_index < s_tx_length) {
            USART3->TDR = s_tx_buffer[s_tx_index++];
        } else {
            USART3->CR1 &= ~USART_CR1_TXEIE;
            USART3->CR1 |=  USART_CR1_TCIE;
        }
    }

    /* TX complete */
    if ((USART3->ISR & USART_ISR_TC) && (USART3->CR1 & USART_CR1_TCIE)) {
        USART3->ICR |= USART_ICR_TCCF;
        USART3->CR1 &= ~USART_CR1_TCIE;
        s_tx_busy = 0;
        if (USART3_PORT.completion_function != 0)
            USART3_PORT.completion_function(s_tx_length);
    }
}

/* =========================================================================
 * buffer_read - read one byte from circular buffer
 * ====================================================================== */
static uint8_t buffer_read(uint8_t *byte)
{
    if (s_read_pos == s_write_pos) return 0;
    *byte = s_rx_buffer[s_read_pos];
    s_read_pos = (s_read_pos + 1) % RX_BUFFER_SIZE;
    return 1;
}

/* =========================================================================
 * SerialInitialise
 * ====================================================================== */
void SerialInitialise(uint32_t baudRate,
                      SerialPort *serial_port,
                      void (*completion_function)(uint32_t))
{
    serial_port->completion_function = completion_function;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    RCC->AHBENR  |= serial_port->MaskAHBENR;

    serial_port->GPIO->MODER   |= serial_port->SerialPinModeValue;
    serial_port->GPIO->OSPEEDR |= serial_port->SerialPinSpeedValue;
    serial_port->GPIO->AFR[0]  |= serial_port->SerialPinAlternatePinValueLow;
    serial_port->GPIO->AFR[1]  |= serial_port->SerialPinAlternatePinValueHigh;

    RCC->APB1ENR |= serial_port->MaskAPB1ENR;
    RCC->APB2ENR |= serial_port->MaskAPB2ENR;

    uint16_t *baud_rate_config = (uint16_t*)&serial_port->UART->BRR;
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

/* =========================================================================
 * enable_interrupt - tasks a-e: circular buffer RX
 * ====================================================================== */
void enable_interrupt(SerialPort *serial_port)
{
    __disable_irq();
    serial_port->UART->CR1 |= USART_CR1_RXNEIE;
    NVIC_SetPriority(USART3_IRQn, 1);
    NVIC_EnableIRQ(USART3_IRQn);
    __enable_irq();
}

/* =========================================================================
 * enable_interrupt_part_f - task f: TX interrupt + double-buffer RX
 * ====================================================================== */
void enable_interrupt_part_f(SerialPort *serial_port)
{
    __disable_irq();

    s_part_f_enabled      = 1;
    s_tx_busy             = 0;
    s_tx_index            = 0;
    s_tx_length           = 0;
    s_rx_db_state         = RX_DB_WAIT_START;
    s_rx_active_buf       = 0;
    s_rx_ready_buf        = -1;
    s_rx_msg_ready        = 0;
    s_rx_dropped_msg      = 0;

    serial_port->UART->CR1 |= USART_CR1_RXNEIE;
    NVIC_SetPriority(USART3_IRQn, 1);
    NVIC_EnableIRQ(USART3_IRQn);

    __enable_irq();
}

/* =========================================================================
 * TX functions (blocking poll) - tasks a-e
 * ====================================================================== */
void SerialOutputChar(uint8_t data, SerialPort *serial_port)
{
    while ((serial_port->UART->ISR & USART_ISR_TXE) == 0) {}
    serial_port->UART->TDR = data;
}

void SerialOutputString(uint8_t *pt, SerialPort *serial_port)
{
    uint32_t counter = 0;
    while (*pt) {
        SerialOutputChar(*pt, serial_port);
        counter++;
        pt++;
    }
    if (serial_port->completion_function != 0)
        serial_port->completion_function(counter);
}

void sendString(uint8_t *pt, SerialPort *serial_port)
{
    SerialOutputString(pt, serial_port);
}

/* =========================================================================
 * calculateChecksum
 * ====================================================================== */
uint8_t calculateChecksum(void *data, uint8_t size, uint8_t type)
{
    uint8_t *body = (uint8_t*)data;
    uint8_t  chk  = 0;
    chk ^= SERIAL_START_BYTE;
    chk ^= size;
    chk ^= type;
    for (uint8_t i = 0; i < size; i++) chk ^= body[i];
    chk ^= SERIAL_STOP_BYTE;
    return chk;
}

/* =========================================================================
 * sendMsg - polling TX framed packet (tasks a-e)
 * ====================================================================== */
void sendMsg(void *data, uint8_t size, uint8_t type, SerialPort *serial_port)
{
    uint8_t *body    = (uint8_t*)data;
    uint8_t  checksum = calculateChecksum(data, size, type);

    SerialOutputChar(SERIAL_START_BYTE, serial_port);
    SerialOutputChar(size,              serial_port);
    SerialOutputChar(type,              serial_port);
    for (uint8_t i = 0; i < size; i++) SerialOutputChar(body[i], serial_port);
    SerialOutputChar(SERIAL_STOP_BYTE,  serial_port);
    SerialOutputChar(checksum,          serial_port);
}

/* =========================================================================
 * sendMsgIT - interrupt-driven framed packet TX (task f)
 * ====================================================================== */
uint8_t sendMsgIT(void *data, uint8_t size, uint8_t type, SerialPort *serial_port)
{
    uint8_t *body  = (uint8_t*)data;
    uint8_t  index = 0;

    if (data == 0 || size == 0 || size > RX_BODY_SIZE) return 0;
    if ((uint8_t)(size + 5) > TX_BUFFER_SIZE) return 0;

    __disable_irq();
    if (s_tx_busy) { __enable_irq(); return 0; }

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
    __enable_irq();
    return 1;
}

/* =========================================================================
 * sendStringIT - interrupt-driven string TX (task f)
 * ====================================================================== */
uint8_t sendStringIT(uint8_t *pt, SerialPort *serial_port)
{
    uint8_t length = 0;
    if (pt == 0) return 0;
    while (pt[length] != 0) {
        length++;
        if (length >= TX_BUFFER_SIZE) return 0;
    }

    __disable_irq();
    if (s_tx_busy) { __enable_irq(); return 0; }

    for (uint8_t i = 0; i < length; i++) s_tx_buffer[i] = pt[i];
    s_tx_length = length;
    s_tx_index  = 0;
    s_tx_busy   = 1;
    serial_port->UART->CR1 |= USART_CR1_TXEIE;
    __enable_irq();
    return 1;
}

/* =========================================================================
 * receiveMsg - circular buffer RX (tasks d-e)
 * ====================================================================== */
void receiveMsg(SerialPort *serial_port,
                void (*callback)(uint8_t *data, uint8_t size, uint8_t type))
{
    uint8_t  byte, body_size, msg_type;
    uint8_t  body[RX_BODY_SIZE];
    uint8_t  received_chk, calculated_chk;
    uint32_t timeout;

    if (callback == 0) return;

    timeout = 0xFFFFFFF;
    do {
        if (buffer_read(&byte) && byte == SERIAL_START_BYTE) break;
        if (--timeout == 0) return;
    } while (1);

    timeout = 0xFFFFFFF;
    while (!buffer_read(&body_size)) { if (--timeout == 0) return; }
    if (body_size == 0 || body_size > RX_BODY_SIZE) return;

    timeout = 0xFFFFFFF;
    while (!buffer_read(&msg_type)) { if (--timeout == 0) return; }

    for (uint8_t i = 0; i < body_size; i++) {
        timeout = 0xFFFFFFF;
        while (!buffer_read(&byte)) { if (--timeout == 0) return; }
        body[i] = byte;
    }

    timeout = 0xFFFFFFF;
    while (!buffer_read(&byte)) { if (--timeout == 0) return; }
    if (byte != SERIAL_STOP_BYTE) {
        sendString((uint8_t*)"ERR: no stop byte\r\n", &USART1_PORT);
        return;
    }

    timeout = 0xFFFFFFF;
    while (!buffer_read(&received_chk)) { if (--timeout == 0) return; }
    calculated_chk = calculateChecksum(body, body_size, msg_type);
    if (calculated_chk != received_chk) {
        sendString((uint8_t*)"ERR: bad checksum\r\n", &USART1_PORT);
        return;
    }

    callback(body, body_size, msg_type);
}

/* =========================================================================
 * receiveMsgDoubleBuffer - double-buffer RX (task f)
 * ====================================================================== */
void receiveMsgDoubleBuffer(SerialPort *serial_port,
                            void (*callback)(uint8_t *data, uint8_t size, uint8_t type))
{
    int8_t  ready;
    uint8_t size, type;
    (void)serial_port;
    if (callback == 0) return;

    __disable_irq();
    if (!s_rx_msg_ready) { __enable_irq(); return; }
    ready = s_rx_ready_buf;
    size  = s_rx_body_len[ready];
    type  = s_rx_msg_type[ready];
    __enable_irq();

    callback((uint8_t*)s_rx_body_buf[ready], size, type);

    __disable_irq();
    if (s_rx_ready_buf == ready) {
        s_rx_msg_ready = 0;
        s_rx_ready_buf = -1;
    }
    __enable_irq();
}

uint8_t SerialTxBusy(void)     { return 0; }
uint8_t SerialTxBusy_IT(void)  { return s_tx_busy; }
uint8_t SerialRxDroppedMessage(void) { return s_rx_dropped_msg; }
