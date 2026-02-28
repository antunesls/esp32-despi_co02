# Partial Refresh Example for DESPI-C02 Driver

This example validates the fast refresh capabilities of the DESPI-C02 driver for the GDEY042T81 E-Paper Display.

## How to run

1. Configure the correct pins for your board in `../../include/epd_gdey042t81.h` (or adapt them programmatically or via idf.py menuconfig if you implement Kconfig).
2. Run `idf.py build flash monitor`

## Expected Output

1. The display will initialize.
2. The screen will clear to pure white using `epd_clear_white()`
3. It will set a base map on the screen using a full refresh.
4. It will then switch to fast update mode (`epd_init_fast()`) and loop 5 times updating a counter using `epd_display_fast()`. This takes ~1.5 seconds per refresh.
5. The display will enter deep sleep.
