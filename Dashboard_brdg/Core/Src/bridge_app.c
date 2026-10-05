/**
 * @file    bridge_app.c
 * @brief   CAN -> UART bridge main logic (BRG-01..04).
 *
 *  MSG 0x20 CAN_RAW : ts u32 LE | id u16 LE | dlc u8 | data[dlc]   (LEN = 7 + dlc)
 *  MSG 0x21 STATS   : rx_total u32 | dropped u16 | bus_err u8 | bus_off u8  (every 1 s,
 *                     doubles as the heartbeat that lets the PC detect BRIDGE LOST)
 */
#include "bridge_app.h"
#include "bridge_can.h"
#include "bridge_uart.h"
#include "protocol.h"
#include "main.h"

#define POP_PER_LOOP     4u                                   /* frames handled per pass */
#define MIN_FREE_FOR_RAW (BUART_FRAME_OVERHEAD + 7u + 8u)     /* biggest 0x20 frame */

static uint32_t s_t1000;

static inline void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;         p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

/** Forward queued CAN frames to the UART ring. */
static void bridge_poll(void)
{
    uint32_t    ts;
    can_frame_t f;
    uint8_t     pl[7u + 8u];

    for (uint8_t n = 0u; n < POP_PER_LOOP; n++) {
        /* Back-pressure: if the UART ring is nearly full, leave the frame in
         * the CAN ring (32 deep) instead of dropping it here. */
        if (buart_free() < MIN_FREE_FOR_RAW) {
            break;
        }
        if (!bcan_pop(&ts, &f)) {
            break;
        }
        put_u32(&pl[0], ts);
        put_u16(&pl[4], (uint16_t)f.id);
        pl[6] = f.dlc;
        for (uint8_t i = 0u; i < f.dlc; i++) {
            pl[7u + i] = f.data[i];
        }
        (void)buart_send(UART_MSG_CAN_RAW, pl, (uint8_t)(7u + f.dlc));
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);           /* activity LED (PC13) */
    }
    buart_kick();
}

/** MSG 0x21 once per second. */
static void bridge_stats_1s(void)
{
    uint8_t  pl[8];
    uint32_t dropped = (uint32_t)bcan_drops() + (uint32_t)buart_drops();

    put_u32(&pl[0], bcan_rx_total());
    put_u16(&pl[4], (uint16_t)((dropped > 0xFFFFu) ? 0xFFFFu : dropped));
    pl[6] = bcan_bus_err();
    pl[7] = bcan_bus_off();
    (void)buart_send(UART_MSG_STATS, pl, 8u);
}

void bridge_setup(void)
{
    wdg_freeze_in_debug();
    buart_init();                 /* output side first ...                         */
    (void)bcan_init();            /* ... CAN last, so the RX ISR finds a ready ring */
}

void bridge_loop(void)
{
    bridge_poll();
    if (tick_due(&s_t1000, 1000u)) {
        bridge_stats_1s();
    }
    wdg_kick();
}
