/**
 * @file    app_config.h  (Diag tool - STM32F103 + ST7735 TFT)
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define APP_CAN_HANDLE      hcan
#define CAN_RX_RING_SIZE    16u

/* ST7735 panel tuning. If the picture is mirrored, rotated or shifted by 1-2 px,
 * adjust these two values (see the guideline, "Troubleshooting"). */
#define TFT_MADCTL_LANDSCAPE  0xA0u   /* MY | MV : landscape 160x128. Try 0x60 if mirrored. */
#define TFT_X_OFFSET          0u      /* green-tab panels usually need 1 or 2 */
#define TFT_Y_OFFSET          0u

#endif /* APP_CONFIG_H */
