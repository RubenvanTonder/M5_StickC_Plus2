#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_HAL_MAX_APS     24
#define WIFI_HAL_MAX_PROBES  16
#define WIFI_HAL_MAX_CH      13

typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    uint8_t channel;
    int8_t rssi;
    char auth[8];
} wifi_hal_ap_t;

typedef struct {
    uint8_t src_mac[6];
    char ssid[33];
    int8_t rssi;
    uint8_t channel;
} wifi_hal_probe_t;

typedef struct {
    uint32_t total_pkts;
    uint32_t mgmt_pkts;
    uint32_t data_pkts;
    uint32_t ctrl_pkts;
    uint32_t pps;
    uint16_t ch_pkts[WIFI_HAL_MAX_CH];
    uint8_t current_ch;
    bool active;
} wifi_hal_sniffer_stats_t;

bool wifi_hal_init(void);
void wifi_hal_deinit(void);

void wifi_hal_start_scan(void);
bool wifi_hal_is_scanning(void);
int wifi_hal_get_aps(wifi_hal_ap_t *out_aps, int max_aps);

void wifi_hal_sniffer_start(void);
void wifi_hal_sniffer_stop(void);
void wifi_hal_sniffer_tick(void);
void wifi_hal_get_sniffer_stats(wifi_hal_sniffer_stats_t *out_stats);
int wifi_hal_get_probes(wifi_hal_probe_t *out_probes, int max_probes);

const char *wifi_hal_source_label(void);

uint16_t wifi_hal_get_battery_mv(void);

#ifdef __cplusplus
}
#endif
