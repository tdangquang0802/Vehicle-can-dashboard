/**
 * @file    sim.c
 * @brief   Vehicle simulation (ECU-01).
 *
 * Hardware used (labels set in CubeMX, see guideline):
 *   ADC1 scan, rank 1 = IN1 (PA1, coolant pot), rank 2 = IN4 (PA4, Vbat pot)
 *   WHEEL_BTN (PB0, pull-up): pressed = wheel-speed sensor fault
 */
#include "sim.h"
#include "app_config.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;

#define ADC_AVG_N      8u
#define ENGINE_CRANK_MS 1000u

/* ------------------------------------------------------------------------- */
/* 95 s drive cycle: IDLE / ACCEL / CRUISE / DECEL, one entry per phase.     */
/* !! Placeholder: replace the table with the one of your existing project. */
/* ------------------------------------------------------------------------- */
typedef enum { PH_IDLE, PH_ACCEL, PH_CRUISE, PH_DECEL } phase_t;

typedef struct {
    uint16_t ticks;      /* segment length in 10 ms ticks */
    uint16_t v0_x10;     /* start speed, 0.1 km/h         */
    uint16_t v1_x10;     /* end speed, 0.1 km/h           */
    phase_t  phase;
} cycle_seg_t;

static const cycle_seg_t s_cycle[] = {
    { 1000,   0,   0, PH_IDLE   },   /* 10 s */
    { 2000,   0, 600, PH_ACCEL  },   /* 20 s */
    { 2000, 600, 600, PH_CRUISE },   /* 20 s */
    { 1000, 600, 900, PH_ACCEL  },   /* 10 s */
    { 1000, 900, 900, PH_CRUISE },   /* 10 s */
    { 2000, 900,   0, PH_DECEL  },   /* 20 s */
    {  500,   0,   0, PH_IDLE   },   /*  5 s  -> total 95 s */
};
#define CYCLE_SEGS (sizeof(s_cycle) / sizeof(s_cycle[0]))

static const uint8_t s_throttle_by_phase[] = { 0u, 70u, 30u, 0u };

/* ------------------------------------------------------------------------- */
typedef struct {
    uint16_t buf[ADC_AVG_N];
    uint32_t sum;
    uint8_t  idx;
    bool     primed;
} avg_t;

static vehicle_t s_v;
static avg_t     s_avg_coolant, s_avg_vbat;
static uint8_t   s_seg;
static uint16_t  s_seg_t;
static uint32_t  s_t0;

/** Moving average over the last 8 samples; the first sample primes the window. */
static uint16_t avg_push(avg_t *a, uint16_t s)
{
    if (!a->primed) {
        for (uint8_t i = 0u; i < ADC_AVG_N; i++) {
            a->buf[i] = s;
        }
        a->sum    = (uint32_t)s * ADC_AVG_N;
        a->idx    = 0u;
        a->primed = true;
    } else {
        a->sum -= a->buf[a->idx];
        a->buf[a->idx] = s;
        a->sum += s;
        a->idx = (uint8_t)((a->idx + 1u) % ADC_AVG_N);
    }
    return (uint16_t)(a->sum / ADC_AVG_N);
}

/** One scan of the two ADC ranks (polling, a few microseconds). */
static bool adc_read2(uint16_t *coolant, uint16_t *vbat)
{
    bool ok;

    if (HAL_ADC_Start(&hadc1) != HAL_OK) {
        return false;
    }
    ok = (HAL_ADC_PollForConversion(&hadc1, 2) == HAL_OK);
    if (ok) {
        *coolant = (uint16_t)HAL_ADC_GetValue(&hadc1);
        ok = (HAL_ADC_PollForConversion(&hadc1, 2) == HAL_OK);
    }
    if (ok) {
        *vbat = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }
    (void)HAL_ADC_Stop(&hadc1);
    return ok;
}

/* ------------------------------------------------------------------------- */
void sim_init(void)
{
    uint16_t c = 2844u;   /* fallback ~85 degC  */
    uint16_t b = 2703u;   /* fallback ~12.7 V   */

    s_v = (vehicle_t){0};
    s_avg_coolant = (avg_t){0};
    s_avg_vbat    = (avg_t){0};
    s_seg = 0u;
    s_seg_t = 0u;
    s_t0 = tick_ms();

    (void)adc_read2(&c, &b);                 /* prime the filters so we do not    */
    s_v.coolant_adc = avg_push(&s_avg_coolant, c);   /* start from a false 0 value */
    (void)avg_push(&s_avg_vbat, b);
    s_v.engine_state = ENGINE_CRANK;
}

void sim_update_10ms(void)
{
    /* 1) Drive cycle: linear interpolation inside the current segment */
    const cycle_seg_t *seg = &s_cycle[s_seg];
    int32_t dv = (int32_t)seg->v1_x10 - (int32_t)seg->v0_x10;
    s_v.speed_x10 = (uint16_t)((int32_t)seg->v0_x10 + (dv * (int32_t)s_seg_t) / (int32_t)seg->ticks);
    s_v.throttle  = s_throttle_by_phase[seg->phase];
    if (++s_seg_t >= seg->ticks) {
        s_seg_t = 0u;
        s_seg = (uint8_t)((s_seg + 1u) % CYCLE_SEGS);
    }

    /* 2) rpm follows speed */
    s_v.rpm = (uint16_t)(((uint32_t)s_v.speed_x10 * RPM_PER_KMH) / 10u);
#if RPM_IDLE > 0u
    if (s_v.rpm < RPM_IDLE) {
        s_v.rpm = RPM_IDLE;
    }
#endif

    /* 3) ADC sensors, 8-sample moving average */
    uint16_t c, b;
    if (adc_read2(&c, &b)) {
        s_v.coolant_adc = avg_push(&s_avg_coolant, c);
        uint16_t vb = avg_push(&s_avg_vbat, b);

        /* coolant degC = -40 + adc*180/4095  ->  raw = degC + 40 = adc*180/4095 */
        s_v.coolant_raw = (uint8_t)(((uint32_t)s_v.coolant_adc * 180u) / 4095u);
        /* Vbat = 6.0 + adc*10/4095 [V]  ->  x10: 60 + adc*100/4095 */
        s_v.vbat_x10 = (uint8_t)(60u + ((uint32_t)vb * 100u) / 4095u);
    }

    /* 4) Wheel-speed sensor fault button (active low) */
    s_v.wheel_sensor_fault =
        (HAL_GPIO_ReadPin(WHEEL_BTN_GPIO_Port, WHEEL_BTN_Pin) == GPIO_PIN_RESET);

    /* 5) Engine state: CRANK for the first second after reset, then RUN */
    s_v.engine_state =
        ((uint32_t)(tick_ms() - s_t0) < ENGINE_CRANK_MS) ? ENGINE_CRANK : ENGINE_RUN;
}

const vehicle_t *sim_get(void)
{
    return &s_v;
}
