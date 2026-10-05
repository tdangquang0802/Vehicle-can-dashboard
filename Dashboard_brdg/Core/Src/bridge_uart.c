/**
 * @file    bridge_uart.c
 * @brief   256-byte TX ring drained by USART1 DMA (DMA1 Channel 4).
 *
 * Concurrency model (no locks):
 *   - s_head is written ONLY by the main loop (buart_send),
 *   - s_tail is written ONLY by the DMA-complete ISR,
 *   - the DMA is started ONLY from the main loop (buart_kick), and s_dma_busy
 *     guarantees at most one burst is in flight.
 * Indices are free-running 16-bit counters; the ring size is a power of two.
 */
#include "bridge_uart.h"
#include "protocol.h"
#include "main.h"
#include <string.h>

#define TX_RING_SIZE 256u

extern UART_HandleTypeDef huart1;

static uint8_t           s_tx[TX_RING_SIZE];
static volatile uint16_t s_head;
static volatile uint16_t s_tail;
static volatile bool     s_dma_busy;
static volatile uint16_t s_dma_len;
static volatile uint16_t s_drops;

void buart_init(void)
{
    s_head = 0u;
    s_tail = 0u;
    s_dma_busy = false;
    s_dma_len  = 0u;
    s_drops    = 0u;
}

uint16_t buart_free(void)
{
    return (uint16_t)(TX_RING_SIZE - (uint16_t)(s_head - s_tail));
}

uint16_t buart_drops(void)
{
    return s_drops;
}

bool buart_send(uint8_t msg, const uint8_t *pl, uint8_t n)
{
    uint8_t  crc_buf[1u + BUART_MAX_PAYLOAD];
    uint16_t h;

    if (n > BUART_MAX_PAYLOAD || (n > 0u && pl == NULL)) {
        s_drops++;
        return false;
    }
    if (buart_free() < (uint16_t)(n + BUART_FRAME_OVERHEAD)) {
        s_drops++;                                    /* never overwrite queued data */
        return false;
    }

    crc_buf[0] = msg;
    if (n > 0u) {
        memcpy(&crc_buf[1], pl, n);
    }
    uint8_t crc = crc8(crc_buf, (uint16_t)(n + 1u));  /* CRC over MSG + payload */

    h = s_head;
    s_tx[h++ & (TX_RING_SIZE - 1u)] = UART_SOF1;
    s_tx[h++ & (TX_RING_SIZE - 1u)] = UART_SOF2;
    s_tx[h++ & (TX_RING_SIZE - 1u)] = n;              /* LEN = payload length */
    s_tx[h++ & (TX_RING_SIZE - 1u)] = msg;
    for (uint8_t i = 0u; i < n; i++) {
        s_tx[h++ & (TX_RING_SIZE - 1u)] = pl[i];
    }
    s_tx[h++ & (TX_RING_SIZE - 1u)] = crc;
    s_tx[h++ & (TX_RING_SIZE - 1u)] = UART_EOF;

    __sync_synchronize();
    s_head = h;                                       /* publish the whole frame at once */
    return true;
}

void buart_kick(void)
{
    if (s_dma_busy) {
        return;
    }
    uint16_t used = (uint16_t)(s_head - s_tail);
    if (used == 0u) {
        return;
    }
    /* DMA needs a contiguous block: stop at the end of the array, the rest
     * goes out in the next burst. */
    uint16_t idx = (uint16_t)(s_tail & (TX_RING_SIZE - 1u));
    uint16_t len = (uint16_t)(TX_RING_SIZE - idx);
    if (len > used) {
        len = used;
    }

    s_dma_len  = len;
    s_dma_busy = true;                                /* set BEFORE starting the DMA */
    if (HAL_UART_Transmit_DMA(&huart1, &s_tx[idx], len) != HAL_OK) {
        s_dma_busy = false;
    }
}

/** DMA burst finished (ISR): release the bytes that were sent. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        s_tail = (uint16_t)(s_tail + s_dma_len);
        s_dma_busy = false;
    }
}

/** UART/DMA error (ISR): drop the in-flight burst so the line never locks up. */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1) {
        if (s_dma_busy) {
            s_tail = (uint16_t)(s_tail + s_dma_len);
            s_dma_busy = false;
        }
        s_drops++;
    }
}
