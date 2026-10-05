/**
 * @file    can_drv.c
 * @brief   CAN driver on top of the STM32 HAL (works for F1 and F4 HAL).
 */
#include "can_drv.h"
#include "main.h"

#ifndef CAN_RX_RING_SIZE
#define CAN_RX_RING_SIZE 16u
#endif

_Static_assert((CAN_RX_RING_SIZE & (CAN_RX_RING_SIZE - 1u)) == 0u,
               "CAN_RX_RING_SIZE must be a power of two");

extern CAN_HandleTypeDef APP_CAN_HANDLE;
#define HCAN (&APP_CAN_HANDLE)

#define MAX_LIST_IDS 56u            /* 14 banks x 4 IDs (16-bit list mode) */

/** Ring item: the frame plus the time it was received. */
typedef struct {
    can_frame_t f;
    uint32_t    ts;
} can_rx_item_t;

static can_rx_item_t     s_rx_store[CAN_RX_RING_SIZE];
static rb_t              s_rx_rb;
static volatile uint32_t s_rx_total;
#ifndef CAN_RX_ONLY
static uint32_t          s_tx_overrun;
#endif

/* ------------------------------------------------------------------------- */
/* Filters                                                                   */
/* ------------------------------------------------------------------------- */
static status_t cfg_filters(const uint16_t *ids, uint8_t n)
{
    CAN_FilterTypeDef f = {0};

    f.FilterFIFOAssignment = CAN_RX_FIFO0;
    f.FilterActivation     = CAN_FILTER_ENABLE;
    f.SlaveStartFilterBank = 14;                 /* all banks belong to CAN1 */

    if (ids == NULL || n == 0u) {
        /* Pass-all for standard frames: 32-bit mask mode, ID = 0, mask only
         * checks the IDE bit (bit 2 of the low half) so extended frames are out. */
        f.FilterBank      = 0;
        f.FilterMode      = CAN_FILTERMODE_IDMASK;
        f.FilterScale     = CAN_FILTERSCALE_32BIT;
        f.FilterIdHigh    = 0x0000;
        f.FilterIdLow     = 0x0000;
        f.FilterMaskIdHigh = 0x0000;
        f.FilterMaskIdLow  = 0x0004;
        return (HAL_CAN_ConfigFilter(HCAN, &f) == HAL_OK) ? ST_OK : ST_HW;
    }

    if (n > MAX_LIST_IDS) {
        return ST_PARAM;
    }

    /* ID-list mode, 16-bit scale: 4 standard IDs per bank.
     * In 16-bit scale the 11-bit ID sits in bits [15:5]; IDE = RTR = 0. */
    f.FilterMode  = CAN_FILTERMODE_IDLIST;
    f.FilterScale = CAN_FILTERSCALE_16BIT;

    uint8_t bank = 0u;
    for (uint8_t i = 0u; i < n; i += 4u) {
        uint16_t v[4];
        for (uint8_t k = 0u; k < 4u; k++) {
            /* unused slots repeat the last valid ID (harmless duplicates) */
            uint8_t idx = (uint8_t)(((i + k) < n) ? (i + k) : (n - 1u));
            v[k] = (uint16_t)((ids[idx] & 0x7FFu) << 5);
        }
        f.FilterBank       = bank++;
        f.FilterIdLow      = v[0];
        f.FilterIdHigh     = v[1];
        f.FilterMaskIdLow  = v[2];
        f.FilterMaskIdHigh = v[3];
        if (HAL_CAN_ConfigFilter(HCAN, &f) != HAL_OK) {
            return ST_HW;
        }
    }
    return ST_OK;
}

