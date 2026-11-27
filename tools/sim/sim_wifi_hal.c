#define _POSIX_C_SOURCE 200809L

#include "hal/wifi_hal.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static bool s_initialized = false;
static bool s_scanning = false;
static bool s_scan_thread_active = false;
static pthread_t s_scan_thread;

static wifi_hal_ap_t s_aps[WIFI_HAL_MAX_APS];
static int s_ap_count = 0;
static const char *s_source_label = "Host nmcli";

static wifi_hal_sniffer_stats_t s_sniffer;
static wifi_hal_probe_t s_probes[WIFI_HAL_MAX_PROBES];
static int s_probe_count = 0;
static uint32_t s_rng_state = 0x5A17C0DEu;

static uint32_t next_rand(void)
{
    s_rng_state = s_rng_state * 1664525u + 1013904223u;
    return s_rng_state;
}

static int cmp_ap_rssi_desc(const void *a, const void *b)
{
    const wifi_hal_ap_t *pa = (const wifi_hal_ap_t *)a;
    const wifi_hal_ap_t *pb = (const wifi_hal_ap_t *)b;
    return (int)pb->rssi - (int)pa->rssi;
}

static void split_nmcli_fields(const char *line, char fields[5][64])
{
    for (int i = 0; i < 5; ++i) {
        fields[i][0] = '\0';
    }
    int f = 0;
    size_t pos = 0;
    for (const char *p = line; *p && *p != '\n' && *p != '\r'; ++p) {
        if (*p == '\\' && p[1] != '\0') {
            ++p;
            if (f < 5 && pos + 1 < 64) {
                fields[f][pos++] = *p;
            }
            continue;
        }
        if (*p == ':' && f < 4) {
            fields[f][pos] = '\0';
            f++;
            pos = 0;
            continue;
        }
        if (f < 5 && pos + 1 < 64) {
            fields[f][pos++] = *p;
        }
    }
    if (f < 5) {
        fields[f][pos] = '\0';
    }
}

static void parse_mac(const char *str, uint8_t mac[6])
{
    unsigned int b[6] = {0};
    if (sscanf(str, "%x:%x:%x:%x:%x:%x",
               &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) == 6) {
        for (int i = 0; i < 6; ++i) {
            mac[i] = (uint8_t)b[i];
        }
    } else {
        memset(mac, 0, 6);
    }
}

static void simplify_auth(const char *sec, char *out, size_t out_sz)
{
    if (!sec || !*sec || strcmp(sec, "--") == 0) {
        snprintf(out, out_sz, "OPEN");
    } else if (strstr(sec, "WPA3") && strstr(sec, "WPA2")) {
        snprintf(out, out_sz, "WPA2/3");
    } else if (strstr(sec, "WPA3")) {
        snprintf(out, out_sz, "WPA3");
    } else if (strstr(sec, "WPA2")) {
        snprintf(out, out_sz, "WPA2");
    } else if (strstr(sec, "WPA")) {
        snprintf(out, out_sz, "WPA");
    } else if (strstr(sec, "WEP")) {
        snprintf(out, out_sz, "WEP");
    } else {
        snprintf(out, out_sz, "%.7s", sec);
    }
}

static void load_fallback_aps(wifi_hal_ap_t *out, int *out_count)
{
    static const struct {
        const char *ssid;
        uint8_t ch;
        int8_t rssi;
        const char *auth;
    } k_mock[] = {
        {"LabNet-5G",     6, -44, "WPA2"},
        {"IoT-TestAP",    1, -61, "WPA2"},
        {"Guest-Sandbox", 11, -73, "OPEN"},
        {"ESP-Mesh-Node", 3, -78, "WPA3"},
        {"Corp-Lab-Test", 9, -84, "WPA2E"},
    };
    int n = (int)(sizeof(k_mock) / sizeof(k_mock[0]));
    for (int i = 0; i < n; ++i) {
        memset(&out[i], 0, sizeof(out[i]));
        snprintf(out[i].ssid, sizeof(out[i].ssid), "%s", k_mock[i].ssid);
        out[i].channel = k_mock[i].ch;
        out[i].rssi = k_mock[i].rssi;
        snprintf(out[i].auth, sizeof(out[i].auth), "%s", k_mock[i].auth);
    }
    *out_count = n;
}

