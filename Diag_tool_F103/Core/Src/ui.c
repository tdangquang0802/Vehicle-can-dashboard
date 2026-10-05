/**
 * @file    ui.c
 * @brief   Screens HOME / LIVE / DTC / CLEAR on a 160x128 TFT.
 *
 * Screen layout (font 6x8, 26 columns):
 *   y = 1       top bar: "LINK OK|LINK LOST"  ...........  [MIL]   (always visible)
 *   y = 10      separator line
 *   y = 14+12*r body rows r = 0..8 (each row is one text "cell")
 *
 * Only cells whose text/colour changed are redrawn (DUI-07).  Every result is
 * also printed on the UART (DUI-08).
 *
 * OBD requests: the obd_client allows ONE outstanding request. The UI keeps a
 * small FIFO of requests plus an "owner" tag for the request in flight, so a
 * button press during a Live-data poll is simply queued behind it.
 */
#include "ui.h"
#include "buttons.h"
#include "dtc_text.h"
#include "obd_client.h"
#include "protocol.h"
#include "tft.h"
#include "common.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---- layout ---- */
#define UI_COLS    26u
#define UI_ROWS    9u
#define ROW_Y0     14u
#define ROW_DY     12u
#define BAR_H      10u

/* ---- timing ---- */
#define LIVE_PERIOD_MS   66u     /* 3 PIDs x 66 ms = ~5 Hz per parameter (DUI-04) */
#define CLEAR_ARM_MS     3000u   /* second Clear press must come within 3 s (DUI-06) */

/* ---- colours ---- */
#define C_BLACK   0x0000u
#define C_WHITE   0xFFFFu
#define C_RED     0xF800u
#define C_GREEN   0x07E0u
#define C_CYAN    0x07FFu
#define C_AMBER   0xFD20u
#define C_GRAY    0x8410u

typedef enum { SCR_HOME, SCR_LIVE, SCR_DTC, SCR_CLEAR } screen_t;
typedef enum { OWN_NONE, OWN_LIVE, OWN_READ, OWN_CLEAR, OWN_VERIFY } owner_t;
typedef enum { VIEW_NONE, VIEW_WAIT, VIEW_OK, VIEW_TIMEOUT, VIEW_NEG } dtc_view_t;
typedef enum {
    CLR_IDLE, CLR_ARMED, CLR_BUSY, CLR_CLEARED, CLR_REMAIN,
    CLR_REJECTED, CLR_NO_RESP, CLR_ERROR, CLR_CANCELLED
} clr_state_t;

/* ------------------------------------------------------------------------- */
/* State                                                                     */
/* ------------------------------------------------------------------------- */
static screen_t   s_scr = SCR_HOME;
static bool       s_dirty = true;            /* body must be cleared and redrawn */

/* request FIFO + owner of the request currently on the bus */
#define Q_LEN 4u
typedef struct { uint8_t sid, pid; owner_t owner; } qitem_t;
static qitem_t    s_q[Q_LEN];
static uint8_t    s_qn;
static owner_t    s_owner = OWN_NONE;

/* LIVE */
static uint8_t    s_live_idx;
static uint32_t   s_live_last_req;
static bool       s_live_have[3];
static bool       s_live_err;
static uint16_t   s_rpm;
static uint8_t    s_speed;
static int16_t    s_coolant;

/* DTC */
static dtc_view_t s_view = VIEW_NONE;
static uint8_t    s_dtc_total, s_dtc_n, s_view_nrc;
static uint16_t   s_dtc_codes[2];

/* CLEAR */
static clr_state_t s_clr = CLR_IDLE;
static uint32_t    s_clr_t0;
static uint8_t     s_clr_remain, s_clr_nrc;

/* text cache: what is currently on the glass */
static char       s_row[UI_ROWS][UI_COLS + 1u];
static uint16_t   s_row_col[UI_ROWS];
static char       s_shown[UI_ROWS][UI_COLS + 1u];
static uint16_t   s_shown_col[UI_ROWS];
static int8_t     s_top_link = -1, s_top_mil = -1;   /* -1 = unknown, forces first draw */

/* ------------------------------------------------------------------------- */
/* Request FIFO                                                              */
/* ------------------------------------------------------------------------- */
static bool q_push(uint8_t sid, uint8_t pid, owner_t owner)
{
    if (s_qn >= Q_LEN) {
        return false;
    }
    s_q[s_qn++] = (qitem_t){ sid, pid, owner };
    return true;
}

static void q_pop(void)
{
    for (uint8_t i = 1u; i < s_qn; i++) {
        s_q[i - 1u] = s_q[i];
    }
    s_qn--;
}

