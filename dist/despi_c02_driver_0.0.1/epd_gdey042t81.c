/*
 * GDEY042T81 4.2" E-Paper Display Driver  –  ESP-IDF Implementation
 * Based on GxEPD2 by Jean-Marc Zingg (https://github.com/ZinggJM/GxEPD2)
 * File: GxEPD2_420_GDEY042T81.cpp  –  Version 1.6.0
 *
 * CS and DC are controlled MANUALLY per byte:
 *   CS_LOW → DC_LOW → SPI(cmd) → CS_HIGH        (for commands)
 *   CS_LOW → DC_HIGH → SPI(data) → CS_HIGH       (for data)
 *
 * Controller: SSD1683  |  400 × 300  B/W
 * SPI via DESPI-C02 connector board on ESP32-S3
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_rom_sys.h"

#include "epd_gdey042t81.h"
#include "font8x8.h"

static const char *TAG = "EPD";

/* ── Private state ──────────────────────────────────────────────────── */
static spi_device_handle_t s_spi = NULL;

/* ══════════════════════════════════════════════════════════════════════
 *  LOW-LEVEL SPI  –  Manual CS/DC control, matching Arduino byte order
 * ══════════════════════════════════════════════════════════════════════ */

/** Send one raw byte over SPI (no CS/DC handling). */
static void spi_write_byte(uint8_t val)
{
    spi_transaction_t t = {
        .length   = 8,
        .tx_data  = {val},
        .flags    = SPI_TRANS_USE_TXDATA,
    };
    spi_device_polling_transmit(s_spi, &t);
}

/** Send a buffer over SPI (no CS/DC handling). */
static void spi_write_buf(const uint8_t *buf, size_t len)
{
    /* For large buffers, send in chunks to avoid watchdog issues */
    const size_t CHUNK = 4096;
    size_t offset = 0;

    while (offset < len) {
        size_t chunk_len = (len - offset > CHUNK) ? CHUNK : (len - offset);
        spi_transaction_t t = {
            .length    = chunk_len * 8,
            .tx_buffer = buf + offset,
        };
        spi_device_polling_transmit(s_spi, &t);
        offset += chunk_len;
    }
}

/**
 * Send command byte – exactly matches Arduino EPD_W21_WriteCMD():
 *   CS_LOW → DC_LOW → SPI(cmd) → CS_HIGH
 */
static void epd_write_cmd(uint8_t cmd)
{
    gpio_set_level(EPD_PIN_DC, 0);   /* D/C = 0: command */
    gpio_set_level(EPD_PIN_CS, 0);
    esp_rom_delay_us(1);             /* Hold time before SPI */
    spi_write_byte(cmd);
    esp_rom_delay_us(1);             /* Hold time after SPI */
    gpio_set_level(EPD_PIN_CS, 1);
}

/**
 * Send single data byte – exactly matches Arduino EPD_W21_WriteDATA():
 *   CS_LOW → DC_HIGH → SPI(data) → CS_HIGH
 */
static void epd_write_data(uint8_t data)
{
    gpio_set_level(EPD_PIN_DC, 1);   /* D/C = 1: data */
    gpio_set_level(EPD_PIN_CS, 0);
    esp_rom_delay_us(1);
    spi_write_byte(data);
    esp_rom_delay_us(1);
    gpio_set_level(EPD_PIN_CS, 1);
}

/**
 * Send data buffer – bulk version for framebuffer transfer.
 *   CS_LOW → DC_HIGH → SPI(buf...) → CS_HIGH
 */
static void epd_write_data_buf(const uint8_t *buf, size_t len)
{
    gpio_set_level(EPD_PIN_DC, 1);
    gpio_set_level(EPD_PIN_CS, 0);
    esp_rom_delay_us(1);
    spi_write_buf(buf, len);
    esp_rom_delay_us(1);
    gpio_set_level(EPD_PIN_CS, 1);
}

