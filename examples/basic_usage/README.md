# Basic Usage Example for DESPI-C02 Driver

This example validates the usage of the DESPI-C02 driver for the GDEY042T81 E-Paper Display.

## How to run

1. Configure the correct pins for your board in `../include/epd_gdey042t81.h` (or adapt them programmatically or via idf.py menuconfig if you implement Kconfig).
2. Run `idf.py build flash monitor`

## Expected Output

1. The display will initialize.
2. The screen will clear to pure white.
3. Text will be drawn to the internal framebuffer buffer representing:
   - "Hello, E-Paper!"
   - "ESP-IDF Component"
   - "DESPI-C02 Driver"
4. The buffer will be flushed to the display with `epd_display()`.
5. The display will enter deep sleep.

Enjoy using your E-Paper Display!
