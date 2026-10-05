/**
 * @file    can_drv.h
 * @brief   CAN driver shared by the ECU (F407), Bridge (F103) and Diag tool (F103).
 *
 * Bit timing, ABOM and NORMAL mode are configured in CubeMX (see the guideline);
 * this module only sets up the acceptance filters, starts the peripheral and
 * moves received frames from the RX interrupt into a ring buffer.
 *
 * Each node provides an "app_config.h" that defines:
 *   APP_CAN_HANDLE      name of the CubeMX CAN handle (hcan1 on F407, hcan on F103)
 *   CAN_RX_RING_SIZE    (optional) ring depth, power of two, default 16
 *   CAN_RX_ONLY         (optional) define to REMOVE can_tx() at compile time.
 *                       The Bridge uses this to guarantee it never transmits (SYS-04).
 *
 * Design reference: Function design v1.0, sections 1.1, 2.1, 3.1.
 */
#ifndef CAN_DRV_H
#define CAN_DRV_H

#include "common.h"
#include "app_config.h"

/**
 * Configure filters, start the CAN peripheral and enable the RX interrupt.
 * @param rx_ids  list of 11-bit standard IDs to accept, or NULL to accept all
 *                standard frames (mask mode, extended frames rejected).
 * @param n_ids   number of IDs in rx_ids (max 56).
 *
 * IMPORTANT: call this LAST during start-up, after every buffer/state that the
 * main loop needs has been initialised (a frame may arrive immediately).
 */
status_t can_init(const uint16_t *rx_ids, uint8_t n_ids);

#ifndef CAN_RX_ONLY
/** Non-blocking transmit. Returns false if no mailbox is free (tx_overrun++). */
bool     can_tx(const can_frame_t *f);
uint32_t can_tx_overrun(void);
#endif

/** Pop one received frame (main loop only). */
bool     can_rx_pop(can_frame_t *f);
/** Same as can_rx_pop() but also returns the reception time stamp (ms). */
bool     can_rx_pop_ts(can_frame_t *f, uint32_t *ts_ms);

/* ---- statistics (used by the Bridge heartbeat, BRG-04) ---- */
uint32_t can_rx_total(void);   /* frames seen by the RX ISR (incl. dropped ones) */
uint16_t can_rx_drops(void);   /* frames lost because the ring was full          */
uint8_t  can_bus_err(void);    /* max(TEC, REC) from CAN_ESR, 0..255             */
uint8_t  can_bus_off(void);    /* 1 while the controller is in bus-off           */

#endif /* CAN_DRV_H */
