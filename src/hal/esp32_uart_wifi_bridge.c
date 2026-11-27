// esp32_uart_wifi_bridge.c – UART server that fulfills the simulator’s Wi‑Fi protocol
// Protocol (host → ESP32):
//   0x01 – request AP scan
// Response (ESP32 → host):
//   uint16_t ap_count (big‑endian)                // number of AP records
//   For each AP:
//     uint8_t ssid_len (0‑32)
//     char    ssid[ssid_len]                     // no NUL terminator
//     uint8_t channel
//     int8_t  rssi
//     uint8_t auth_code (0=OPEN,1=WEP,2=WPA,3=WPA2,4=WPA3,5=WPA2/3,6=OTHER)

#include "hal/wifi_hal.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include <arpa/inet.h>
#include <string.h>

#define TAG            "uart_wifi_bridge"
#define UART_NUM       UART_NUM_0
#define UART_BAUD      115200
#define UART_BUF_SIZE  256
#define CMD_SCAN       0x01

static void uart_init(void)
{
    const uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, UART_BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    /* Flush any stale RX data (e.g. terminal escape sequences) before starting */
    uart_flush_input(UART_NUM);
}

static uint8_t auth_to_code(const char *auth)
{
    if (!auth) return 6;
    if (strcmp(auth, "OPEN") == 0)     return 0;
    if (strcmp(auth, "WEP") == 0)      return 1;
    if (strcmp(auth, "WPA") == 0)      return 2;
    if (strcmp(auth, "WPA2") == 0)     return 3;
    if (strcmp(auth, "WPA3") == 0)     return 4;
    if (strcmp(auth, "WPA2/3") == 0)   return 5;
    return 6;
}

static void wait_for_scan(void)
{
    const TickType_t timeout = pdMS_TO_TICKS(10000);
    TickType_t start = xTaskGetTickCount();
    while (wifi_hal_is_scanning()) {
        if ((xTaskGetTickCount() - start) > timeout) {
            ESP_LOGW(TAG, "Scan timeout");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void uart_wifi_bridge_task(void *arg)
{
    uint8_t cmd;
    (void)arg;
    while (1) {
        int len = uart_read_bytes(UART_NUM, &cmd, 1, pdMS_TO_TICKS(100));
        if (len != 1) continue; // timeout
        if (cmd != CMD_SCAN) {
            ESP_LOGD(TAG, "Ignoring non-scan byte 0x%02X", cmd);
            continue;
        }
        ESP_LOGI(TAG, "Received scan request from host");
        wifi_hal_start_scan();
        wait_for_scan();
        wifi_hal_ap_t aps[WIFI_HAL_MAX_APS];
        int cnt = wifi_hal_get_aps(aps, WIFI_HAL_MAX_APS);
        ESP_LOGI(TAG, "Sending %d APs to host", cnt);
        uint8_t resp[UART_BUF_SIZE];
        size_t pos = 0;
        uint16_t be_cnt = htons((uint16_t)cnt);
        memcpy(&resp[pos], &be_cnt, sizeof(be_cnt));
        pos += sizeof(be_cnt);
        for (int i = 0; i < cnt; ++i) {
            uint8_t ssid_len = (uint8_t)strnlen(aps[i].ssid, sizeof(aps[i].ssid));
            if (ssid_len > 32) ssid_len = 32;
            resp[pos++] = ssid_len;
            memcpy(&resp[pos], aps[i].ssid, ssid_len);
            pos += ssid_len;
            resp[pos++] = aps[i].channel;
            resp[pos++] = (uint8_t)aps[i].rssi;
            resp[pos++] = auth_to_code(aps[i].auth);
        }
        int sent = uart_write_bytes(UART_NUM, (const char *)resp, pos);
// After sending data, log the number of bytes written
        if (sent != (int)pos) {
            ESP_LOGE(TAG, "UART send error (%d/%zu)", sent, pos);
        } else {
            ESP_LOGI(TAG, "Sent %d bytes representing %d APs to host", sent, cnt);
        }
    }
    vTaskDelete(NULL);
}

void esp32_uart_wifi_bridge_start(void)
{
    uart_init();
    xTaskCreate(uart_wifi_bridge_task, "uart_wifi_bridge", 4096, NULL, 5, NULL);
}
