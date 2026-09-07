# STM32 Automotive CAN Gateway & Cycle Simulator

A two-node automotive embedded communication project based on **STM32F407**, **FreeRTOS**, **CAN**, and **UART**.

The project simulates vehicle operating data on one STM32 board, transfers the data over CAN to a Gateway board, and forwards the latest vehicle status to a PC application through UART.

---

## 1. Project Overview

The system consists of two STM32F407 boards:

- **Cycle Simulator** — generates simulated vehicle speed, RPM, cycle time, and operating phase.
- **CAN Gateway** — receives vehicle data over CAN, sends an ACK/NACK response, aggregates the latest data, and forwards it to a PC via UART.

### System Architecture

```text
                 CAN Bus
        500 kbit/s, Standard ID
 ┌──────────────────────┐
 │  STM32F407           │
 │  Cycle Simulator     │
 │                      │
 │  FreeRTOS            │
 │  Vehicle Cycle Task  │
 │  CAN TX/RX           │
 └──────────┬───────────┘
            │
            │ Vehicle Cycle
            │ CAN ID: 0x088
            ▼
 ┌──────────────────────┐
 │  STM32F407           │
 │  CAN Gateway         │
 │                      │
 │  FreeRTOS            │
 │  CAN RX Task         │
 │  UART TX Task        │
 │  Data Aggregation    │
 └──────────┬───────────┘
            │
            │ UART 115200 8N1
            │ Binary Protocol
            ▼
 ┌──────────────────────┐
 │  PC / Qt Dashboard    │
 │                      │
 │  UART Frame Parser    │
 │  Vehicle Monitoring  │
 └──────────────────────┘
```

---

## 2. Features

### Cycle Simulator

- Simulates a predefined vehicle driving cycle.
- Generates:
  - Vehicle speed
  - Wheel RPM
  - Cycle time
  - Driving phase
- Sends vehicle data over CAN every 1 second.
- Waits for Gateway ACK.
- Supports CAN transmission retry mechanism.
- Reports communication failure through a CAN fault message.
- Uses FreeRTOS for task scheduling and synchronization.

### CAN Gateway

- Receives CAN messages using interrupt-driven RX.
- Uses a FreeRTOS queue to transfer CAN data from ISR to a processing task.
- Validates and processes vehicle-cycle messages.
- Sends CAN ACK/NACK responses.
- Maintains the latest vehicle-cycle snapshot.
- Forwards vehicle data to a PC through UART.
- Detects stale CAN data.
- Forwards communication faults to the PC.

### PC Interface

The Gateway uses a lightweight binary UART protocol designed for a Qt/C++ dashboard.

The protocol includes:

- Start-of-frame detection
- Message ID
- Payload length
- Packed payload
- CRC-8 validation
- End-of-frame marker

---

## 3. Hardware

### Required Hardware

| Component | Quantity | Description |
|---|---:|---|
| STM32F407 board | 2 | Simulator and Gateway |
| CAN transceiver | 2 | CAN physical layer |
| USB-UART interface | 1 | Gateway-to-PC communication |
| CAN bus wiring | 1 | CANH, CANL and GND |
| 120 Ω termination resistor | 2 | CAN bus termination |

### CAN Bus

The two boards communicate using:

```text
CANH ───────────────── CANH
CANL ───────────────── CANL
GND  ───────────────── GND
```

A 120 Ω termination resistor should be installed at each physical end of the CAN bus.

With both boards powered off, the resistance measured between CANH and CANL should normally be approximately:

```text
60 Ω
```

---

## 4. Software Environment

### Development Environment

- STM32CubeIDE 1.19.x
- GNU Arm Embedded Toolchain
- STM32 HAL
- FreeRTOS Kernel
- C language

### Target MCU

```text
STM32F407
ARM Cortex-M4
```

### Communication

| Interface | Configuration |
|---|---|
| CAN | 500 kbit/s |
| CAN Frame | Standard ID, 11-bit |
| UART | 115200 baud |
| UART Format | 8-N-1 |

---

## 5. Repository Structure

A typical project structure is:

```text
.
├── CycleSimulator/
│   ├── Core/
│   │   ├── Inc/
│   │   └── Src/
│   ├── FreeRTOS/
│   └── ...
│
├── Gateway/
│   ├── Core/
│   │   ├── Inc/
│   │   └── Src/
│   ├── FreeRTOS/
│   └── ...
│
├── Common/
│   └── can_protocol.h
│
└── README.md
```

