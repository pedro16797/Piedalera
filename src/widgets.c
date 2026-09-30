#include "widgets.h"

#define BLACK_W     3

// White key index of each key, or -1 for black keys (which sit over the gap
// after the white key before them)
static const int8_t WHITE_INDEX[20] = {
    0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6, 7, -1, 8, -1, 9, 10, -1, 11,
};

static int white_before(int key) {
    return WHITE_INDEX[key - 1];
}

int widget_key_x(int x, int key) {
    if (WHITE_INDEX[key] >= 0) {
        return x + WHITE_INDEX[key] * WIDGET_KEY_STEP;
    }
    return x + (white_before(key) + 1) * WIDGET_KEY_STEP - 1 - BLACK_W / 2;
}

int widget_key_width(int key) {
    return WHITE_INDEX[key] >= 0 ? WIDGET_KEY_STEP - 1 : BLACK_W;
}

static void dotted(gfx_t *g, int x, int y, int w, int h, bool on) {
    for (int j = 0; j < h; j++) {
        for (int i = (j & 1); i < w; i += 2) {
            gfx_fill(g, x + i, y + j, 1, 1, on);
        }
    }
}

// Dark area of a black key: the notch plus a one pixel margin
static bool in_notch(int x, int y, int black_h, int key, int px, int py) {
    if (key < 0 || key > 19 || WHITE_INDEX[key] >= 0) {
        return false;
    }
    int kx = widget_key_x(x, key);
    return px >= kx - 1 && px <= kx + BLACK_W && py >= y && py <= y + black_h;
}

// Outline of a white key's visible shape, which wraps around the notches
// of its neighbours
static void white_outline(gfx_t *g, int x, int y, int h, int black_h, int key) {
    int kx = widget_key_x(x, key), w = widget_key_width(key);
    #define IN_SHAPE(px, py) ((px) >= kx && (px) < kx + w && (py) >= y && (py) < y + h && \
        !in_notch(x, y, black_h, key - 1, px, py) && !in_notch(x, y, black_h, key + 1, px, py))
    for (int py = y; py < y + h; py++) {
        for (int px = kx; px < kx + w; px++) {
            if (!IN_SHAPE(px, py)) {
                continue;
            }
            bool edge = !IN_SHAPE(px - 1, py) || !IN_SHAPE(px + 1, py) ||
                        !IN_SHAPE(px, py - 1) || !IN_SHAPE(px, py + 1);
            gfx_fill(g, px, py, 1, 1, edge);
        }
    }
    #undef IN_SHAPE
}

void widget_keyboard(gfx_t *g, int x, int y, int h, uint32_t pressed,
                     uint32_t marked) {
    int black_h = h * 3 / 5;
    for (int key = 0; key < 20; key++) {
        if (WHITE_INDEX[key] < 0) {
            continue;
        }
        int kx = widget_key_x(x, key), w = widget_key_width(key);
        gfx_fill(g, kx, y, w, h, true);
        if (marked & (1u << key)) {
            dotted(g, kx + 1, y + black_h + 1, w - 2, h - black_h - 2, false);
        }
    }
    for (int key = 0; key < 20; key++) {
        if (WHITE_INDEX[key] >= 0) {
            continue;
        }
        int kx = widget_key_x(x, key);
        // Notch with a dark margin, lit inside when pressed
        gfx_fill(g, kx - 1, y, BLACK_W + 2, black_h + 1, false);
        if (pressed & (1u << key)) {
            gfx_fill(g, kx, y, BLACK_W, black_h, true);
        } else if (marked & (1u << key)) {
            dotted(g, kx, y, BLACK_W, black_h, true);
        }
    }
    // Pressed white keys: outline only
    for (int key = 0; key < 20; key++) {
        if (WHITE_INDEX[key] >= 0 && (pressed & (1u << key))) {
            white_outline(g, x, y, h, black_h, key);
        }
    }
}

// 3x5 font for small labels: up to 5 column masks per glyph, bit 0 on top
static const struct {
    char c;
    uint8_t width;
    uint8_t cols[5];
} TINY[] = {
    { '0', 3, { 31, 17, 31 } }, { '1', 3, { 18, 31, 16 } }, { '2', 3, { 29, 21, 23 } },
    { '3', 3, { 21, 21, 31 } }, { '4', 3, { 7, 4, 31 } },   { '5', 3, { 23, 21, 29 } },
    { '6', 3, { 31, 21, 29 } }, { '7', 3, { 1, 29, 3 } },   { '8', 3, { 31, 21, 31 } },
    { '9', 3, { 23, 21, 31 } }, { 'H', 3, { 31, 4, 31 } },  { 'd', 3, { 28, 20, 31 } },
    { 'h', 3, { 31, 4, 28 } },  { 's', 3, { 20, 18, 10 } }, { '+', 3, { 4, 14, 4 } },
    { '-', 3, { 4, 4, 4 } },    { 'M', 5, { 31, 2, 4, 2, 31 } },
    { 'm', 5, { 28, 4, 28, 4, 24 } },
};

void widget_tiny_text(gfx_t *g, int x, int y, const char *str) {
    for (; *str; str++) {
        for (unsigned i = 0; i < sizeof(TINY) / sizeof(TINY[0]); i++) {
            if (TINY[i].c != *str) {
                continue;
            }
            for (int col = 0; col < TINY[i].width; col++) {
                for (int bit = 0; bit < 5; bit++) {
                    if (TINY[i].cols[col] >> bit & 1) {
                        gfx_fill(g, x + col, y + bit, 1, 1, true);
                    }
                }
            }
            x += TINY[i].width + 1;
        }
    }
}

int widget_tiny_width(const char *str) {
    int w = 0;
    for (; *str; str++) {
        for (unsigned i = 0; i < sizeof(TINY) / sizeof(TINY[0]); i++) {
            if (TINY[i].c == *str) {
                w += TINY[i].width + 1;
            }
        }
    }
    return w ? w - 1 : 0;
}

void widget_arc(gfx_t *g, int x, int y, int key_a, int key_b) {
    int x0 = widget_key_x(x, key_a) + 1;
    int x1 = widget_key_x(x, key_b) + widget_key_width(key_b) - 2;
    gfx_fill(g, x0, y, 1, 1, true);
    gfx_fill(g, x1, y, 1, 1, true);
    gfx_fill(g, x0 + 1, y + 1, x1 - x0 - 1, 1, true);
}

void widget_bar(gfx_t *g, int x, int y, int w, int h, int value, int lo, int hi) {
    gfx_fill(g, x, y, w, h, true);
    gfx_fill(g, x + 1, y + 1, w - 2, h - 2, false);
    int inner = w - 4;
    if (hi <= lo) {
        return;
    }
    if (lo < 0 && hi > 0) {
        // Signed: grows left or right from the middle
        int mid = x + 2 + inner / 2;
        int len = value * (inner / 2) / (value < 0 ? -lo : hi);
        gfx_fill(g, len < 0 ? mid + len : mid, y + 2, len < 0 ? -len : len, h - 4, true);
        gfx_fill(g, mid, y + 1, 1, h - 2, true);
        return;
    }
    gfx_fill(g, x + 2, y + 2, (value - lo) * inner / (hi - lo), h - 4, true);
}
