/**
 * @file    tft.c
 * @brief   ST7735 driver. SPI1 (18 MHz) with DMA1 Channel 3 for pixel data.
 *
 * No framebuffer: pixels are streamed row by row from a single 320-byte line
 * buffer (160 px x 2 bytes), which is shared by fill_rect and text rendering.
 *
 * CubeMX user labels: TFT_CS (PA4), TFT_DC (PB0), TFT_RST (PB1).
 */
#include "tft.h"
#include "app_config.h"
#include "common.h"
#include "main.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;
extern const uint8_t font5x7[95][5];

#define CS_LOW()    HAL_GPIO_WritePin(TFT_CS_GPIO_Port,  TFT_CS_Pin,  GPIO_PIN_RESET)
#define CS_HIGH()   HAL_GPIO_WritePin(TFT_CS_GPIO_Port,  TFT_CS_Pin,  GPIO_PIN_SET)
#define DC_CMD()    HAL_GPIO_WritePin(TFT_DC_GPIO_Port,  TFT_DC_Pin,  GPIO_PIN_RESET)
#define DC_DATA()   HAL_GPIO_WritePin(TFT_DC_GPIO_Port,  TFT_DC_Pin,  GPIO_PIN_SET)

#define DMA_WAIT_MS 20u

static uint8_t           s_line[TFT_WIDTH * 2u];     /* one display row, RGB565 big-endian */
static volatile bool     s_dma_done;

/* ------------------------------------------------------------------------- */
/* Low-level SPI helpers                                                     */
/* ------------------------------------------------------------------------- */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1) {
        s_dma_done = true;
    }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1) {
        s_dma_done = true;           /* never leave the UI waiting on a dead DMA */
    }
}

/** Send a block through DMA and wait for completion (CPU idle, ISRs still run). */
static void spi_tx_dma(const uint8_t *p, uint16_t n)
{
    s_dma_done = false;
    if (HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *)p, n) != HAL_OK) {
        return;
    }
    uint32_t t0 = tick_ms();
    while (!s_dma_done && (uint32_t)(tick_ms() - t0) < DMA_WAIT_MS) {
        /* wait */
    }
}

static void wr_cmd(uint8_t c)
{
    DC_CMD();
    (void)HAL_SPI_Transmit(&hspi1, &c, 1u, 10u);
    DC_DATA();
}

static void wr_data(const uint8_t *d, uint16_t n)
{
    (void)HAL_SPI_Transmit(&hspi1, (uint8_t *)d, n, 10u);
}

/** Command followed by 0..n parameter bytes (CS handled here). */
static void cmd_args(uint8_t c, const uint8_t *args, uint8_t n)
{
    CS_LOW();
    wr_cmd(c);
    if (n > 0u) {
        wr_data(args, n);
    }
    CS_HIGH();
}

static void delay_ms(uint32_t ms)
{
    uint32_t t0 = tick_ms();
    while ((uint32_t)(tick_ms() - t0) < ms) {
        wdg_kick();                  /* init takes ~0.5 s, keep the IWDG happy */
    }
}

/** CASET / RASET / RAMWR for the rectangle; leaves CS low, DC = data. */
static void set_window(uint8_t x, uint8_t y, uint8_t w, uint8_t h)
{
    uint16_t x0 = (uint16_t)(x + TFT_X_OFFSET), x1 = (uint16_t)(x0 + w - 1u);
    uint16_t y0 = (uint16_t)(y + TFT_Y_OFFSET), y1 = (uint16_t)(y0 + h - 1u);
    uint8_t  a[4];

    CS_LOW();
    wr_cmd(0x2A);                                   /* CASET */
    a[0] = (uint8_t)(x0 >> 8); a[1] = (uint8_t)x0; a[2] = (uint8_t)(x1 >> 8); a[3] = (uint8_t)x1;
    wr_data(a, 4u);
    wr_cmd(0x2B);                                   /* RASET */
    a[0] = (uint8_t)(y0 >> 8); a[1] = (uint8_t)y0; a[2] = (uint8_t)(y1 >> 8); a[3] = (uint8_t)y1;
    wr_data(a, 4u);
    wr_cmd(0x2C);                                   /* RAMWR */
}

