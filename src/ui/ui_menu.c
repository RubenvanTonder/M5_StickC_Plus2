#include "ui/ui_menu.h"

#include <stdio.h>
#include <string.h>

#include "hal/wifi_hal.h"

#define UI_STATUS_BAR_H   17
#define UI_HINT_BAR_H     17
#define UI_VIEW_TOP       UI_STATUS_BAR_H
#define UI_VIEW_BOTTOM    (GFX_HEIGHT - UI_HINT_BAR_H)
#define UI_VISIBLE_ROWS   4
#define UI_ROW_H          24
#define UI_ROW_PAD_Y      2
#define UI_TOOL_ROWS      4

static const ui_menu_item_t s_wifi_items[] = {
    {"SC", GFX_COLOR_CYAN,   "AP Scanner",     "SSID, CH, RSSI, Auth",     NULL, UI_TOOL_WIFI_SCAN},
    {"SN", GFX_COLOR_ACCENT, "Packet Sniffer", "802.11 mgmt & channel hop", NULL, UI_TOOL_WIFI_SNIFFER},
    {"PR", GFX_COLOR_AMBER,  "Probe Monitor",  "Client probe requests",    NULL, UI_TOOL_WIFI_PROBES},
};

static const ui_menu_page_t s_wifi_page = {
    .title = "Wi-Fi",
    .items = s_wifi_items,
    .item_count = (uint8_t)(sizeof(s_wifi_items) / sizeof(s_wifi_items[0])),
};

static const ui_menu_item_t s_ble_items[] = {
    {"AV", GFX_COLOR_CYAN,   "Adv Scanner",    "MAC, RSSI, payload hex",   NULL, UI_TOOL_BLE_SCAN},
    {"GT", GFX_COLOR_PURPLE, "GATT Explorer",  "Services & char browser",  NULL, UI_TOOL_BLE_GATT},
};

static const ui_menu_page_t s_ble_page = {
    .title = "BLE",
    .items = s_ble_items,
    .item_count = (uint8_t)(sizeof(s_ble_items) / sizeof(s_ble_items[0])),
};

static const ui_menu_item_t s_ir_items[] = {
    {"TX", GFX_COLOR_RED,    "NEC / RC5 TX",   "GPIO19 RMT carrier test",  NULL, UI_TOOL_IR_TX},
    {"RX", GFX_COLOR_AMBER,  "IR Capture",     "Decode external 38kHz RX", NULL, UI_TOOL_IR_RX},
};

static const ui_menu_page_t s_ir_page = {
    .title = "Infrared",
    .items = s_ir_items,
    .item_count = (uint8_t)(sizeof(s_ir_items) / sizeof(s_ir_items[0])),
};

static const ui_menu_item_t s_bus_items[] = {
    {"I2", GFX_COLOR_ACCENT, "I2C Scan & Dump", "GPIO21/22 bus & Grove",   NULL, UI_TOOL_I2C_SCAN},
    {"UA", GFX_COLOR_CYAN,   "UART Probe",      "Baud detect & bridge",    NULL, UI_TOOL_UART_PROBE},
    {"IO", GFX_COLOR_AMBER,  "GPIO / ADC Mon",  "Live pin & voltage read", NULL, UI_TOOL_GPIO_ADC},
};

static const ui_menu_page_t s_bus_page = {
    .title = "Bus / IO",
    .items = s_bus_items,
    .item_count = (uint8_t)(sizeof(s_bus_items) / sizeof(s_bus_items[0])),
};

static const ui_menu_item_t s_board_items[] = {
    {"IM", GFX_COLOR_CYAN,   "IMU Live",        "MPU6886 @ 0x68",          NULL, UI_TOOL_BOARD_IMU},
    {"RT", GFX_COLOR_PURPLE, "RTC Clock",       "BM8563 @ 0x51",           NULL, UI_TOOL_BOARD_RTC},
    {"BT", GFX_COLOR_AMBER,  "Buzzer & LED",    "GPIO2 PWM / GPIO19 LED",  NULL, UI_TOOL_BOARD_IO_TEST},
};

static const ui_menu_page_t s_board_page = {
    .title = "Board",
    .items = s_board_items,
    .item_count = (uint8_t)(sizeof(s_board_items) / sizeof(s_board_items[0])),
};

