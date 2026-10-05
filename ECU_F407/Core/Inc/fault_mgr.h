/**
 * @file    fault_mgr.h
 * @brief   DTC state machine IDLE -> PENDING -> CONFIRMED (ECU-03/04/05/07).
 */
#ifndef FAULT_MGR_H
#define FAULT_MGR_H

#include "common.h"
#include "sim.h"

typedef enum { DTC_P0217, DTC_P0118, DTC_P0562, DTC_C0035, DTC_COUNT } dtc_id_t;
typedef enum { DTC_IDLE, DTC_PENDING, DTC_CONFIRMED } dtc_state_t;

void     fm_init(void);
/** Evaluate all fault conditions; call every 10 ms. */
void     fm_run_10ms(const vehicle_t *v);
/** true when at least one DTC is CONFIRMED (drives the MIL). */
bool     fm_mil(void);
/** bit i = DTC i is CONFIRMED (bit0 P0217, bit1 P0118, bit2 P0562, bit3 C0035). */
uint16_t fm_bitmap(void);
/** Returns the TOTAL number of confirmed DTCs; writes at most `max` codes. */
uint8_t  fm_get_confirmed(uint16_t *codes, uint8_t max);
/** Same as above for PENDING DTCs (Service 07). */
uint8_t  fm_get_pending(uint16_t *codes, uint8_t max);
/** Service 04: every DTC back to IDLE, MIL off. Conditions are re-evaluated next tick. */
void     fm_clear_all(void);

#endif /* FAULT_MGR_H */
