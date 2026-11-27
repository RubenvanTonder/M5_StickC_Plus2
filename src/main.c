#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_monitor.h"
#include "esp_chip_info.h"
#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"

#include "board_pins.h"
#include "hal/wifi_hal.h"
#include "hal/display_hal.h"
#include "ui/ui_menu.h"
#include "ui/gfx_rgb565.h"

static const char *TAG = "stickc_plus2";

/* Set to 1 to enable the ST7789V2 screen for standalone hardware tests,
 * set to 0 to keep it disabled (e.g. when running with the simulator). */
#define BOARD_SCREEN_ENABLED 0

/* UI task configuration */
#define UI_TASK_STACK_SIZE    8192
#define UI_TASK_PRIORITY      5
#define UI_TICK_MS            30
#define UI_BUTTON_DEBOUNCE_MS 20

/* Button input queue */
static QueueHandle_t s_button_queue = NULL;

/* Framebuffer for UI rendering */
static gfx_fb_t s_framebuffer;

/* ADC oneshot handle and calibration for battery voltage reading */
static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_adc_cali;

/**
 * @brief Read battery voltage from GPIO 38 (ADC1_CHANNEL_0).
 *
 * The M5StickC Plus2 uses a voltage divider on the battery sense pin.
 * Raw ADC reading is converted to millivolts using ESP-IDF calibration.
 */
uint16_t board_get_battery_mv(void)
{
    uint32_t adc_reading = 0;
    for (int i = 0; i < 8; ++i) {
        int raw = 0;
        adc_oneshot_read(s_adc_handle, ADC_CHANNEL_0, &raw);
        adc_reading += (uint32_t)raw;
    }
    adc_reading /= 8;

    int mv = 0;
    adc_cali_raw_to_voltage(s_adc_cali, (int)adc_reading, &mv);
    /* The M5StickC Plus2 voltage divider scales the battery down; multiply
     * by 2 to approximate the actual battery voltage. */
    return (uint16_t)((uint32_t)mv * 2);
}

/**
 * @brief Latch the M5StickC Plus2 power-hold pin HIGH.
 *
 * Without an AXP192 PMIC, releasing the power button cuts battery power
 * unless GPIO 4 is driven HIGH early during startup.
 */
static void board_power_hold_init(void)
{
    const gpio_config_t hold_cfg = {
        .pin_bit_mask = (1ULL << BOARD_POWER_HOLD_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&hold_cfg));
    ESP_ERROR_CHECK(gpio_set_level(BOARD_POWER_HOLD_GPIO, 1));
    ESP_LOGD(TAG, "Power hold latched HIGH on GPIO %d", BOARD_POWER_HOLD_GPIO);
}

/**
 * @brief Initialize the ST7789V2 display and UI system.
 */
static void board_init_display(void)
{
#if BOARD_SCREEN_ENABLED == 1
    if (!display_hal_init()) {
        ESP_LOGE(TAG, "Display initialization failed");
        return;
    }
    ESP_LOGI(TAG, "Display initialized successfully");
#else
    ESP_LOGI(TAG, "Display disabled (BOARD_SCREEN_ENABLED=0)");
#endif
}

/**
 * @brief Initialize button GPIOs (A=GPIO37, B=GPIO39, PWR=GPIO35).
 *
 * All buttons are input-only with external pull-ups on the M5StickC Plus2.
 */
static void board_init_buttons(void)
{
    const gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BOARD_BTN_A_GPIO) |
                        (1ULL << BOARD_BTN_B_GPIO) |
                        (1ULL << BOARD_BTN_PWR_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,  // We'll poll in a task
    };
    ESP_ERROR_CHECK(gpio_config(&btn_cfg));
    ESP_LOGI(TAG, "Buttons initialized (A=GPIO%d, B=GPIO%d, PWR=GPIO%d)",
             BOARD_BTN_A_GPIO, BOARD_BTN_B_GPIO, BOARD_BTN_PWR_GPIO);
}

/**
 * @brief Button polling task - detects short/long presses and sends to queue.
 */
static void button_task(void *arg)
{
    (void)arg;
    const TickType_t poll_interval = pdMS_TO_TICKS(10);
    const int long_press_ticks = UI_BUTTON_DEBOUNCE_MS * 50;  // ~500ms for long press

    // Button state tracking
    struct {
        int gpio;
        ui_input_t short_press;
        ui_input_t long_press;
        int press_count;
        bool was_pressed;
    } buttons[] = {
        {BOARD_BTN_A_GPIO, UI_BTN_A_SHORT, UI_BTN_A_LONG, 0, false},
        {BOARD_BTN_B_GPIO, UI_BTN_B_SHORT, UI_BTN_B_LONG, 0, false},
        {BOARD_BTN_PWR_GPIO, UI_BTN_C_SHORT, UI_BTN_C_LONG, 0, false},
    };
    const int num_buttons = sizeof(buttons) / sizeof(buttons[0]);

    while (1) {
        for (int i = 0; i < num_buttons; ++i) {
            bool pressed = (gpio_get_level(buttons[i].gpio) == 0);  // Active low

            if (pressed && !buttons[i].was_pressed) {
                // Button just pressed
                buttons[i].press_count = 1;
                buttons[i].was_pressed = true;
            } else if (pressed && buttons[i].was_pressed) {
                // Button held
                buttons[i].press_count++;
                if (buttons[i].press_count == long_press_ticks) {
                    // Long press detected
                    ui_input_t input = buttons[i].long_press;
                    xQueueSend(s_button_queue, &input, 0);
                }
            } else if (!pressed && buttons[i].was_pressed) {
                // Button released
                if (buttons[i].press_count > 0 && buttons[i].press_count < long_press_ticks) {
                    // Short press detected
                    ui_input_t input = buttons[i].short_press;
                    xQueueSend(s_button_queue, &input, 0);
                }
                buttons[i].press_count = 0;
                buttons[i].was_pressed = false;
            }
        }
        vTaskDelay(poll_interval);
    }
}