static void *scan_worker(void *arg)
{
    (void)arg;
    wifi_hal_ap_t parsed[WIFI_HAL_MAX_APS];
    int count = 0;
    const char *label = "Host nmcli";

    const char *mock_env = getenv("STICK_SIM_MOCK_WIFI");
    bool force_mock = (mock_env && strcmp(mock_env, "1") == 0);

    FILE *fp = NULL;
    if (!force_mock) {
        fp = popen("nmcli -t -f SSID,BSSID,CHAN,SIGNAL,SECURITY dev wifi list --rescan auto 2>/dev/null", "r");
    }

    if (fp) {
        char line[256];
        while (count < WIFI_HAL_MAX_APS && fgets(line, sizeof(line), fp)) {
            char f[5][64];
            split_nmcli_fields(line, f);
            int ch = atoi(f[2]);
            int sig = atoi(f[3]);
            if (ch <= 0) {
                continue;
            }
            wifi_hal_ap_t *ap = &parsed[count];
            memset(ap, 0, sizeof(*ap));
            if (f[0][0] == '\0') {
                snprintf(ap->ssid, sizeof(ap->ssid), "<hidden>");
            } else {
                snprintf(ap->ssid, sizeof(ap->ssid), "%.32s", f[0]);
            }
            parse_mac(f[1], ap->bssid);
            ap->channel = (uint8_t)ch;
            /* Convert NetworkManager 0..100% quality to approximate dBm */
            ap->rssi = (int8_t)((sig / 2) - 100);
            simplify_auth(f[4], ap->auth, sizeof(ap->auth));
            count++;
        }
        int rc = pclose(fp);
        if (rc != 0 && count == 0) {
            load_fallback_aps(parsed, &count);
            label = "Simulated";
        }
    } else {
        load_fallback_aps(parsed, &count);
        label = "Simulated";
    }

    if (count > 1) {
        qsort(parsed, (size_t)count, sizeof(parsed[0]), cmp_ap_rssi_desc);
    }

    pthread_mutex_lock(&s_lock);
    memcpy(s_aps, parsed, (size_t)count * sizeof(parsed[0]));
    s_ap_count = count;
    s_source_label = label;
    s_scanning = false;
    pthread_mutex_unlock(&s_lock);

    return NULL;
}

bool wifi_hal_init(void)
{
    if (!s_initialized) {
        memset(&s_sniffer, 0, sizeof(s_sniffer));
        s_sniffer.current_ch = 1;
        s_rng_state ^= (uint32_t)time(NULL);
        s_initialized = true;
    }
    return true;
}

void wifi_hal_deinit(void)
{
    wifi_hal_sniffer_stop();
    if (s_scan_thread_active) {
        pthread_join(s_scan_thread, NULL);
        s_scan_thread_active = false;
    }
    s_initialized = false;
}

void wifi_hal_start_scan(void)
{
    wifi_hal_init();
    wifi_hal_sniffer_stop();

    pthread_mutex_lock(&s_lock);
    bool busy = s_scanning;
    if (!busy) {
        s_scanning = true;
    }
    pthread_mutex_unlock(&s_lock);

    if (busy) {
        return;
    }

    if (s_scan_thread_active) {
        pthread_join(s_scan_thread, NULL);
        s_scan_thread_active = false;
    }

    if (pthread_create(&s_scan_thread, NULL, scan_worker, NULL) == 0) {
        s_scan_thread_active = true;
    } else {
        pthread_mutex_lock(&s_lock);
        load_fallback_aps(s_aps, &s_ap_count);
        s_source_label = "Simulated";
        s_scanning = false;
        pthread_mutex_unlock(&s_lock);
    }
}

bool wifi_hal_is_scanning(void)
{
    pthread_mutex_lock(&s_lock);
    bool scanning = s_scanning;
    pthread_mutex_unlock(&s_lock);
    return scanning;
}

int wifi_hal_get_aps(wifi_hal_ap_t *out_aps, int max_aps)
{
    if (!out_aps || max_aps <= 0) {
        return 0;
    }
    pthread_mutex_lock(&s_lock);
    int n = s_ap_count < max_aps ? s_ap_count : max_aps;
    if (n > 0) {
        memcpy(out_aps, s_aps, (size_t)n * sizeof(out_aps[0]));
    }
    pthread_mutex_unlock(&s_lock);
    return n;
}

