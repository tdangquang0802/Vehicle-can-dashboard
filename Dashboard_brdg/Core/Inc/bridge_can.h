/**
 * @file    bridge_can.h
 * @brief   Thin RX-only wrappers over can_drv for the bridge (BRG-01, BRG-03).
 *
 * CAN: 500 kbps, NORMAL mode (so the bridge ACKs frames), ABOM on - all set in
 * CubeMX.  Filter: pass every standard frame (extended frames are rejected).
 */
#ifndef BRIDGE_CAN_H
#define BRIDGE_CAN_H

#include "can_drv.h"

static inline status_t  bcan_init(void)                           { return can_init(NULL, 0u); }
static inline bool      bcan_pop(uint32_t *ts, can_frame_t *f)    { return can_rx_pop_ts(f, ts); }
static inline uint32_t  bcan_rx_total(void)                       { return can_rx_total(); }
static inline uint16_t  bcan_drops(void)                          { return can_rx_drops(); }
static inline uint8_t   bcan_bus_err(void)                        { return can_bus_err(); }
static inline uint8_t   bcan_bus_off(void)                        { return can_bus_off(); }

#endif /* BRIDGE_CAN_H */
