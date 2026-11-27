#include "hal/wifi_hal.h"

#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_hal";

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_initialized = false;
static bool s_scanning = false;

static wifi_hal_ap_t s_aps[WIFI_HAL_MAX_APS];
static int s_ap_count = 0;

static wifi_hal_sniffer_stats_t s_sniffer;
static uint32_t s_last_total_pkts = 0;

static wifi_hal_probe_t s_probes[WIFI_HAL_MAX_PROBES];
static int s_probe_count = 0;
static int s_probe_head = 0;

static const char *auth_mode_str(wifi_auth_mode_t mode)
{
    switch (mode) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2E";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/3";
        default:                        return "OTHER";
    }
}

static int cmp_ap_rssi_desc(const void *a, const void *b)
{
    const wifi_hal_ap_t *pa = (const wifi_hal_ap_t *)a;
    const wifi_hal_ap_t *pb = (const wifi_hal_ap_t *)b;
    return (int)pb->rssi - (int)pa->rssi;
}

static void handle_scan_done(void)
{
    uint16_t ap_num = 0;
    if (esp_wifi_scan_get_ap_num(&ap_num) != ESP_OK) {
        portENTER_CRITICAL(&s_lock);
        s_scanning = false;
        portEXIT_CRITICAL(&s_lock);
        return;
    }

    uint16_t fetch = ap_num > WIFI_HAL_MAX_APS ? WIFI_HAL_MAX_APS : ap_num;
    static wifi_ap_record_t records[WIFI_HAL_MAX_APS];
    memset(records, 0, sizeof(records));

    if (fetch > 0 && esp_wifi_scan_get_ap_records(&fetch, records) != ESP_OK) {
        fetch = 0;
    }

    static wifi_hal_ap_t parsed[WIFI_HAL_MAX_APS];
    memset(parsed, 0, sizeof(parsed));
    for (uint16_t i = 0; i < fetch; ++i) {
        const char *raw_ssid = (const char *)records[i].ssid;
        if (raw_ssid[0] == '\0') {
            snprintf(parsed[i].ssid, sizeof(parsed[i].ssid), "<hidden>");
        } else {
            snprintf(parsed[i].ssid, sizeof(parsed[i].ssid), "%.32s", raw_ssid);
        }
        memcpy(parsed[i].bssid, records[i].bssid, 6);
        parsed[i].channel = records[i].primary;
        parsed[i].rssi = records[i].rssi;
        snprintf(parsed[i].auth, sizeof(parsed[i].auth), "%s",
                 auth_mode_str(records[i].authmode));
    }

    if (fetch > 1) {
        qsort(parsed, fetch, sizeof(parsed[0]), cmp_ap_rssi_desc);
    }

    portENTER_CRITICAL(&s_lock);
    memcpy(s_aps, parsed, (size_t)fetch * sizeof(parsed[0]));
    s_ap_count = (int)fetch;
    s_scanning = false;
    portEXIT_CRITICAL(&s_lock);

    ESP_LOGD(TAG, "Scan complete: %u APs stored", (unsigned)fetch);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        handle_scan_done();
    }
}

static void record_probe_locked(const uint8_t *mac, const char *ssid,
                                int8_t rssi, uint8_t ch)
{
    for (int i = 0; i < s_probe_count; ++i) {
        if (memcmp(s_probes[i].src_mac, mac, 6) == 0 &&
            strncmp(s_probes[i].ssid, ssid, sizeof(s_probes[i].ssid)) == 0) {
            s_probes[i].rssi = rssi;
            s_probes[i].channel = ch;
            return;
        }
    }

    wifi_hal_probe_t *slot = &s_probes[s_probe_head];
    memcpy(slot->src_mac, mac, 6);
    snprintf(slot->ssid, sizeof(slot->ssid), "%.32s", ssid);
    slot->rssi = rssi;
    slot->channel = ch;

    s_probe_head = (s_probe_head + 1) % WIFI_HAL_MAX_PROBES;
    if (s_probe_count < WIFI_HAL_MAX_PROBES) {
        s_probe_count++;
    }
}

static void promisc_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (!buf) {
        return;
    }
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    const uint8_t *pl = pkt->payload;
    uint32_t len = pkt->rx_ctrl.sig_len;
    uint8_t ch = pkt->rx_ctrl.channel;
    if (ch < 1 || ch > WIFI_HAL_MAX_CH) {
        ch = s_sniffer.current_ch ? s_sniffer.current_ch : 1;
    }

    portENTER_CRITICAL_ISR(&s_lock);
    s_sniffer.total_pkts++;
    s_sniffer.ch_pkts[ch - 1]++;

    if (type == WIFI_PKT_MGMT) {
        s_sniffer.mgmt_pkts++;
        /* 802.11 mgmt header = 24 bytes; subtype 4 = Probe Request */
        if (len >= 26) {
            uint8_t fc0 = pl[0];
            uint8_t ftype = (fc0 >> 2) & 0x03;
            uint8_t subtype = (fc0 >> 4) & 0x0F;
            if (ftype == 0 && subtype == 4) {
                const uint8_t *src_mac = &pl[10];
                char ssid[33] = "<wildcard>";
                /* Tagged parameters start at offset 24: tag=0 is SSID */
                if (pl[24] == 0) {
                    uint8_t slen = pl[25];
                    if (slen > 0 && slen <= 32 && (uint32_t)(26 + slen) <= len) {
                        memcpy(ssid, &pl[26], slen);
                        ssid[slen] = '\0';
                    }
                }
                record_probe_locked(src_mac, ssid, (int8_t)pkt->rx_ctrl.rssi, ch);
            }
        }
    } else if (type == WIFI_PKT_DATA) {
        s_sniffer.data_pkts++;
    } else if (type == WIFI_PKT_CTRL) {
        s_sniffer.ctrl_pkts++;
    }
    portEXIT_CRITICAL_ISR(&s_lock);
}

