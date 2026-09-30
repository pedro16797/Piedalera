#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GFX_MAX_WIDTH   128
#define GFX_MAX_HEIGHT  64

// 1 bit per pixel in SSD1305 order: 8-pixel tall pages, one byte per column
// with bit 0 on top, pages stored one after the other
typedef struct {
    uint8_t width, height;
    uint8_t buf[GFX_MAX_WIDTH * GFX_MAX_HEIGHT / 8];
} gfx_t;

// Monochrome image, one 32-bit mask per column with bit 0 on top
typedef struct {
    uint8_t width, height;
    const uint32_t *cols;
} sprite_t;

void gfx_init(gfx_t *g, uint8_t width, uint8_t height);
void gfx_clear(gfx_t *g);

// Set (on) or clear a rectangle, clipped to the screen
void gfx_fill(gfx_t *g, int x, int y, int w, int h, bool on);
void gfx_invert(gfx_t *g, int x, int y, int w, int h);

// Draw a sprite's lit pixels at any position, clipped to the screen
void gfx_blit(gfx_t *g, const sprite_t *s, int x, int y);

// 8x8 text, g j p q y reaching a 9th row; x and y can be anything,
// off-screen parts are clipped
void gfx_text(gfx_t *g, int x, int y, const char *str);

// Same, drawing only rows top to bottom - 1
void gfx_text_clipped(gfx_t *g, int x, int y, const char *str, int top, int bottom);

// Same, each font pixel drawn as scale x scale
void gfx_text_scaled(gfx_t *g, int x, int y, const char *str, int scale);

// Scaled text with each character as wide as its ink and gap pixels apart;
// returns the width drawn. g may be NULL to only measure.
int gfx_text_ink(gfx_t *g, int x, int y, const char *str, int scale, int gap);

// Scaled text with characters advance pixels apart instead of 8 * scale
void gfx_text_spaced(gfx_t *g, int x, int y, const char *str, int scale, int advance);
