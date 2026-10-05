/**
 * @file    common.c
 * @brief   Pure-logic helpers (no HAL dependency, builds on a PC for host tests).
 */
#include "common.h"
#include <string.h>

/* ------------------------------------------------------------------------- */
bool tick_due(uint32_t *last, uint32_t period)
{
    uint32_t now = tick_ms();
    uint32_t dt  = now - *last;          /* unsigned math: safe across wrap-around */

    if (dt < period) {
        return false;
    }
    if (dt >= (2u * period)) {
        *last = now;                     /* more than one period late: resync */
    } else {
        *last += period;                 /* normal case: no drift */
    }
    return true;
}

/* ------------------------------------------------------------------------- */
uint8_t crc8(const uint8_t *d, uint16_t n)
{
    uint8_t crc = 0u;

    while (n--) {
        crc ^= *d++;
        for (uint8_t i = 0u; i < 8u; i++) {
            crc = (uint8_t)((crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x07u) : (uint8_t)(crc << 1));
        }
    }
    return crc;
}

/* ------------------------------------------------------------------------- */
bool rb_init(rb_t *r, void *storage, uint16_t cap, uint16_t item_size)
{
    if (r == NULL || storage == NULL || item_size == 0u ||
        cap == 0u || (cap & (cap - 1u)) != 0u) {   /* cap must be power of two */
        return false;
    }
    r->buf       = (uint8_t *)storage;
    r->cap       = cap;
    r->item_size = item_size;
    r->head      = 0u;
    r->tail      = 0u;
    r->drops     = 0u;
    return true;
}

bool rb_push(rb_t *r, const void *item)
{
    uint16_t head = r->head;

    if ((uint16_t)(head - r->tail) >= r->cap) {
        r->drops++;                                  /* full: drop newest */
        return false;
    }
    memcpy(&r->buf[(uint32_t)(head & (r->cap - 1u)) * r->item_size], item, r->item_size);
    __sync_synchronize();                            /* data visible before head moves */
    r->head = (uint16_t)(head + 1u);
    return true;
}

bool rb_pop(rb_t *r, void *item)
{
    uint16_t tail = r->tail;

    if (tail == r->head) {
        return false;
    }
    __sync_synchronize();
    memcpy(item, &r->buf[(uint32_t)(tail & (r->cap - 1u)) * r->item_size], r->item_size);
    __sync_synchronize();                            /* copy done before slot is released */
    r->tail = (uint16_t)(tail + 1u);
    return true;
}

uint16_t rb_count(const rb_t *r) { return (uint16_t)(r->head - r->tail); }
uint16_t rb_drops(const rb_t *r) { return r->drops; }
