/**
 ******************************************************************************
 * @file    serial.c
 * @brief   Serial (UART) module implementation for MTRX2700 C Lab - Exercise 3
 *
 * Internal design notes
 * ----------------------
 * Receive state machine
 *   Each port tracks where it is in the packet framing protocol via the
 *   rx_state field.  Bytes arriving in the RXNE interrupt are fed one at a
 *   time through the state machine:
 *
 *     WAIT_START -> WAIT_SIZE -> WAIT_TYPE -> READING_BODY -> WAIT_CHECKSUM -> WAIT_STOP
 *                                                                               |
 *                                                              (calls rx_complete callback)
 *
 *   If the checksum does not match, or if the STOP byte is wrong, the state
 *   machine silently resets to WAIT_START so that the next valid packet can
 *   be received.
 *
 * Buffer overflow protection
 *   If the incoming SIZE field is larger than SERIAL_RX_BUFFER_SIZE the
 *   entire packet is discarded (state resets to WAIT_START).
 ******************************************************************************
 */

#include "serial.h"
#include "stm32f303xc.h"
#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * Internal receive state machine states
 * ---------------------------------------------------------------------- */
typedef enum {
    RX_WAIT_START    = 0,
    RX_WAIT_SIZE     = 1,
    RX_WAIT_TYPE     = 2,
    RX_READING_BODY  = 3,
    RX_WAIT_CHECKSUM = 4,
    RX_WAIT_STOP     = 5
} RxState;

/* -------------------------------------------------------------------------
 * Full serial port structure (hidden from users - opaque via typedef in .h)
 * ---------------------------------------------------------------------- */
struct _SerialPort {
    /* Hardware registers */
    USART_TypeDef   *UART;
    GPIO_TypeDef    *GPIO;

    /* RCC enable masks */
    volatile uint32_t MaskAPB2ENR;
    volatile uint32_t MaskAPB1ENR;
    volatile uint32_t MaskAHBENR;

    /* GPIO configuration values */
    volatile uint32_t SerialPinModeValue;
    volatile uint32_t SerialPinSpeedValue;
    volatile uint32_t SerialPinAlternatePinValueLow;
    volatile uint32_t SerialPinAlternatePinValueHigh;

    /* IRQ number for this UART (used to enable NVIC interrupt) */
    IRQn_Type IRQn;

    /* Callbacks */
    SerialTxCompleteCallback tx_complete;
    SerialRxCompleteCallback rx_complete;

    /* ---- Receive state machine ---- */
    RxState  rx_state;
    uint8_t  rx_expected_size;   /* body byte count declared in packet header  */
    uint8_t  rx_msg_type;        /* message type byte from packet header        */
    uint8_t  rx_bytes_received;  /* body bytes received so far                  */
    uint8_t  rx_checksum_calc;   /* running XOR checksum                        */
    uint8_t  rx_buffer[SERIAL_RX_BUFFER_SIZE];  /* fixed-size internal buffer   */
};

/* -------------------------------------------------------------------------
 * Port instances
 *   USART1 -> PC10 (TX) / PC11 (RX), AF7
 *   USART2 -> PA2  (TX) / PA3  (RX), AF7
 * ---------------------------------------------------------------------- */
SerialPort USART1_PORT = {
    .UART                          = USART1,
    .GPIO                          = GPIOC,
    .MaskAPB2ENR                   = RCC_APB2ENR_USART1EN,
    .MaskAPB1ENR                   = 0x00,
    .MaskAHBENR                    = RCC_AHBENR_GPIOCEN,
    .SerialPinModeValue            = 0xA00,      /* PC10+PC11 alternate function */
    .SerialPinSpeedValue           = 0xF00,
    .SerialPinAlternatePinValueLow = 0x770000,   /* AF7 on PC10, PC11 (AFR[0])  */
    .SerialPinAlternatePinValueHigh= 0x00,
    .IRQn                          = USART1_IRQn,
    .tx_complete                   = NULL,
    .rx_complete                   = NULL,
    .rx_state                      = RX_WAIT_START,
    .rx_expected_size              = 0,
    .rx_msg_type                   = 0,
    .rx_bytes_received             = 0,
    .rx_checksum_calc              = 0,
    .rx_buffer                     = {0}
};