static const ui_menu_item_t s_system_items[] = {
    {"BL", GFX_COLOR_MUTED,  "Display Sleep",   "Blank GPIO27 keep tasks", NULL, UI_TOOL_SYS_SCREEN_OFF},
    {"IN", GFX_COLOR_CYAN,   "Device Info",     "Chip, heap, GPIO map",    NULL, UI_TOOL_SYS_INFO},
    {"PW", GFX_COLOR_RED,    "Power Off",       "Drop GPIO4 battery hold", NULL, UI_TOOL_SYS_POWER_OFF},
};

static const ui_menu_page_t s_system_page = {
    .title = "System",
    .items = s_system_items,
    .item_count = (uint8_t)(sizeof(s_system_items) / sizeof(s_system_items[0])),
};

static const ui_menu_item_t s_root_items[] = {
    {"WF", GFX_COLOR_ACCENT, "Wi-Fi",           "AP scan, sniffer, probes", &s_wifi_page,   UI_TOOL_NONE},
    {"BT", GFX_COLOR_CYAN,   "BLE",             "Adv scan, GATT explorer",  &s_ble_page,    UI_TOOL_NONE},
    {"IR", GFX_COLOR_RED,    "Infrared",        "RMT TX/RX protocol lab",   &s_ir_page,     UI_TOOL_NONE},
    {"IO", GFX_COLOR_AMBER,  "Bus / IO",        "I2C, UART, GPIO, ADC",     &s_bus_page,    UI_TOOL_NONE},
    {"HW", GFX_COLOR_PURPLE, "Board",           "IMU, RTC, buzzer, LED",    &s_board_page,  UI_TOOL_NONE},
    {"SY", GFX_COLOR_MUTED,  "System",          "Screen, info, power off",  &s_system_page, UI_TOOL_NONE},
};

static const ui_menu_page_t s_root_page = {
    .title = "STICK-OS",
    .items = s_root_items,
    .item_count = (uint8_t)(sizeof(s_root_items) / sizeof(s_root_items[0])),
};

static const char *tool_title(ui_tool_id_t id)
{
    switch (id) {
        case UI_TOOL_WIFI_SCAN:      return "Wi-Fi > AP Scan";
        case UI_TOOL_WIFI_SNIFFER:   return "Wi-Fi > Sniffer";
        case UI_TOOL_WIFI_PROBES:    return "Wi-Fi > Probes";
        case UI_TOOL_BLE_SCAN:       return "BLE > Adv Scan";
        case UI_TOOL_BLE_GATT:       return "BLE > GATT";
        case UI_TOOL_IR_TX:          return "IR > NEC TX";
        case UI_TOOL_IR_RX:          return "IR > Capture";
        case UI_TOOL_I2C_SCAN:       return "Bus > I2C Scan";
        case UI_TOOL_UART_PROBE:     return "Bus > UART Probe";
        case UI_TOOL_GPIO_ADC:       return "Bus > GPIO/ADC";
        case UI_TOOL_BOARD_IMU:      return "Board > MPU6886";
        case UI_TOOL_BOARD_RTC:      return "Board > BM8563";
        case UI_TOOL_BOARD_IO_TEST:  return "Board > IO Test";
        case UI_TOOL_SYS_SCREEN_OFF: return "Sys > Display";
        case UI_TOOL_SYS_INFO:       return "Sys > Info";
        case UI_TOOL_SYS_POWER_OFF:  return "Sys > Power Off";
        default:                     return "Tool";
    }
}

static bool is_wifi_tool(ui_tool_id_t id)
{
    return id == UI_TOOL_WIFI_SCAN ||
           id == UI_TOOL_WIFI_SNIFFER ||
           id == UI_TOOL_WIFI_PROBES;
}

static void enter_tool(ui_state_t *ui, ui_tool_id_t id)
{
    ui->active_tool = id;
    ui->tool_running = true;
    ui->tool_selected = 0;
    ui->tool_scroll = 0;
    ui->tool_tick = 0;
    ui->status.wifi_active = is_wifi_tool(id);

    if (id == UI_TOOL_WIFI_SCAN) {
        wifi_hal_start_scan();
    } else if (id == UI_TOOL_WIFI_SNIFFER || id == UI_TOOL_WIFI_PROBES) {
        wifi_hal_sniffer_start();
    }
}

static void exit_tool(ui_state_t *ui)
{
    if (ui->active_tool == UI_TOOL_WIFI_SNIFFER ||
        ui->active_tool == UI_TOOL_WIFI_PROBES) {
        wifi_hal_sniffer_stop();
    }
    ui->active_tool = UI_TOOL_NONE;
    ui->tool_running = false;
    ui->status.wifi_active = false;
}

