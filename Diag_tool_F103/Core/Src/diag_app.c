/**
 * @file    diag_app.c
 * @brief   Diag tool super-loop.
 *
 *   every loop : obd_poll() (drain CAN ring, timeouts), wdg_kick()
 *   10 ms      : ui_run_10ms() (buttons, requests, drawing)
 *
 * A full-screen redraw can take a few tens of ms; frames that arrive meanwhile
 * wait in the 16-deep RX ring, so nothing is lost.
 */
#include "diag_app.h"
#include "can_drv.h"
#include "obd_client.h"
#include "protocol.h"
#include "ui.h"
#include "main.h"

static uint32_t s_t10;

void diag_setup(void)
{
    static const uint16_t rx_ids[] = { CAN_ID_OBD_RESP, CAN_ID_VEHICLE_STATUS };

    wdg_freeze_in_debug();
    obd_client_init();
    ui_init();                               /* TFT init takes ~0.5 s */

    /* Start CAN last, once the client and UI state exist. */
    (void)can_init(rx_ids, 2u);
}

void diag_loop(void)
{
    obd_poll();
    if (tick_due(&s_t10, 10u)) {
        ui_run_10ms();
    }
    wdg_kick();
}