/* ── Busy wait – matches Epaper_READBUSY() ──────────────────────────── */
static void epd_wait_busy(void)
{
    while (gpio_get_level(EPD_PIN_BUSY) != 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ── Hardware reset ─────────────────────────────────────────────────── */
static void epd_hw_reset(void)
{
    gpio_set_level(EPD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

/* ══════════════════════════════════════════════════════════════════════
 *  SPI BUS + GPIO INIT
 * ══════════════════════════════════════════════════════════════════════ */

esp_err_t epd_spi_init(void)
{
    /* GPIO: DC, RST, CS as output; BUSY as input */
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << EPD_PIN_DC) |
                        (1ULL << EPD_PIN_RST) |
                        (1ULL << EPD_PIN_CS),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);
    gpio_set_level(EPD_PIN_CS, 1);   /* CS idle HIGH */
    gpio_set_level(EPD_PIN_RST, 1);

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << EPD_PIN_BUSY),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_cfg);

    /* SPI bus – CS managed by GPIO (not SPI peripheral) */
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num     = EPD_PIN_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = EPD_PIN_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* SPI device – CS = -1 (we control CS manually via GPIO) */
    const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = EPD_SPI_SPEED_HZ,
        .mode           = EPD_SPI_MODE,
        .spics_io_num   = -1,            /* Manual CS! */
        .queue_size     = 1,
        .flags          = 0,             /* No special flags needed */
    };

    ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI add device failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

/* ══════════════════════════════════════════════════════════════════════
 *  UPDATE / POWER FUNCTIONS  –  from GxEPD2_420_GDEY042T81.cpp
 * ══════════════════════════════════════════════════════════════════════ */

static bool s_power_is_on  = false;
static bool s_use_fast     = true;   /* useFastFullUpdate – matches GxEPD2 default */

/* GxEPD2: _PowerOn() */
static void epd_power_on(void)
{
    if (!s_power_is_on) {
        epd_write_cmd(0x22);
        epd_write_data(0xE0);
        epd_write_cmd(0x20);
        epd_wait_busy();
    }
    s_power_is_on = true;
}

/* GxEPD2: _PowerOff() */
static void epd_power_off(void)
{
    if (s_power_is_on) {
        epd_write_cmd(0x22);
        epd_write_data(0x83);
        epd_write_cmd(0x20);
        epd_wait_busy();
    }
    s_power_is_on = false;
}

/* GxEPD2: _Update_Full() */
static void epd_update_full(void)
{
    epd_write_cmd(0x21);         /* Display Update Control */
    epd_write_data(0x40);        /* bypass RED as 0 */
    epd_write_data(0x00);        /* single chip application */
    if (s_use_fast) {
        epd_write_cmd(0x1A);     /* Write to temperature register */
        epd_write_data(0x6E);    /* 2024 version, ok for 2023 version */
        epd_write_cmd(0x22);
        epd_write_data(0xD7);
    } else {
        epd_write_cmd(0x22);
        epd_write_data(0xF7);
    }
    epd_write_cmd(0x20);
    epd_wait_busy();
    s_power_is_on = false;
}

/* GxEPD2: _Update_Part() */
static void epd_update_part(void)
{
    epd_write_cmd(0x21);         /* Display Update Control */
    epd_write_data(0x00);        /* RED normal */
    epd_write_data(0x00);        /* single chip application */
    epd_write_cmd(0x22);
    epd_write_data(0xFC);
    epd_write_cmd(0x20);
    epd_wait_busy();
    s_power_is_on = true;
}

/* GxEPD2: _setPartialRamArea() */
static void epd_set_ram_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    epd_write_cmd(0x11);         /* Data entry mode */
    epd_write_data(0x03);        /* x increase, y increase : normal mode */
    epd_write_cmd(0x44);
    epd_write_data(x / 8);
    epd_write_data((x + w - 1) / 8);
    epd_write_cmd(0x45);
    epd_write_data(y % 256);
    epd_write_data(y / 256);
    epd_write_data((y + h - 1) % 256);
    epd_write_data((y + h - 1) / 256);
    epd_write_cmd(0x4E);
    epd_write_data(x / 8);
    epd_write_cmd(0x4F);
    epd_write_data(y % 256);
    epd_write_data(y / 256);
}

/* ══════════════════════════════════════════════════════════════════════
 *  INIT  –  GxEPD2 _InitDisplay()
 * ══════════════════════════════════════════════════════════════════════ */

static void epd_init_display(void)
{
    vTaskDelay(pdMS_TO_TICKS(10));   /* 10 ms according to specs */
    epd_write_cmd(0x12);             /* SWRESET */
    vTaskDelay(pdMS_TO_TICKS(10));   /* 10 ms according to specs */

    epd_write_cmd(0x01);             /* Set MUX as 300 */
    epd_write_data(0x2B);
    epd_write_data(0x01);
    epd_write_data(0x00);

    epd_write_cmd(0x3C);             /* BorderWaveform */
    epd_write_data(0x01);

    epd_write_cmd(0x18);             /* Read built-in temperature sensor */
    epd_write_data(0x80);

    epd_set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);
}

