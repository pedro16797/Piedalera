#pragma once

#include <stdint.h>

#define GFX_MAX_WIDTH   128
#define GFX_MAX_HEIGHT  64

// 1 bit per pixel in SSD1305 order: 8-pixel tall pages, one byte per column
// with bit 0 on top, pages stored one after the other
typedef struct {
    uint8_t width, height;
    uint8_t buf[GFX_MAX_WIDTH * GFX_MAX_HEIGHT / 8];
} gfx_t;

void gfx_init(gfx_t *g, uint8_t width, uint8_t height);
void gfx_clear(gfx_t *g);

// 8x8 text; x and y can be anything, off-screen parts are clipped
void gfx_text(gfx_t *g, int x, int y, const char *str);
