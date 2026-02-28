# DESPI-C02 Driver for ESP-IDF

"Bare Metal" driver for the ESP-IDF framework designed to operate the **DESPI-C02** connection board.

**📌 IMPORTANT:** This driver has been exhaustively tested and works perfectly (Zero Static/Noise) specifically with the **Good Display GDEY042T81** (4.2" E-Paper display, 400x300 resolution, Black/White) using the internal **SSD1683** controller.
(The initialization sequence was based on the signal stability of the GxEPD2 project and adapted natively in C for the fast ESP-IDF APIs).

## Features
* **Full Refresh** (Clear the screen to white or black without ghosting)
* **Fast Refresh** (Fast update of ~1.5s without flashing the entire screen)
* **Partial Refresh** (Update in a specific memory window)
* Pure routines using `spi_device_polling_transmit` with micro-delays (`esp_rom_delay_us`) to stabilize the controller over the high speed of the ESP.
* Safe and optimized implementation of deep sleep with `epd_sleep()`.

## How to use in your ESP-IDF project
1. Include this directory in your build or install via the Component Registry
2. Import `#include "epd_gdey042t81.h"`
3. Call:
```c
epd_init();           // Starts the SPI bus and wakes up the screen
epd_clear_white();    // Clears the screen
epd_display(buffer);  // Sends your framebuffer (image/text)
epd_sleep();          // Puts the screen to sleep
```

## How to change Pins
The default pins are listed in `include/epd_gdey042t81.h`. Just change the defines in this header according to your CPU (Ex: ESP32-S3):
```c
#define EPD_PIN_MOSI    11
#define EPD_PIN_SCLK    12
#define EPD_PIN_CS      10
#define EPD_PIN_DC      47
#define EPD_PIN_RST     13
#define EPD_PIN_BUSY    21
```

## Installation via ESP Component Registry

```bash
idf.py add-dependency "antunesls/despi_c02_driver"
```

## Examples

The component repository contains practical examples for you to test quickly.

1. **Basic Usage (`examples/basic_usage`):**
Demonstrates full initialization (Full Refresh), clearing the screen and writing basic texts.

2. **Partial / Fast Refresh (`examples/partial_refresh`):**
Demonstrates how to use the fast update mode (~1.5s), setting up a `basemap` for the initial background and iterating a counter.

**How to run any example:**

```bash
cd examples/basic_usage
idf.py set-target esp32s3 # replace with your MCU
idf.py build flash monitor
```
