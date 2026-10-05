/**
 * @file    wdg.c
 * @brief   IWDG helpers (NFR-05).  The IWDG itself is configured in CubeMX
 *          (timeout ~500 ms) and starts inside MX_IWDG_Init().
 */
#include "common.h"
#include "main.h"

extern IWDG_HandleTypeDef hiwdg;

void wdg_kick(void)
{
    (void)HAL_IWDG_Refresh(&hiwdg);
}

void wdg_freeze_in_debug(void)
{
#if defined(DEBUG)
    /* Without this, hitting a breakpoint would let the IWDG reset the MCU. */
    __HAL_DBGMCU_FREEZE_IWDG();
#endif
}
