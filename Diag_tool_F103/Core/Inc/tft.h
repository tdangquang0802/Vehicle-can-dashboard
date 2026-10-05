/**
 * @file    tft.h
 * @brief   ST7735 1.8" (128x160) driver, landscape 160x128, SPI1 + DMA, no framebuffer.
 *          (DUI-01, DUI-07)
 */
#ifndef TFT_DRV_H
#define TFT_DRV_H

#include <stdint.h>

#define TFT_WIDTH  160u
#define TFT_HEIGHT 128u

#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8u) << 8) | (((g) & 0xFCu) << 3) | ((b) >> 3)))

void tft_init(void);
void tft_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t rgb565);
/** Draw text with the built-in 6x8 font (5x7 glyph + 1 column spacing). */
void tft_text(uint8_t x, uint8_t y, const char *s, uint16_t fg, uint16_t bg);

#endif /* TFT_DRV_H */