/** Start the next request if the client is idle (queue first, then Live polling). */
static void launch_next(void)
{
    static const uint8_t live_pids[3] = { OBD_PID_RPM, OBD_PID_SPEED, OBD_PID_COOLANT };

    if (obd_state() != REQ_IDLE) {
        return;
    }
    if (s_qn > 0u) {
        if (obd_request(s_q[0].sid, s_q[0].pid) == ST_OK) {
            s_owner = s_q[0].owner;
            q_pop();
        }
        return;                                   /* ST_HW: keep it queued, retry next tick */
    }
    if (s_scr == SCR_LIVE && (uint32_t)(tick_ms() - s_live_last_req) >= LIVE_PERIOD_MS) {
        if (obd_request(OBD_SID_CURRENT_DATA, live_pids[s_live_idx]) == ST_OK) {
            s_owner         = OWN_LIVE;
            s_live_last_req = tick_ms();
            s_live_idx      = (uint8_t)((s_live_idx + 1u) % 3u);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* Result handling (also prints to the UART, DUI-08)                         */
/* ------------------------------------------------------------------------- */
static void print_dtcs(const char *tag)
{
    char c[6];
    printf("%s total=%u", tag, (unsigned)s_dtc_total);
    for (uint8_t i = 0u; i < s_dtc_n; i++) {
        dtc_str(s_dtc_codes[i], c);
        printf(" %s(%s)", c, dtc_desc(s_dtc_codes[i]));
    }
    printf("\r\n");
}

static void on_result(req_state_t st)
{
    const obd_result_t *r = obd_result();

    switch (s_owner) {
    case OWN_LIVE:
        if (st == REQ_DONE) {
            s_live_err = false;
            if (r->pid == OBD_PID_RPM)        { s_rpm = dec_rpm(r);         s_live_have[0] = true; }
            if (r->pid == OBD_PID_SPEED)      { s_speed = dec_speed(r);     s_live_have[1] = true; }
            if (r->pid == OBD_PID_COOLANT) {
                s_coolant = dec_coolant(r);   s_live_have[2] = true;
                /* one UART line per complete 0C/0D/05 cycle (~5 Hz) */
                printf("LIVE rpm=%u speed=%u coolant=%d\r\n",
                       (unsigned)s_rpm, (unsigned)s_speed, (int)s_coolant);
            }
        } else if (st == REQ_FAIL_TIMEOUT) {
            s_live_err = true;
            printf("LIVE no response\r\n");
        }
        break;

    case OWN_READ:
        if (st == REQ_DONE) {
            s_dtc_total = dec_dtc_total(r);
            s_dtc_n     = dec_dtc_codes(r, s_dtc_codes);
            s_view      = VIEW_OK;
            print_dtcs("READ");
        } else if (st == REQ_FAIL_TIMEOUT) {
            s_view = VIEW_TIMEOUT;
            printf("READ no response\r\n");
        } else {
            s_view = VIEW_NEG;
            s_view_nrc = r->nrc;
            printf("READ negative response NRC=0x%02X\r\n", (unsigned)r->nrc);
        }
        break;

    case OWN_CLEAR:
        if (st == REQ_DONE) {
            /* DIAG-05: confirm with a Service 03 (queue it, it runs next) */
            (void)q_push(OBD_SID_STORED_DTC, 0u, OWN_VERIFY);
        } else if (st == REQ_FAIL_TIMEOUT) {
            s_clr = CLR_NO_RESP;
            printf("CLEAR no response\r\n");
        } else if (r->nrc == OBD_NRC_CONDITIONS_NOT_CORRECT) {
            s_clr = CLR_REJECTED;
            printf("CLEAR rejected (speed>0)\r\n");
        } else {
            s_clr = CLR_ERROR;
            s_clr_nrc = r->nrc;
            printf("CLEAR negative response NRC=0x%02X\r\n", (unsigned)r->nrc);
        }
        break;

    case OWN_VERIFY:
        if (st == REQ_DONE) {
            s_clr_remain = dec_dtc_total(r);
            s_clr = (s_clr_remain == 0u) ? CLR_CLEARED : CLR_REMAIN;
            printf("CLEAR done, DTC remaining=%u\r\n", (unsigned)s_clr_remain);
        } else {
            s_clr = CLR_CLEARED;                  /* 04 was accepted, only the check failed */
            printf("CLEAR done, verification failed\r\n");
        }
        break;

    default:
        break;
    }
    s_owner = OWN_NONE;
}

/* ------------------------------------------------------------------------- */
/* Buttons / screen transitions                                              */
/* ------------------------------------------------------------------------- */
static void goto_screen(screen_t s)
{
    if (s == s_scr) {
        return;
    }
    s_scr   = s;
    s_dirty = true;
    if (s != SCR_CLEAR && s_clr == CLR_ARMED) {
        s_clr = CLR_IDLE;                          /* leaving the screen disarms Clear */
    }
}

static void request_read(void)
{
    s_view = q_push(OBD_SID_STORED_DTC, 0u, OWN_READ) ? VIEW_WAIT : VIEW_TIMEOUT;
}

static void arm_clear(void)
{
    s_clr    = CLR_ARMED;
    s_clr_t0 = tick_ms();
}

static void on_button(btn_t b)
{
    switch (b) {
    case BTN_LIVE:                                  /* HOME/DTC/CLEAR -> LIVE, LIVE -> HOME */
        if (s_scr == SCR_LIVE) {
            goto_screen(SCR_HOME);
        } else {
            goto_screen(SCR_LIVE);
            s_live_err = false;
            memset(s_live_have, 0, sizeof s_live_have);
        }
        break;

    case BTN_READ:                                  /* any screen -> DTC and (re)send 03 */
        goto_screen(SCR_DTC);
        request_read();
        break;

    case BTN_CLEAR:
        if (s_scr != SCR_CLEAR) {                   /* first press: go to CLEAR and arm */
            goto_screen(SCR_CLEAR);
            arm_clear();
        } else if (s_clr == CLR_ARMED &&
                   (uint32_t)(tick_ms() - s_clr_t0) < CLEAR_ARM_MS) {
            if (q_push(OBD_SID_CLEAR_DTC, 0u, OWN_CLEAR)) {   /* second press: send 04 */
                s_clr = CLR_BUSY;
            }
        } else if (s_clr != CLR_BUSY) {
            arm_clear();                            /* idle/result/cancelled: arm again */
        }
        break;

    default:
        break;
    }
}

/* ------------------------------------------------------------------------- */
/* Rendering                                                                 */
/* ------------------------------------------------------------------------- */
static void set_row(uint8_t r, uint16_t col, const char *fmt, ...) __attribute__((format(printf, 3, 4)));

static void set_row(uint8_t r, uint16_t col, const char *fmt, ...)
{
    char    tmp[40];
    va_list ap;

    va_start(ap, fmt);
    (void)vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);

    size_t n = strlen(tmp);
    for (size_t i = 0u; i < UI_COLS; i++) {          /* pad with spaces: overwrites old text */
        s_row[r][i] = (i < n) ? tmp[i] : ' ';
    }
    s_row[r][UI_COLS] = '\0';
    s_row_col[r] = col;
}

static void build_body(void)
{
    char c[6];

    for (uint8_t r = 0u; r < UI_ROWS; r++) {         /* start from blank rows */
        set_row(r, C_WHITE, "%s", "");
    }

    switch (s_scr) {
    case SCR_HOME:
        set_row(0, C_CYAN,  "VEHICLE CAN DIAG");
        set_row(2, C_WHITE, "LIVE  : live data");
        set_row(3, C_WHITE, "READ  : read DTC");
        set_row(4, C_WHITE, "CLEAR : clear DTC");
        break;

    case SCR_LIVE:
        set_row(0, C_CYAN, "LIVE DATA");
        if (s_live_have[0]) { set_row(2, C_WHITE, "RPM     %5u rpm",  (unsigned)s_rpm); }
        else                { set_row(2, C_GRAY,  "RPM       --"); }
        if (s_live_have[1]) { set_row(3, C_WHITE, "Speed   %5u km/h", (unsigned)s_speed); }
        else                { set_row(3, C_GRAY,  "Speed     --"); }
        if (s_live_have[2]) { set_row(4, C_WHITE, "Coolant %5d C",   (int)s_coolant); }
        else                { set_row(4, C_GRAY,  "Coolant   --"); }
        if (s_live_err)     { set_row(6, C_RED,   "No response"); }
        break;

    case SCR_DTC:
        set_row(0, C_CYAN, "DTC LIST");
        switch (s_view) {
        case VIEW_WAIT:    set_row(2, C_WHITE, "Reading..."); break;
        case VIEW_TIMEOUT: set_row(2, C_RED,   "No response"); break;
        case VIEW_NEG:     set_row(2, C_RED,   "Negative resp 0x%02X", (unsigned)s_view_nrc); break;
        case VIEW_OK:
            if (s_dtc_total == 0u) {
                set_row(2, C_GREEN, "No DTC stored");
            } else {
                if (s_dtc_total > s_dtc_n) { set_row(1, C_WHITE, "Showing %u of %u", (unsigned)s_dtc_n, (unsigned)s_dtc_total); }
                else                       { set_row(1, C_WHITE, "Total: %u", (unsigned)s_dtc_total); }
                /* Rows 2..7 (6 lines) are reserved for the list. With the MVP limit of 2 DTC per
                 * frame no scrolling is needed; scrolling with READ arrives with ISO-TP (ECU-11). */
                for (uint8_t i = 0u; i < s_dtc_n; i++) {
                    dtc_str(s_dtc_codes[i], c);
                    set_row((uint8_t)(2u + i), C_AMBER, "%s %s", c, dtc_desc(s_dtc_codes[i]));
                }
            }
            break;
        default:           set_row(2, C_GRAY, "Press READ"); break;
        }
        break;

    case SCR_CLEAR:
        set_row(0, C_CYAN, "CLEAR DTC");
        switch (s_clr) {
        case CLR_IDLE:      set_row(2, C_WHITE, "Press CLEAR to erase"); break;
        case CLR_ARMED: {
            uint32_t el  = (uint32_t)(tick_ms() - s_clr_t0);
            uint32_t sec = (el >= CLEAR_ARM_MS) ? 0u : (CLEAR_ARM_MS - el + 999u) / 1000u;
            set_row(2, C_AMBER, "Press again (%u s)", (unsigned)sec);
            break;
        }
        case CLR_BUSY:      set_row(2, C_WHITE, "Clearing..."); break;
        case CLR_CLEARED:   set_row(2, C_GREEN, "Cleared"); break;
        case CLR_REMAIN:    set_row(2, C_AMBER, "Cleared, %u remain", (unsigned)s_clr_remain); break;
        case CLR_REJECTED:  set_row(2, C_RED,   "Rejected (speed>0)"); break;
        case CLR_NO_RESP:   set_row(2, C_RED,   "No response"); break;
        case CLR_ERROR:     set_row(2, C_RED,   "Error NRC 0x%02X", (unsigned)s_clr_nrc); break;
        case CLR_CANCELLED: set_row(2, C_GRAY,  "Cancelled"); break;
        }
        break;
    }
}

static void render_top(void)
{
    int8_t link = obd_link_ok() ? 1 : 0;
    int8_t mil  = obd_mil() ? 1 : 0;

    if (link != s_top_link) {
        s_top_link = link;
        tft_text(2u, 1u, link ? "LINK OK  " : "LINK LOST", link ? C_GREEN : C_RED, C_BLACK);
    }
    if (mil != s_top_mil) {
        s_top_mil = mil;
        tft_text(126u, 1u, " MIL ", mil ? C_BLACK : C_GRAY, mil ? C_AMBER : C_BLACK);
    }
}

static void render_body(void)
{
    if (s_dirty) {                                    /* new screen: wipe body, drop the cache */
        tft_fill_rect(0u, (uint8_t)(BAR_H + 1u), TFT_WIDTH, (uint8_t)(TFT_HEIGHT - BAR_H - 1u), C_BLACK);
        for (uint8_t r = 0u; r < UI_ROWS; r++) {
            s_shown[r][0] = '\0';
            s_shown_col[r] = 0u;
        }
        s_dirty = false;
    }
    build_body();
    for (uint8_t r = 0u; r < UI_ROWS; r++) {          /* draw only the cells that changed */
        if (s_shown_col[r] != s_row_col[r] || strcmp(s_shown[r], s_row[r]) != 0) {
            tft_text(2u, (uint8_t)(ROW_Y0 + ROW_DY * r), s_row[r], s_row_col[r], C_BLACK);
            memcpy(s_shown[r], s_row[r], UI_COLS + 1u);
            s_shown_col[r] = s_row_col[r];
        }
    }
}

/* ------------------------------------------------------------------------- */
void ui_init(void)
{
    tft_init();
    tft_fill_rect(0u, BAR_H, TFT_WIDTH, 1u, C_GRAY);      /* separator under the top bar */
    s_dirty = true;
    printf("Diag tool ready\r\n");
}

void ui_run_10ms(void)
{
    btn_t b = buttons_poll_10ms();
    if (b != BTN_NONE) {
        on_button(b);
    }

    req_state_t st = obd_state();
    if (st == REQ_DONE || st == REQ_FAIL_TIMEOUT || st == REQ_FAIL_NEG) {
        on_result(st);
        obd_ack();
    }
    launch_next();

    /* Clear confirmation window expired -> cancel (DUI-06) */
    if (s_clr == CLR_ARMED && (uint32_t)(tick_ms() - s_clr_t0) >= CLEAR_ARM_MS) {
        s_clr = CLR_CANCELLED;
    }

    render_top();
    render_body();
}