static void seed_sim_probes_locked(void)
{
    static const char *k_default_ssids[] = {
        "CorpGuest", "HomeWiFi-5G", "iPhone-Hotspot", "LabNet-5G", "eduroam"
    };
    s_probe_count = 0;
    for (int i = 0; i < 5; ++i) {
        wifi_hal_probe_t *pr = &s_probes[s_probe_count++];
        pr->src_mac[0] = 0xDA;
        pr->src_mac[1] = 0xA1;
        pr->src_mac[2] = 0x19;
        pr->src_mac[3] = (uint8_t)(0x10 + i * 17);
        pr->src_mac[4] = (uint8_t)(0x42 + i * 9);
        pr->src_mac[5] = (uint8_t)(0x8B + i * 5);
        if (i < s_ap_count && s_aps[i].ssid[0] != '<') {
            snprintf(pr->ssid, sizeof(pr->ssid), "%.32s", s_aps[i].ssid);
            pr->channel = s_aps[i].channel <= WIFI_HAL_MAX_CH ? s_aps[i].channel : 6;
        } else {
            snprintf(pr->ssid, sizeof(pr->ssid), "%.32s", k_default_ssids[i]);
            pr->channel = (uint8_t)((i * 5) % WIFI_HAL_MAX_CH + 1);
        }
        pr->rssi = (int8_t)(-48 - i * 7);
    }
}

void wifi_hal_sniffer_start(void)
{
    wifi_hal_init();
    pthread_mutex_lock(&s_lock);
    if (!s_sniffer.active) {
        memset(&s_sniffer, 0, sizeof(s_sniffer));
        s_sniffer.current_ch = 1;
        s_sniffer.active = true;
        s_source_label = "Sim 802.11";
        seed_sim_probes_locked();
    }
    pthread_mutex_unlock(&s_lock);
}

void wifi_hal_sniffer_stop(void)
{
    pthread_mutex_lock(&s_lock);
    s_sniffer.active = false;
    pthread_mutex_unlock(&s_lock);
}

void wifi_hal_sniffer_tick(void)
{
    pthread_mutex_lock(&s_lock);
    if (!s_sniffer.active) {
        pthread_mutex_unlock(&s_lock);
        return;
    }

    s_sniffer.current_ch = (uint8_t)((s_sniffer.current_ch % WIFI_HAL_MAX_CH) + 1);
    uint8_t ch = s_sniffer.current_ch;

    /* Bias simulated traffic toward common 2.4GHz channels 1, 6, 11 */
    uint32_t base = (ch == 1 || ch == 6 || ch == 11) ? 14u : 4u;
    uint32_t mgmt = base + (next_rand() % 8u);
    uint32_t data = (base / 2u) + (next_rand() % 6u);
    uint32_t ctrl = 1u + (next_rand() % 4u);
    uint32_t delta = mgmt + data + ctrl;

    s_sniffer.mgmt_pkts += mgmt;
    s_sniffer.data_pkts += data;
    s_sniffer.ctrl_pkts += ctrl;
    s_sniffer.total_pkts += delta;
    s_sniffer.pps = delta * 4u;
    s_sniffer.ch_pkts[ch - 1] = (uint16_t)(s_sniffer.ch_pkts[ch - 1] + delta);

    if (s_probe_count > 0) {
        int idx = (int)(next_rand() % (uint32_t)s_probe_count);
        s_probes[idx].rssi = (int8_t)(-45 - (int)(next_rand() % 35u));
    }
    pthread_mutex_unlock(&s_lock);
}

void wifi_hal_get_sniffer_stats(wifi_hal_sniffer_stats_t *out_stats)
{
    if (!out_stats) {
        return;
    }
    pthread_mutex_lock(&s_lock);
    *out_stats = s_sniffer;
    pthread_mutex_unlock(&s_lock);
}

int wifi_hal_get_probes(wifi_hal_probe_t *out_probes, int max_probes)
{
    if (!out_probes || max_probes <= 0) {
        return 0;
    }
    pthread_mutex_lock(&s_lock);
    int n = s_probe_count < max_probes ? s_probe_count : max_probes;
    if (n > 0) {
        memcpy(out_probes, s_probes, (size_t)n * sizeof(out_probes[0]));
    }
    pthread_mutex_unlock(&s_lock);
    return n;
}

const char *wifi_hal_source_label(void)
{
    pthread_mutex_lock(&s_lock);
    const char *label = s_source_label;
    pthread_mutex_unlock(&s_lock);
    return label;
}

uint16_t wifi_hal_get_battery_mv(void)
{
    /* Simulated battery: slowly decays from 4.2V toward 3.7V over time */
    static uint32_t s_ticks = 0;
    s_ticks++;
    uint32_t decay = s_ticks / 20u; /* 1mV every 20 ticks (~100ms each) */
    uint16_t mv = 4200u;
    if (decay > 500u) decay = 500u;
    mv -= (uint16_t)decay;
    return mv;
}
