/**
 * @file    ecu_app.c
 * @brief   ECU super-loop: scheduler, 0x100 / 0x101 transmitters (ECU-02, NFR-01).
 *
 * Schedule (all against tick_ms(), no HAL_Delay):
 *   every loop : obdsrv_poll(), wdg_kick()
 *   10 ms      : sim_update_10ms() + fm_run_10ms()
 *   20 ms      : 0x100 VEHICLE_STATE
 *   100 ms     : 0x101 VEHICLE_STATUS
 *   1 s        : alive LED
 */
#include "ecu_app.h"
#include "can_drv.h"
#include "fault_mgr.h"
#include "obd_srv.h"
#include "protocol.h"
#include "sim.h"
#include "main.h"

static uint32_t s_t10, s_t20, s_t100, s_t1000;
static uint8_t  s_cnt_state, s_cnt_status;      /* rolling counters, byte 7 */

/** 0x100: speed, rpm, coolant, throttle, Vbat, counter (little-endian). */
static void app_tx_state(void)
{
    const vehicle_t *v = sim_get();
    can_frame_t f = { .id = CAN_ID_VEHICLE_STATE, .dlc = 8u };

    f.data[0] = (uint8_t)(v->speed_x10 & 0xFFu);
    f.data[1] = (uint8_t)(v->speed_x10 >> 8);
    f.data[2] = (uint8_t)(v->rpm & 0xFFu);
    f.data[3] = (uint8_t)(v->rpm >> 8);
    f.data[4] = v->coolant_raw;
    f.data[5] = v->throttle;
    f.data[6] = v->vbat_x10;
    f.data[7] = s_cnt_state++;
    (void)can_tx(&f);        /* if it fails, skip this cycle: never queue stale data */
}

/** 0x101: MIL, DTC count, fault bitmap, engine state, counter. */
static void app_tx_status(void)
{
    const vehicle_t *v = sim_get();
    uint16_t bm = fm_bitmap();
    can_frame_t f = { .id = CAN_ID_VEHICLE_STATUS, .dlc = 8u };

    f.data[0] = fm_mil() ? 0x01u : 0x00u;
    f.data[1] = fm_get_confirmed(NULL, 0u);
    f.data[2] = (uint8_t)(bm & 0xFFu);
    f.data[3] = (uint8_t)(bm >> 8);
    f.data[4] = v->engine_state;
    f.data[5] = 0u;
    f.data[6] = 0u;
    f.data[7] = s_cnt_status++;
    (void)can_tx(&f);
}

void ecu_setup(void)
{
    static const uint16_t rx_ids[] = { CAN_ID_OBD_REQ_BCAST, CAN_ID_OBD_REQ_PHYS };

    wdg_freeze_in_debug();
    sim_init();
    fm_init();

    /* Start CAN last: everything the main loop touches already exists. */
    (void)can_init(rx_ids, 2u);
}

void ecu_loop(void)
{
    obdsrv_poll();

    if (tick_due(&s_t10, 10u)) {
        sim_update_10ms();
        fm_run_10ms(sim_get());
    }
    if (tick_due(&s_t20, 20u)) {
        app_tx_state();
    }
    if (tick_due(&s_t100, 100u)) {
        app_tx_status();
    }
    if (tick_due(&s_t1000, 1000u)) {
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);
    }
    wdg_kick();
}