The exact directory structure may depend on the STM32CubeIDE project configuration.

---

## 6. CAN Protocol

The CAN identifier is organized as follows:

```text
10 9 8 7 | 6 5 4 3 | 2 1 0
  Node    Message      Sequence
            Type
```

### Node IDs

| Node | Value |
|---|---:|
| Broadcast | `0x0` |
| Simulator | `0x1` |
| Gateway | `0x2` |
| PC Tool | `0xF` |

### Message Types

| Message | Value | Description |
|---|---:|---|
| HEARTBEAT | `0x0` | Heartbeat |
| VEHICLE_CYCLE | `0x1` | Vehicle cycle data |
| ACK | `0x2` | Positive acknowledgement |
| NACK | `0x3` | Negative acknowledgement |
| SIM_FAULT | `0x4` | Simulator fault |

### Main CAN IDs

| CAN ID | Direction | Description |
|---|---|---|
| `0x088` | Simulator → Gateway | Vehicle cycle |
| `0x0A0` | Simulator → Gateway | Simulator fault |
| `0x080` | Simulator → Gateway | Simulator heartbeat |
| `0x110` | Gateway → Simulator | ACK |
| `0x118` | Gateway → Simulator | NACK |
| `0x100` | Gateway → Simulator | Gateway heartbeat |

---

## 7. Vehicle Cycle Payload

The vehicle-cycle CAN payload is 8 bytes:

```text
Byte 0     Sequence
Byte 1-2   Cycle time [s]
Byte 3-4   Speed × 10 [km/h]
Byte 5-6   Wheel RPM
Byte 7     Driving phase
```

All multi-byte integer values are transmitted in **little-endian** format.

### Driving Phases

| Value | Phase |
|---:|---|
| `0` | IDLE |
| `1` | ACCEL |
| `2` | CRUISE |
| `3` | DECEL |

---

## 8. Vehicle Driving Cycle

The simulator uses the following predefined cycle:

| Time [s] | Speed [km/h] | Phase |
|---:|---:|---|
| 0 | 0 | IDLE |
| 5 | 0 | IDLE |
| 20 | 50 | ACCEL |
| 35 | 50 | CRUISE |
| 50 | 90 | ACCEL |
| 70 | 90 | CRUISE |
| 85 | 0 | DECEL |
| 95 | 0 | IDLE |

The simulator interpolates the speed linearly between waypoints.

The cycle repeats after 95 seconds.

Wheel RPM is calculated from vehicle speed using the configured wheel circumference:

```text
Wheel circumference = 1.885 m
```

```text
RPM = (Speed[km/h] × 1000 / 60) / Wheel circumference
```

---

## 9. CAN ACK / Retry Mechanism

The Simulator requires an ACK from the Gateway after each vehicle-cycle transmission.

Current configuration:

```text
ACK timeout = 200 ms
Maximum retries = 3
```

The communication sequence is:

```text
Simulator                  Gateway
    │                         │
    │── Vehicle Cycle ───────>│
    │                         │
    │<──────── ACK ───────────│
    │                         │
    │       Next cycle        │
```

If the ACK is not received within the timeout:

```text
TX
 │
 ├── Timeout
 │
 ├── Retry #2
 │
 ├── Timeout
 │
 ├── Retry #3
 │
 └── Failure
```

After all retries fail, the Simulator sends a `SIM_FAULT` CAN message.

---

## 10. UART Protocol

The Gateway communicates with the PC using a binary frame.

### Frame Format

```text
+------+------+-----+--------+---------+------+-----+
| SOF1 | SOF2 | LEN | MSG_ID | PAYLOAD | CRC8 | EOF |
+------+------+-----+--------+---------+------+-----+
|  AA  |  55  |     |        |         |      | 0A  |
+------+------+-----+--------+---------+------+-----+
```

### Frame Definition

```text
SOF1    = 0xAA
SOF2    = 0x55
LEN     = Payload length
MSG_ID  = Message identifier
CRC8    = CRC-8 over MSG_ID + PAYLOAD
EOF     = 0x0A
```

The total frame size is:

```text
LEN + 6 bytes
```

### CRC-8

```text
Polynomial : 0x07
Initial    : 0x00
Reflection  : None
Final XOR   : 0x00
```

---

## 11. UART Message Types

### Cycle Snapshot

```text
MSG_ID = 0x01
Payload length = 9 bytes
```

Payload:

```text
Byte 0-1   Cycle time [s]
Byte 2-3   Speed × 10 [km/h]
Byte 4-5   RPM
Byte 6     Phase
Byte 7     Data validity
Byte 8     Reserved
```