esp_err_t epd_init(void)
{
    ESP_LOGI(TAG, "Init: full refresh mode (400x300) [GxEPD2]");

    if (s_spi == NULL) {
        esp_err_t ret = epd_spi_init();
        if (ret != ESP_OK) return ret;
    }

    epd_hw_reset();
    epd_init_display();
    s_use_fast = true;   /* default: use fast full update */
    return ESP_OK;
}

/* ── 180° rotation ──────────────────────────────────────────────────── */
esp_err_t epd_init_180(void)
{
    ESP_LOGI(TAG, "Init: 180° mode [GxEPD2]");

    if (s_spi == NULL) {
        esp_err_t ret = epd_spi_init();
        if (ret != ESP_OK) return ret;
    }

    epd_hw_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
    epd_write_cmd(0x12);
    vTaskDelay(pdMS_TO_TICKS(10));

    epd_write_cmd(0x01);
    epd_write_data(0x2B);
    epd_write_data(0x01);
    epd_write_data(0x00);

    epd_write_cmd(0x3C);
    epd_write_data(0x01);

    epd_write_cmd(0x18);
    epd_write_data(0x80);

    /* 180° rotation: X decreases, Y decreases */
    epd_write_cmd(0x11);
    epd_write_data(0x00);   /* x decrease, y decrease */
    epd_write_cmd(0x44);
    epd_write_data((EPD_WIDTH - 1) / 8);
    epd_write_data(0x00);
    epd_write_cmd(0x45);
    epd_write_data((EPD_HEIGHT - 1) % 256);
    epd_write_data((EPD_HEIGHT - 1) / 256);
    epd_write_data(0x00);
    epd_write_data(0x00);
    epd_write_cmd(0x4E);
    epd_write_data((EPD_WIDTH - 1) / 8);
    epd_write_cmd(0x4F);
    epd_write_data((EPD_HEIGHT - 1) % 256);
    epd_write_data((EPD_HEIGHT - 1) / 256);

    s_use_fast = true;
    return ESP_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  FAST INIT  –  same as epd_init but marks fast mode
 *  GxEPD2 handles fast vs normal in the update function, not init.
 * ══════════════════════════════════════════════════════════════════════ */

esp_err_t epd_init_fast(void)
{
    ESP_LOGI(TAG, "Init: fast refresh mode [GxEPD2]");

    if (s_spi == NULL) {
        esp_err_t ret = epd_spi_init();
        if (ret != ESP_OK) return ret;
    }

    epd_hw_reset();
    epd_init_display();
    s_use_fast = true;
    return ESP_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  DISPLAY  –  Send framebuffer to display
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Full refresh: write to RAM 0x26 (previous) + 0x24 (current), then update.
 * Matches GxEPD2 clearScreen() / writeImageForFullRefresh() pattern.
 */
esp_err_t epd_display(const uint8_t *buf)
{
    if (buf == NULL) return ESP_ERR_INVALID_ARG;

    epd_set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);

    epd_write_cmd(0x26);   /* set previous */
    epd_write_data_buf(buf, EPD_ARRAY);

    epd_write_cmd(0x24);   /* set current */
    epd_write_data_buf(buf, EPD_ARRAY);

    epd_update_full();
    return ESP_OK;
}

/** Fast refresh display – same as epd_display, GxEPD2 handles speed in update. */
esp_err_t epd_display_fast(const uint8_t *buf)
{
    return epd_display(buf);  /* s_use_fast flag controls the update speed */
}

/** Clear to white – GxEPD2 clearScreen(0xFF). */
esp_err_t epd_clear_white(void)
{
    uint8_t *buf = heap_caps_malloc(EPD_ARRAY, MALLOC_CAP_DMA);
    if (!buf) return ESP_ERR_NO_MEM;
    memset(buf, 0xFF, EPD_ARRAY);

    epd_set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);

    epd_write_cmd(0x26);
    epd_write_data_buf(buf, EPD_ARRAY);
    epd_write_cmd(0x24);
    epd_write_data_buf(buf, EPD_ARRAY);

    heap_caps_free(buf);
    epd_update_full();
    return ESP_OK;
}

