#define _POSIX_C_SOURCE 200809L

#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "hal/wifi_hal.h"
#include "ui/gfx_rgb565.h"
#include "ui/ui_menu.h"

static struct termios s_orig_termios;
static bool s_raw_enabled = false;

static void rgb565_to_rgb888(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t r5 = (uint8_t)((c >> 11) & 0x1FU);
    uint8_t g6 = (uint8_t)((c >> 5) & 0x3FU);
    uint8_t b5 = (uint8_t)(c & 0x1FU);
    *r = (uint8_t)((r5 << 3) | (r5 >> 2));
    *g = (uint8_t)((g6 << 2) | (g6 >> 4));
    *b = (uint8_t)((b5 << 3) | (b5 >> 2));
}

static bool write_ppm(const char *path, const gfx_fb_t *fb)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        perror("fopen");
        return false;
    }
    fprintf(fp, "P6\n%d %d\n255\n", GFX_WIDTH, GFX_HEIGHT);
    uint8_t rgb[3];
    for (size_t i = 0; i < GFX_FB_PIXELS; ++i) {
        rgb565_to_rgb888(fb->pixels[i], &rgb[0], &rgb[1], &rgb[2]);
        fwrite(rgb, 1, 3, fp);
    }
    fclose(fp);
    return true;
}

static void disable_raw_mode(void)
{
    if (s_raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_orig_termios);
        printf("\x1b[?25h\x1b[0m\n");
        fflush(stdout);
        s_raw_enabled = false;
    }
}

static bool enable_raw_mode(void)
{
    if (!isatty(STDIN_FILENO)) {
        return false;
    }
    if (tcgetattr(STDIN_FILENO, &s_orig_termios) != 0) {
        return false;
    }
    atexit(disable_raw_mode);

    struct termios raw = s_orig_termios;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return false;
    }
    s_raw_enabled = true;
    printf("\x1b[?1049h\x1b[?25l");
    return true;
}

static void render_ansi_terminal(const gfx_fb_t *fb, bool compact, const char *notice)
{
    int step = compact ? 2 : 1;
    // Move cursor to top without clearing to reduce flicker
    printf("\x1b[H");
    printf("M5StickC Plus2 RGB565 SSH Simulator (%dx%d)%s\n",
           GFX_WIDTH, GFX_HEIGHT, compact ? " [compact]" : "");

    for (int y = 0; y < GFX_HEIGHT; y += 2 * step) {
        for (int x = 0; x < GFX_WIDTH; x += step) {
            uint8_t tr, tg, tb;
            uint8_t br = 0, bg = 0, bb = 0;
            rgb565_to_rgb888(fb->pixels[y * GFX_WIDTH + x], &tr, &tg, &tb);
            if (y + step < GFX_HEIGHT) {
                rgb565_to_rgb888(fb->pixels[(y + step) * GFX_WIDTH + x], &br, &bg, &bb);
            }
            printf("\x1b[38;2;%u;%u;%um\x1b[48;2;%u;%u;%um▀",
                   tr, tg, tb, br, bg, bb);
        }
        printf("\x1b[0m\n");
    }

    printf("\x1b[0mControls: [s/Down]=BtnB(Down)  [w/Up]=BtnC(Up)  [Enter/d]=BtnA(Select)  [Backspace/a]=Long-C(Back)  [p]=PPM  [q]=Quit\n");
    if (notice && *notice) {
        printf("%s\n", notice);
    }
    fflush(stdout);
}

static ui_input_t map_nav_char(char ch)
{
    switch (ch) {
        case 'a': return UI_BTN_A_SHORT;
        case 'A': return UI_BTN_A_LONG;
        case 'b': return UI_BTN_B_SHORT;
        case 'B': return UI_BTN_B_LONG;
        case 'c': return UI_BTN_C_SHORT;
        case 'C': return UI_BTN_C_LONG;
        default:  return UI_BTN_NONE;
    }
}

