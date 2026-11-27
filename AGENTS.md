# M5Stack StickC Plus2 - Agent Documentation

## Project Overview
This is an ESP-IDF project for the **M5Stack StickC Plus2** (ESP32-PICO-V3-02) featuring:
- **Display**: ST7789V2 240×135 RGB565 SPI LCD
- **Buttons**: 3× input-only (A=GPIO37, B=GPIO39, PWR=GPIO35)
- **Sensors**: MPU6886 IMU (I2C 0x68), BM8563 RTC (I2C 0x51)
- **Connectivity**: Wi-Fi, BLE, IR (GPIO19), UART bridge
- **Power**: Battery voltage monitoring (ADC1_CH0, GPIO38), power hold (GPIO4)

## Architecture
**Dual-target design** - same UI code runs on:
1. **Hardware** (`src/`) - ESP32 firmware with real peripherals
2. **Simulator** (`tools/sim/`) - Linux terminal app using ANSI graphics

## Key Directories
```
src/
├── main.c                 # Entry point, tasks, initialization
├── hal/
│   ├── display_hal.c      # ST7789V2 driver via esp_lcd
│   ├── esp32_wifi_hal.c   # Wi-Fi scanning/sniffing/probes
│   └── esp32_uart_wifi_bridge.c  # UART protocol for simulator
└── ui/
    ├── gfx_rgb565.c       # RGB565 framebuffer + 5×7 font
    └── ui_menu.c          # Hierarchical menu system

tools/sim/
├── sim_main.c             # Terminal UI renderer
├── sim_wifi_hal.c         # Mock Wi-Fi via nmcli
└── sim_hw_wifi_hal.c      # Hardware bridge via UART

include/
├── board_pins.h           # GPIO pin definitions
├── hal/
│   ├── wifi_hal.h         # Wi-Fi HAL API
│   └── display_hal.h      # Display HAL API
└── ui/
    ├── gfx_rgb565.h       # Graphics API
    └── ui_menu.h          # Menu API
```

## Build & Flash
```bash
# Hardware build (PlatformIO + ESP-IDF)
cd /home/ruben/Documents/PlatformIO/Projects/M5Stack_StickC_Plus2
/home/ruben/.platformio/penv/bin/platformio run
/home/ruben/.platformio/penv/bin/platformio run --target upload

# Simulator build
make -C tools/sim
./tools/sim/stick_sim              # Interactive
./tools/sim/stick_sim --compact    # Small terminal
./tools/sim/stick_sim --nav aa --dump out.ppm  # Scripted screenshot
```

## Serial Monitor
```bash
# PlatformIO monitor (requires interactive terminal)
/home/ruben/.platformio/penv/bin/platformio device monitor --port /dev/ttyACM1 --baud 115200

# Direct Python monitoring
python3 -c "
import serial, time
ser = serial.Serial('/dev/ttyACM1', 115200, timeout=1)
ser.setDTR(False); ser.setRTS(False); time.sleep(0.1)
ser.setDTR(True); ser.setRTS(True); time.sleep(1)
while True:
    line = ser.readline()
    if line: print(line.decode('utf-8', errors='replace').strip())
"
```

## Display Configuration (ST7789V2)
- **Resolution**: 240×135 (landscape)
- **Controller**: 240×240 with window offset
- **Color Order**: BGR (not RGB!)
- **SPI**: 40 MHz, SPI2_HOST, DMA
- **Orientation**: `swap_xy=true`, `mirror_x=true`, `mirror_y=false`
- **Gaps**: `x_gap=0`, `y_gap=52` (centers 135px in 240px)
- **Backlight**: LEDC PWM on GPIO27 (5 kHz, 8-bit)

## Button Mapping
| Button | GPIO | Simulator Key | Action |
|--------|------|---------------|--------|
| A (front) | 37 | Enter/Space/d/l/→ | Select/Enter |
| B (front) | 39 | s/j/↓ | Down |
| B (long) | 39 | S/J | Fast Down |
| PWR (side) | 35 | w/k/↑ | Up |
| PWR (long) | 35 | a/h/x/←/Backspace | Back/Exit |

## Wi-Fi HAL API (`include/hal/wifi_hal.h`)
- `wifi_hal_init()/deinit()` - Initialize
- `wifi_hal_start_scan()` - Trigger AP scan
- `wifi_hal_get_aps()` - Get scanned APs
- `wifi_hal_sniffer_start()/stop()/tick()` - Packet sniffer
- `wifi_hal_get_probes()` - Probe request monitor
- `wifi_hal_source_label()` - "Host nmcli" or "ESP32 UART"

## UART Wi-Fi Bridge Protocol
**Host → ESP32**: `0x01` (scan request)
**ESP32 → Host**: 
```
uint16_t ap_count (big-endian)
For each AP:
  uint8_t ssid_len
  char ssid[ssid_len]
  uint8_t channel
  int8_t rssi
  uint8_t auth_code (0=OPEN,1=WEP,2=WPA,3=WPA2,4=WPA3,5=WPA2/3,6=OTHER)
```

## Common Issues
1. **Screen not turning on**: Check `BOARD_SCREEN_ENABLED=1` in `main.c`, verify BGR color order, check gaps (0,52)
2. **No serial output**: Device may be in bootloader. Use esptool `--after hard-reset run` then monitor
3. **Simulator Wi-Fi empty**: Set `STICK_SIM_MOCK_WIFI=1` for fallback data, or ensure `nmcli` works
4. **Build fails**: Ensure `esp_lcd` component is in `src/CMakeLists.txt` REQUIRES

## Testing Checklist
- [ ] Hardware builds: `platformio run` succeeds
- [ ] Simulator builds: `make -C tools/sim` succeeds
- [ ] Simulator renders: `./tools/sim/stick_sim --nav a --dump test.ppm` creates valid PPM
- [ ] All menu screens render (main, Wi-Fi, BLE, IR, Bus, Board, System)
- [ ] Wi-Fi scan works in simulator (nmcli or mock)
- [ ] Hardware flashes and boots (check serial for "Display initialized successfully")
- [ ] Buttons respond on hardware (navigate menus)

## Key Files to Modify
| Feature | Files |
|---------|-------|
| Add menu item | `src/ui/ui_menu.c` (menu definitions) |
| New tool/view | `src/ui/ui_menu.c` (render_tool_view) |
| Display driver | `src/hal/display_hal.c` |
| Wi-Fi scanning | `src/hal/esp32_wifi_hal.c` |
| Button handling | `src/main.c` (button_task) |
| Simulator keys | `tools/sim/sim_main.c` (read_key_event) |
| Pin assignments | `include/board_pins.h` |

## Memory Usage (Typical)
- **RAM**: ~108 KB / 320 KB (33%)
- **Flash**: ~906 KB / 1048 KB (86%)

## Dependencies
- ESP-IDF 6.1.0 (via PlatformIO `framework-espidf @ 4.60100.0`)
- `esp_lcd` component for ST7789 driver
- `driver`, `esp_event`, `nvs_flash`, `esp_adc` components