SerialPort USART2_PORT = {
    .UART                          = USART2,
    .GPIO                          = GPIOA,
    .MaskAPB2ENR                   = 0x00,
    .MaskAPB1ENR                   = RCC_APB1ENR_USART2EN,
    .MaskAHBENR                    = RCC_AHBENR_GPIOAEN,
    .SerialPinModeValue            = 0xA0,       /* PA2+PA3 alternate function   */
    .SerialPinSpeedValue           = 0xF0,
    .SerialPinAlternatePinValueLow = 0x7700,     /* AF7 on PA2, PA3 (AFR[0])    */
    .SerialPinAlternatePinValueHigh= 0x00,
    .IRQn                          = USART2_IRQn,
    .tx_complete                   = NULL,
    .rx_complete                   = NULL,
    .rx_state                      = RX_WAIT_START,
    .rx_expected_size              = 0,
    .rx_msg_type                   = 0,
    .rx_bytes_received             = 0,
    .rx_checksum_calc              = 0,
    .rx_buffer                     = {0}
};

/* =========================================================================
 * SerialInitialise
 * ====================================================================== */
void SerialInitialise(uint32_t baudRate,
                      SerialPort *serial_port,
                      SerialTxCompleteCallback tx_complete,
                      SerialRxCompleteCallback rx_complete)
{
    /* Store callbacks */
    serial_port->tx_complete = tx_complete;
    serial_port->rx_complete = rx_complete;

    /* Reset receive state machine */
    serial_port->rx_state          = RX_WAIT_START;
    serial_port->rx_bytes_received = 0;
    serial_port->rx_checksum_calc  = 0;

    /* Enable clocks -------------------------------------------------------- */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    RCC->AHBENR  |= serial_port->MaskAHBENR;
    RCC->APB1ENR |= serial_port->MaskAPB1ENR;
    RCC->APB2ENR |= serial_port->MaskAPB2ENR;

    /* Configure GPIO pins -------------------------------------------------- */
    serial_port->GPIO->MODER    = serial_port->SerialPinModeValue;
    serial_port->GPIO->OSPEEDR  = serial_port->SerialPinSpeedValue;
    serial_port->GPIO->AFR[0]  |= serial_port->SerialPinAlternatePinValueLow;
    serial_port->GPIO->AFR[1]  |= serial_port->SerialPinAlternatePinValueHigh;

    /* Configure baud rate -------------------------------------------------- */
    uint16_t *baud_rate_config = (uint16_t*)&serial_port->UART->BRR;
    switch (baudRate) {
        case BAUD_9600:   *baud_rate_config = 0x341; break; /* 8MHz / 9600   */
        case BAUD_19200:  *baud_rate_config = 0x1A1; break;
        case BAUD_38400:  *baud_rate_config = 0xD0;  break;
        case BAUD_57600:  *baud_rate_config = 0x8B;  break;
        case BAUD_115200: *baud_rate_config = 0x46;  break; /* 8MHz / 115200 */
        default:          *baud_rate_config = 0x46;  break;
    }

    /* Enable UART with TX, RX, and RXNE interrupt -------------------------- */
    serial_port->UART->CR1 |= USART_CR1_TE     /* transmit enable  */
                             | USART_CR1_RE     /* receive enable   */
                             | USART_CR1_RXNEIE /* RXNE interrupt   */
                             | USART_CR1_UE;    /* UART enable      */

    /* Enable the interrupt in the NVIC ------------------------------------- */
    NVIC_SetPriority(serial_port->IRQn, 1);
    NVIC_EnableIRQ(serial_port->IRQn);
}

/* =========================================================================
 * SerialOutputChar  (blocking poll on TXE)
 * ====================================================================== */
void SerialOutputChar(uint8_t data, SerialPort *serial_port)
{
    while ((serial_port->UART->ISR & USART_ISR_TXE) == 0) {}
    serial_port->UART->TDR = data;
}

/* =========================================================================
 * SerialOutputString  (null-terminated debug string)
 * ====================================================================== */
void SerialOutputString(uint8_t *pt, SerialPort *serial_port)
{
    uint32_t counter = 0;
    while (*pt) {
        SerialOutputChar(*pt, serial_port);
        counter++;
        pt++;
    }
    if (serial_port->tx_complete != NULL) {
        serial_port->tx_complete(counter);
    }
}

