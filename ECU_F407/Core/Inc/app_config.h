/**
 * @file    app_config.h  (Vehicle ECU - STM32F407)
 * @brief   Node-level configuration. Pure #defines, no HAL includes.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* CubeMX handle name of CAN1 on the F407 */
#define APP_CAN_HANDLE      hcan1
#define CAN_RX_RING_SIZE    16u

/* ECU-08: reject Service 04 (clear DTC) with NRC 0x22 while speed > 0.
 * Set to 0 to allow clearing at any time (handy for demos, see risk 2.10-1). */
#define ECU08_ENABLED       1

/* Engine speed model: rpm = speed[km/h] * RPM_PER_KMH (+ idle floor when running).
 * Replace with the constant of your existing simulation if it differs. */
#define RPM_PER_KMH         40u
#define RPM_IDLE            0u

#endif /* APP_CONFIG_H */
