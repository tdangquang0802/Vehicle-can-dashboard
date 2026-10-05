/**
 * @file    tick_hal.c
 * @brief   tick_ms() on top of the HAL tick (SysTick, 1 ms).
 *
 * CubeMX: SYS -> Timebase Source = SysTick (no RTOS in this design, SYS-03).
 */
#include "common.h"
#include "main.h"

uint32_t tick_ms(void)
{
    return HAL_GetTick();
}
