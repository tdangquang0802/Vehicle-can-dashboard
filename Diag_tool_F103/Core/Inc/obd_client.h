/**
 * @file    obd_client.h
 * @brief   OBD-II client state machine on the Diag tool (DIAG-02, 04, 05).
 *
 *   IDLE --obd_request()--> WAIT --reply--> DONE | FAIL_NEG
 *                            |--100 ms--> resend once --100 ms--> FAIL_TIMEOUT
 *   DONE / FAIL_* --obd_ack()--> IDLE
 *
 * Also tracks the ECU heartbeat (frame 0x101) for the LINK / MIL indicators.
 */
#ifndef OBD_CLIENT_H
#define OBD_CLIENT_H

#include "common.h"

typedef enum { REQ_IDLE, REQ_WAIT, REQ_DONE, REQ_FAIL_TIMEOUT, REQ_FAIL_NEG } req_state_t;

typedef struct {
    uint8_t sid;
    uint8_t pid;       /* only meaningful for service 01 */
    uint8_t nrc;       /* valid in REQ_FAIL_NEG          */
    uint8_t raw[8];    /* the 0x7E8 frame payload        */
} obd_result_t;

void                obd_client_init(void);
/** Send a request on 0x7DF. Returns ST_BUSY if one is already waiting. */
status_t            obd_request(uint8_t sid, uint8_t pid);
/** Drain the CAN RX ring and run timeouts. Call every main-loop iteration. */
void                obd_poll(void);
req_state_t         obd_state(void);
const obd_result_t *obd_result(void);
void                obd_ack(void);

/** true if a 0x101 frame was seen within the last 500 ms. */
bool                obd_link_ok(void);
/** MIL bit from the latest 0x101 frame. */
bool                obd_mil(void);

/* ---- response decoders ---- */
uint8_t  dec_dtc_total(const obd_result_t *r);                 /* raw[2]            */
uint8_t  dec_dtc_codes(const obd_result_t *r, uint16_t *out);  /* returns <= 2      */
uint16_t dec_rpm(const obd_result_t *r);                       /* (256A + B) / 4    */
uint8_t  dec_speed(const obd_result_t *r);                     /* A, km/h           */
int16_t  dec_coolant(const obd_result_t *r);                   /* A - 40, degC      */

#endif /* OBD_CLIENT_H */
