// display_hal.c – ST7789V2 display driver using ESP-IDF esp_lcd panel driver
// M5StickC Plus2: 240x135 RGB565, SPI interface

#include "hal/display_hal.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_st7789.h"

#include "board_pins.h"

static const char *TAG = "display_hal";

static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;
static bool s_initialized = false;
static uint8_t s_backlight_level = 255;
static bool s_backlight_pwm_initialized = false;

bool display_hal_init(void)
{
    if (s_initialized) {
        return true;
    }

    ESP_LOGI(TAG, "Initializing ST7789V2 display (240x135)");

    // 1. Configure backlight GPIO (GPIO 27) with LEDC PWM for brightness control
    // LEDC timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,  // 5 kHz PWM frequency
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&ledc_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "LEDC timer configured");

    // LEDC channel configuration - start with backlight OFF (duty=0)
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BOARD_LCD_BL_GPIO,
        .duty = 0,
        .hpoint = 0,
    };
    ret = ledc_channel_config(&ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(ret));
        return false;
    }
    s_backlight_pwm_initialized = true;
    ESP_LOGI(TAG, "LEDC channel configured on GPIO %d", BOARD_LCD_BL_GPIO);

    // 2. Configure reset GPIO (GPIO 12) - set to output, initially HIGH (not in reset)
    // Reset is active LOW according to board_pins.h
    gpio_config_t rst_cfg = {
        .pin_bit_mask = (1ULL << BOARD_LCD_RST_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&rst_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Reset GPIO config failed: %s", esp_err_to_name(ret));
        return false;
    }
    gpio_set_level(BOARD_LCD_RST_GPIO, 1);  // Release reset (active low)
    ESP_LOGI(TAG, "Reset GPIO %d configured and released", BOARD_LCD_RST_GPIO);

    // 3. Initialize SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = BOARD_LCD_MOSI_GPIO,
        .miso_io_num = -1,  // Not used for display output only
        .sclk_io_num = BOARD_LCD_CLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GFX_WIDTH * GFX_HEIGHT * sizeof(uint16_t),
    };
    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "SPI bus initialized (MOSI=%d, CLK=%d)", BOARD_LCD_MOSI_GPIO, BOARD_LCD_CLK_GPIO);

    // 4. Configure panel IO (SPI interface)
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = BOARD_LCD_DC_GPIO,
        .cs_gpio_num = BOARD_LCD_CS_GPIO,
        .pclk_hz = 40 * 1000 * 1000,  // 40 MHz SPI clock
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &s_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel IO create failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "Panel IO created (DC=%d, CS=%d)", BOARD_LCD_DC_GPIO, BOARD_LCD_CS_GPIO);

    // 5. Configure ST7789 panel
    // Note: M5StickC Plus2 ST7789V2 uses BGR color order (not RGB)
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_LCD_RST_GPIO,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,  // ST7789V2 on M5StickC uses BGR
        .bits_per_pixel = 16,  // RGB565
    };

    esp_lcd_panel_handle_t panel_handle = NULL;
    ret = esp_lcd_new_panel_st7789(s_io_handle, &panel_cfg, &panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel create failed: %s", esp_err_to_name(ret));
        return false;
    }
    s_panel_handle = panel_handle;
    ESP_LOGI(TAG, "ST7789 panel created");

    // 6. Initialize the panel (this does hardware reset + init sequence)
    ret = esp_lcd_panel_reset(s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel reset failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "Panel reset done");

    ret = esp_lcd_panel_init(s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel init failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "Panel init done");

    // 7. Configure display orientation and offsets for M5StickC Plus2
    // The ST7789V2 on M5StickC Plus2 is 240x135 (landscape orientation)
    // The controller is 240x240, we need to configure the visible window
    // Based on M5StickC Plus2 specs: 240x135, landscape
    ret = esp_lcd_panel_swap_xy(s_panel_handle, true);   // Swap X/Y for landscape
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel swap_xy failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "Panel swap_xy done");

    ret = esp_lcd_panel_mirror(s_panel_handle, true, false);  // Mirror X
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel mirror failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "Panel mirror done");

    // For 240x135 on 240x240 controller with swap_xy=true:
    // The visible area is 240x135, centered vertically
    // x_gap = 0, y_gap = (240 - 135) / 2 = 52
    ret = esp_lcd_panel_set_gap(s_panel_handle, 0, 52);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel set_gap failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "Panel set_gap done (0, 52)");

    // 8. Turn on display
    ret = esp_lcd_panel_disp_on_off(s_panel_handle, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel disp_on_off failed: %s", esp_err_to_name(ret));
        return false;
    }
    ESP_LOGI(TAG, "Display turned ON");

    // 9. Set backlight to default level (full brightness)
    display_hal_set_backlight(s_backlight_level);

    s_initialized = true;
    ESP_LOGI(TAG, "Display initialized successfully");

    return true;
}

void display_hal_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Deinitializing display");

    // Turn off backlight
    if (s_backlight_pwm_initialized) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        gpio_set_level(BOARD_LCD_BL_GPIO, 0);
    }

    // Turn off display
    if (s_panel_handle) {
        esp_lcd_panel_disp_on_off(s_panel_handle, false);
        esp_lcd_panel_del(s_panel_handle);
        s_panel_handle = NULL;
    }

    // Delete panel IO
    if (s_io_handle) {
        esp_lcd_panel_io_del(s_io_handle);
        s_io_handle = NULL;
    }

    // Free SPI bus
    spi_bus_free(SPI2_HOST);

    s_initialized = false;
    ESP_LOGI(TAG, "Display deinitialized");
}

void display_hal_flush(const gfx_fb_t *fb)
{
    if (!s_initialized || !fb) {
        return;
    }

    // Push the framebuffer to the display
    // The framebuffer is already in RGB565 format, matching the panel's 16-bit color
    esp_lcd_panel_draw_bitmap(s_panel_handle, 0, 0, GFX_WIDTH, GFX_HEIGHT, fb->pixels);
}

void display_hal_set_backlight(uint8_t brightness)
{
    s_backlight_level = brightness;

    if (!s_initialized) {
        return;
    }

    if (s_backlight_pwm_initialized) {
        // Use LEDC PWM for smooth brightness control (0-255)
        uint32_t duty = (uint32_t)brightness;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        // Fallback to GPIO on/off
        if (brightness == 0) {
            gpio_set_level(BOARD_LCD_BL_GPIO, 0);
        } else {
            gpio_set_level(BOARD_LCD_BL_GPIO, 1);
        }
    }
}

void display_hal_set_sleep(bool on)
{
    if (!s_initialized || !s_panel_handle) {
        return;
    }

    if (on) {
        // Enter sleep mode
        esp_lcd_panel_disp_on_off(s_panel_handle, false);
        if (s_backlight_pwm_initialized) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        } else {
            gpio_set_level(BOARD_LCD_BL_GPIO, 0);
        }
    } else {
        // Wake from sleep
        display_hal_set_backlight(s_backlight_level);
        esp_lcd_panel_disp_on_off(s_panel_handle, true);
    }
}