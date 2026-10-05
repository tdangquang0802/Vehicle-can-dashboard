/**
 * @file    obd_client.c
 * @brief   Pure-logic OBD client (no HAL, runs in host tests).
 */
#include "obd_client.h"
#include "can_drv.h"
#include "protocol.h"
#include <string.h>

#define OBD_TIMEOUT_MS   100u     /* DIAG-04 */
#define OBD_RETRIES      1u
#define LINK_TIMEOUT_MS  500u

static req_state_t  s_state = REQ_IDLE;
static obd_result_t s_res;
static can_frame_t  s_req;        /* kept for the retry */
static uint32_t     s_t0;
static uint8_t      s_retries;

static uint32_t     s_link_ts;
static bool         s_link_seen;
static bool         s_mil;

void obd_client_init(void)
{
    s_state     = REQ_IDLE;
    s_link_seen = false;
    s_mil       = false;
    memset(&s_res, 0, sizeof s_res);
}

status_t obd_request(uint8_t sid, uint8_t pid)
{
    if (s_state == REQ_WAIT) {
        return ST_BUSY;                           /* one outstanding request at a time */
    }
    memset(&s_req, 0, sizeof s_req);
    s_req.id  = CAN_ID_OBD_REQ_BCAST;
    s_req.dlc = 8u;
    if (sid == OBD_SID_CURRENT_DATA) {            /* [02 01 PID] */
        s_req.data[0] = 2u;
        s_req.data[1] = sid;
        s_req.data[2] = pid;
    } else {                                      /* [01 SID] */
        s_req.data[0] = 1u;
        s_req.data[1] = sid;
    }
    if (!can_tx(&s_req)) {
        return ST_HW;                             /* no free mailbox; caller may retry */
    }
    memset(&s_res, 0, sizeof s_res);
    s_res.sid = sid;
    s_res.pid = (sid == OBD_SID_CURRENT_DATA) ? pid : 0u;
    s_t0      = tick_ms();
    s_retries = OBD_RETRIES;
    s_state   = REQ_WAIT;
    return ST_OK;
}

static void handle_response(const can_frame_t *f)
{
    if (f->data[1] == (uint8_t)(s_res.sid + OBD_POS_OFFSET) &&
        (s_res.sid != OBD_SID_CURRENT_DATA || f->data[2] == s_res.pid)) {
        memcpy(s_res.raw, f->data, 8u);
        s_state = REQ_DONE;
    } else if (f->data[1] == OBD_NEG_RESP && f->data[2] == s_res.sid) {
        memcpy(s_res.raw, f->data, 8u);
        s_res.nrc = f->data[3];
        s_state   = REQ_FAIL_NEG;
    }
    /* anything else (late reply to an older request) is ignored */
}

void obd_poll(void)
{
    can_frame_t f;
    uint32_t    ts;

    while (can_rx_pop_ts(&f, &ts)) {
        if (f.id == CAN_ID_VEHICLE_STATUS) {              /* ECU heartbeat + MIL */
            s_link_ts   = ts;                             /* ISR time stamp, not "now" */
            s_link_seen = true;
            s_mil       = (f.data[0] & 0x01u) != 0u;
        } else if (f.id == CAN_ID_OBD_RESP && s_state == REQ_WAIT) {
            handle_response(&f);
        }
    }

    if (s_state == REQ_WAIT && (uint32_t)(tick_ms() - s_t0) >= OBD_TIMEOUT_MS) {
        if (s_retries > 0u) {
            s_retries--;
            (void)can_tx(&s_req);                         /* resend the same request */
            s_t0 = tick_ms();
        } else {
            s_state = REQ_FAIL_TIMEOUT;                   /* UI shows "No response" */
        }
    }
}

req_state_t         obd_state(void)  { return s_state; }
const obd_result_t *obd_result(void) { return &s_res; }

void obd_ack(void)
{
    if (s_state != REQ_WAIT) {
        s_state = REQ_IDLE;
    }
}

bool obd_link_ok(void)
{
    return s_link_seen && (uint32_t)(tick_ms() - s_link_ts) <= LINK_TIMEOUT_MS;
}

bool obd_mil(void) { return s_mil; }

/* ------------------------------------------------------------------------- */
uint8_t dec_dtc_total(const obd_result_t *r) { return r->raw[2]; }

uint8_t dec_dtc_codes(const obd_result_t *r, uint16_t *out)
{
    /* raw[0] = len = 2 + 2k  ->  number of codes actually carried */
    uint8_t carried = (r->raw[0] >= 2u) ? (uint8_t)((r->raw[0] - 2u) / 2u) : 0u;
    uint8_t total   = r->raw[2];
    uint8_t k       = (total < carried) ? total : carried;

    if (k > 2u) {
        k = 2u;
    }
    for (uint8_t i = 0u; i < k; i++) {
        out[i] = (uint16_t)(((uint16_t)r->raw[3u + 2u * i] << 8) | r->raw[4u + 2u * i]);
    }
    return k;
}

uint16_t dec_rpm(const obd_result_t *r)
{
    return (uint16_t)((((uint16_t)r->raw[3] << 8) | r->raw[4]) / 4u);
}

uint8_t dec_speed(const obd_result_t *r)   { return r->raw[3]; }
int16_t dec_coolant(const obd_result_t *r) { return (int16_t)((int16_t)r->raw[3] - 40); }
