#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/gfx_rgb565.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_BTN_NONE = 0,
    UI_BTN_A_SHORT, /* Front M5 button: Select / Enter / Toggle */
    UI_BTN_A_LONG,  /* Front M5 button: Context action */
    UI_BTN_B_SHORT, /* Side button B: Down */
    UI_BTN_B_LONG,  /* Side button B: Fast down */
    UI_BTN_C_SHORT, /* Side PWR button C: Up */
    UI_BTN_C_LONG,  /* Side PWR button C (hold ~700ms): Back */
} ui_input_t;

typedef struct {
    bool wifi_active;
    bool ble_active;
    uint16_t battery_mv;
    char time_str[6]; /* "HH:MM" */
} ui_status_t;

typedef enum {
    UI_TOOL_NONE = 0,
    UI_TOOL_WIFI_SCAN,
    UI_TOOL_WIFI_SNIFFER,
    UI_TOOL_WIFI_PROBES,
    UI_TOOL_BLE_SCAN,
    UI_TOOL_BLE_GATT,
    UI_TOOL_IR_TX,
    UI_TOOL_IR_RX,
    UI_TOOL_I2C_SCAN,
    UI_TOOL_UART_PROBE,
    UI_TOOL_GPIO_ADC,
    UI_TOOL_BOARD_IMU,
    UI_TOOL_BOARD_RTC,
    UI_TOOL_BOARD_IO_TEST,
    UI_TOOL_SYS_SCREEN_OFF,
    UI_TOOL_SYS_INFO,
    UI_TOOL_SYS_POWER_OFF,
} ui_tool_id_t;

typedef struct ui_menu_page ui_menu_page_t;

typedef struct {
    const char *badge;
    uint16_t badge_color;
    const char *title;
    const char *subtitle;
    const ui_menu_page_t *submenu;
    ui_tool_id_t tool_id;
} ui_menu_item_t;

struct ui_menu_page {
    const char *title;
    const ui_menu_item_t *items;
    uint8_t item_count;
};

#define UI_MAX_STACK_DEPTH 4

typedef struct {
    const ui_menu_page_t *stack[UI_MAX_STACK_DEPTH];
    uint8_t selected[UI_MAX_STACK_DEPTH];
    uint8_t scroll[UI_MAX_STACK_DEPTH];
    uint8_t depth;
    ui_tool_id_t active_tool;
    bool tool_running;
    uint8_t tool_selected;
    uint8_t tool_scroll;
    uint32_t tool_tick;
    ui_status_t status;
} ui_state_t;

void ui_init(ui_state_t *ui);
void ui_tick(ui_state_t *ui);
void ui_handle_input(ui_state_t *ui, ui_input_t input);
void ui_render(const ui_state_t *ui, gfx_fb_t *fb);

#ifdef __cplusplus
}
#endif