bool wifi_hal_init(void)
{
    if (s_initialized) {
        return true;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
        return false;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    memset(&s_sniffer, 0, sizeof(s_sniffer));
    s_sniffer.current_ch = 1;
    s_initialized = true;
    ESP_LOGI(TAG, "ESP32 Wi-Fi HAL initialized (STA)");
    return true;
}

void wifi_hal_deinit(void)
{
    wifi_hal_sniffer_stop();
    if (s_initialized) {
        esp_wifi_stop();
        esp_wifi_deinit();
        s_initialized = false;
    }
}

void wifi_hal_start_scan(void)
{
    if (!wifi_hal_init()) {
        return;
    }
    wifi_hal_sniffer_stop();

    portENTER_CRITICAL(&s_lock);
    bool already = s_scanning;
    if (!already) {
        s_scanning = true;
    }
    portEXIT_CRITICAL(&s_lock);

    if (already) {
        return;
    }

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
        portENTER_CRITICAL(&s_lock);
        s_scanning = false;
        portEXIT_CRITICAL(&s_lock);
    } else {
        ESP_LOGD(TAG, "Async Wi-Fi scan started");
    }
}

bool wifi_hal_is_scanning(void)
{
    portENTER_CRITICAL(&s_lock);
    bool scanning = s_scanning;
    portEXIT_CRITICAL(&s_lock);
    return scanning;
}

int wifi_hal_get_aps(wifi_hal_ap_t *out_aps, int max_aps)
{
    if (!out_aps || max_aps <= 0) {
        return 0;
    }
    portENTER_CRITICAL(&s_lock);
    int n = s_ap_count < max_aps ? s_ap_count : max_aps;
    if (n > 0) {
        memcpy(out_aps, s_aps, (size_t)n * sizeof(out_aps[0]));
    }
    portEXIT_CRITICAL(&s_lock);
    return n;
}

void wifi_hal_sniffer_start(void)
{
    if (!wifi_hal_init()) {
        return;
    }

    portENTER_CRITICAL(&s_lock);
    bool active = s_sniffer.active;
    if (!active) {
        memset(&s_sniffer, 0, sizeof(s_sniffer));
        s_sniffer.current_ch = 1;
        s_sniffer.active = true;
        s_last_total_pkts = 0;
    }
    portEXIT_CRITICAL(&s_lock);

    if (active) {
        return;
    }

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                       WIFI_PROMIS_FILTER_MASK_DATA |
                       WIFI_PROMIS_FILTER_MASK_CTRL,
    };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&promisc_rx_cb);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(true);
    ESP_LOGI(TAG, "Promiscuous sniffer started");
}

void wifi_hal_sniffer_stop(void)
{
    portENTER_CRITICAL(&s_lock);
    bool active = s_sniffer.active;
    s_sniffer.active = false;
    portEXIT_CRITICAL(&s_lock);

    if (active) {
        esp_wifi_set_promiscuous(false);
        esp_wifi_set_promiscuous_rx_cb(NULL);
        ESP_LOGI(TAG, "Promiscuous sniffer stopped");
    }
}

void wifi_hal_sniffer_tick(void)
{
    portENTER_CRITICAL(&s_lock);
    bool active = s_sniffer.active;
    if (active) {
        uint8_t next_ch = (uint8_t)((s_sniffer.current_ch % WIFI_HAL_MAX_CH) + 1);
        s_sniffer.current_ch = next_ch;
        s_sniffer.pps = (s_sniffer.total_pkts - s_last_total_pkts) * 4U;
        s_last_total_pkts = s_sniffer.total_pkts;
    }
    uint8_t ch = s_sniffer.current_ch;
    portEXIT_CRITICAL(&s_lock);

    if (active) {
        esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
    }
}

void wifi_hal_get_sniffer_stats(wifi_hal_sniffer_stats_t *out_stats)
{
    if (!out_stats) {
        return;
    }
    portENTER_CRITICAL(&s_lock);
    *out_stats = s_sniffer;
    portEXIT_CRITICAL(&s_lock);
}

int wifi_hal_get_probes(wifi_hal_probe_t *out_probes, int max_probes)
{
    if (!out_probes || max_probes <= 0) {
        return 0;
    }
    portENTER_CRITICAL(&s_lock);
    int n = s_probe_count < max_probes ? s_probe_count : max_probes;
    if (n > 0) {
        memcpy(out_probes, s_probes, (size_t)n * sizeof(out_probes[0]));
    }
    portEXIT_CRITICAL(&s_lock);
    return n;
}

const char *wifi_hal_source_label(void)
{
    return "ESP32";
}

uint16_t wifi_hal_get_battery_mv(void)
{
    /* Battery voltage is read on GPIO 38 (ADC1_CHANNEL_0) in app_main.
     * Return a cached value from the main task context via a static. */
    extern uint16_t board_get_battery_mv(void);
    return board_get_battery_mv();
}