void ui_init(ui_state_t *ui)
{
    memset(ui, 0, sizeof(*ui));
    ui->stack[0] = &s_root_page;
    ui->depth = 0;
    ui->active_tool = UI_TOOL_NONE;
    ui->tool_running = false;
    ui->status.wifi_active = false;
    ui->status.ble_active = false;
    ui->status.battery_mv = wifi_hal_get_battery_mv();
    snprintf(ui->status.time_str, sizeof(ui->status.time_str), "10:35");
}

void ui_tick(ui_state_t *ui)
{
    ui->tool_tick++;
    if (ui->active_tool == UI_TOOL_WIFI_SNIFFER ||
        ui->active_tool == UI_TOOL_WIFI_PROBES) {
        if (ui->tool_running) {
            wifi_hal_sniffer_tick();
        }
    }

    /* Refresh battery voltage every 100 ticks (~10 seconds) */
    if ((ui->tool_tick % 100) == 0) {
        ui->status.battery_mv = wifi_hal_get_battery_mv();
    }
}

static void move_list_cursor(uint8_t *sel, uint8_t *scroll, int count,
                             int visible_rows, int delta)
{
    if (count <= 0) {
        *sel = 0;
        *scroll = 0;
        return;
    }
    int next = (int)(*sel) + delta;
    if (next < 0) {
        next = count - 1;
    } else if (next >= count) {
        next = 0;
    }
    *sel = (uint8_t)next;
    if (*sel < *scroll) {
        *scroll = *sel;
    } else if (*sel >= *scroll + visible_rows) {
        *scroll = (uint8_t)(*sel - visible_rows + 1);
    }
}

static int current_tool_item_count(const ui_state_t *ui)
{
    if (ui->active_tool == UI_TOOL_WIFI_SCAN) {
        wifi_hal_ap_t aps[WIFI_HAL_MAX_APS];
        return wifi_hal_get_aps(aps, WIFI_HAL_MAX_APS);
    }
    if (ui->active_tool == UI_TOOL_WIFI_PROBES) {
        wifi_hal_probe_t probes[WIFI_HAL_MAX_PROBES];
        return wifi_hal_get_probes(probes, WIFI_HAL_MAX_PROBES);
    }
    return 0;
}

void ui_handle_input(ui_state_t *ui, ui_input_t input)
{
    if (ui->active_tool != UI_TOOL_NONE) {
        if (input == UI_BTN_C_LONG) {
            exit_tool(ui);
            return;
        }
        if (input == UI_BTN_A_SHORT || input == UI_BTN_A_LONG) {
            if (ui->active_tool == UI_TOOL_WIFI_SCAN) {
                ui->tool_selected = 0;
                ui->tool_scroll = 0;
                wifi_hal_start_scan();
            } else {
                ui->tool_running = !ui->tool_running;
            }
            return;
        }
        int count = current_tool_item_count(ui);
        if (count > 0) {
            if (input == UI_BTN_B_SHORT) {
                move_list_cursor(&ui->tool_selected, &ui->tool_scroll, count, UI_TOOL_ROWS, 1);
            } else if (input == UI_BTN_B_LONG) {
                move_list_cursor(&ui->tool_selected, &ui->tool_scroll, count, UI_TOOL_ROWS, 2);
            } else if (input == UI_BTN_C_SHORT) {
                move_list_cursor(&ui->tool_selected, &ui->tool_scroll, count, UI_TOOL_ROWS, -1);
            }
        }
        return;
    }

    uint8_t d = ui->depth;
    const ui_menu_page_t *page = ui->stack[d];
    const ui_menu_item_t *cur = &page->items[ui->selected[d]];

    switch (input) {
        case UI_BTN_B_SHORT:
            move_list_cursor(&ui->selected[d], &ui->scroll[d],
                             page->item_count, UI_VISIBLE_ROWS, 1);
            break;
        case UI_BTN_B_LONG:
            move_list_cursor(&ui->selected[d], &ui->scroll[d],
                             page->item_count, UI_VISIBLE_ROWS, 2);
            break;
        case UI_BTN_C_SHORT:
            move_list_cursor(&ui->selected[d], &ui->scroll[d],
                             page->item_count, UI_VISIBLE_ROWS, -1);
            break;
        case UI_BTN_C_LONG:
            if (ui->depth > 0) {
                ui->depth--;
            }
            break;
        case UI_BTN_A_SHORT:
        case UI_BTN_A_LONG:
            if (cur->submenu != NULL && ui->depth + 1 < UI_MAX_STACK_DEPTH) {
                ui->depth++;
                ui->stack[ui->depth] = cur->submenu;
                ui->selected[ui->depth] = 0;
                ui->scroll[ui->depth] = 0;
            } else if (cur->tool_id != UI_TOOL_NONE) {
                enter_tool(ui, cur->tool_id);
            }
            break;
        default:
            break;
    }
}

