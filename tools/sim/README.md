# M5StickC Plus2 RGB565 Simulator (`stick_sim`)

Host-side Linux simulator that compiles the exact `240×135` RGB565 graphics and menu code ([src/ui/gfx_rgb565.c](../../src/ui/gfx_rgb565.c) and [src/ui/ui_menu.c](../../src/ui/ui_menu.c)) natively so you can test navigation and inspect bitmaps over SSH without flashing hardware.

## Prerequisites

- `gcc` and `make`
- A UTF-8 terminal with 24-bit truecolor support (for interactive SSH mode)
- `python3` with `Pillow` (`PIL`) for converting `.ppm` dumps to `.png`

## Build

```bash
make -C tools/sim
```

This builds `tools/sim/stick_sim`.

## Interactive SSH Mode

Run full 1:1 `240×135` pixel output (`240×68` half-block `▀` characters):

```bash
make -C tools/sim run
```

For smaller terminal windows (2× downsampled, `120×34` characters):

```bash
make -C tools/sim run-compact
```

### Controls

| Key(s) | Simulated Stick Button | Action |
| :--- | :--- | :--- |
| `s`, `j`, `↓` | `Btn B` short (`GPIO 39`) | Move Down |
| `S`, `J` | `Btn B` long (`GPIO 39`) | Fast Down (2 rows) |
| `w`, `k`, `↑` | `Btn C` short (`GPIO 35`) | Move Up |
| `Enter`, `Space`, `d`, `l`, `→` | `Btn A` short (`GPIO 37`) | Select / Enter / Toggle |
| `Backspace`, `Esc`, `a`, `h`, `x`, `←` | `Btn C` long (`GPIO 35`) | Back / Exit Tool |
| `p`, `P` | — | Save `screenshot.ppm` in current directory |
| `q`, `Q` | — | Quit |

## Generating Screenshots (Headless `.ppm` / `.png`)

Generate the default set of `240×135` preview images (`preview_menu.png`, `preview_wifi.png`, `preview_wifi_scan.png`):

```bash
make -C tools/sim preview
```

### Custom Scripted Screenshots

You can replay button sequences with `--nav`, advance simulation ticks with `--ticks`, and dump a binary `240×135` PPM file with `--dump`:

```bash
./tools/sim/stick_sim --nav aba --ticks 14 --dump out.ppm
python3 tools/sim/ppm_to_png.py out.ppm out.png
```

### Live Host Wi-Fi vs Simulated Wi-Fi

- **AP Scanner** runs `nmcli` in a background thread to display real nearby Wi-Fi networks (`Host nmcli`). If `nmcli` is unavailable, or if you set `STICK_SIM_MOCK_WIFI=1`, it falls back to simulated APs (`Simulated`).
- **Packet Sniffer** and **Probe Monitor** simulate live 802.11 channel hopping and probe requests on the Linux host (`Sim 802.11`), while [src/hal/esp32_wifi_hal.c](../../src/hal/esp32_wifi_hal.c) implements real ESP-IDF Wi-Fi scanning and promiscuous RX capture on hardware.

`--nav` character mapping:
- `a` / `A` — `Btn A` short / long (`Select` / `Context`)
- `b` / `B` — `Btn B` short / long (`Down` / `Fast Down`)
- `c` / `C` — `Btn C` short / long (`Up` / `Back`)

To export a zoomed nearest-neighbor PNG instead of 1:1 `240×135` (for easier viewing on HiDPI screens):

```bash
python3 tools/sim/ppm_to_png.py out.ppm out_3x.png --scale 3
```

## Cleanup

```bash
make -C tools/sim clean
```