/**
 * @brief UI rendering and input handling task.
 */
static void ui_task(void *arg)
{
    (void)arg;
    ui_state_t ui;
    ui_init(&ui);

    const TickType_t tick_interval = pdMS_TO_TICKS(UI_TICK_MS);
    ui_input_t input;

    while (1) {
        // Handle button input
        if (xQueueReceive(s_button_queue, &input, 0) == pdTRUE) {
            ui_handle_input(&ui, input);
        }

        // Update UI state (animations, periodic updates)
        ui_tick(&ui);

        // Render to framebuffer
        ui_render(&ui, &s_framebuffer);

        // Flush framebuffer to display
#if BOARD_SCREEN_ENABLED == 1
        display_hal_flush(&s_framebuffer);
#endif

        vTaskDelay(tick_interval);
    }
}

static void log_system_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "M5StickC Plus2 headless template started (ESP-IDF %s)",
             esp_get_idf_version());
    ESP_LOGD(TAG, "Chip cores: %u, silicon rev: %u.%u, reset reason: %d",
             (unsigned)chip_info.cores,
             (unsigned)(chip_info.revision / 100),
             (unsigned)(chip_info.revision % 100),
             (int)esp_reset_reason());
    ESP_LOGD(TAG, "Free heap: %" PRIu32 " bytes (min: %" PRIu32 " bytes)",
             esp_get_free_heap_size(),
             esp_get_minimum_free_heap_size());
}

static void log_scan_results(void)
{
    wifi_hal_ap_t aps[WIFI_HAL_MAX_APS];
    int cnt = wifi_hal_get_aps(aps, WIFI_HAL_MAX_APS);
    ESP_LOGI(TAG, "Wi-Fi scan: %d AP%s found", cnt, cnt == 1 ? "" : "s");
    for (int i = 0; i < cnt; ++i) {
        ESP_LOGI(TAG, "  [%d] %-32s CH=%2d RSSI=%4d AUTH=%s",
                 i, aps[i].ssid, aps[i].channel, aps[i].rssi, aps[i].auth);
    }
}

void app_main(void)
{
    /* Ensure runtime log level allows DEBUG messages globally and for TAG */
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    // Early UART output to verify boot
    printf("\n\n=== M5StickC Plus2 Starting ===\n");
    printf("Free heap: %lu bytes\n", esp_get_free_heap_size());
    fflush(stdout);

    board_power_hold_init();
    printf("Power hold init done\n");
    fflush(stdout);

    board_init_display();
    printf("Display init done\n");
    fflush(stdout);

    board_init_buttons();
    printf("Buttons init done\n");
    fflush(stdout);

    log_system_info();
    printf("System info logged\n");
    fflush(stdout);

    /* Initialize ADC1 oneshot for battery voltage reading on GPIO 38 (ADC_CHANNEL_0) */
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, ADC_CHANNEL_0, &chan_cfg));

    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_cfg, &s_adc_cali));

    /* Create button input queue */
    s_button_queue = xQueueCreate(10, sizeof(ui_input_t));
    if (!s_button_queue) {
        ESP_LOGE(TAG, "Failed to create button queue");
        return;
    }

    /* Initialize Wi-Fi HAL and UART bridge */
    extern void esp32_uart_wifi_bridge_start(void);
    if (!wifi_hal_init()) {
        ESP_LOGW(TAG, "Wi-Fi HAL initialization failed");
    } else {
        // HAL initialized – start UART bridge so the simulator can query real Wi‑Fi data
        esp32_uart_wifi_bridge_start();
        ESP_LOGI(TAG, "UART Wi‑Fi bridge started");

        // Start an automatic Wi-Fi scan so we can verify results via the monitor
        wifi_hal_start_scan();
        ESP_LOGI(TAG, "Automatic Wi-Fi scan started");
    }

    /* Start button polling task */
    xTaskCreate(button_task, "button_task", 4096, NULL, 5, NULL);

    /* Start UI task - this runs the menu system and renders to display */
    xTaskCreate(ui_task, "ui_task", UI_TASK_STACK_SIZE, NULL, UI_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "System initialized, UI task running");

    /* Main task can now do background work or just sleep */
    uint32_t tick = 0;
    while (1) {
        int64_t uptime_s = esp_timer_get_time() / 1000000LL;
        uint16_t bat_mv = board_get_battery_mv();
        ESP_LOGI(TAG, "Heartbeat #%" PRIu32 " | uptime=%" PRId64 "s | free_heap=%" PRIu32 "B | bat=%u.%02uV",
                 tick++, uptime_s, esp_get_free_heap_size(),
                 (unsigned)(bat_mv / 1000U), (unsigned)((bat_mv % 1000U) / 10U));

        // Periodically re-scan and log results every 30 seconds
        if ((tick % 6) == 0 && tick > 0) {
            wifi_hal_start_scan();
            vTaskDelay(pdMS_TO_TICKS(2000)); // wait for scan to complete
            log_scan_results();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
