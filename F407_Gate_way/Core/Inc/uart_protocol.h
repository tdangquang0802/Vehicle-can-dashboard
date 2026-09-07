/**
  ******************************************************************************
  * @file    uart_protocol.h
  * @brief   UART frame definition from the Gateway board up to the PC (Qt dashboard).
  *
  * Frame layout (byte order as transmitted on the wire):
  *
  *   [0]        SOF1        = 0xAA
  *   [1]        SOF2        = 0x55
  *   [2]        LEN         = number of PAYLOAD bytes (header/CRC/EOF excluded)
  *   [3]        MSG_ID      = see UART_MsgId_t
  *   [4..4+LEN-1] PAYLOAD   = little-endian packed struct matching MSG_ID
  *   [4+LEN]    CRC8        = CRC8_Calc() over [MSG_ID, PAYLOAD...] only
  *   [5+LEN]    EOF         = 0x0A
  *
  *   Total frame length = LEN + 6 bytes.
  *
  * The receiving side (Qt/C++) SHOULD parse this as a small state machine
  * (wait SOF1 -> SOF2 -> read LEN -> read LEN+2 remaining bytes -> check CRC
  * -> check EOF) so it can resynchronize automatically if a byte is dropped
  * on the wire.
  *
  * CRC8: polynomial 0x07, initial value 0x00, no bit reflection, no final
  * XOR (equivalent to the standard CRC-8/SMBUS).
  ******************************************************************************
  */
#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>

#define UART_SOF1   0xAAu
#define UART_SOF2   0x55u
#define UART_EOF    0x0Au

typedef enum {
    PC_MSG_CYCLE_SNAPSHOT = 0x01, /* Gateway -> PC : latest simulated vehicle-cycle sample */
    PC_MSG_LINK_STATUS    = 0x02, /* Gateway -> PC : link status with SIM board (reserved) */
    PC_MSG_FAULT          = 0x03  /* Gateway -> PC : forwarded fault report                */
} UART_MsgId_t;

/* PC_MSG_CYCLE_SNAPSHOT : 9-byte payload.
 * Gateway sends this either immediately when a new CAN frame arrives, or
 * periodically (every UART_PUSH_INTERVAL_MS) if there is nothing new. */
typedef struct {
    uint16_t cycle_time_s;    /* offset 0-1 : elapsed time within the cycle loop, seconds */
    uint16_t speed_x10_kmh;   /* offset 2-3 : simulated speed, km/h * 10                  */
    uint16_t rpm;             /* offset 4-5 : equivalent wheel RPM                        */
    uint8_t  phase;           /* offset 6   : 0=IDLE, 1=ACCEL, 2=CRUISE, 3=DECEL          */
    uint8_t  data_valid;      /* offset 7   : 1 = fresh (< UART_STALE_TIMEOUT_MS)         */
    uint8_t  reserved;        /* offset 8   : always 0x00                                 */
} __attribute__((packed)) UART_CycleSnapshot_t;

/* PC_MSG_LINK_STATUS : 5-byte payload (reserved, not sent by current firmware) */
typedef struct {
    uint8_t  node_online;
    uint32_t last_seen_ms;
} __attribute__((packed)) UART_LinkStatus_t;

/* PC_MSG_FAULT : 1-byte payload, forwarded directly from CAN_FaultPayload_t */
typedef struct {
    uint8_t fault_code;
} __attribute__((packed)) UART_FaultPayload_t;

/* ==========================================================================
 * TIMING
 * ========================================================================== */
#define UART_PUSH_INTERVAL_MS   500u   /* periodic push interval, even with no new data */
#define UART_STALE_TIMEOUT_MS   3000u  /* no fresh CAN data for this long -> mark "not valid" */
#define UART_BAUDRATE           115200u

/* ==========================================================================
 * CRC8 (poly 0x07, init 0x00, no reflection, no final XOR)
 * ========================================================================== */
static inline uint8_t CRC8_Calc(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0x00;
    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            if (crc & 0x80) { crc = (uint8_t)((crc << 1) ^ 0x07); }
            else            { crc = (uint8_t)(crc << 1); }
        }
    }
    return crc;
}

#endif /* UART_PROTOCOL_H */
