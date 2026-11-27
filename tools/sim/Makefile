CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -I../../include
LDFLAGS := -pthread
SRCS := sim_main.c sim_wifi_hal.c ../../src/ui/gfx_rgb565.c ../../src/ui/ui_menu.c
BIN := stick_sim

.PHONY: all run run-compact preview clean

all: $(BIN)

$(BIN): $(SRCS) ../../include/ui/gfx_rgb565.h ../../include/ui/ui_menu.h ../../include/hal/wifi_hal.h
	$(CC) $(CFLAGS) $(SRCS) -o $(BIN) $(LDFLAGS)

run: $(BIN)
	./$(BIN)

run-compact: $(BIN)
	./$(BIN) --compact

preview: $(BIN)
	./$(BIN) --dump preview_menu.ppm
	python3 ppm_to_png.py preview_menu.ppm preview_menu.png --scale 1
	./$(BIN) --nav a --dump preview_wifi.ppm
	python3 ppm_to_png.py preview_wifi.ppm preview_wifi.png --scale 1
	./$(BIN) --nav aa --dump preview_wifi_scan.ppm
	python3 ppm_to_png.py preview_wifi_scan.ppm preview_wifi_scan.png --scale 1
	./$(BIN) --nav aba --ticks 14 --dump preview_wifi_sniffer.ppm
	python3 ppm_to_png.py preview_wifi_sniffer.ppm preview_wifi_sniffer.png --scale 1
	./$(BIN) --nav abba --ticks 6 --dump preview_wifi_probes.ppm
	python3 ppm_to_png.py preview_wifi_probes.ppm preview_wifi_probes.png --scale 1

clean:
	rm -f $(BIN) *.ppm *.png
