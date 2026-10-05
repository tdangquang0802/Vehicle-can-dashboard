/**
 * @file    sim.h
 * @brief   Vehicle simulation: speed/rpm cycle, ADC sensors, wheel-sensor button.
 *          (ECU-01)
 */
#ifndef SIM_H
#define SIM_H

#include "common.h"

#define ENGINE_OFF    0u
#define ENGINE_CRANK  1u
#define ENGINE_RUN    2u

typedef struct {
    uint16_t speed_x10;          /* 0.1 km/h                                   */
    uint16_t rpm;
    uint8_t  coolant_raw;        /* degC + 40  (0..180 -> -40..140 degC)       */
    uint8_t  throttle;           /* %                                          */
    uint8_t  vbat_x10;           /* 0.1 V      (60..160 -> 6.0..16.0 V)        */
    uint8_t  engine_state;       /* ENGINE_OFF / CRANK / RUN                   */
    bool     wheel_sensor_fault; /* push button held (PB0 == 0)                */
    uint16_t coolant_adc;        /* 0..4095, after the 8-sample moving average */
} vehicle_t;

void             sim_init(void);
void             sim_update_10ms(void);   /* call every 10 ms from the scheduler */
const vehicle_t *sim_get(void);

#endif /* SIM_H */
