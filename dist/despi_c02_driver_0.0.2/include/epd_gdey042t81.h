/*
 * GDEY042T81 4.2" E-Paper Display Driver
 * Based on GxEPD2 by Jean-Marc Zingg (https://github.com/ZinggJM/GxEPD2)
 * File: GxEPD2_420_GDEY042T81 – Version 1.6.0
 * Controller: SSD1683  |  Resolution: 400 × 300 (B/W)
 * Interface:  SPI via DESPI-C02 connector board
 * Target:     ESP32-S3 (ESP-IDF v5.5)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Display parameters ─────────────────────────────────────────────── */
#define EPD_WIDTH       400
#define EPD_HEIGHT      300
#define EPD_ARRAY       (EPD_WIDTH * EPD_HEIGHT / 8)  /* 15 000 bytes */

/* ── GPIO mapping  (ESP32-S3 SPI2 IO_MUX + GPIO matrix) ────────────── */
#define EPD_PIN_MOSI    11   /* SPI2 MOSI  (IO_MUX) */
#define EPD_PIN_SCLK    12   /* SPI2 SCLK  (IO_MUX) */
#define EPD_PIN_CS      10   /* SPI2 CS0   (IO_MUX) */
#define EPD_PIN_DC      47   /* Data / Command       */
#define EPD_PIN_RST     13   /* Hardware reset        */
#define EPD_PIN_BUSY    21   /* Busy flag (input)     */

/* ── SPI parameters ─────────────────────────────────────────────────── */
#define EPD_SPI_SPEED_HZ    (4 * 1000 * 1000)   /* 4 MHz (more stable for e-paper) */
#define EPD_SPI_MODE        0                     /* CPOL=0, CPHA=0    */

/* ══════════════════════════════════════════════════════════════════════
 *  PUBLIC API  –  matches official example capabilities
 * ══════════════════════════════════════════════════════════════════════ */

/* ── SPI bus init (call once) ───────────────────────────────────────── */
esp_err_t epd_spi_init(void);

/* ── Full screen refresh ────────────────────────────────────────────── */
esp_err_t epd_init(void);                              /* EPD_HW_Init()            */
esp_err_t epd_init_180(void);                           /* EPD_HW_Init_180()        */
esp_err_t epd_display(const uint8_t *buf);              /* EPD_WhiteScreen_ALL()    */
esp_err_t epd_clear_white(void);                        /* EPD_WhiteScreen_White()  */
esp_err_t epd_clear_black(void);                        /* EPD_WhiteScreen_Black()  */

/* ── Fast refresh (~1.5s) ───────────────────────────────────────────── */
esp_err_t epd_init_fast(void);                          /* EPD_HW_Init_Fast()       */
esp_err_t epd_display_fast(const uint8_t *buf);         /* EPD_WhiteScreen_ALL_Fast */

/* ── Partial refresh ────────────────────────────────────────────────── */
esp_err_t epd_set_basemap(const uint8_t *buf);          /* EPD_SetRAMValue_BaseMap  */
esp_err_t epd_display_part(int x_start, int y_start,
                           const uint8_t *buf,
                           int part_column, int part_line);  /* EPD_Dis_Part */
esp_err_t epd_display_part_all(const uint8_t *buf);     /* EPD_Dis_PartAll          */

/* ── Deep sleep ─────────────────────────────────────────────────────── */
esp_err_t epd_sleep(void);                              /* EPD_DeepSleep()          */

/* ── Framebuffer helpers ────────────────────────────────────────────── */
void epd_set_pixel(uint8_t *buf, int x, int y, bool black);
void epd_draw_char(uint8_t *buf, char ch, int x, int y, int scale, bool black);
void epd_draw_string(uint8_t *buf, const char *str, int x, int y, int scale, bool black);

#ifdef __cplusplus
}
#endif
