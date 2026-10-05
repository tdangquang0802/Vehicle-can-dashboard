/**
 * @file    obd_srv.c
 * @brief   OBD-II request handler (ISO 15765-4, single frame only).
 *
 * Request frame : [len, SID, params..., pad]  on 0x7DF (broadcast) or 0x7E0
 * Response frame: [len, SID+0x40, data...]    on 0x7E8, padded with 0x00
 */
#include "obd_srv.h"
#include "app_config.h"
#include "can_drv.h"
#include "fault_mgr.h"
#include "protocol.h"
#include "sim.h"
#include <string.h>

#define OBD_REPLY_RETRY_MS  50u    /* how long a reply may wait for a free TX mailbox */

/* One-slot holding area for a reply that could not be sent immediately. */
static can_frame_t s_pend;
static bool        s_pend_valid;
static uint32_t    s_pend_t0;

/* ------------------------------------------------------------------------- */
/** Wrap `len` payload bytes into a 0x7E8 frame (len byte first, zero padded). */
static void obd_send(const uint8_t *p, uint8_t len)
{
    can_frame_t f;

    if (len > 7u) {
        return;
    }
    memset(&f, 0, sizeof f);
    f.id      = CAN_ID_OBD_RESP;
    f.dlc     = 8u;
    f.data[0] = len;
    memcpy(&f.data[1], p, len);

    if (!can_tx(&f)) {                 /* mailbox full: retry from obdsrv_poll() */
        s_pend       = f;
        s_pend_valid = true;
        s_pend_t0    = tick_ms();
    }
}

/** Negative response: 03 7F SID NRC */
static void obd_neg(uint8_t sid, uint8_t nrc)
{
    const uint8_t p[3] = { OBD_NEG_RESP, sid, nrc };
    obd_send(p, 3u);
}

/* ------------------------------------------------------------------------- */
/** Service 01 - current data. */
static void svc01(uint8_t pid)
{
    const vehicle_t *v = sim_get();
    uint8_t p[4];

    switch (pid) {
    case OBD_PID_COOLANT:                              /* 03 41 05 A, A - 40 = degC */
        p[0] = OBD_SID_CURRENT_DATA + OBD_POS_OFFSET;
        p[1] = pid;
        p[2] = v->coolant_raw;
        obd_send(p, 3u);
        break;

    case OBD_PID_RPM: {                                /* 04 41 0C A B, (256A+B)/4 = rpm */
        uint32_t x = (uint32_t)v->rpm * 4u;
        if (x > 0xFFFFu) {
            x = 0xFFFFu;
        }
        p[0] = OBD_SID_CURRENT_DATA + OBD_POS_OFFSET;
        p[1] = pid;
        p[2] = (uint8_t)(x >> 8);
        p[3] = (uint8_t)(x & 0xFFu);
        obd_send(p, 4u);
        break;
    }

    case OBD_PID_SPEED: {                              /* 03 41 0D A, A = km/h */
        uint16_t kmh = (uint16_t)(v->speed_x10 / 10u);
        p[0] = OBD_SID_CURRENT_DATA + OBD_POS_OFFSET;
        p[1] = pid;
        p[2] = (uint8_t)((kmh > 255u) ? 255u : kmh);
        obd_send(p, 3u);
        break;
    }

    default:
        obd_neg(OBD_SID_CURRENT_DATA, OBD_NRC_REQUEST_OUT_OF_RANGE);
        break;
    }
}

/** Service 03 (stored) / 07 (pending): single frame carries at most 2 DTCs. */
static void svc03_07(uint8_t sid)
{
    uint16_t codes[2];
    uint8_t  total = (sid == OBD_SID_STORED_DTC) ? fm_get_confirmed(codes, 2u)
                                                 : fm_get_pending(codes, 2u);
    uint8_t  k = (total < 2u) ? total : 2u;
    uint8_t  p[6];

    p[0] = (uint8_t)(sid + OBD_POS_OFFSET);
    p[1] = total;                                      /* TOTAL count, not only k */
    for (uint8_t i = 0u; i < k; i++) {
        p[2u + 2u * i] = (uint8_t)(codes[i] >> 8);
        p[3u + 2u * i] = (uint8_t)(codes[i] & 0xFFu);
    }
    obd_send(p, (uint8_t)(2u + 2u * k));               /* total == 0 -> "02 43 00" */
}

/** Service 04 - clear DTCs. */
static void svc04(void)
{
#if ECU08_ENABLED
    if (sim_get()->speed_x10 > 0u) {                   /* ECU-08: not while driving */
        obd_neg(OBD_SID_CLEAR_DTC, OBD_NRC_CONDITIONS_NOT_CORRECT);
        return;
    }
#endif
    fm_clear_all();
    {
        const uint8_t p[1] = { (uint8_t)(OBD_SID_CLEAR_DTC + OBD_POS_OFFSET) };
        obd_send(p, 1u);                               /* 01 44 */
    }
}

/* ------------------------------------------------------------------------- */
static void obd_handle(const can_frame_t *rq)
{
    uint8_t len, sid;

    /* 0x7DF and 0x7E0 are handled identically; the reply is always 0x7E8. */
    if (rq->id != CAN_ID_OBD_REQ_BCAST && rq->id != CAN_ID_OBD_REQ_PHYS) {
        return;
    }
    len = rq->data[0];
    if (len < 1u || len > 7u) {
        return;                                        /* malformed: stay silent */
    }
    sid = rq->data[1];

    switch (sid) {
    case OBD_SID_CURRENT_DATA:
        /* A missing PID byte is mapped to 0xFF, which is unsupported -> NRC 0x31 */
        svc01((len >= 2u) ? rq->data[2] : 0xFFu);
        break;
    case OBD_SID_STORED_DTC:
    case OBD_SID_PENDING_DTC:
        svc03_07(sid);
        break;
    case OBD_SID_CLEAR_DTC:
        svc04();
        break;
    default:
        obd_neg(sid, OBD_NRC_SERVICE_NOT_SUPPORTED);
        break;
    }
}

void obdsrv_poll(void)
{
    can_frame_t rq;

    if (s_pend_valid) {                                /* finish the previous reply first */
        if (can_tx(&s_pend)) {
            s_pend_valid = false;
        } else if ((uint32_t)(tick_ms() - s_pend_t0) > OBD_REPLY_RETRY_MS) {
            s_pend_valid = false;                      /* give up, tester will retry */
        }
        return;
    }
    if (can_rx_pop(&rq)) {
        obd_handle(&rq);
    }
}
