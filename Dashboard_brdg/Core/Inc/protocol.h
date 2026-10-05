/**
 * @file    protocol.h
 * @brief   Single source of truth for CAN IDs, OBD constants, DTC codes and the
 *          UART framing shared by ECU, Diag tool and Bridge.
 *
 * Design reference: System design v1.0, sections 2.3, 2.4, 2.7.
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

/* ---- CAN IDs (11-bit) -------------------------------------------------- */
#define CAN_ID_VEHICLE_STATE    0x100u  /* ECU  -> bus, every 20 ms  */
#define CAN_ID_VEHICLE_STATUS   0x101u  /* ECU  -> bus, every 100 ms */
#define CAN_ID_OBD_REQ_BCAST    0x7DFu  /* Diag -> ECU (functional)  */
#define CAN_ID_OBD_REQ_PHYS     0x7E0u  /* Diag -> ECU (physical)    */
#define CAN_ID_OBD_RESP         0x7E8u  /* ECU  -> Diag              */

/* ---- OBD-II (ISO 15765-4, single frame) -------------------------------- */
#define OBD_SID_CURRENT_DATA    0x01u
#define OBD_SID_STORED_DTC      0x03u
#define OBD_SID_CLEAR_DTC       0x04u
#define OBD_SID_PENDING_DTC     0x07u

#define OBD_PID_COOLANT         0x05u
#define OBD_PID_RPM             0x0Cu
#define OBD_PID_SPEED           0x0Du

#define OBD_POS_OFFSET          0x40u   /* positive response = SID + 0x40 */
#define OBD_NEG_RESP            0x7Fu
#define OBD_NRC_SERVICE_NOT_SUPPORTED   0x11u
#define OBD_NRC_CONDITIONS_NOT_CORRECT  0x22u
#define OBD_NRC_REQUEST_OUT_OF_RANGE    0x31u

/* ---- DTC codes (SAE J2012 2-byte form) --------------------------------- */
#define DTC_CODE_P0217          0x0217u
#define DTC_CODE_P0118          0x0118u
#define DTC_CODE_P0562          0x0562u
#define DTC_CODE_C0035          0x4035u /* top 2 bits: 00 = P, 01 = C */

/* ---- UART frame: AA 55 LEN MSG PAYLOAD CRC8 0A ------------------------- */
#define UART_SOF1               0xAAu
#define UART_SOF2               0x55u
#define UART_EOF                0x0Au
#define UART_MSG_CAN_RAW        0x20u   /* ts u32 | id u16 | dlc u8 | data[dlc] */
#define UART_MSG_STATS          0x21u   /* rx_total u32 | dropped u16 | bus_err u8 | bus_off u8 */

#endif /* PROTOCOL_H */
