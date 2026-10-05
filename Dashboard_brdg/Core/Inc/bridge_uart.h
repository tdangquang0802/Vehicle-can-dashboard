/**
 * @file    bridge_uart.h
 * @brief   UART TX framing + DMA ring for Bridge -> PC (BRG-02).
 *
 * Frame: AA 55 LEN MSG PAYLOAD CRC8 0A     (LEN = payload length)
 *        CRC-8 (poly 0x07, init 0) over MSG + PAYLOAD.
 */
#ifndef BRIDGE_UART_H
#define BRIDGE_UART_H

#include "common.h"

#define BUART_MAX_PAYLOAD   32u
#define BUART_FRAME_OVERHEAD 6u      /* AA 55 LEN MSG CRC EOF */

void     buart_init(void);
/** Queue one frame. Returns false (drop counted) if it does not fit. */
bool     buart_send(uint8_t msg, const uint8_t *pl, uint8_t n);
/** Start a DMA burst if the line is idle and data is waiting. Call every loop. */
void     buart_kick(void);
uint16_t buart_free(void);
uint16_t buart_drops(void);

#endif /* BRIDGE_UART_H */
