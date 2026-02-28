#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "epd_gdey042t81.h"

static const char *TAG = "epd_partial";

static uint8_t image_buffer[EPD_ARRAY];

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing SPI for EPD...");
    esp_err_t ret = epd_spi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return;
    }

    /* 1. Full Screen initialization and Base Map */
    ESP_LOGI(TAG, "Waking up and initializing display...");
    epd_init();

    ESP_LOGI(TAG, "Clearing screen to White...");
    epd_clear_white();
    vTaskDelay(pdMS_TO_TICKS(1000)); // Delay between commands

    // Prepare full frame layout (Basemap)
    memset(image_buffer, 0xFF, sizeof(image_buffer)); // 0xFF = White
    epd_draw_string(image_buffer, "Partial Refresh Test", 10, 10, 2, true);
    epd_draw_string(image_buffer, "Counter:", 10, 50, 3, true);

    ESP_LOGI(TAG, "Sending base map to display...");
    epd_set_basemap(image_buffer);

    vTaskDelay(pdMS_TO_TICKS(2000)); // Show basemap for 2s

    /* 2. Start Partial Refresh Updates */
    ESP_LOGI(TAG, "Starting partial update sequence...");
    epd_init_fast();

    char count_str[16];
    for (int i = 1; i <= 5; i++) {
        ESP_LOGI(TAG, "Partial Refresh iteration: %d", i);

        // Clear only the partial area in buffer (example coordinates)
        // Note: For simple driver use, we usually clear and redraw over a small region or whole buffer for fast refresh
        // Here we clear a block where the text goes, or just rewrite everything if we are using fast refresh full-buffer.
        
        memset(image_buffer, 0xFF, sizeof(image_buffer)); // Redraw onto clean buffer for simplicity
        epd_draw_string(image_buffer, "Partial Refresh Test", 10, 10, 2, true);
        epd_draw_string(image_buffer, "Counter:", 10, 50, 3, true);
        
        snprintf(count_str, sizeof(count_str), "%d", i);
        epd_draw_string(image_buffer, count_str, 160, 50, 3, true);

        // Use fast display function (approx 1.5s instead of 3s)
        epd_display_fast(image_buffer);
        
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "Entering Deep Sleep to save power...");
    epd_sleep();

    ESP_LOGI(TAG, "Example finished.");
}
