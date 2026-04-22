// Taken and built upon from
// https://github.com/stefanbw/MTRX2700-2026/blob/main/W06-UART-modular-design/Src/serial.c

#include "serial.h"
#include <stddef.h>
#include "stm32f303xc.h"

#define START_BYTE 0x02
#define STOP_BYTE  0x03
#define RX_BUFFER_SIZE 128

struct _SerialPort {
    USART_TypeDef *UART;
    GPIO_TypeDef *GPIO;
    volatile uint32_t MaskAPB2ENR;
    volatile uint32_t MaskAPB1ENR;
    volatile uint32_t MaskAHBENR;
    volatile uint32_t SerialPinModeValue;
    volatile uint32_t SerialPinSpeedValue;
    volatile uint32_t SerialPinAlternatePinValueLow;
    volatile uint32_t SerialPinAlternatePinValueHigh;
    void (*completion_function)(uint32_t);
};

SerialPort USART1_PORT = {USART1,
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

// Circular receive buffer — written by ISR, read by receiveMsg
static volatile uint8_t s_rx_buffer[RX_BUFFER_SIZE];
static volatile uint8_t s_write_pos = 0;
static volatile uint8_t s_read_pos  = 0;



//-------------------------------------------------------------------------------
#define TX_BUFFER_SIZE 128
#define RX_BODY_SIZE   64

// Part F enable flag
static volatile uint8_t s_part_f_enabled = 0;

// ---------------- TX interrupt buffer ----------------
static volatile uint8_t s_tx_buffer[TX_BUFFER_SIZE];
static volatile uint8_t s_tx_length = 0;
static volatile uint8_t s_tx_index = 0;
static volatile uint8_t s_tx_busy = 0;

// ---------------- RX double buffer ----------------
static volatile uint8_t s_rx_body_buffer[2][RX_BODY_SIZE];
static volatile uint8_t s_rx_body_length[2] = {0, 0};
static volatile uint8_t s_rx_msg_type[2] = {0, 0};

static volatile uint8_t s_rx_active_buffer = 0;
static volatile int8_t  s_rx_ready_buffer = -1;
static volatile uint8_t s_rx_message_ready = 0;
static volatile uint8_t s_rx_dropped_message = 0;

typedef enum {
    RX_WAIT_START,
    RX_READ_SIZE,
    RX_READ_TYPE,
    RX_READ_BODY,
    RX_READ_STOP,
    RX_READ_CHECKSUM
} RxState;

static volatile RxState s_rx_state = RX_WAIT_START;
static volatile uint8_t s_rx_expected_size = 0;
static volatile uint8_t s_rx_body_index = 0;
static volatile uint8_t s_rx_running_checksum = 0;
static volatile uint8_t s_rx_current_type = 0;
//-------------------------------------------------------------------------------

//-------------------------------------------------------------------------------

static void part_f_rx_process_byte(uint8_t byte)
{
    uint8_t active = s_rx_active_buffer;

    switch (s_rx_state)
    {
        case RX_WAIT_START:
            if (byte == START_BYTE) {
                s_rx_running_checksum = START_BYTE;
                s_rx_body_index = 0;
                s_rx_expected_size = 0;
                s_rx_state = RX_READ_SIZE;
            }
            break;

        case RX_READ_SIZE:
            if (byte == 0 || byte > RX_BODY_SIZE) {
                s_rx_state = RX_WAIT_START;
            } else {
                s_rx_expected_size = byte;
                s_rx_running_checksum ^= byte;
                s_rx_state = RX_READ_TYPE;
            }
            break;

        case RX_READ_TYPE:
            s_rx_current_type = byte;
            s_rx_running_checksum ^= byte;
            s_rx_state = RX_READ_BODY;
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
            if (byte == STOP_BYTE) {
                s_rx_running_checksum ^= STOP_BYTE;
                s_rx_state = RX_READ_CHECKSUM;
            } else {
                s_rx_state = RX_WAIT_START;
            }
            break;

        case RX_READ_CHECKSUM:
            if (byte == s_rx_running_checksum) {
                if (s_rx_message_ready) {
                    // The previous complete message has not been used yet.
                    s_rx_dropped_message = 1;
                } else {
                    s_rx_body_length[active] = s_rx_expected_size;
                    s_rx_msg_type[active] = s_rx_current_type;
                    s_rx_ready_buffer = active;
                    s_rx_message_ready = 1;

                    // Switch ISR receiving to the other buffer.
                    s_rx_active_buffer ^= 1;
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
//-------------------------------------------------------------------------------










// -------------------------------------------------------
// USART1_IRQHandler
// Fires automatically when a byte arrives
// Stores byte in circular buffer and gets out fast
// -------------------------------------------------------
/*
void USART1_EXTI25_IRQHandler(void)
{
    // Clear overrun error if set — otherwise UART stops receiving
    if (USART1->ISR & USART_ISR_ORE) {
        USART1->ICR |= USART_ICR_ORECF;
    }

    if (USART1->ISR & USART_ISR_RXNE) {
        uint8_t byte = (uint8_t)(USART1->RDR & 0xFF);

        uint8_t next_write = (s_write_pos + 1) % RX_BUFFER_SIZE;

        // Only store if buffer not full
        if (next_write != s_read_pos) {
            s_rx_buffer[s_write_pos] = byte;
            s_write_pos = next_write;
        }
    }
}
*/

void USART1_EXTI25_IRQHandler(void)
{
    // Clear overrun error if set — otherwise UART can stop receiving
    if (USART1->ISR & USART_ISR_ORE) {
        USART1->ICR |= USART_ICR_ORECF;
    }

    // ---------------- RX interrupt ----------------
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

    // ---------------- TX interrupt ----------------
    if ((USART1->ISR & USART_ISR_TXE) && (USART1->CR1 & USART_CR1_TXEIE)) {
        if (s_tx_index < s_tx_length) {
            USART1->TDR = s_tx_buffer[s_tx_index];
            s_tx_index++;
        } else {
            USART1->CR1 &= ~USART_CR1_TXEIE;
            USART1->CR1 |= USART_CR1_TCIE;
        }
    }

    // ---------------- Transmission complete ----------------
    if ((USART1->ISR & USART_ISR_TC) && (USART1->CR1 & USART_CR1_TCIE)) {
        USART1->ICR |= USART_ICR_TCCF;
        USART1->CR1 &= ~USART_CR1_TCIE;

        s_tx_busy = 0;

        if (USART1_PORT.completion_function != 0) {
            USART1_PORT.completion_function(s_tx_length);
        }
    }
}


// Internal helper — read one byte from circular buffer
// Returns 1 if byte available, 0 if buffer empty
static uint8_t buffer_read(uint8_t *byte)
{
    if (s_read_pos == s_write_pos) {
        return 0;
    }
    *byte = s_rx_buffer[s_read_pos];
    s_read_pos = (s_read_pos + 1) % RX_BUFFER_SIZE;
    return 1;
}

void SerialInitialise(uint32_t baudRate, SerialPort *serial_port, void (*completion_function)(uint32_t)) {

    serial_port->completion_function = completion_function;

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    RCC->AHBENR |= serial_port->MaskAHBENR;
    serial_port->GPIO->MODER = serial_port->SerialPinModeValue;
    serial_port->GPIO->OSPEEDR = serial_port->SerialPinSpeedValue;
    serial_port->GPIO->AFR[0] |= serial_port->SerialPinAlternatePinValueLow;
    serial_port->GPIO->AFR[1] |= serial_port->SerialPinAlternatePinValueHigh;
    RCC->APB1ENR |= serial_port->MaskAPB1ENR;
    RCC->APB2ENR |= serial_port->MaskAPB2ENR;

    uint16_t *baud_rate_config = (uint16_t*)&serial_port->UART->BRR;

    switch(baudRate){
    case BAUD_9600:
        *baud_rate_config = 0x341;
        break;
    case BAUD_19200:
        *baud_rate_config = 0x1A1;
        break;
    case BAUD_38400:
        *baud_rate_config = 0x0D0;
        break;
    case BAUD_57600:
        *baud_rate_config = 0x08B;
        break;
    case BAUD_115200:
        *baud_rate_config = 0x046;
        break;
    }

    serial_port->UART->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    // Enable receive interrupt

}

void enable_interrupt(SerialPort *serial_port){
    __disable_irq();
    serial_port->UART->CR1 |= USART_CR1_RXNEIE;
    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);
    __enable_irq();
}

void SerialOutputChar(uint8_t data, SerialPort *serial_port) {
    while((serial_port->UART->ISR & USART_ISR_TXE) == 0){}
    serial_port->UART->TDR = data;
}

void SerialOutputString(uint8_t *pt, SerialPort *serial_port) {
    uint32_t counter = 0;
    while(*pt) {
        SerialOutputChar(*pt, serial_port);
        counter++;
        pt++;
    }
    if (serial_port->completion_function != 0) {
        serial_port->completion_function(counter);
    }
}

void SerialReceiveString(uint8_t *buffer, size_t length, SerialPort *serial_port) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = SerialReceiveByte(serial_port);
    }
}

uint8_t SerialReceiveByte(SerialPort *serial_port) {
    while ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {}
    return (uint8_t)(serial_port->UART->RDR);
}

void sendString(uint8_t *pt, SerialPort *serial_port) {
    uint32_t counter = 0;
    while(*pt) {
        SerialOutputChar(*pt, serial_port);
        counter++;
        pt++;
    }
    if (serial_port->completion_function != 0) {
        serial_port->completion_function(counter);
    }
}

uint8_t calculateChecksum(void *data, uint8_t size, uint8_t type) {
    uint8_t *body = (uint8_t *)data;
    uint8_t checksum = 0;

    checksum ^= START_BYTE;
    checksum ^= size;
    checksum ^= type;
    for (uint8_t i = 0; i < size; i++) {
        checksum ^= body[i];
    }
    checksum ^= STOP_BYTE;

    return checksum;
}

void sendMsg(void *data, uint8_t size, uint8_t type, SerialPort *serial_port)
{
    uint8_t *body = (uint8_t *)data;
    uint8_t checksum = calculateChecksum(data, size, type);

    SerialOutputChar(START_BYTE, serial_port);
    SerialOutputChar(size, serial_port);
    SerialOutputChar(type, serial_port);

    for (uint8_t i = 0; i < size; i++) {
        SerialOutputChar(body[i], serial_port);
    }

    SerialOutputChar(STOP_BYTE, serial_port);
    SerialOutputChar(checksum, serial_port);
}


void receiveMsg(SerialPort *serial_port, void (*callback)(uint8_t *data, uint8_t size, uint8_t type))
{
    uint8_t byte;
    uint8_t body_size;
    uint8_t msg_type;
    uint8_t body[64];
    uint8_t received_checksum;
    uint8_t calculated_checksum;

    if (callback == 0) {
        return;
    }

    // Step 1: Wait for START byte
    uint32_t timeout;
    timeout = 0xFFFFFF;
    do {
        if (buffer_read(&byte)) {
            if (byte == START_BYTE) break;
        }
        if (--timeout == 0) return;
    } while (1);

    // Step 2: Read SIZE
    timeout = 0xFFFFFF;
    while (!buffer_read(&body_size)) {
        if (--timeout == 0) return;
    }
    if (body_size == 0 || body_size > 64) return;

    // Step 3: Read TYPE
    timeout = 0xFFFFFF;
    while (!buffer_read(&msg_type)) {
        if (--timeout == 0) return;
    }

    // Step 4: Read BODY
    for (uint8_t i = 0; i < body_size; i++) {
        timeout = 0xFFFFFF;
        while (!buffer_read(&byte)) {
            if (--timeout == 0) return;
        }
        body[i] = byte;
    }

    // Step 5: Check STOP byte
    timeout = 0xFFFFFF;
    while (!buffer_read(&byte)) {
        if (--timeout == 0) return;
    }
    if (byte != STOP_BYTE) {
        sendString((uint8_t *)"ERR: no stop byte\r\n", serial_port);
        return;
    }

    // Step 6: Verify checksum
    timeout = 0xFFFFFF;
    while (!buffer_read(&received_checksum)) {
        if (--timeout == 0) return;
    }
    calculated_checksum = calculateChecksum(body, body_size, msg_type);

    if (calculated_checksum != received_checksum) {
        sendString((uint8_t *)"ERR: bad checksum\r\n", serial_port);
        return;
    }

    // Step 7: Fire callback
    callback(body, body_size, msg_type);
}

void receiveMsgPolling(SerialPort *serial_port, void (*callback)(uint8_t *data, uint8_t size, uint8_t type))
{
    uint8_t byte;
    uint8_t body_size;
    uint8_t msg_type;
    uint8_t body[64];
    uint8_t received_checksum;
    uint8_t calculated_checksum;

    if (callback == 0) {
        return;
    }

    // Step 1: Wait for START byte — polls hardware directly
    do {
        byte = SerialReceiveByte(serial_port);
    } while (byte != START_BYTE);

    // Step 2: Read SIZE
    body_size = SerialReceiveByte(serial_port);
    if (body_size == 0 || body_size > 64) return;

    // Step 3: Read TYPE
    msg_type = SerialReceiveByte(serial_port);

    // Step 4: Read BODY
    for (uint8_t i = 0; i < body_size; i++) {
        body[i] = SerialReceiveByte(serial_port);
    }

    // Step 5: Check STOP byte
    byte = SerialReceiveByte(serial_port);
    if (byte != STOP_BYTE) {
        sendString((uint8_t *)"ERR: no stop byte\r\n", serial_port);
        return;
    }

    // Step 6: Verify checksum
    received_checksum = SerialReceiveByte(serial_port);
    calculated_checksum = calculateChecksum(body, body_size, msg_type);

    if (calculated_checksum != received_checksum) {
        sendString((uint8_t *)"ERR: bad checksum\r\n", serial_port);
        return;
    }

    // Step 7: Fire callback
    callback(body, body_size, msg_type);
}











void enable_interrupt_part_f(SerialPort *serial_port)
{
    __disable_irq();

    s_part_f_enabled = 1;

    s_rx_state = RX_WAIT_START;
    s_rx_active_buffer = 0;
    s_rx_ready_buffer = -1;
    s_rx_message_ready = 0;
    s_rx_dropped_message = 0;

    serial_port->UART->CR1 |= USART_CR1_RXNEIE;

    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);

    __enable_irq();
}

uint8_t sendStringIT(uint8_t *pt, SerialPort *serial_port)
{
    uint8_t length = 0;

    if (pt == 0) {
        return 0;
    }

    while (pt[length] != 0) {
        length++;
        if (length >= TX_BUFFER_SIZE) {
            return 0;
        }
    }

    __disable_irq();

    if (s_tx_busy) {
        __enable_irq();
        return 0;
    }

    for (uint8_t i = 0; i < length; i++) {
        s_tx_buffer[i] = pt[i];
    }

    s_tx_length = length;
    s_tx_index = 0;
    s_tx_busy = 1;

    serial_port->UART->CR1 |= USART_CR1_TXEIE;

    __enable_irq();

    return 1;
}

uint8_t sendMsgIT(void *data, uint8_t size, uint8_t type, SerialPort *serial_port)
{
    uint8_t *body = (uint8_t *)data;
    uint8_t checksum;
    uint8_t index = 0;

    if (data == 0 || size == 0 || size > RX_BODY_SIZE) {
        return 0;
    }

    if ((size + 5) > TX_BUFFER_SIZE) {
        return 0;
    }

    __disable_irq();

    if (s_tx_busy) {
        __enable_irq();
        return 0;
    }

    checksum = calculateChecksum(data, size, type);

    s_tx_buffer[index++] = START_BYTE;
    s_tx_buffer[index++] = size;
    s_tx_buffer[index++] = type;

    for (uint8_t i = 0; i < size; i++) {
        s_tx_buffer[index++] = body[i];
    }

    s_tx_buffer[index++] = STOP_BYTE;
    s_tx_buffer[index++] = checksum;

    s_tx_length = index;
    s_tx_index = 0;
    s_tx_busy = 1;

    serial_port->UART->CR1 |= USART_CR1_TXEIE;

    __enable_irq();

    return 1;
}

void receiveMsgDoubleBuffer(SerialPort *serial_port,
        void (*callback)(uint8_t *data, uint8_t size, uint8_t type))
{
    int8_t ready;
    uint8_t size;
    uint8_t type;

    (void)serial_port;

    if (callback == 0) {
        return;
    }

    __disable_irq();

    if (!s_rx_message_ready) {
        __enable_irq();
        return;
    }

    ready = s_rx_ready_buffer;
    size = s_rx_body_length[ready];
    type = s_rx_msg_type[ready];

    __enable_irq();

    // Callback uses the completed buffer while ISR receives into the other buffer.
    callback((uint8_t *)s_rx_body_buffer[ready], size, type);

    __disable_irq();

    if (s_rx_ready_buffer == ready) {
        s_rx_message_ready = 0;
        s_rx_ready_buffer = -1;
    }

    __enable_irq();
}

uint8_t SerialTxBusy(void)
{
    return s_tx_busy;
}

uint8_t SerialRxDroppedMessage(void)
{
    return s_rx_dropped_message;
}
