/**
 * @file serial.c
 * @brief Serial (UART) module -- bare-metal STM32F303.
 *
 * Task a) -- sendBytes / receiveBytes
 * Task b) -- sendString
 * Task c) -- sendMsg
 * Task d) -- receiveMsg (polling)
 * Task e) -- SerialSetRxCallback + ISR (interrupt-driven RX)
 * Task f) -- sendStringDB (interrupt-driven TX, double buffer)
 *
 * USART1: PC10=TX, PC11=RX, AF7
 * USART3: PC10=TX, PC11=RX, AF7
 * Clock:  HSI = 8MHz, BRR = 0x46 -> 115200 baud
 */

#include "serial.h"
#include "registers.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * Serial port struct (hidden from users)
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
    SerialRxCallback rx_callback;
};

/* -----------------------------------------------------------------------
 * Port instances
 * --------------------------------------------------------------------- */

/* USART1: PC10=TX, PC11=RX, AF7 -- debug to PC */
SerialPort USART1_PORT = {
    USART1,
    GPIOC,
    RCC_APB2ENR_USART1EN,
    0x00,
    RCC_AHBENR_GPIOCEN,
    0xA00,       /* PC10+PC11 alternate function mode */
    0xF00,       /* PC10+PC11 high speed              */
    0x770000,    /* AF7 on PC10, PC11 in AFR[0]       */
    0x00,
    0x00,
    0x00
};

/* USART3: PC10=TX, PC11=RX, AF7 -- board-to-board Ex5 */
SerialPort USART3_PORT = {
    USART3,
    GPIOC,
    0x00,
    RCC_APB1ENR_USART3EN,
    RCC_AHBENR_GPIOCEN,
    0xA00,
    0xF00,
    0x770000,
    0x00,
    0x00,
    0x00
};

/* -----------------------------------------------------------------------
 * Initialisation
 * --------------------------------------------------------------------- */