static void draw_battery_icon(gfx_fb_t *fb, int x, int y, uint16_t mv)
{
    uint16_t fill_col = GFX_COLOR_ACCENT;
    int level = 4;
    if (mv < 3500) {
        fill_col = GFX_COLOR_RED;
        level = 1;
    } else if (mv < 3750) {
        fill_col = GFX_COLOR_AMBER;
        level = 2;
    } else if (mv < 3950) {
        level = 3;
    }

    gfx_draw_rect(fb, x, y, 14, 8, GFX_COLOR_MUTED);
    gfx_fill_rect(fb, x + 14, y + 2, 2, 4, GFX_COLOR_MUTED);
    int fill_w = level * 2 + 2;
    gfx_fill_rect(fb, x + 2, y + 2, fill_w, 4, fill_col);
}

static void draw_scrollbar(gfx_fb_t *fb, int top_y, int bottom_y,
                           int count, int visible, int scroll)
{
    if (count <= visible) {
        return;
    }
    int track_x = 234;
    int track_y = top_y;
    int track_h = bottom_y - top_y;
    gfx_fill_rect(fb, track_x, track_y, 3, track_h, GFX_COLOR_PANEL);

    int thumb_h = (track_h * visible) / count;
    if (thumb_h < 10) {
        thumb_h = 10;
    }
    int max_scroll = count - visible;
    int thumb_y = track_y + ((track_h - thumb_h) * scroll) / max_scroll;
    gfx_fill_rect(fb, track_x, thumb_y, 3, thumb_h, GFX_COLOR_ACCENT);
}

static void render_status_bar(const ui_state_t *ui, gfx_fb_t *fb)
{
    gfx_fill_rect(fb, 0, 0, GFX_WIDTH, UI_STATUS_BAR_H - 1, GFX_COLOR_PANEL);
    gfx_draw_hline(fb, 0, UI_STATUS_BAR_H - 1, GFX_WIDTH, GFX_COLOR_BORDER);

    char title_buf[32];
    if (ui->active_tool != UI_TOOL_NONE) {
        snprintf(title_buf, sizeof(title_buf), "%s", tool_title(ui->active_tool));
    } else if (ui->depth == 0) {
        snprintf(title_buf, sizeof(title_buf), "%s", ui->stack[0]->title);
    } else {
        snprintf(title_buf, sizeof(title_buf), "< %s", ui->stack[ui->depth]->title);
    }

    uint16_t title_col = (ui->depth == 0 && ui->active_tool == UI_TOOL_NONE)
                             ? GFX_COLOR_ACCENT
                             : GFX_COLOR_TEXT;
    gfx_draw_text(fb, 6, 5, title_buf, title_col, GFX_COLOR_PANEL, false, 1);

    gfx_draw_text(fb, 134, 5, "W",
                  ui->status.wifi_active ? GFX_COLOR_ACCENT : GFX_COLOR_BORDER,
                  GFX_COLOR_PANEL, false, 1);
    gfx_draw_text(fb, 144, 5, "B",
                  ui->status.ble_active ? GFX_COLOR_CYAN : GFX_COLOR_BORDER,
                  GFX_COLOR_PANEL, false, 1);

    draw_battery_icon(fb, 158, 4, ui->status.battery_mv);

    char vbat[8];
    snprintf(vbat, sizeof(vbat), "%u.%uV",
             (unsigned)(ui->status.battery_mv / 1000U),
             (unsigned)((ui->status.battery_mv % 1000U) / 100U));
    gfx_draw_text(fb, 178, 5, vbat, GFX_COLOR_MUTED, GFX_COLOR_PANEL, false, 1);
    gfx_draw_text(fb, 206, 5, ui->status.time_str, GFX_COLOR_TEXT, GFX_COLOR_PANEL, false, 1);
}

