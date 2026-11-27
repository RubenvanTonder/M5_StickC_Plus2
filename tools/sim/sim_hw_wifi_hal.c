// sim_hw_wifi_hal.c – Wi‑Fi HAL for the simulator that talks to a real ESP32 over UART
// Implements the same API as include/hal/wifi_hal.h but obtains data from the hardware.
// Simple binary protocol (host → ESP32):
//   0x01 – request AP scan
// ESP32 → host response:
//   uint16_t ap_count (network byte order, i.e., big‑endian)
//   For each AP:
//     uint8_t ssid_len (0‑32)
//     char ssid[ssid_len]
//     uint8_t channel
//     int8_t rssi
//     uint8_t auth_code (0=OPEN,1=WEP,2=WPA,3=WPA2,4=WPA3,5=WPA2/3,6=OTHER)
// The host maps auth_code to the string used by the UI.

#define _POSIX_C_SOURCE 200809L

#include "hal/wifi_hal.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <arpa/inet.h> // for ntohs
#include <endian.h>

#define CMD_SCAN 0x01  // Command byte that requests an AP scan from the ESP32

static const char *g_uart_path = "/dev/ttyUSB0"; // can be overridden via env var STICK_SIM_UART
static int g_uart_fd = -1;
static bool g_initialized = false;
static bool g_scanning = false;
static wifi_hal_ap_t s_aps[WIFI_HAL_MAX_APS];
static int s_ap_count = 0;
static const char *s_source_label = "ESP32 UART";

static void close_uart(void) {
    if (g_uart_fd >= 0) {
        close(g_uart_fd);
        g_uart_fd = -1;
    }
}

static bool open_uart(void) {
    const char *env = getenv("STICK_SIM_UART");
    if (env && *env) g_uart_path = env;
    g_uart_fd = open(g_uart_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (g_uart_fd < 0) {
        perror("open UART");
        return false;
    }
    struct termios tio;
    if (tcgetattr(g_uart_fd, &tio) != 0) {
        perror("tcgetattr");
        close_uart();
        return false;
    }
    // Set raw mode (equivalent to cfmakeraw)
    tio.c_iflag &= ~(IXON | IXOFF | IXANY);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ICANON | ISIG);
    tio.c_cflag |= CS8;
    // Set baud rate (use cfsetispeed / cfsetospeed instead of cfsetspeed)
    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);
    tio.c_cflag |= CLOCAL | CREAD;
    if (tcsetattr(g_uart_fd, TCSANOW, &tio) != 0) {
        perror("tcsetattr");
        close_uart();
        return false;
    }
    return true;
}

bool wifi_hal_init(void) {
    if (g_initialized) return true;
    if (!open_uart()) return false;
    g_initialized = true;
    return true;
}

void wifi_hal_deinit(void) {
    close_uart();
    g_initialized = false;
    g_scanning = false;
    s_ap_count = 0;
}

// Send a single‑byte command to the ESP32 and wait for an ACK (optional).
static bool send_cmd(uint8_t cmd) {
    if (g_uart_fd < 0) return false;
    ssize_t n = write(g_uart_fd, &cmd, 1);
    return n == 1;
}

static bool read_exact(void *buf, size_t len, int timeout_ms) {
    uint8_t *p = (uint8_t*)buf;
    size_t left = len;
    while (left) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(g_uart_fd, &fds);
        struct timeval tv = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };
        int r = select(g_uart_fd + 1, &fds, NULL, NULL, &tv);
        if (r <= 0) return false; // timeout or error
        ssize_t n = read(g_uart_fd, p, left);
        if (n <= 0) return false;
        p += n;
        left -= n;
    }
    return true;
}

void wifi_hal_start_scan(void) {
    if (!wifi_hal_init()) return;
    g_scanning = true;
    // Send scan request to ESP32 UART bridge
    fprintf(stderr, "[sim_hw_wifi_hal] Sending scan command (0x%02X) to ESP32\n", CMD_SCAN);
    if (!send_cmd(CMD_SCAN)) {
        fprintf(stderr, "[sim_hw_wifi_hal] Failed to send scan command to ESP32\n");
        g_scanning = false;
        return;
    }
    // Read AP count (uint16_t big‑endian)
    uint16_t cnt = 0;
    if (!read_exact(&cnt, sizeof(cnt), 2000)) {
        fprintf(stderr, "[sim_hw_wifi_hal] Timeout reading AP count from ESP32\n");
        g_scanning = false;
        return;
    }
    cnt = ntohs(cnt);
    fprintf(stderr, "[sim_hw_wifi_hal] Received %u APs from ESP32\n", cnt);
    if (cnt > WIFI_HAL_MAX_APS) cnt = WIFI_HAL_MAX_APS;
    s_ap_count = cnt;
    for (int i = 0; i < cnt; ++i) {
        uint8_t ssid_len = 0;
        if (!read_exact(&ssid_len, 1, 2000)) { g_scanning = false; return; }
        if (ssid_len > 32) ssid_len = 32;
        char ssid_buf[33] = {0};
        if (!read_exact(ssid_buf, ssid_len, 2000)) { g_scanning = false; return; }
        ssid_buf[ssid_len] = '\0';
        uint8_t ch = 0; int8_t rssi = 0; uint8_t auth = 0;
        if (!read_exact(&ch, 1, 2000) || !read_exact(&rssi, 1, 2000) || !read_exact(&auth, 1, 2000)) { g_scanning = false; return; }
        wifi_hal_ap_t *ap = &s_aps[i];
        snprintf(ap->ssid, sizeof(ap->ssid), "%s", ssid_buf);
        ap->channel = ch;
        ap->rssi = rssi;
        switch (auth) {
            case 0: strncpy(ap->auth, "OPEN", sizeof(ap->auth)); break;
            case 1: strncpy(ap->auth, "WEP", sizeof(ap->auth)); break;
            case 2: strncpy(ap->auth, "WPA", sizeof(ap->auth)); break;
            case 3: strncpy(ap->auth, "WPA2", sizeof(ap->auth)); break;
            case 4: strncpy(ap->auth, "WPA3", sizeof(ap->auth)); break;
            case 5: strncpy(ap->auth, "WPA2/3", sizeof(ap->auth)); break;
            default: strncpy(ap->auth, "OTHER", sizeof(ap->auth)); break;
        }
    }
    g_scanning = false;
}

bool wifi_hal_is_scanning(void) {
    return g_scanning;
}

int wifi_hal_get_aps(wifi_hal_ap_t *out_aps, int max_aps) {
    if (!out_aps || max_aps <= 0) return 0;
    int n = s_ap_count < max_aps ? s_ap_count : max_aps;
    if (n > 0) memcpy(out_aps, s_aps, (size_t)n * sizeof(out_aps[0]));
    return n;
}

void wifi_hal_sniffer_start(void) { /* not implemented for UART bridge */ }
void wifi_hal_sniffer_stop(void)  { /* not implemented */ }
void wifi_hal_sniffer_tick(void)  { /* not implemented */ }
void wifi_hal_get_sniffer_stats(wifi_hal_sniffer_stats_t *out_stats) { if (out_stats) memset(out_stats, 0, sizeof(*out_stats)); }
int wifi_hal_get_probes(wifi_hal_probe_t *out_probes, int max_probes) { (void)out_probes; (void)max_probes; return 0; }

const char *wifi_hal_source_label(void) {
    return s_source_label;
}
