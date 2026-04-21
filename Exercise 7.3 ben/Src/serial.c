#include "serial.h"

/*
 * Packet format:
 *   [STX][LEN][TYPE][BODY...][ETX][CHECKSUM]
 *
 *   LEN      = total packet length (STX + LEN + TYPE + BODY + ETX + CHECKSUM)
 *   CHECKSUM = XOR of all bytes from STX up to and including ETX
 *
 * Framing rule (Fix #2):
 *   The ISR and the blocking receiver use LEN to find the end of the packet,
 *   NOT ETX. ETX is only validated as a sanity byte at position (LEN - 2).
 *   This makes the body data-agnostic: any byte value, including 0x03, is
 *   legal in the body.
 */

static SerialRxCallback rx_callback = 0;

/* =========================
   TX interrupt state
   ========================= */
static volatile uint8_t  tx_buffer[SERIAL_TX_BUFFER_SIZE];
static volatile uint16_t tx_len = 0;
static volatile uint16_t tx_index = 0;
static volatile uint8_t  tx_busy_flag = 0;

/* =========================
   RX double-buffer state
   ========================= */
static volatile uint8_t rx_buffer_a[SERIAL_MAX_PACKET];
static volatile uint8_t rx_buffer_b[SERIAL_MAX_PACKET];

static volatile uint8_t *rx_active_buffer = rx_buffer_a;
static volatile uint8_t *rx_ready_buffer  = rx_buffer_b;

static volatile uint16_t rx_active_index = 0;
static volatile uint16_t rx_ready_len = 0;
static volatile uint16_t rx_expected_len = 0;   /* captured from LEN field */
static volatile uint8_t  rx_message_ready = 0;

/* =========================
   Helpers
   ========================= */
static uint8_t serial_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t checksum = 0;

    for (uint16_t i = 0; i < len; i++)
    {
        checksum ^= data[i];
    }

    return checksum;
}

static void serial_swap_rx_buffers(void)
{
    volatile uint8_t *temp = rx_ready_buffer;
    rx_ready_buffer = rx_active_buffer;
    rx_active_buffer = temp;
}

static void serial_reset_active_rx(void)
{
    rx_active_index = 0;
    rx_expected_len = 0;
}

static void serial_finish_rx_packet(void)
{
    /* Fix #4: do NOT invoke the user callback here. The ISR only swaps
     * buffers and sets the ready flag. The main loop's serial_process_rx()
     * fires the callback in thread context. */
    rx_ready_len = rx_active_index;
    serial_swap_rx_buffers();
    rx_message_ready = 1;
    serial_reset_active_rx();
}

/* =========================
   Init
   USART1 on PC4 / PC5
   ========================= */
