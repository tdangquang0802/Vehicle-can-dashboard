/**
 * @file    app_config.h  (Dashboard bridge - STM32F103)
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_CAN_HANDLE      hcan
#define CAN_RX_RING_SIZE    32u     /* BRG-03: no frame loss at ~70 frame/s */

/* SYS-04: the bridge only listens (ACKs frames) and NEVER sends.
 * Defining CAN_RX_ONLY removes can_tx() from can_drv, so any attempt to call it
 * fails at link time instead of silently disturbing the bus. */
#define CAN_RX_ONLY         1

#endif /* APP_CONFIG_H */
