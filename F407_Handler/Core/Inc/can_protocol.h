/**
  ******************************************************************************
  * @file    can_protocol.h
  * @brief   Shared CAN protocol between the Cycle Simulator board and the
  *          Gateway board. Both boards are now STM32F407, so node naming is
  *          based on ROLE (SIM / GATEWAY), not on chip type - this avoids the
  *          confusing "F103_SIM on an F407 chip" naming from the previous
  *          revision of this project.
  *
  * IMPORTANT: copy the EXACT SAME file into both projects.
  *
  * Standard ID layout (11 bit, range 0x000 - 0x7FF):
  *      bit10 bit9 bit8 bit7 | bit6 bit5 bit4 bit3 | bit2 bit1 bit0
  *      |     NodeID (4b)   | |    MsgType (4b)   | |    Seq (3b)  |
  *
  * The 3-bit Seq field inside the ID is always left at 0. ACK/NACK matching
  * uses an explicit 1-byte "seq" field inside the payload instead.
  ******************************************************************************
  */
#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

/* ==========================================================================
 * NODE ID (4 bit, 0x0 - 0xF) - named by ROLE, not by chip type
 * ========================================================================== */
typedef enum {
    CAN_NODE_BROADCAST = 0x0,
    CAN_NODE_SIM        = 0x1, /* Generates the simulated vehicle-cycle signal */
    CAN_NODE_GATEWAY    = 0x2, /* Receives, acknowledges, forwards to PC       */
    CAN_NODE_PC_TOOL    = 0xF  /* Reserved for a direct USB-CAN PC tap         */
} CAN_NodeId_t;

/* ==========================================================================
 * MESSAGE TYPE (4 bit, 0x0 - 0xF)
 * ========================================================================== */
typedef enum {
    CAN_MSG_HEARTBEAT     = 0x0, /* Reserved for future link monitoring          */
    CAN_MSG_VEHICLE_CYCLE = 0x1, /* SIM -> GATEWAY : simulated speed/rpm/phase   */
    CAN_MSG_ACK           = 0x2, /* GATEWAY -> SIM : frame accepted and valid    */
    CAN_MSG_NACK          = 0x3, /* GATEWAY -> SIM : frame rejected, please resend */
    CAN_MSG_SIM_FAULT     = 0x4  /* SIM -> GATEWAY : send failed after max retries */
    /* 0x5 - 0xF : reserved for future extension */
} CAN_MsgType_t;

/* ==========================================================================
 * ID BUILD / PARSE MACROS
 * ========================================================================== */
#define CAN_MAKE_ID(node, type, seq)                                        \
    ( (((uint32_t)(node) & 0xFu) << 7) |                                    \
      (((uint32_t)(type) & 0xFu) << 3) |                                    \
      ( (uint32_t)(seq)  & 0x7u) )

#define CAN_GET_NODE(id)   ( ((uint32_t)(id) >> 7) & 0xFu )
#define CAN_GET_TYPE(id)   ( ((uint32_t)(id) >> 3) & 0xFu )
#define CAN_GET_SEQ(id)    (  (uint32_t)(id)        & 0x7u )

/* ==========================================================================
 * CONCRETE IDs
 * ========================================================================== */
#define CAN_ID_SIM_VEHICLE_CYCLE   CAN_MAKE_ID(CAN_NODE_SIM,     CAN_MSG_VEHICLE_CYCLE, 0) /* 0x088 */
#define CAN_ID_SIM_FAULT           CAN_MAKE_ID(CAN_NODE_SIM,     CAN_MSG_SIM_FAULT,     0) /* 0x0A0 */
#define CAN_ID_SIM_HEARTBEAT       CAN_MAKE_ID(CAN_NODE_SIM,     CAN_MSG_HEARTBEAT,     0) /* 0x080 */

#define CAN_ID_GATEWAY_ACK         CAN_MAKE_ID(CAN_NODE_GATEWAY, CAN_MSG_ACK,           0) /* 0x110 */
#define CAN_ID_GATEWAY_NACK        CAN_MAKE_ID(CAN_NODE_GATEWAY, CAN_MSG_NACK,          0) /* 0x118 */
#define CAN_ID_GATEWAY_HEARTBEAT   CAN_MAKE_ID(CAN_NODE_GATEWAY, CAN_MSG_HEARTBEAT,     0) /* 0x100 */

/* ==========================================================================
 * HARDWARE FILTERS
 * ========================================================================== */
#define CAN_FILTER_ID_FROM_SIM       ( (uint32_t)CAN_NODE_SIM     << 7 )
#define CAN_FILTER_ID_FROM_GATEWAY   ( (uint32_t)CAN_NODE_GATEWAY << 7 )
#define CAN_FILTER_MASK_NODE_ONLY    ( (uint32_t)0xFu << 7 )

/* ==========================================================================
 * PAYLOAD STRUCTS (DLC = 8 bytes / frame, packed)
 * ========================================================================== */

typedef enum {
    CYCLE_PHASE_IDLE   = 0,
    CYCLE_PHASE_ACCEL  = 1,
    CYCLE_PHASE_CRUISE = 2,
    CYCLE_PHASE_DECEL  = 3
} CAN_CyclePhase_t;

/* CAN_MSG_VEHICLE_CYCLE : SIM -> GATEWAY */
typedef struct {
    uint8_t  seq;               /* byte0 : increments per send, used for ACK matching */
    uint16_t cycle_time_s;      /* byte1-2: elapsed time within the current cycle loop, seconds */
    uint16_t speed_x10_kmh;     /* byte3-4: simulated vehicle speed, km/h * 10 (from lookup table) */
    uint16_t rpm;               /* byte5-6: equivalent wheel RPM derived from the simulated speed  */
    uint8_t  phase;             /* byte7 : see CAN_CyclePhase_t                                     */
} __attribute__((packed)) CAN_VehicleCyclePayload_t;

/* CAN_MSG_ACK / CAN_MSG_NACK : GATEWAY -> SIM */
typedef struct {
    uint8_t  msgtype_echo;      /* byte0 : echoes the MsgType of the acknowledged frame */
    uint8_t  seq_echo;          /* byte1 : echoes the seq of the acknowledged frame     */
    uint8_t  status;            /* byte2 : see CAN_AckStatus_t                          */
    uint8_t  reserved[5];       /* byte3-7 */
} __attribute__((packed)) CAN_AckPayload_t;

typedef enum {
    CAN_ACK_OK    = 0x00,
    CAN_ACK_ERROR = 0x01
} CAN_AckStatus_t;

/* CAN_MSG_SIM_FAULT : SIM -> GATEWAY */
typedef struct {
    uint8_t fault_code;         /* byte0 : 1 = send failed after max retries */
    uint8_t reserved[7];
} __attribute__((packed)) CAN_FaultPayload_t;

/* ==========================================================================
 * TIMING
 * ========================================================================== */
#define CAN_CYCLE_SAMPLE_INTERVAL_MS   1000u  /* how often the SIM board advances the cycle and sends a frame */
#define CAN_ACK_TIMEOUT_MS             200u   /* time to wait for ACK/NACK per frame */
#define CAN_MAX_RETRY                  3u     /* max resend attempts per frame before reporting a fault */

#endif /* CAN_PROTOCOL_H */