void serial_init(void)
{
    /* Enable clocks for GPIOC and USART1 */
    RCC->AHBENR  |= RCC_AHBENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    /* PC4 = TX, PC5 = RX -> alternate function */
    GPIOC->MODER &= ~((3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOC->MODER |=  ((2U << (4U * 2U)) | (2U << (5U * 2U)));

    /* High speed */
    GPIOC->OSPEEDR &= ~((3U << (4U * 2U)) | (3U << (5U * 2U)));
    GPIOC->OSPEEDR |=  ((3U << (4U * 2U)) | (3U << (5U * 2U)));

    /* AF7 for USART1 on PC4, PC5 */
    GPIOC->AFR[0] &= ~((0xFU << (4U * 4U)) | (0xFU << (5U * 4U)));
    GPIOC->AFR[0] |=  ((7U   << (4U * 4U)) | (7U   << (5U * 4U)));

    USART1->CR1 = 0;
    USART1->CR2 = 0;
    USART1->CR3 = 0;

    /* 115200 baud @ 8 MHz HSI */
    USART1->BRR = 69U;

    /* Enable TX, RX and USART */
    USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    /* Clear software state */
    tx_len = 0;
    tx_index = 0;
    tx_busy_flag = 0;

    rx_active_buffer = rx_buffer_a;
    rx_ready_buffer  = rx_buffer_b;
    rx_active_index = 0;
    rx_ready_len = 0;
    rx_expected_len = 0;
    rx_message_ready = 0;
}

/* =========================
   Blocking TX
   ========================= */
void serial_send_byte(uint8_t byte)
{
    while (!(USART1->ISR & USART_ISR_TXE))
    {
    }

    USART1->TDR = byte;
}

void serial_send_array(const uint8_t *data, uint16_t len)
{
    if (data == 0)
    {
        return;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        serial_send_byte(data[i]);
    }
}

void sendString(const char *str)
{
    if (str == 0)
    {
        return;
    }

    while (*str)
    {
        serial_send_byte((uint8_t)(*str));
        str++;
    }
}

void sendMsg(uint8_t msgType, const void *body, uint8_t bodyLen)
{
    const uint8_t *payload = (const uint8_t *)body;
    uint8_t packet[SERIAL_MAX_PACKET];
    uint16_t idx = 0;
    uint8_t checksum;
    uint8_t totalLen;

    /* Fix #1: guard bodyLen BEFORE computing totalLen to avoid uint8_t wrap. */
    if (bodyLen > SERIAL_MAX_BODY)
    {
        return;
    }

    if ((body == 0) && (bodyLen > 0))
    {
        return;
    }

    totalLen = (uint8_t)(bodyLen + 5U);

    packet[idx++] = SERIAL_STX;
    packet[idx++] = totalLen;
    packet[idx++] = msgType;

    for (uint8_t i = 0; i < bodyLen; i++)
    {
        packet[idx++] = payload[i];
    }

    packet[idx++] = SERIAL_ETX;

    checksum = serial_checksum(packet, idx);
    packet[idx++] = checksum;

    serial_send_array(packet, idx);
}

/* =========================
   Interrupt TX
   ========================= */
int serial_send_array_it(const uint8_t *data, uint16_t len)
{
    if ((data == 0) || (len == 0) || (len > SERIAL_TX_BUFFER_SIZE) || tx_busy_flag)
    {
        return 0;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        tx_buffer[i] = data[i];
    }

    tx_len = len;
    tx_index = 0;
    tx_busy_flag = 1;

    NVIC_EnableIRQ(USART1_IRQn);
    USART1->CR1 |= USART_CR1_TXEIE;

    return 1;
}

int serial_send_msg_it(uint8_t msgType, const void *body, uint8_t bodyLen)
{
    const uint8_t *payload = (const uint8_t *)body;
    uint8_t packet[SERIAL_TX_BUFFER_SIZE];
    uint16_t idx = 0;
    uint8_t checksum;
    uint8_t totalLen;

    /* Fix #1: guard bodyLen BEFORE computing totalLen to avoid uint8_t wrap. */
    if (bodyLen > SERIAL_MAX_BODY)
    {
        return 0;
    }

    if ((body == 0) && (bodyLen > 0))
    {
        return 0;
    }

    totalLen = (uint8_t)(bodyLen + 5U);

    packet[idx++] = SERIAL_STX;
    packet[idx++] = totalLen;
    packet[idx++] = msgType;

    for (uint8_t i = 0; i < bodyLen; i++)
    {
        packet[idx++] = payload[i];
    }

    packet[idx++] = SERIAL_ETX;

    checksum = serial_checksum(packet, idx);
    packet[idx++] = checksum;

    return serial_send_array_it(packet, idx);
}

uint8_t serial_tx_busy(void)
{
    return tx_busy_flag;
}

/* =========================
   Blocking RX (Fix #2: LEN-based framing)
   ========================= */
int receiveMsg(uint8_t *buffer, uint16_t *len)
{
    uint16_t idx = 0;
    uint8_t byte;
    uint8_t expected_total;
    uint8_t expected_chk;
    uint8_t received_chk;

    if ((buffer == 0) || (len == 0))
    {
        return 0;
    }

    /* Wait for STX */
    do
    {
        while (!(USART1->ISR & USART_ISR_RXNE)) { }
        byte = (uint8_t)USART1->RDR;
    } while (byte != SERIAL_STX);

    buffer[idx++] = byte;

    /* Read LEN */
    while (!(USART1->ISR & USART_ISR_RXNE)) { }
    expected_total = (uint8_t)USART1->RDR;
    buffer[idx++] = expected_total;

    if ((expected_total < 5U) || (expected_total > SERIAL_MAX_PACKET))
    {
        return 0;
    }

    /* Read remaining (expected_total - 2) bytes: TYPE, BODY..., ETX, CHECKSUM */
    while (idx < expected_total)
    {
        while (!(USART1->ISR & USART_ISR_RXNE)) { }
        buffer[idx++] = (uint8_t)USART1->RDR;
    }

    /* Validate ETX sits at position (LEN - 2) */
    if (buffer[expected_total - 2U] != SERIAL_ETX)
    {
        return 0;
    }

    /* Validate checksum (covers everything except the checksum byte itself) */
    expected_chk = serial_checksum(buffer, (uint16_t)(expected_total - 1U));
    received_chk = buffer[expected_total - 1U];

    if (expected_chk != received_chk)
    {
        return 0;
    }

    *len = idx;
    return 1;
}

/* =========================
   Interrupt RX control
   ========================= */
void serial_set_callback(SerialRxCallback cb)
{
    rx_callback = cb;
}

void serial_enable_rx_interrupt(void)
{
    rx_active_buffer = rx_buffer_a;
    rx_ready_buffer  = rx_buffer_b;
    rx_active_index = 0;
    rx_ready_len = 0;
    rx_expected_len = 0;
    rx_message_ready = 0;

    USART1->ICR = 0xFFFFFFFFU;
    USART1->RQR |= USART_RQR_RXFRQ;

    /* Flush any stale RX data left over from a previous session
     * (e.g. after a DTR-triggered reset when the host opens screen). */
    (void)USART1->RDR;
    (void)USART1->RDR;

    USART1->CR1 |= USART_CR1_RXNEIE;

    NVIC_EnableIRQ(USART1_IRQn);
}

void serial_disable_rx_interrupt(void)
{
    USART1->CR1 &= ~USART_CR1_RXNEIE;
    NVIC_DisableIRQ(USART1_IRQn);
}

/* Fix #3: atomic snapshot of the ready buffer under IRQ lock. */
int serial_get_latest_packet(uint8_t *buffer, uint16_t *len)
{
    uint16_t n;

    if ((buffer == 0) || (len == 0))
    {
        return 0;
    }

    __disable_irq();

    if (!rx_message_ready)
    {
        __enable_irq();
        return 0;
    }

    n = rx_ready_len;
    for (uint16_t i = 0; i < n; i++)
    {
        buffer[i] = rx_ready_buffer[i];
    }

    rx_message_ready = 0;

    __enable_irq();

    *len = n;
    return 1;
}

/* Fix #4: thread-context callback dispatch.
 *
 * Called from the main loop. If a packet is ready, this takes a local
 * snapshot under IRQ lock and then invokes the user callback outside the
 * critical section so the callback can do real work without starving the
 * UART ISR.
 */
void serial_process_rx(void)
{
    uint8_t  local_buf[SERIAL_MAX_PACKET];
    uint16_t local_len;

    if (!serial_get_latest_packet(local_buf, &local_len))
    {
        return;
    }

    if (rx_callback != 0)
    {
        rx_callback(local_buf, local_len);
    }
}

/* =========================
   IRQ handler
   Must match startup file: USART1_EXTI25_IRQHandler on STM32F303VC
   ========================= */
void USART1_EXTI25_IRQHandler(void)
{
    /* ---------- RX ---------- */
    if (USART1->ISR & USART_ISR_RXNE)
    {
        uint8_t byte = (uint8_t)USART1->RDR;

        /* Reject any byte received before STX */
        if (rx_active_index == 0U)
        {
            if (byte != SERIAL_STX)
            {
                /* fall through to TX / ORE handlers */
                goto rx_done;
            }
        }

        /* Safety: never overrun the buffer */
        if (rx_active_index >= SERIAL_MAX_PACKET)
        {
            serial_reset_active_rx();
            goto rx_done;
        }

        rx_active_buffer[rx_active_index++] = byte;

        /* Second byte is LEN: capture expected total packet size. */
        if (rx_active_index == 2U)
        {
            rx_expected_len = byte;

            /* Minimum valid packet: STX + LEN + TYPE + ETX + CHECKSUM = 5 bytes */
            if ((rx_expected_len < 5U) || (rx_expected_len > SERIAL_MAX_PACKET))
            {
                serial_reset_active_rx();
                goto rx_done;
            }
        }

        /* Fix #2: end-of-packet is driven by LEN, not by ETX. ETX is a
         * sanity byte we check at position (LEN - 2). */
        if ((rx_active_index >= 2U) && (rx_active_index == rx_expected_len))
        {
            uint16_t etx_pos = (uint16_t)(rx_expected_len - 2U);
            uint16_t chk_pos = (uint16_t)(rx_expected_len - 1U);
            uint8_t  received_chk = rx_active_buffer[chk_pos];
            uint8_t  computed_chk = serial_checksum((const uint8_t *)rx_active_buffer, chk_pos);

            if ((rx_active_buffer[etx_pos] == SERIAL_ETX) && (received_chk == computed_chk))
            {
                serial_finish_rx_packet();
            }
            else
            {
                serial_reset_active_rx();
            }
        }
    }

rx_done:

    /* ---------- TX: feed next byte into TDR ---------- */
    if ((USART1->ISR & USART_ISR_TXE) && (USART1->CR1 & USART_CR1_TXEIE))
    {
        if (tx_index < tx_len)
        {
            USART1->TDR = tx_buffer[tx_index++];
        }
        else
        {
            /* Fix #5: all bytes have been handed to TDR but the last one
             * is still shifting out. Switch from TXE to TC so we only
             * clear tx_busy_flag once the line is actually idle. */
            USART1->CR1 &= ~USART_CR1_TXEIE;
            USART1->CR1 |=  USART_CR1_TCIE;
        }
    }

    /* ---------- TX complete: last bit has left the shift register ---------- */
    if ((USART1->ISR & USART_ISR_TC) && (USART1->CR1 & USART_CR1_TCIE))
    {
        USART1->ICR |= USART_ICR_TCCF;
        USART1->CR1 &= ~USART_CR1_TCIE;
        tx_busy_flag = 0;
        tx_len = 0;
        tx_index = 0;
    }

    /* Overrun recovery */
    if (USART1->ISR & USART_ISR_ORE)
    {
        USART1->ICR |= USART_ICR_ORECF;
    }
}