/* =========================================================================
 * sendMsg  (framed structured packet transmission)
 *
 * Packet layout:
 *   [START | size | msg_type | body[0..size-1] | checksum | STOP]
 *
 * Checksum = XOR(size, msg_type, body[0], ..., body[size-1])
 * ====================================================================== */
void sendMsg(void *data, uint8_t size, uint8_t msg_type, SerialPort *serial_port)
{
    uint8_t *bytes = (uint8_t*)data;
    uint8_t checksum = 0;

    /* Build and seed the running checksum with the header fields */
    checksum ^= size;
    checksum ^= msg_type;

    /* --- Start byte --- */
    SerialOutputChar(SERIAL_START_BYTE, serial_port);

    /* --- Header: size and message type --- */
    SerialOutputChar(size,     serial_port);
    SerialOutputChar(msg_type, serial_port);

    /* --- Body --- */
    for (uint8_t i = 0; i < size; i++) {
        checksum ^= bytes[i];
        SerialOutputChar(bytes[i], serial_port);
    }

    /* --- Checksum --- */
    SerialOutputChar(checksum, serial_port);

    /* --- Stop byte --- */
    SerialOutputChar(SERIAL_STOP_BYTE, serial_port);
}

/* =========================================================================
 * SerialReceiveHandler  (call this from the USARTx_IRQHandler)
 *
 * State machine processes one byte per interrupt.
 * ====================================================================== */
void SerialReceiveHandler(SerialPort *serial_port)
{
    /* Only handle RXNE (data register not empty) */
    if ((serial_port->UART->ISR & USART_ISR_RXNE) == 0) {
        return;
    }

    /* Reading RDR automatically clears the RXNE flag */
    uint8_t byte = (uint8_t)(serial_port->UART->RDR & 0xFF);

    switch (serial_port->rx_state) {

        /* ----------------------------------------------------------------- */
        case RX_WAIT_START:
            if (byte == SERIAL_START_BYTE) {
                serial_port->rx_state         = RX_WAIT_SIZE;
                serial_port->rx_checksum_calc = 0;
                serial_port->rx_bytes_received= 0;
            }
            /* Any other byte is silently ignored while hunting for start */
            break;

        /* ----------------------------------------------------------------- */
        case RX_WAIT_SIZE:
            if (byte == 0 || byte > SERIAL_RX_BUFFER_SIZE) {
                /* Invalid size - discard and wait for next packet */
                serial_port->rx_state = RX_WAIT_START;
            } else {
                serial_port->rx_expected_size  = byte;
                serial_port->rx_checksum_calc ^= byte;   /* XOR size into checksum */
                serial_port->rx_state          = RX_WAIT_TYPE;
            }
            break;

        /* ----------------------------------------------------------------- */
        case RX_WAIT_TYPE:
            serial_port->rx_msg_type       = byte;
            serial_port->rx_checksum_calc ^= byte;       /* XOR type into checksum */
            serial_port->rx_state          = RX_READING_BODY;
            break;

        /* ----------------------------------------------------------------- */
        case RX_READING_BODY:
            serial_port->rx_buffer[serial_port->rx_bytes_received] = byte;
            serial_port->rx_checksum_calc ^= byte;       /* XOR body byte into checksum */
            serial_port->rx_bytes_received++;

            if (serial_port->rx_bytes_received >= serial_port->rx_expected_size) {
                serial_port->rx_state = RX_WAIT_CHECKSUM;
            }
            break;

        /* ----------------------------------------------------------------- */
        case RX_WAIT_CHECKSUM:
            if (byte == serial_port->rx_checksum_calc) {
                /* Checksum valid - advance to wait for stop byte */
                serial_port->rx_state = RX_WAIT_STOP;
            } else {
                /* Bad checksum - discard packet */
                serial_port->rx_state = RX_WAIT_START;
            }
            break;

        /* ----------------------------------------------------------------- */
        case RX_WAIT_STOP:
            if (byte == SERIAL_STOP_BYTE) {
                /* Complete, valid packet received - fire callback */
                if (serial_port->rx_complete != NULL) {
                    serial_port->rx_complete(serial_port->rx_buffer,
                                             serial_port->rx_bytes_received);
                }
            }
            /* Whether stop byte was correct or not, reset for next packet */
            serial_port->rx_state = RX_WAIT_START;
            break;

        /* ----------------------------------------------------------------- */
        default:
            serial_port->rx_state = RX_WAIT_START;
            break;
    }
}
