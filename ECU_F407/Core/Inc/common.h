/**
 * @file    common.h
 * @brief   Shared types and helpers used by every CANsyn node
 *          (time base helpers, CRC-8, lock-free ring buffer).
 *
 * Design reference: Function design v1.0, sections 0 and 0.1.
 *
 * Rules that apply to ALL firmware in this project:
 *  - C99, STM32 HAL, no dynamic allocation.
 *  - ISRs only push data into ring buffers; all processing is done in the
 *    main loop, so no locking is required (single producer / single consumer).
 *  - All time checks use tick_ms(); HAL_Delay() is never used (SYS-03).
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------------- */
/* Basic types                                                               */
/* ------------------------------------------------------------------------- */

/** Generic return status for functions that can fail. */
typedef enum {
    ST_OK = 0,
    ST_BUSY,
    ST_PARAM,
    ST_TIMEOUT,
    ST_HW
} status_t;

/** Classic CAN 2.0A frame (11-bit ID). */
typedef struct {
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} can_frame_t;

/* ------------------------------------------------------------------------- */
/* Time base                                                                 */
/* ------------------------------------------------------------------------- */

/**
 * Milliseconds since reset (SysTick, 1 ms).
 * Firmware: implemented in tick_hal.c.  Host tests provide their own.
 */
uint32_t tick_ms(void);

/**
 * Periodic-task helper.
 * Returns true when (now - *last) >= period, then advances *last by exactly
 * one period so the cycle does not drift.  If the caller is more than one
 * full period late, *last is re-synchronised to "now" instead (no burst).
 */
bool tick_due(uint32_t *last, uint32_t period);

/* ------------------------------------------------------------------------- */
/* CRC-8                                                                     */
/* ------------------------------------------------------------------------- */

/** CRC-8, polynomial 0x07, init 0x00, no reflection, no final XOR. */
uint8_t crc8(const uint8_t *d, uint16_t n);

/* ------------------------------------------------------------------------- */
/* Ring buffer (single producer, single consumer, lock-free)                 */
/* ------------------------------------------------------------------------- */

/**
 * Indices are free-running 16-bit counters, so the buffer really holds `cap`
 * items (not cap-1).  `cap` MUST be a power of two.
 *  - `head` is written only by the producer (ISR),
 *  - `tail` is written only by the consumer (main loop).
 */
typedef struct {
    uint8_t           *buf;
    uint16_t           cap;
    uint16_t           item_size;
    volatile uint16_t  head;
    volatile uint16_t  tail;
    volatile uint16_t  drops;
} rb_t;

/** Bind the ring to caller-provided storage (cap * item_size bytes). */
bool     rb_init(rb_t *r, void *storage, uint16_t cap, uint16_t item_size);
/** Called from ISR. Returns false (and counts a drop) when the ring is full. */
bool     rb_push(rb_t *r, const void *item);
/** Called from main loop. Returns false when the ring is empty. */
bool     rb_pop(rb_t *r, void *item);
uint16_t rb_count(const rb_t *r);
uint16_t rb_drops(const rb_t *r);

/* ------------------------------------------------------------------------- */
/* Watchdog (firmware only, implemented in wdg.c)                            */
/* ------------------------------------------------------------------------- */

/** Refresh the IWDG. Call once per main-loop iteration (NFR-05). */
void wdg_kick(void);
/** In Debug builds, stop the IWDG while the core is halted by the debugger. */
void wdg_freeze_in_debug(void);

#endif /* COMMON_H */
