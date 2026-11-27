#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GFX_WIDTH   240
#define GFX_HEIGHT  135
#define GFX_FB_PIXELS (GFX_WIDTH * GFX_HEIGHT)

/* Standard 16-bit RGB565 color helper */
#define RGB565(r, g, b) \
    ((uint16_t)((((uint16_t)(r) & 0xF8U) << 8) | \
                (((uint16_t)(g) & 0xFCU) << 3) | \
                (((uint16_t)(b) & 0xF8U) >> 3)))

/* Hacker Dark Theme palette */
#define GFX_COLOR_BG          RGB565(13, 17, 23)     /* #0D1117 */
#define GFX_COLOR_PANEL       RGB565(22, 27, 34)     /* #161B22 */
#define GFX_COLOR_BORDER      RGB565(48, 54, 61)     /* #30363D */
#define GFX_COLOR_ACCENT      RGB565(0, 230, 118)    /* #00E676 */
#define GFX_COLOR_ACCENT_DIM  RGB565(13, 46, 30)     /* #0D2E1E */
#define GFX_COLOR_CYAN        RGB565(57, 197, 207)   /* #39C5CF */
#define GFX_COLOR_AMBER       RGB565(240, 165, 0)    /* #F0A500 */
#define GFX_COLOR_RED         RGB565(248, 81, 73)    /* #F85149 */
#define GFX_COLOR_PURPLE      RGB565(163, 113, 247)  /* #A371F7 */
#define GFX_COLOR_TEXT        RGB565(230, 237, 243)  /* #E6EDF3 */
#define GFX_COLOR_MUTED       RGB565(139, 148, 158)  /* #8B949E */
#define GFX_COLOR_BLACK       RGB565(0, 0, 0)

typedef struct {
    uint16_t pixels[GFX_FB_PIXELS];
} gfx_fb_t;

void gfx_clear(gfx_fb_t *fb, uint16_t color);
void gfx_draw_pixel(gfx_fb_t *fb, int x, int y, uint16_t color);
void gfx_draw_hline(gfx_fb_t *fb, int x, int y, int w, uint16_t color);
void gfx_draw_vline(gfx_fb_t *fb, int x, int y, int h, uint16_t color);
void gfx_fill_rect(gfx_fb_t *fb, int x, int y, int w, int h, uint16_t color);
void gfx_draw_rect(gfx_fb_t *fb, int x, int y, int w, int h, uint16_t color);
void gfx_fill_round_rect(gfx_fb_t *fb, int x, int y, int w, int h, uint16_t color);
void gfx_draw_char(gfx_fb_t *fb, int x, int y, char c, uint16_t fg, uint16_t bg,
                   bool opaque_bg, int scale);
void gfx_draw_text(gfx_fb_t *fb, int x, int y, const char *str, uint16_t fg,
                   uint16_t bg, bool opaque_bg, int scale);
int gfx_text_width(const char *str, int scale);

#ifdef __cplusplus
}
#endif