`data_valid`:

```text
0 = CAN data is stale
1 = CAN data is fresh
```

The Gateway considers CAN data stale after:

```text
3000 ms
```

### Fault Message

```text
MSG_ID = 0x03
Payload length = 1 byte
```

Payload:

```text
Byte 0 = Fault code
```

---

## 12. FreeRTOS Architecture

### Simulator

Main task:

```text
Vehicle Cycle Task
       │
       ├── Calculate vehicle state
       ├── Build CAN payload
       ├── Send CAN frame
       ├── Wait for ACK
       └── Print status to UART
```

CAN reception is handled through an interrupt callback.

A binary semaphore is used to notify the task when an ACK/NACK is received.

A mutex protects CAN transmission and ACK state.

### Gateway

```text
CAN RX Interrupt
       │
       ▼
FreeRTOS Queue
       │
       ▼
CAN RX Processing Task
       │
       ├── Parse CAN message
       ├── Update data aggregation
       ├── Send ACK
       └── Forward faults
       
New Data Semaphore
       │
       ▼
UART TX Task
       │
       ▼
UART Binary Frame
       │
       ▼
PC / Qt Dashboard
```

The CAN ISR performs minimal processing and pushes received frames into a FreeRTOS queue.

---

## 13. Build and Flash

### 1. Import the projects

Open the STM32CubeIDE workspace and import both:

```text
CycleSimulator
Gateway
```

### 2. Build

Build both projects:

```text
Project → Build Project
```

### 3. Flash

Flash the corresponding firmware to each STM32F407 board:

```text
Cycle Simulator firmware → Simulator board
Gateway firmware         → Gateway board
```

### 4. Connect the CAN bus

```text
Simulator CANH ───── Gateway CANH
Simulator CANL ───── Gateway CANL
Simulator GND  ───── Gateway GND
```

Make sure the CAN transceivers are correctly powered and terminated.

---

## 14. Expected Operation

After startup, the Gateway prints:

```text
F407 Gateway (FreeRTOS) starting...
```

The Simulator periodically generates vehicle-cycle data.

For example:

```text
[CYCLE] t=0s speed=0.0km/h rpm=0 phase=0
[CYCLE] t=5s speed=0.0km/h rpm=0 phase=0
[CYCLE] t=20s speed=50.0km/h rpm=442 phase=2
[CYCLE] t=50s speed=90.0km/h rpm=795 phase=2
```

The Gateway does **not** output these human-readable cycle messages. Its UART output is a binary protocol intended for the PC/Qt application.

---

## 15. Debugging

When communication does not work, check the system in the following order:

### CAN physical layer

- CANH connected to CANH
- CANL connected to CANL
- Common GND
- CAN transceivers correctly powered
- 120 Ω termination at both ends of the bus

### CAN configuration

Both boards must use:

```text
500 kbit/s
11-bit Standard ID
```

### Gateway RX

Verify:

```text
CAN1 RX FIFO0
CAN1_RX0_IRQn
HAL_CAN_IRQHandler()
HAL_CAN_RxFifo0MsgPendingCallback()
```

### FreeRTOS

Verify that:

```text
CAN RX ISR
    ↓
xQueueSendFromISR()
    ↓
CAN RX Task
```

is working correctly.

### UART

Use:

```text
115200 baud
8 data bits
No parity
1 stop bit
```

The Gateway UART stream is binary and should be decoded using the UART protocol rather than viewed directly in a text terminal.

---

## 16. Future Improvements

Potential extensions include:

- Qt/C++ PC dashboard
- CAN bus monitoring and diagnostics
- CAN heartbeat and node supervision
- Additional vehicle signals
- Fault injection and fault recovery
- CAN bus-off recovery
- UART command interface from PC to Gateway
- Configurable driving-cycle profiles
- Automated communication tests
- Hardware-in-the-loop testing
- Unit tests for CAN and UART protocol layers
- DBC-based CAN signal definitions

---

## 17. Project Goals

This project is intended as a practical embedded/automotive software exercise covering:

- STM32 firmware development
- ARM Cortex-M4
- FreeRTOS
- CAN communication
- UART communication
- Interrupt handling
- Inter-task communication
- Mutexes and semaphores
- Communication retry mechanisms
- Binary protocol design
- CRC implementation
- Fault handling
- Embedded debugging
- Gateway architecture
- PC-to-ECU communication

---

## License

This project is for educational and engineering development purposes.