#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/gfx_rgb565.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ST7789V2 display via ESP-IDF esp_lcd panel driver.
 *
 * Sets up SPI bus, panel IO, and panel driver. Configures the display
 * for 240x135 RGB565 with the M5StickC Plus2 pinout.
 *
 * @return true on success, false on failure
 */
bool display_hal_init(void);

/**
 * @brief Deinitialize the display and free resources.
 */
void display_hal_deinit(void);

/**
 * @brief Push the RGB565 framebuffer to the display.
 *
 * This function is called from ui_render() to flush the framebuffer
 * to the physical display via SPI.
 *
 * @param fb Pointer to the gfx_fb_t framebuffer
 */
void display_hal_flush(const gfx_fb_t *fb);

/**
 * @brief Set display backlight brightness.
 *
 * @param brightness 0-255 (0 = off, 255 = max)
 */
void display_hal_set_backlight(uint8_t brightness);

/**
 * @brief Turn display on/off (sleep mode).
 *
 * @param on true to wake, false to sleep
 */
void display_hal_set_sleep(bool on);

#ifdef __cplusplus
}
#endif