static ui_input_t read_key_event(bool *want_quit, bool *want_dump)
{
    *want_quit = false;
    *want_dump = false;

    unsigned char ch = 0;
    if (read(STDIN_FILENO, &ch, 1) != 1) {
        *want_quit = true;
        return UI_BTN_NONE;
    }

    if (ch == '\x1b') {
        struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
        unsigned char seq[2] = {0, 0};
        if (poll(&pfd, 1, 25) > 0 && read(STDIN_FILENO, &seq[0], 1) == 1 && seq[0] == '[') {
            if (poll(&pfd, 1, 25) > 0 && read(STDIN_FILENO, &seq[1], 1) == 1) {
                if (seq[1] == 'A') return UI_BTN_C_SHORT;
                if (seq[1] == 'B') return UI_BTN_B_SHORT;
                if (seq[1] == 'C') return UI_BTN_A_SHORT;
                if (seq[1] == 'D') return UI_BTN_C_LONG;
            }
        }
        return UI_BTN_C_LONG;
    }

    switch (ch) {
        case 'q':
        case 'Q':
            *want_quit = true;
            return UI_BTN_NONE;
        case 'p':
        case 'P':
            *want_dump = true;
            return UI_BTN_NONE;
        case 's':
        case 'j':
            return UI_BTN_B_SHORT;
        case 'S':
        case 'J':
            return UI_BTN_B_LONG;
        case 'w':
        case 'k':
            return UI_BTN_C_SHORT;
        case '\n':
        case '\r':
        case ' ':
        case 'd':
        case 'l':
            return UI_BTN_A_SHORT;
        case 127:
        case '\b':
        case 'a':
        case 'h':
        case 'x':
            return UI_BTN_C_LONG;
        default:
            return UI_BTN_NONE;
    }
}

static void sleep_ms(int ms)
{
    struct timespec ts = {
        .tv_sec = ms / 1000,
        .tv_nsec = (long)(ms % 1000) * 1000000L,
    };
    nanosleep(&ts, NULL);
}

int main(int argc, char **argv)
{
    const char *dump_path = NULL;
    const char *nav_seq = NULL;
    bool compact = false;
    int ticks = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dump_path = argv[++i];
        } else if (strcmp(argv[i], "--nav") == 0 && i + 1 < argc) {
            nav_seq = argv[++i];
        } else if (strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) {
            ticks = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--compact") == 0) {
            compact = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--compact] [--nav <aAbBcC...>] [--ticks <n>] [--dump <out.ppm>]\n",
                   argv[0]);
            return 0;
        }
    }

    wifi_hal_init();
    ui_state_t ui;
    gfx_fb_t fb;
    ui_init(&ui);

    if (nav_seq) {
        for (const char *p = nav_seq; *p; ++p) {
            ui_handle_input(&ui, map_nav_char(*p));
        }
    }

    if (dump_path) {
        /* Wait briefly if an async Wi-Fi scan is in flight */
        for (int w = 0; w < 60 && wifi_hal_is_scanning(); ++w) {
            sleep_ms(50);
            ui_tick(&ui);
        }
        for (int t = 0; t < ticks; ++t) {
            ui_tick(&ui);
        }
        ui_render(&ui, &fb);
        if (!write_ppm(dump_path, &fb)) {
            wifi_hal_deinit();
            return 1;
        }
        printf("Wrote %s (%dx%d)\n", dump_path, GFX_WIDTH, GFX_HEIGHT);
        wifi_hal_deinit();
        return 0;
    }

    // Ensure we are running in a TTY
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "Not a TTY; use --dump <out.ppm> for headless screenshot.\n");
        wifi_hal_deinit();
        return 1;
    }

    // Enable raw mode for terminal input
    if (!enable_raw_mode()) {
        fprintf(stderr, "Failed to enable raw mode.\n");
        wifi_hal_deinit();
        return 1;
    }

    // clear screen once before main loop
    printf("\x1b[2J\x1b[H");

    char notice[128] = "";
    while (1) {
        ui_render(&ui, &fb);
        render_ansi_terminal(&fb, compact, notice);
        notice[0] = '\0';

        struct pollfd pfd = {.fd = STDIN_FILENO, .events = POLLIN};
        int ready = poll(&pfd, 1, 250);
        if (ready < 0) {
            break;
        }
        if (ready == 0) {
            ui_tick(&ui);
            continue;
        }

        bool want_quit = false;
        bool want_dump = false;
        ui_input_t ev = read_key_event(&want_quit, &want_dump);
        if (want_quit) {
            break;
        }
        if (want_dump) {
            if (write_ppm("screenshot.ppm", &fb)) {
                snprintf(notice, sizeof(notice), "Saved screenshot.ppm");
            }
            continue;
        }
        if (ev != UI_BTN_NONE) {
            ui_handle_input(&ui, ev);
        }
    }

    wifi_hal_deinit();
    return 0;
}
