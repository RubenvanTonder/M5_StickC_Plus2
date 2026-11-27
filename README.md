# M5Stack StickC Plus2 - Diagnostic Toolkit

ESP-IDF firmware and host-side simulator for the **M5StickC Plus2** (ESP32-PICO-V3-02). A dual-target "stick-OS" style diagnostic tool focused on Wi-Fi analysis, with a hierarchical menu system that runs identically on real hardware and in a Linux terminal simulator.

## Hardware Features

| Component | GPIO | Notes |
|-----------|------|-------|
| ST7789V2 LCD | SPI2 | 240×135 RGB565, BGR color order |
| Button A | GPIO 37 | Select / Enter |
| Button B | GPIO 39 | Down / Fast Down |
| Button PWR | GPIO 35 | Up / Back |
| Power Hold | GPIO 4 | Keep battery on |
| Battery ADC | GPIO 38 | ADC1_CHANNEL_0 |
| Backlight | GPIO 27 | LEDC PWM, 5 kHz |
| IR LED / TX | GPIO 19 | RMT |
| Buzzer | GPIO 2 | PWM |
| I2C SDA | GPIO 21 | MPU6886 @ 0x68, BM8563 @ 0x51 |
| I2C SCL | GPIO 22 | |

## Menu Structure

```
STICK-OS (root)
├── Wi-Fi          → AP Scanner, Packet Sniffer, Probe Monitor
├── BLE            → Adv Scanner, GATT Explorer
├── Infrared       → NEC/RC5 TX, IR Capture
├── Bus / IO       → I2C Scan, UART Probe, GPIO/ADC Monitor
├── Board          → IMU Live, RTC Clock, Buzzer & LED
└── System         → Display Sleep, Device Info, Power Off
```

## Build & Flash (Hardware)

```bash
cd /home/ruben/Documents/PlatformIO/Projects/M5Stack_StickC_Plus2

# Build
platformio run

# Upload
platformio run --target upload

# Upload + Monitor
platformio run --target upload --target monitor
```

## Build & Run (Simulator)

The simulator compiles the exact same UI code natively on Linux:

```bash
make -C tools/sim

# Interactive ANSI terminal mode
make -C tools/sim run

# Compact mode (2× downsampled)
make -C tools/sim run-compact

# Generate preview screenshots
make -C tools/sim preview

# Scripted screenshot
./tools/sim/stick_sim --nav aa --dump out.ppm
python3 tools/sim/ppm_to_png.py out.ppm out.png
```

### Simulator Controls

| Key(s) | Button | Action |
|--------|--------|--------|
| `s`, `j`, `↓` | B (short) | Down |
| `S`, `J` | B (long) | Fast Down |
| `w`, `k`, `↑` | C (short) | Up |
| `Enter`, `Space`, `d`, `l`, `→` | A (short) | Select |
| `Backspace`, `a`, `h`, `x`, `←` | C (long) | Back |
| `p` | — | Save `screenshot.ppm` |
| `q` | — | Quit |

## UART Wi-Fi Bridge Protocol

The ESP32 exposes a binary protocol over UART0 (115200 baud) so the simulator can query real Wi-Fi data from hardware:

**Host → ESP32**: `0x01` (scan request)

**ESP32 → Host**:
```
uint16_t ap_count (big-endian)
For each AP:
  uint8_t ssid_len
  char    ssid[ssid_len]
  uint8_t channel
  int8_t  rssi
  uint8_t auth_code
```

## Configuration

- `BOARD_SCREEN_ENABLED` in `src/main.c` — set to `1` to enable the ST7789V2 display on hardware, `0` to keep it disabled
- `STICK_SIM_UART` environment variable — override the simulator's UART device path (default: `/dev/ttyUSB0`)
- `STICK_SIM_MOCK_WIFI=1` — force simulated Wi-Fi data in the simulator instead of `nmcli`

## Dependencies

- ESP-IDF 6.1.0 (via PlatformIO `espressif32` platform)
- `esp_lcd` component for ST7789 driver
- `driver`, `esp_event`, `nvs_flash`, `esp_adc` components
- Simulator: `gcc`, `make`, `python3` with `Pillow` (for PPM→PNG conversion)

## License

MIT