static void render_hint_bar(const ui_state_t *ui, gfx_fb_t *fb)
{
    int y0 = UI_VIEW_BOTTOM;
    gfx_draw_hline(fb, 0, y0, GFX_WIDTH, GFX_COLOR_BORDER);
    gfx_fill_rect(fb, 0, y0 + 1, GFX_WIDTH, UI_HINT_BAR_H - 1, GFX_COLOR_PANEL);

    if (ui->active_tool == UI_TOOL_WIFI_SCAN) {
        gfx_draw_text(fb, 6, y0 + 5, "[B/C] Scroll", GFX_COLOR_TEXT, GFX_COLOR_PANEL, false, 1);
        gfx_draw_text(fb, 86, y0 + 5, "[A] Scan", GFX_COLOR_ACCENT, GFX_COLOR_PANEL, false, 1);
        gfx_draw_text(fb, 146, y0 + 5, "[C Hold] Back", GFX_COLOR_AMBER, GFX_COLOR_PANEL, false, 1);
        return;
    }
    if (ui->active_tool != UI_TOOL_NONE) {
        gfx_draw_text(fb, 6, y0 + 5,
                      ui->tool_running ? "[A] Pause" : "[A] Start",
                      GFX_COLOR_ACCENT, GFX_COLOR_PANEL, false, 1);
        gfx_draw_text(fb, 146, y0 + 5, "[C Hold] Back",
                      GFX_COLOR_AMBER, GFX_COLOR_PANEL, false, 1);
        return;
    }

    gfx_draw_text(fb, 6, y0 + 5, "[B] Down", GFX_COLOR_TEXT, GFX_COLOR_PANEL, false, 1);
    gfx_draw_text(fb, 72, y0 + 5, "[A] Select", GFX_COLOR_ACCENT, GFX_COLOR_PANEL, false, 1);
    gfx_draw_text(fb, 146, y0 + 5,
                  ui->depth > 0 ? "[C] Up/Back" : "[C] Up",
                  GFX_COLOR_CYAN, GFX_COLOR_PANEL, false, 1);
}

static void render_menu_view(const ui_state_t *ui, gfx_fb_t *fb)
{
    uint8_t d = ui->depth;
    const ui_menu_page_t *page = ui->stack[d];
    uint8_t first = ui->scroll[d];
    bool has_scroll = page->item_count > UI_VISIBLE_ROWS;
    int row_w = has_scroll ? 226 : 232;

    for (uint8_t slot = 0; slot < UI_VISIBLE_ROWS; ++slot) {
        uint8_t idx = (uint8_t)(first + slot);
        if (idx >= page->item_count) {
            break;
        }
        const ui_menu_item_t *item = &page->items[idx];
        bool is_sel = (idx == ui->selected[d]);
        int ry = UI_VIEW_TOP + UI_ROW_PAD_Y + slot * UI_ROW_H;
        int rh = UI_ROW_H - 2;

        if (is_sel) {
            gfx_fill_round_rect(fb, 4, ry, row_w, rh, GFX_COLOR_ACCENT_DIM);
            gfx_draw_rect(fb, 4, ry, row_w, rh, GFX_COLOR_ACCENT);
            gfx_fill_rect(fb, 4, ry + 3, 3, rh - 6, GFX_COLOR_ACCENT);
        } else {
            gfx_fill_round_rect(fb, 4, ry, row_w, rh, GFX_COLOR_PANEL);
        }

        int bx = 11;
        int by = ry + 4;
        gfx_fill_round_rect(fb, bx, by, 18, 14, item->badge_color);
        gfx_draw_text(fb, bx + 3, by + 3, item->badge,
                      GFX_COLOR_BLACK, item->badge_color, false, 1);

        uint16_t sub_fg = is_sel ? GFX_COLOR_CYAN : GFX_COLOR_MUTED;
        gfx_draw_text(fb, 34, ry + 3, item->title, GFX_COLOR_TEXT, GFX_COLOR_PANEL, false, 1);
        gfx_draw_text(fb, 34, ry + 12, item->subtitle, sub_fg, GFX_COLOR_PANEL, false, 1);

        if (item->submenu != NULL) {
            gfx_draw_text(fb, 4 + row_w - 11, ry + 7, ">",
                          is_sel ? GFX_COLOR_ACCENT : GFX_COLOR_MUTED,
                          GFX_COLOR_PANEL, false, 1);
        }
    }

    draw_scrollbar(fb, UI_VIEW_TOP + 4, UI_VIEW_BOTTOM - 4,
                   page->item_count, UI_VISIBLE_ROWS, ui->scroll[d]);
}

