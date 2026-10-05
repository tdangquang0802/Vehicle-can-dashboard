/**
 * @file    buttons.c
 * CubeMX user labels (GPIO input, pull-up): BTN_LIVE (PB12), BTN_READ (PB13), BTN_CLEAR (PB14).
 */
#include "buttons.h"
#include "main.h"

typedef struct { GPIO_TypeDef *port; uint16_t pin; } btn_hw_t;

static const btn_hw_t k_hw[3] = {
    { BTN_LIVE_GPIO_Port,  BTN_LIVE_Pin  },
    { BTN_READ_GPIO_Port,  BTN_READ_Pin  },
    { BTN_CLEAR_GPIO_Port, BTN_CLEAR_Pin },
};

static uint8_t s_last_raw;   /* previous sample, bit i = button i pressed */
static uint8_t s_stable;     /* debounced state                           */
static uint8_t s_events;     /* press edges not yet reported              */

btn_t buttons_poll_10ms(void)
{
    uint8_t raw = 0u;

    for (uint8_t i = 0u; i < 3u; i++) {
        if (HAL_GPIO_ReadPin(k_hw[i].port, k_hw[i].pin) == GPIO_PIN_RESET) {
            raw |= (uint8_t)(1u << i);
        }
    }
    /* A bit is accepted only where this sample equals the previous one. */
    uint8_t agree      = (uint8_t)~(raw ^ s_last_raw);
    uint8_t new_stable = (uint8_t)((s_stable & ~agree) | (raw & agree));

    s_events |= (uint8_t)(new_stable & ~s_stable);       /* released -> pressed */
    s_stable  = new_stable;
    s_last_raw = raw;

    for (uint8_t i = 0u; i < 3u; i++) {
        if (s_events & (1u << i)) {
            s_events &= (uint8_t)~(1u << i);
            return (btn_t)(BTN_LIVE + i);
        }
    }
    return BTN_NONE;
}