/** Clear to black – GxEPD2 clearScreen(0x00). */
esp_err_t epd_clear_black(void)
{
    uint8_t *buf = heap_caps_malloc(EPD_ARRAY, MALLOC_CAP_DMA);
    if (!buf) return ESP_ERR_NO_MEM;
    memset(buf, 0x00, EPD_ARRAY);

    epd_set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);

    epd_write_cmd(0x26);
    epd_write_data_buf(buf, EPD_ARRAY);
    epd_write_cmd(0x24);
    epd_write_data_buf(buf, EPD_ARRAY);

    heap_caps_free(buf);
    epd_update_full();
    return ESP_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  PARTIAL REFRESH
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * Set base map for partial refresh – writes both buffers, full update.
 * Matches GxEPD2 writeScreenBufferAgain() + refresh(false) pattern.
 */
esp_err_t epd_set_basemap(const uint8_t *buf)
{
    if (buf == NULL) return ESP_ERR_INVALID_ARG;

    epd_set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);

    epd_write_cmd(0x26);
    epd_write_data_buf(buf, EPD_ARRAY);
    epd_write_cmd(0x24);
    epd_write_data_buf(buf, EPD_ARRAY);

    epd_update_full();
    return ESP_OK;
}

/**
 * Partial refresh – GxEPD2 drawImage() / refresh(x,y,w,h) pattern.
 */
esp_err_t epd_display_part(int x_start, int y_start,
                           const uint8_t *buf,
                           int part_column, int part_line)
{
    if (buf == NULL) return ESP_ERR_INVALID_ARG;

    /* Align to byte boundary */
    int w = part_line;
    w += x_start % 8;
    if (w % 8 > 0) w += 8 - w % 8;
    int x = x_start - (x_start % 8);

    epd_set_ram_area(x, y_start, w, part_column);

    epd_write_cmd(0x24);
    epd_write_data_buf(buf, part_column * part_line / 8);

    epd_update_part();
    return ESP_OK;
}

esp_err_t epd_display_part_all(const uint8_t *buf)
{
    if (buf == NULL) return ESP_ERR_INVALID_ARG;

    epd_set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);

    epd_write_cmd(0x24);
    epd_write_data_buf(buf, EPD_ARRAY);

    epd_update_part();
    return ESP_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  DEEP SLEEP
 * ══════════════════════════════════════════════════════════════════════ */

esp_err_t epd_sleep(void)
{
    ESP_LOGI(TAG, "Deep sleep (hibernate)");
    epd_power_off();
    epd_write_cmd(0x10);   /* deep sleep mode */
    epd_write_data(0x01);  /* enter deep sleep */
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

/* ══════════════════════════════════════════════════════════════════════
 *  FRAMEBUFFER HELPERS
 * ══════════════════════════════════════════════════════════════════════ */

void epd_set_pixel(uint8_t *buf, int x, int y, bool black)
{
    if (!buf || x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) return;

    int byte_idx = y * (EPD_WIDTH / 8) + (x / 8);
    uint8_t bit_mask = 0x80 >> (x % 8);

    if (black) {
        buf[byte_idx] &= ~bit_mask;   /* 0 = black */
    } else {
        buf[byte_idx] |= bit_mask;    /* 1 = white */
    }
}

void epd_draw_char(uint8_t *buf, char ch, int x, int y, int scale, bool black)
{
    if (ch < FONT8X8_FIRST_CHAR || ch > FONT8X8_LAST_CHAR) ch = ' ';
    const uint8_t *glyph = font8x8[ch - FONT8X8_FIRST_CHAR];

    for (int row = 0; row < FONT8X8_CHAR_H; row++) {
        uint8_t line = glyph[row];
        for (int col = 0; col < FONT8X8_CHAR_W; col++) {
            bool pixel_on = (line >> (7 - col)) & 1;
            bool draw_black = pixel_on ? black : !black;
            for (int sy = 0; sy < scale; sy++) {
                for (int sx = 0; sx < scale; sx++) {
                    epd_set_pixel(buf,
                                  x + col * scale + sx,
                                  y + row * scale + sy,
                                  draw_black);
                }
            }
        }
    }
}

void epd_draw_string(uint8_t *buf, const char *str, int x, int y, int scale, bool black)
{
    int cursor_x = x;
    int char_w = FONT8X8_CHAR_W * scale;

    while (*str) {
        epd_draw_char(buf, *str, cursor_x, y, scale, black);
        cursor_x += char_w;
        str++;
    }
}