static void render_wifi_scan_view(const ui_state_t *ui, gfx_fb_t *fb)
{
    wifi_hal_ap_t aps[WIFI_HAL_MAX_APS];
    int count = wifi_hal_get_aps(aps, WIFI_HAL_MAX_APS);
    bool scanning = wifi_hal_is_scanning();

    int y = UI_VIEW_TOP + 3;
    gfx_fill_round_rect(fb, 4, y, GFX_WIDTH - 8, 14, GFX_COLOR_PANEL);

    char banner[40];
    if (scanning) {
        const char dots[4][4] = {".", "..", "...", ""};
        snprintf(banner, sizeof(banner), "SCANNING%s", dots[ui->tool_tick % 4U]);
        gfx_draw_text(fb, 10, y + 3, banner, GFX_COLOR_AMBER, 0, false, 1);
    } else {
        snprintf(banner, sizeof(banner), "%d AP%s FOUND", count, count == 1 ? "" : "s");
        gfx_draw_text(fb, 10, y + 3, banner, GFX_COLOR_ACCENT, 0, false, 1);
    }
    gfx_draw_text(fb, 162, y + 3, wifi_hal_source_label(), GFX_COLOR_CYAN, 0, false, 1);

    int hy = y + 17;
    gfx_draw_text(fb, 6, hy, "SSID              CH  RSSI   AUTH", GFX_COLOR_MUTED, 0, false, 1);

    int list_y = hy + 11;
    if (count == 0) {
        gfx_draw_text(fb, 10, list_y + 16,
                      scanning ? "Probing 2.4GHz channels..." : "No APs found. Press [A] to scan",
                      GFX_COLOR_TEXT, 0, false, 1);
        return;
    }

    int row_w = (count > UI_TOOL_ROWS) ? 226 : 232;
    for (int slot = 0; slot < UI_TOOL_ROWS; ++slot) {
        int idx = ui->tool_scroll + slot;
        if (idx >= count) {
            break;
        }
        int ry = list_y + slot * 16;
        bool is_sel = (idx == ui->tool_selected);
        if (is_sel) {
            gfx_fill_round_rect(fb, 4, ry, row_w, 15, GFX_COLOR_ACCENT_DIM);
            gfx_fill_rect(fb, 4, ry + 2, 2, 11, GFX_COLOR_ACCENT);
        }

        char ssid17[18];
        snprintf(ssid17, sizeof(ssid17), "%.17s", aps[idx].ssid);
        char ch_buf[4];
        snprintf(ch_buf, sizeof(ch_buf), "%2u", (unsigned)aps[idx].channel);
        char rssi_buf[8];
        snprintf(rssi_buf, sizeof(rssi_buf), "%ddB", (int)aps[idx].rssi);

        uint16_t rssi_col = GFX_COLOR_ACCENT;
        if (aps[idx].rssi < -75) {
            rssi_col = GFX_COLOR_RED;
        } else if (aps[idx].rssi < -62) {
            rssi_col = GFX_COLOR_AMBER;
        }

        gfx_draw_text(fb, 8, ry + 4, ssid17, GFX_COLOR_TEXT, 0, false, 1);
        gfx_draw_text(fb, 114, ry + 4, ch_buf, GFX_COLOR_CYAN, 0, false, 1);
        gfx_draw_text(fb, 138, ry + 4, rssi_buf, rssi_col, 0, false, 1);
        gfx_draw_text(fb, 180, ry + 4, aps[idx].auth,
                      strcmp(aps[idx].auth, "OPEN") == 0 ? GFX_COLOR_RED : GFX_COLOR_MUTED,
                      0, false, 1);
    }

    draw_scrollbar(fb, list_y, UI_VIEW_BOTTOM - 3, count, UI_TOOL_ROWS, ui->tool_scroll);
}

