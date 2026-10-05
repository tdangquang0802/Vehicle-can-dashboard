/**
 * @file    fault_mgr.c
 * @brief   DTC state machine. Pure logic, no HAL (runs in host tests).
 */
#include "fault_mgr.h"
#include "protocol.h"

#define CONFIRM_TICKS 200u      /* 200 x 10 ms = 2 s of continuous fault */

typedef struct {
    dtc_state_t st;
    uint16_t    cnt;            /* consecutive 10 ms ticks with the condition true */
} dtc_t;

static dtc_t s_dtc[DTC_COUNT];

static const uint16_t s_code[DTC_COUNT] = {
    DTC_CODE_P0217, DTC_CODE_P0118, DTC_CODE_P0562, DTC_CODE_C0035
};

void fm_init(void)
{
    for (uint8_t i = 0u; i < DTC_COUNT; i++) {
        s_dtc[i].st  = DTC_IDLE;
        s_dtc[i].cnt = 0u;
    }
}

/** Fault conditions (section 2.5 / 1.3 of the design). */
static void eval_cond(const vehicle_t *v, bool c[DTC_COUNT])
{
    /* raw ADC outside [100, 3995] -> open / short circuit on the sensor */
    c[DTC_P0118] = (v->coolant_adc < 100u) || (v->coolant_adc > 3995u);
    /* over-temperature, suppressed while the sensor itself is faulty so a
     * broken sensor does not produce a false over-temperature code */
    c[DTC_P0217] = (((int16_t)v->coolant_raw - 40) > 110) && !c[DTC_P0118];
    c[DTC_P0562] = (v->vbat_x10 < 105u);              /* Vbat < 10.5 V */
    c[DTC_C0035] = v->wheel_sensor_fault;
}

void fm_run_10ms(const vehicle_t *v)
{
    bool cond[DTC_COUNT];
    eval_cond(v, cond);

    for (uint8_t i = 0u; i < DTC_COUNT; i++) {
        dtc_t *d = &s_dtc[i];

        switch (d->st) {
        case DTC_IDLE:
            if (cond[i]) {
                d->st  = DTC_PENDING;
                d->cnt = 1u;
            }
            break;

        case DTC_PENDING:
            if (!cond[i]) {                    /* glitch: back to IDLE, restart */
                d->st  = DTC_IDLE;
                d->cnt = 0u;
            } else if (++d->cnt >= CONFIRM_TICKS) {
                d->st = DTC_CONFIRMED;
            }
            break;

        case DTC_CONFIRMED:
        default:
            break;                              /* latched until Service 04 (ECU-05) */
        }
    }
}

bool fm_mil(void)
{
    for (uint8_t i = 0u; i < DTC_COUNT; i++) {
        if (s_dtc[i].st == DTC_CONFIRMED) {
            return true;
        }
    }
    return false;
}

uint16_t fm_bitmap(void)
{
    uint16_t m = 0u;
    for (uint8_t i = 0u; i < DTC_COUNT; i++) {
        if (s_dtc[i].st == DTC_CONFIRMED) {
            m |= (uint16_t)(1u << i);
        }
    }
    return m;
}

static uint8_t collect(dtc_state_t want, uint16_t *codes, uint8_t max)
{
    uint8_t total = 0u;
    for (uint8_t i = 0u; i < DTC_COUNT; i++) {
        if (s_dtc[i].st == want) {
            if (codes != NULL && total < max) {
                codes[total] = s_code[i];
            }
            total++;                            /* always count, even if not stored */
        }
    }
    return total;
}

uint8_t fm_get_confirmed(uint16_t *codes, uint8_t max) { return collect(DTC_CONFIRMED, codes, max); }
uint8_t fm_get_pending(uint16_t *codes, uint8_t max)   { return collect(DTC_PENDING,   codes, max); }

void fm_clear_all(void)
{
    fm_init();   /* all IDLE, cnt = 0 -> MIL off. If a condition is still true
                  * the next fm_run_10ms() puts the DTC back into PENDING (ECU-07). */
}
