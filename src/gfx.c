#include <string.h>

#include "font8x8.h"
#include "gfx.h"

void gfx_init(gfx_t *g, uint8_t width, uint8_t height) {
    g->width = width;
    g->height = height;
    gfx_clear(g);
}

void gfx_clear(gfx_t *g) {
    memset(g->buf, 0, g->width * (g->height / 8));
}

static void column(gfx_t *g, int x, int y, uint8_t bits) {
    if (x < 0 || x >= g->width) {
        return;
    }
    // A glyph column spans up to two pages
    int page = y >> 3;
    int shift = y & 7;
    int pages = g->height / 8;
    if (page >= 0 && page < pages) {
        g->buf[page * g->width + x] |= bits << shift;
    }
    if (shift && page + 1 >= 0 && page + 1 < pages) {
        g->buf[(page + 1) * g->width + x] |= bits >> (8 - shift);
    }
}

void gfx_text(gfx_t *g, int x, int y, const char *str) {
    if (y <= -8 || y >= g->height) {
        return;
    }
    for (; *str && x < g->width; str++, x += 8) {
        uint8_t c = *str;
        if (c < FONT8X8_FIRST || c > FONT8X8_LAST) {
            c = '?';
        }
        const uint8_t *glyph = &font8x8[(c - FONT8X8_FIRST) * 8];
        for (int i = 0; i < 8; i++) {
            column(g, x + i, y, glyph[i]);
        }
    }
}