static void render_wifi_sniffer_view(const ui_state_t *ui, gfx_fb_t *fb)
{
    wifi_hal_sniffer_stats_t st;
    wifi_hal_get_sniffer_stats(&st);

    int y = UI_VIEW_TOP + 3;
    gfx_fill_round_rect(fb, 4, y, GFX_WIDTH - 8, 14, GFX_COLOR_PANEL);

    char hdr[32];
    if (ui->tool_running) {
        snprintf(hdr, sizeof(hdr), "CH:%02u  PPS:%u", (unsigned)st.current_ch, (unsigned)st.pps);
        gfx_draw_text(fb, 10, y + 3, hdr, GFX_COLOR_ACCENT, 0, false, 1);
    } else {
        snprintf(hdr, sizeof(hdr), "PAUSED (CH:%02u)", (unsigned)st.current_ch);
        gfx_draw_text(fb, 10, y + 3, hdr, GFX_COLOR_AMBER, 0, false, 1);
    }
    gfx_draw_text(fb, 162, y + 3, wifi_hal_source_label(), GFX_COLOR_CYAN, 0, false, 1);

    char line1[40], line2[40];
    snprintf(line1, sizeof(line1), "TOT:%-6lu  MGMT:%-6lu",
             (unsigned long)st.total_pkts, (unsigned long)st.mgmt_pkts);
    snprintf(line2, sizeof(line2), "DATA:%-5lu  CTRL:%-6lu",
             (unsigned long)st.data_pkts, (unsigned long)st.ctrl_pkts);
    gfx_draw_text(fb, 8, y + 18, line1, GFX_COLOR_TEXT, 0, false, 1);
    gfx_draw_text(fb, 8, y + 29, line2, GFX_COLOR_MUTED, 0, false, 1);

    /* 13-channel activity bar graph */
    uint16_t max_pkts = 1;
    for (int i = 0; i < WIFI_HAL_MAX_CH; ++i) {
        if (st.ch_pkts[i] > max_pkts) {
            max_pkts = st.ch_pkts[i];
        }
    }

    int chart_base_y = UI_VIEW_BOTTOM - 14;
    int max_bar_h = 34;
    gfx_draw_hline(fb, 8, chart_base_y, GFX_WIDTH - 16, GFX_COLOR_BORDER);

    for (int i = 0; i < WIFI_HAL_MAX_CH; ++i) {
        int bx = 10 + i * 17;
        int bh = (st.ch_pkts[i] * max_bar_h) / max_pkts;
        if (st.ch_pkts[i] > 0 && bh < 2) {
            bh = 2;
        }
        bool cur_ch = ((i + 1) == st.current_ch);
        uint16_t col = cur_ch ? GFX_COLOR_AMBER : GFX_COLOR_ACCENT;
        if (bh > 0) {
            gfx_fill_rect(fb, bx, chart_base_y - bh, 12, bh, col);
        }
        char ch_lbl[3];
        snprintf(ch_lbl, sizeof(ch_lbl), "%X", i + 1);
        gfx_draw_text(fb, bx + 3, chart_base_y + 3, ch_lbl,
                      cur_ch ? GFX_COLOR_AMBER : GFX_COLOR_MUTED, 0, false, 1);
    }
}

static void render_wifi_probes_view(const ui_state_t *ui, gfx_fb_t *fb)
{
    wifi_hal_probe_t probes[WIFI_HAL_MAX_PROBES];
    int count = wifi_hal_get_probes(probes, WIFI_HAL_MAX_PROBES);
    wifi_hal_sniffer_stats_t st;
    wifi_hal_get_sniffer_stats(&st);

    int y = UI_VIEW_TOP + 3;
    gfx_fill_round_rect(fb, 4, y, GFX_WIDTH - 8, 14, GFX_COLOR_PANEL);

    char hdr[32];
    snprintf(hdr, sizeof(hdr), "PROBES:%d  CH:%02u", count, (unsigned)st.current_ch);
    gfx_draw_text(fb, 10, y + 3, hdr,
                  ui->tool_running ? GFX_COLOR_ACCENT : GFX_COLOR_AMBER, 0, false, 1);
    gfx_draw_text(fb, 162, y + 3, wifi_hal_source_label(), GFX_COLOR_CYAN, 0, false, 1);

    int hy = y + 17;
    gfx_draw_text(fb, 6, hy, "MAC       CH  RSSI TARGET SSID", GFX_COLOR_MUTED, 0, false, 1);

    int list_y = hy + 11;
    if (count == 0) {
        gfx_draw_text(fb, 10, list_y + 16, "Listening for probe requests...",
                      GFX_COLOR_TEXT, 0, false, 1);
        return;
    }

    int row_w = (count > UI_TOOL_ROWS) ? 226 : 232;
    for (int slot = 0; slot < UI_TOOL_ROWS; ++slot) {
        int idx = ui->tool_scroll + slot;
        if (idx >= count) {
            break;
        }
        int ry = list_y + slot * 16;
        if (idx == ui->tool_selected) {
            gfx_fill_round_rect(fb, 4, ry, row_w, 15, GFX_COLOR_ACCENT_DIM);
            gfx_fill_rect(fb, 4, ry + 2, 2, 11, GFX_COLOR_ACCENT);
        }

        char mac_tail[10];
        snprintf(mac_tail, sizeof(mac_tail), "%02X:%02X:%02X",
                 probes[idx].src_mac[3], probes[idx].src_mac[4], probes[idx].src_mac[5]);
        char ch_buf[4];
        snprintf(ch_buf, sizeof(ch_buf), "%2u", (unsigned)probes[idx].channel);
        char rssi_buf[6];
        snprintf(rssi_buf, sizeof(rssi_buf), "%d", (int)probes[idx].rssi);
        char ssid13[14];
        snprintf(ssid13, sizeof(ssid13), "%.13s", probes[idx].ssid);

        gfx_draw_text(fb, 8, ry + 4, mac_tail, GFX_COLOR_AMBER, 0, false, 1);
        gfx_draw_text(fb, 66, ry + 4, ch_buf, GFX_COLOR_CYAN, 0, false, 1);
        gfx_draw_text(fb, 88, ry + 4, rssi_buf, GFX_COLOR_MUTED, 0, false, 1);
        gfx_draw_text(fb, 118, ry + 4, ssid13, GFX_COLOR_TEXT, 0, false, 1);
    }

    draw_scrollbar(fb, list_y, UI_VIEW_BOTTOM - 3, count, UI_TOOL_ROWS, ui->tool_scroll);
}