/* ------------------------------------------------------------------------- */
status_t can_init(const uint16_t *rx_ids, uint8_t n_ids)
{
    if (!rb_init(&s_rx_rb, s_rx_store, CAN_RX_RING_SIZE, (uint16_t)sizeof(can_rx_item_t))) {
        return ST_PARAM;
    }
    s_rx_total = 0u;

    status_t st = cfg_filters(rx_ids, n_ids);
    if (st != ST_OK) {
        return st;
    }
    if (HAL_CAN_Start(HCAN) != HAL_OK) {
        return ST_HW;
    }
    if (HAL_CAN_ActivateNotification(HCAN, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        return ST_HW;
    }
    return ST_OK;
}

/* ------------------------------------------------------------------------- */
/* TX (not available on the Bridge)                                          */
/* ------------------------------------------------------------------------- */
#ifndef CAN_RX_ONLY
bool can_tx(const can_frame_t *f)
{
    CAN_TxHeaderTypeDef hdr;
    uint8_t  data[8];
    uint32_t mailbox;

    if (HAL_CAN_GetTxMailboxesFreeLevel(HCAN) == 0u) {
        s_tx_overrun++;                       /* never block the super-loop */
        return false;
    }
    hdr.StdId              = f->id & 0x7FFu;
    hdr.ExtId              = 0u;
    hdr.IDE                = CAN_ID_STD;
    hdr.RTR                = CAN_RTR_DATA;
    hdr.DLC                = (f->dlc > 8u) ? 8u : f->dlc;
    hdr.TransmitGlobalTime = DISABLE;

    for (uint8_t i = 0u; i < 8u; i++) {
        data[i] = f->data[i];
    }
    return HAL_CAN_AddTxMessage(HCAN, &hdr, data, &mailbox) == HAL_OK;
}

uint32_t can_tx_overrun(void)
{
    return s_tx_overrun;
}
#endif /* CAN_RX_ONLY */

/* ------------------------------------------------------------------------- */
/* RX                                                                        */
/* ------------------------------------------------------------------------- */

/** RX FIFO0 interrupt: drain the hardware FIFO into the ring (ISR context). */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *h)
{
    CAN_RxHeaderTypeDef hdr;
    can_rx_item_t       it;

    if (h != HCAN) {
        return;
    }
    /* Always empty the FIFO, even if the ring is full, otherwise the pending
     * interrupt would re-fire forever. */
    while (HAL_CAN_GetRxFifoFillLevel(h, CAN_RX_FIFO0) > 0u) {
        if (HAL_CAN_GetRxMessage(h, CAN_RX_FIFO0, &hdr, it.f.data) != HAL_OK) {
            break;
        }
        s_rx_total++;
        if (hdr.IDE != CAN_ID_STD || hdr.RTR != CAN_RTR_DATA) {
            continue;
        }
        it.f.id  = hdr.StdId;
        it.f.dlc = (uint8_t)((hdr.DLC > 8u) ? 8u : hdr.DLC);
        it.ts    = HAL_GetTick();
        (void)rb_push(&s_rx_rb, &it);         /* drop counted inside rb_push */
    }
}

bool can_rx_pop_ts(can_frame_t *f, uint32_t *ts_ms)
{
    can_rx_item_t it;

    if (!rb_pop(&s_rx_rb, &it)) {
        return false;
    }
    *f = it.f;
    if (ts_ms != NULL) {
        *ts_ms = it.ts;
    }
    return true;
}

bool can_rx_pop(can_frame_t *f)
{
    return can_rx_pop_ts(f, NULL);
}

/* ------------------------------------------------------------------------- */
/* Statistics                                                                */
/* ------------------------------------------------------------------------- */
uint32_t can_rx_total(void) { return s_rx_total; }
uint16_t can_rx_drops(void) { return rb_drops(&s_rx_rb); }

uint8_t can_bus_err(void)
{
    uint32_t esr = HCAN->Instance->ESR;
    uint8_t  tec = (uint8_t)((esr >> 16) & 0xFFu);
    uint8_t  rec = (uint8_t)((esr >> 24) & 0xFFu);
    return (tec > rec) ? tec : rec;
}

uint8_t can_bus_off(void)
{
    return (HCAN->Instance->ESR & CAN_ESR_BOFF) ? 1u : 0u;
}