void SerialInitialise(uint32_t baudRate,
                      SerialPort *serial_port,
                      void (*completion_function)(uint32_t))
{
    serial_port->completion_function = completion_function;

    /* Enable clocks */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    RCC->AHBENR  |= serial_port->MaskAHBENR;
    RCC->APB1ENR |= serial_port->MaskAPB1ENR;
    RCC->APB2ENR |= serial_port->MaskAPB2ENR;

    /* Configure GPIO pins for alternate function */
    serial_port->GPIO->MODER   |= serial_port->SerialPinModeValue;
    serial_port->GPIO->OSPEEDR |= serial_port->SerialPinSpeedValue;
    serial_port->GPIO->AFR[0]  |= serial_port->SerialPinAlternatePinValueLow;
    serial_port->GPIO->AFR[1]  |= serial_port->SerialPinAlternatePinValueHigh;

    /* Set baud rate -- 115200 at 8MHz */
    uint16_t *baud_rate_config = (uint16_t *)&serial_port->UART->BRR;
    switch (baudRate) {
        case BAUD_9600:
        case BAUD_19200:
        case BAUD_38400:
        case BAUD_57600:
        case BAUD_115200:
        default:
            *baud_rate_config = 0x46;
            break;
    }

    /* Ensure TXEIE is disabled at init -- enabling it here causes the ISR
     * to fire immediately since TXE is always set when the TX is idle.
     * TXEIE is only enabled by sendStringDB() when there is data to send. */
    serial_port->UART->CR1 &= ~USART_CR1_TXEIE;

    /* Enable TX, RX and UART */
    serial_port->UART->CR1 |= USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void SerialEnableNVIC(void)
{
    __asm__("cpsid i");
    NVIC_SetPriority(USART1_IRQn, 1);
    NVIC_EnableIRQ(USART1_IRQn);
    __asm__("cpsie i");
}

/* -----------------------------------------------------------------------
 * Task a) -- raw byte send/receive
 * --------------------------------------------------------------------- */

void sendBytes(uint8_t *data, uint32_t len, SerialPort *serial_port)
{
    if (data == 0 || serial_port == 0) return;

    for (uint32_t i = 0; i < len; i++) {
        /* Wait until TX register is empty */
        while ((serial_port->UART->ISR & USART_ISR_TXE) == 0) {}
        serial_port->UART->TDR = data[i];
    }

    /* Wait for transmission to fully complete */
    while ((serial_port->UART->ISR & USART_ISR_TC) == 0) {}

    if (serial_port->completion_function != 0) {
        serial_port->completion_function(len);
    }
}

void receiveBytes(uint8_t *buf, uint32_t len, SerialPort *serial_port)
{
    if (buf == 0 || serial_port == 0) return;

    for (uint32_t i = 0; i < len; i++) {
        /* Wait until RX register has data */
        while ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {}
        buf[i] = (uint8_t)serial_port->UART->RDR;
    }
}

/* -----------------------------------------------------------------------
 * Task b) -- debug string
 * --------------------------------------------------------------------- */

void sendString(uint8_t *pt, SerialPort *serial_port)
{
    if (pt == 0 || serial_port == 0) return;

    uint32_t counter = 0;
    while (*pt) {
        while ((serial_port->UART->ISR & USART_ISR_TXE) == 0) {}
        serial_port->UART->TDR = *pt;
        counter++;
        pt++;
    }

    /* Wait for last byte to finish */
    while ((serial_port->UART->ISR & USART_ISR_TC) == 0) {}

    if (serial_port->completion_function != 0) {
        serial_port->completion_function(counter);
    }
}

/* -----------------------------------------------------------------------
 * Task c) -- structured packet TX
 *
 * Packet: [ STX | LEN | TYPE | BODY... | ETX | BCC ]
 * BCC = XOR of LEN, TYPE, and all BODY bytes
 * --------------------------------------------------------------------- */

void sendMsg(void *data, uint8_t size, uint8_t msg_type,
             SerialPort *serial_port)
{
    if (data == 0 || serial_port == 0) return;

    uint8_t *bytes = (uint8_t *)data;
    uint8_t  bcc   = 0;

    #define TX_BYTE(b) \
        do { \
            while ((serial_port->UART->ISR & USART_ISR_TXE) == 0) {} \
            serial_port->UART->TDR = (b); \
        } while (0)

    TX_BYTE(SERIAL_STX);

    bcc ^= size;
    TX_BYTE(size);

    bcc ^= msg_type;
    TX_BYTE(msg_type);

    for (uint8_t i = 0; i < size; i++) {
        bcc ^= bytes[i];
        TX_BYTE(bytes[i]);
    }

    TX_BYTE(SERIAL_ETX);
    TX_BYTE(bcc);

    /* Wait for last byte to finish */
    while ((serial_port->UART->ISR & USART_ISR_TC) == 0) {}

    #undef TX_BYTE
}

/* -----------------------------------------------------------------------
 * Task d) -- polling RX
 *
 * Reads bytes one at a time until ETX received,
 * verifies BCC checksum, then calls callback.
 * --------------------------------------------------------------------- */

void receiveMsg(SerialPort *serial_port, SerialRxCallback callback)
{
    if (serial_port == 0 || callback == 0) return;

    static uint8_t rx_buf[SERIAL_RX_BUFFER_SIZE];
    uint8_t byte;
    uint8_t expected_len   = 0;
    uint8_t msg_type       = 0;
    uint8_t bytes_received = 0;
    uint8_t bcc_calc       = 0;

    /* Wait for STX */
    do {
        while ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {}
        byte = (uint8_t)serial_port->UART->RDR;
    } while (byte != SERIAL_STX);

    /* Read LEN */
    while ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {}
    expected_len  = (uint8_t)serial_port->UART->RDR;
    bcc_calc     ^= expected_len;

    /* Read TYPE */
    while ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {}
    msg_type   = (uint8_t)serial_port->UART->RDR;
    bcc_calc  ^= msg_type;
    (void)msg_type;

    /* Read BODY */
    for (uint8_t i = 0; i < expected_len; i++) {
        while ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {}
        rx_buf[i]  = (uint8_t)serial_port->UART->RDR;
        bcc_calc  ^= rx_buf[i];
        bytes_received++;
    }

    /* Read ETX */
    while ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {}
    byte = (uint8_t)serial_port->UART->RDR;
    if (byte != SERIAL_ETX) return;   /* bad frame -- discard */

    /* Read BCC */
    while ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {}
    byte = (uint8_t)serial_port->UART->RDR;

    /* Verify checksum -- only call callback if valid */
    if (byte == bcc_calc) {
        callback(rx_buf, bytes_received);
    }
}

/* -----------------------------------------------------------------------
 * Task e) -- interrupt-driven RX
 *
 * Packet state machine fed by ISR.
 * Once registered via SerialSetRxCallback(), packets are received
 * automatically without polling.
 * --------------------------------------------------------------------- */

void SerialSetRxCallback(SerialPort *serial_port, SerialRxCallback callback)
{
    if (serial_port == 0) return;
    serial_port->rx_callback = callback;
    /* Enable RX not-empty interrupt */
    serial_port->UART->CR1 |= USART_CR1_RXNEIE;
}

/* Packet state machine */
typedef enum {
    RX_WAIT_STX  = 0,
    RX_WAIT_LEN  = 1,
    RX_WAIT_TYPE = 2,
    RX_READ_BODY = 3,
    RX_WAIT_ETX  = 4,
    RX_WAIT_BCC  = 5
} RxState;

static RxState  rx_state          = RX_WAIT_STX;
static uint8_t  rx_expected_len   = 0;
static uint8_t  rx_msg_type       = 0;
static uint8_t  rx_bytes_received = 0;
static uint8_t  rx_bcc_calc       = 0;
static uint8_t  rx_buffer[SERIAL_RX_BUFFER_SIZE];

static void process_rx_byte(uint8_t byte)
{
    switch (rx_state) {

        case RX_WAIT_STX:
            if (byte == SERIAL_STX) {
                rx_state          = RX_WAIT_LEN;
                rx_bcc_calc       = 0;
                rx_bytes_received = 0;
            }
            break;

        case RX_WAIT_LEN:
            if (byte == 0 || byte > SERIAL_RX_BUFFER_SIZE) {
                rx_state = RX_WAIT_STX;
            } else {
                rx_expected_len = byte;
                rx_bcc_calc    ^= byte;
                rx_state        = RX_WAIT_TYPE;
            }
            break;

        case RX_WAIT_TYPE:
            rx_msg_type  = byte;
            rx_bcc_calc ^= byte;
            rx_state     = RX_READ_BODY;
            (void)rx_msg_type;
            break;

        case RX_READ_BODY:
            rx_buffer[rx_bytes_received] = byte;
            rx_bcc_calc ^= byte;
            rx_bytes_received++;
            if (rx_bytes_received >= rx_expected_len) {
                rx_state = RX_WAIT_ETX;
            }
            break;

        case RX_WAIT_ETX:
            if (byte == SERIAL_ETX) {
                rx_state = RX_WAIT_BCC;
            } else {
                rx_state = RX_WAIT_STX;
            }
            break;

        case RX_WAIT_BCC:
            if (byte == rx_bcc_calc) {
                /* Valid packet -- fire callback */
                if (USART1_PORT.rx_callback != 0) {
                    USART1_PORT.rx_callback(rx_buffer, rx_bytes_received);
                }
            }
            rx_state = RX_WAIT_STX;
            break;

        default:
            rx_state = RX_WAIT_STX;
            break;
    }
}

/* -----------------------------------------------------------------------
 * Task f) -- interrupt-driven TX with double buffer (advanced)
 * --------------------------------------------------------------------- */

volatile uint8_t  db_buffer1[DB_BUFFER_SIZE];
volatile uint8_t  db_buffer2[DB_BUFFER_SIZE];

volatile uint8_t  *fill_buffer     = db_buffer1;
volatile uint8_t  *transmit_buffer = db_buffer2;
volatile uint32_t  fill_index      = 0;
volatile uint32_t  transmit_index  = 0;
volatile uint32_t  transmit_length = 0;
volatile uint8_t   is_transmitting = 0;

void sendStringDB(uint8_t *pt, SerialPort *serial_port)
{
    __asm__("cpsid i");

    /* Append to fill buffer */
    while (*pt && fill_index < (DB_BUFFER_SIZE - 1)) {
        fill_buffer[fill_index++] = *pt++;
    }
    fill_buffer[fill_index] = '\0';

    /* If not transmitting, swap buffers and start TX */
    if (!is_transmitting) {
        volatile uint8_t *temp = fill_buffer;
        fill_buffer     = transmit_buffer;
        transmit_buffer = temp;

        transmit_length = fill_index;
        fill_index      = 0;
        transmit_index  = 0;

        is_transmitting = 1;
        serial_port->UART->CR1 |= USART_CR1_TXEIE;
    }

    __asm__("cpsie i");
}

/* -----------------------------------------------------------------------
 * USART1 ISR -- handles RX (state machine) and TX (double buffer)
 * --------------------------------------------------------------------- */

void USART1_EXTI25_IRQHandler(void)
{
    /* Clear framing/overrun errors */
    if ((USART1->ISR & USART_ISR_FE) || (USART1->ISR & USART_ISR_ORE)) {
        USART1->ICR  = USART_ICR_FECF | USART_ICR_ORECF;
        rx_state     = RX_WAIT_STX;
        return;
    }

    /* RX -- feed byte into packet state machine */
    if (USART1->ISR & USART_ISR_RXNE) {
        uint8_t c = (uint8_t)USART1->RDR;
        process_rx_byte(c);
    }

    /* TX -- double buffer */
    if (is_transmitting && transmit_length == 0) {
        USART1->CR1 &= ~USART_CR1_TXEIE;
        is_transmitting = 0;
    }

    if ((USART1->ISR & USART_ISR_TXE) && is_transmitting) {
        if (transmit_index < transmit_length) {
            USART1->TDR = transmit_buffer[transmit_index++];
        } else {
            USART1->CR1 &= ~USART_CR1_TXEIE;

            if (USART1_PORT.completion_function != 0) {
                USART1_PORT.completion_function(transmit_length);
            }

            is_transmitting = 0;

            /* If more data filled during transmission, swap and restart */
            if (fill_index > 0) {
                volatile uint8_t *temp = fill_buffer;
                fill_buffer     = transmit_buffer;
                transmit_buffer = temp;

                transmit_length = fill_index;
                fill_index      = 0;
                transmit_index  = 0;

                is_transmitting = 1;
                USART1->CR1    |= USART_CR1_TXEIE;
            }
        }
    }
}