static void render_tool_view(const ui_state_t *ui, gfx_fb_t *fb)
{
    if (ui->active_tool == UI_TOOL_WIFI_SCAN) {
        render_wifi_scan_view(ui, fb);
        return;
    }
    if (ui->active_tool == UI_TOOL_WIFI_SNIFFER) {
        render_wifi_sniffer_view(ui, fb);
        return;
    }
    if (ui->active_tool == UI_TOOL_WIFI_PROBES) {
        render_wifi_probes_view(ui, fb);
        return;
    }

    int y = UI_VIEW_TOP + 6;
    gfx_fill_round_rect(fb, 6, y, GFX_WIDTH - 12, 16, GFX_COLOR_PANEL);
    gfx_draw_text(fb, 12, y + 4,
                  ui->tool_running ? "STATUS: RUNNING" : "STATUS: PAUSED",
                  ui->tool_running ? GFX_COLOR_ACCENT : GFX_COLOR_AMBER,
                  GFX_COLOR_PANEL, false, 1);

    y += 22;
    switch (ui->active_tool) {
        case UI_TOOL_I2C_SCAN:
            gfx_draw_text(fb, 8, y,      "Bus: SDA=GPIO21 SCL=GPIO22", GFX_COLOR_MUTED, 0, false, 1);
            gfx_draw_text(fb, 8, y + 14, "[0x51] BM8563   RTC", GFX_COLOR_ACCENT, 0, false, 1);
            gfx_draw_text(fb, 8, y + 26, "[0x68] MPU6886  6-Axis IMU", GFX_COLOR_ACCENT, 0, false, 1);
            gfx_draw_text(fb, 8, y + 42, "Grove port shares I2C0", GFX_COLOR_CYAN, 0, false, 1);
            break;
        case UI_TOOL_SYS_INFO:
            gfx_draw_text(fb, 8, y,      "Board : M5StickC Plus2", GFX_COLOR_TEXT, 0, false, 1);
            gfx_draw_text(fb, 8, y + 12, "MCU   : ESP32-PICO-V3-02", GFX_COLOR_TEXT, 0, false, 1);
            gfx_draw_text(fb, 8, y + 24, "Panel : ST7789V2 240x135", GFX_COLOR_CYAN, 0, false, 1);
            gfx_draw_text(fb, 8, y + 36, "Btns  : A=37 B=39 C=35", GFX_COLOR_ACCENT, 0, false, 1);
            gfx_draw_text(fb, 8, y + 48, "Hold  : GPIO4  BL: GPIO27", GFX_COLOR_MUTED, 0, false, 1);
            break;
        default:
            gfx_draw_text(fb, 8, y + 4,  "Module ready for ESP-IDF", GFX_COLOR_TEXT, 0, false, 1);
            gfx_draw_text(fb, 8, y + 18, "implementation.", GFX_COLOR_TEXT, 0, false, 1);
            gfx_draw_text(fb, 8, y + 38, "Hold [C] to return to menu", GFX_COLOR_CYAN, 0, false, 1);
            break;
    }
}

void ui_render(const ui_state_t *ui, gfx_fb_t *fb)
{
    gfx_clear(fb, GFX_COLOR_BG);
    render_status_bar(ui, fb);
    if (ui->active_tool != UI_TOOL_NONE) {
        render_tool_view(ui, fb);
    } else {
        render_menu_view(ui, fb);
    }
    render_hint_bar(ui, fb);
}
