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

typedef enum { CLEAR, SET, INVERT } op_t;

static void rect(gfx_t *g, int x, int y, int w, int h, op_t op) {
    int x0 = x < 0 ? 0 : x, x1 = x + w > g->width ? g->width : x + w;
    int y0 = y < 0 ? 0 : y, y1 = y + h > g->height ? g->height : y + h;
    for (int page = y0 >> 3; page < (y1 + 7) >> 3; page++) {
        // Bits of this page inside [y0, y1)
        int top = page * 8;
        uint8_t mask = 0xFF;
        if (y0 > top) mask &= 0xFF << (y0 - top);
        if (y1 < top + 8) mask &= 0xFF >> (top + 8 - y1);
        uint8_t *row = &g->buf[page * g->width];
        for (int i = x0; i < x1; i++) {
            row[i] = op == SET ? row[i] | mask : op == CLEAR ? row[i] & ~mask : row[i] ^ mask;
        }
    }
}

void gfx_fill(gfx_t *g, int x, int y, int w, int h, bool on) {
    rect(g, x, y, w, h, on ? SET : CLEAR);
}

void gfx_invert(gfx_t *g, int x, int y, int w, int h) {
    rect(g, x, y, w, h, INVERT);
}

void gfx_blit(gfx_t *g, const sprite_t *s, int x, int y) {
    int pages = g->height / 8;
    for (int i = 0; i < s->width; i++) {
        int col = x + i;
        if (col < 0 || col >= g->width || !s->cols[i]) {
            continue;
        }
        // Move the column to y, then take it a page at a time
        for (int page = 0; page < pages; page++) {
            int shift = y - page * 8;
            if (shift >= 8 || shift <= -32) {
                continue;
            }
            uint64_t bits = s->cols[i];
            bits = shift >= 0 ? bits << shift : bits >> -shift;
            g->buf[page * g->width + col] |= (uint8_t)bits;
        }
    }
}

// Glyph columns, 9 rows for the characters with a long tail
static void glyph(uint8_t c, uint16_t cols[8]) {
    for (int i = 0; i < FONT8X8_TALL_COUNT; i++) {
        if (font8x8_tall[i].c == c) {
            for (int j = 0; j < 8; j++) cols[j] = font8x8_tall[i].cols[j];
            return;
        }
    }
    if (c < FONT8X8_FIRST || c > FONT8X8_LAST) {
        c = '?';
    }
    for (int j = 0; j < 8; j++) cols[j] = font8x8[(c - FONT8X8_FIRST) * 8 + j];
}

static void column(gfx_t *g, int x, int y, uint16_t bits) {
    if (x < 0 || x >= g->width) {
        return;
    }
    // Up to 9 rows, so a column spans at most two pages
    int page = y >> 3;
    uint16_t shifted = bits << (y & 7);
    int pages = g->height / 8;
    if (page >= 0 && page < pages) {
        g->buf[page * g->width + x] |= (uint8_t)shifted;
    }
    if (page + 1 >= 0 && page + 1 < pages) {
        g->buf[(page + 1) * g->width + x] |= shifted >> 8;
    }
}

void gfx_text_scaled(gfx_t *g, int x, int y, const char *str, int scale) {
    gfx_text_spaced(g, x, y, str, scale, 8 * scale);
}

void gfx_text_spaced(gfx_t *g, int x, int y, const char *str, int scale, int advance) {
    for (; *str && x < g->width; str++, x += advance) {
        uint16_t cols[8];
        glyph(*str, cols);
        for (int i = 0; i < 8; i++) {
            for (int bit = 0; bit < 9; bit++) {
                if (cols[i] >> bit & 1) {
                    gfx_fill(g, x + i * scale, y + bit * scale, scale, scale, true);
                }
            }
        }
    }
}

void gfx_text(gfx_t *g, int x, int y, const char *str) {
    if (y <= -9 || y >= g->height) {
        return;
    }
    for (; *str && x < g->width; str++, x += 8) {
        uint16_t cols[8];
        glyph(*str, cols);
        for (int i = 0; i < 8; i++) {
            column(g, x + i, y, cols[i]);
        }
    }
}
