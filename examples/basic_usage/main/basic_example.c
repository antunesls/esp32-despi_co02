#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "epd_gdey042t81.h"

static const char *TAG = "epd_example";

// 400x300 B/W needs 400*300/8 = 15000 bytes
static uint8_t image_buffer[EPD_WIDTH * EPD_HEIGHT / 8];

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing SPI for EPD...");
    esp_err_t ret = epd_spi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return;
    }

    ESP_LOGI(TAG, "Waking up and initializing E-Paper Display...");
    epd_init();

    ESP_LOGI(TAG, "Clearing screen to White...");
    epd_clear_white();
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Clear buffer to white (value 0xFF for this default convention usually)
    memset(image_buffer, 0xFF, sizeof(image_buffer));

    ESP_LOGI(TAG, "Drawing some text on the buffer...");
    epd_draw_string(image_buffer, "Hello, E-Paper!", 10, 10, 2, true);
    epd_draw_string(image_buffer, "ESP-IDF Component", 10, 40, 2, true);
    epd_draw_string(image_buffer, "DESPI-C02 Driver", 10, 70, 3, true);

    ESP_LOGI(TAG, "Sending framebuffer to display...");
    epd_display(image_buffer);

    ESP_LOGI(TAG, "Entering Deep Sleep to save power...");
    epd_sleep();

    ESP_LOGI(TAG, "Example finished.");
}