/* ------------------------------------------------------------------------- */
/* Public API                                                                */
/* ------------------------------------------------------------------------- */
void tft_init(void)
{
    /* hardware reset */
    CS_HIGH();
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET);
    delay_ms(20u);
    HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET);
    delay_ms(120u);

    cmd_args(0x01, NULL, 0u);  delay_ms(150u);                    /* SWRESET */
    cmd_args(0x11, NULL, 0u);  delay_ms(150u);                    /* SLPOUT  */

    { static const uint8_t a[] = {0x01, 0x2C, 0x2D};               cmd_args(0xB1, a, 3u); } /* FRMCTR1 */
    { static const uint8_t a[] = {0x01, 0x2C, 0x2D};               cmd_args(0xB2, a, 3u); } /* FRMCTR2 */
    { static const uint8_t a[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}; cmd_args(0xB3, a, 6u); } /* FRMCTR3 */
    { static const uint8_t a[] = {0x07};                           cmd_args(0xB4, a, 1u); } /* INVCTR  */
    { static const uint8_t a[] = {0xA2, 0x02, 0x84};               cmd_args(0xC0, a, 3u); } /* PWCTR1  */
    { static const uint8_t a[] = {0xC5};                           cmd_args(0xC1, a, 1u); } /* PWCTR2  */
    { static const uint8_t a[] = {0x0A, 0x00};                     cmd_args(0xC2, a, 2u); } /* PWCTR3  */
    { static const uint8_t a[] = {0x8A, 0x2A};                     cmd_args(0xC3, a, 2u); } /* PWCTR4  */
    { static const uint8_t a[] = {0x8A, 0xEE};                     cmd_args(0xC4, a, 2u); } /* PWCTR5  */
    { static const uint8_t a[] = {0x0E};                           cmd_args(0xC5, a, 1u); } /* VMCTR1  */
    cmd_args(0x20, NULL, 0u);                                                              /* INVOFF  */
    { static const uint8_t a[] = {TFT_MADCTL_LANDSCAPE};           cmd_args(0x36, a, 1u); } /* MADCTL  */
    { static const uint8_t a[] = {0x05};                           cmd_args(0x3A, a, 1u); } /* COLMOD: 16 bit */
    { static const uint8_t a[] = {0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,
                                  0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10};
      cmd_args(0xE0, a, 16u); }                                                            /* GMCTRP1 */
    { static const uint8_t a[] = {0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,
                                  0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10};
      cmd_args(0xE1, a, 16u); }                                                            /* GMCTRN1 */
    cmd_args(0x13, NULL, 0u);  delay_ms(10u);                     /* NORON   */
    cmd_args(0x29, NULL, 0u);  delay_ms(100u);                    /* DISPON  */

    tft_fill_rect(0u, 0u, TFT_WIDTH, TFT_HEIGHT, 0x0000u);
}

void tft_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t rgb565)
{
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT || w == 0u || h == 0u) {
        return;
    }
    if ((uint16_t)x + w > TFT_WIDTH) { w = (uint8_t)(TFT_WIDTH - x); }      /* clip */
    if ((uint16_t)y + h > TFT_HEIGHT) { h = (uint8_t)(TFT_HEIGHT - y); }

    uint8_t hi = (uint8_t)(rgb565 >> 8), lo = (uint8_t)rgb565;
    for (uint8_t i = 0u; i < w; i++) {
        s_line[2u * i]      = hi;
        s_line[2u * i + 1u] = lo;
    }
    set_window(x, y, w, h);
    for (uint8_t r = 0u; r < h; r++) {
        spi_tx_dma(s_line, (uint16_t)(2u * w));                       /* one row per DMA burst */
    }
    CS_HIGH();
}

void tft_text(uint8_t x, uint8_t y, const char *s, uint16_t fg, uint16_t bg)
{
    size_t len = strlen(s);
    if (x >= TFT_WIDTH || y > (TFT_HEIGHT - 8u) || len == 0u) {
        return;
    }
    size_t max_chars = (TFT_WIDTH - x) / 6u;                              /* clip to the screen */
    if (len > max_chars) {
        len = max_chars;
    }
    uint8_t w = (uint8_t)(len * 6u);

    uint8_t fh = (uint8_t)(fg >> 8), fl = (uint8_t)fg;
    uint8_t bh = (uint8_t)(bg >> 8), bl = (uint8_t)bg;

    set_window(x, y, w, 8u);
    for (uint8_t row = 0u; row < 8u; row++) {                         /* 8 pixel rows */
        uint16_t o = 0u;
        for (size_t c = 0u; c < len; c++) {
            char ch = s[c];
            if (ch < 0x20 || ch > 0x7E) {
                ch = '?';
            }
            const uint8_t *g = font5x7[(uint8_t)ch - 0x20u];
            for (uint8_t col = 0u; col < 5u; col++) {
                bool on = ((g[col] >> row) & 1u) != 0u;
                s_line[o++] = on ? fh : bh;
                s_line[o++] = on ? fl : bl;
            }
            s_line[o++] = bh;                                         /* spacing column */
            s_line[o++] = bl;
        }
        spi_tx_dma(s_line, o);
    }
    CS_HIGH();
}
