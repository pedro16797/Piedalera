#include "config_mode.h"
#include "icons_sprites.h"
#include "keyboard.h"
#include "ui.h"
#include "widgets.h"

// Keyboard strip across the top of every screen
#define KB_H        12
#define KB_H_SMALL  8
#define TEXT_Y      14

static const char *const NOTE_NAMES[12] = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B",
};

typedef struct {
    const sprite_t *icon;
    const char *name;
    int16_t lo, hi;     // bar range
} config_item_t;

static const config_item_t CONFIG_ITEMS[] = {
    [CONFIG_MSG_BRIGHTNESS] = { &SPRITE_BULB,      "Shine",      0,  16 },
    [CONFIG_MSG_VELOCITY]   = { &SPRITE_VELOCITY,  "Velocity",   1, 127 },
    [CONFIG_MSG_BANK]       = { &SPRITE_BANK,      "Bank",       0, 127 },
    [CONFIG_MSG_TRANSPOSE]  = { &SPRITE_TRANSPOSE, "Transp.",  -12,  12 },
    [CONFIG_MSG_DEBOUNCE]   = { &SPRITE_DEBOUNCE,  "Debounce",   0,  50 },
    [CONFIG_MSG_BOOTSEL]    = { &SPRITE_FLASH,     NULL,         0,   0 },
};

// Value screen: icon, value right-aligned against the name column, then the
// setting's name (up to 8 characters) above its bar
#define COLUMN_X    64

// Write a signed decimal at p; returns the new end
static char *put_int(char *p, int v, bool sign) {
    char tmp[6];
    int n = 0;
    unsigned u = v < 0 ? -v : v;
    if (v < 0) {
        *p++ = '-';
    } else if (sign && v > 0) {
        *p++ = '+';
    }
    do { tmp[n++] = '0' + u % 10; u /= 10; } while (u);
    while (n) {
        *p++ = tmp[--n];
    }
    *p = '\0';
    return p;
}

static char *put_str(char *p, const char *s) {
    while (*s) {
        *p++ = *s++;
    }
    *p = '\0';
    return p;
}

static int kb_x(const gfx_t *g) {
    return (g->width - WIDGET_KEYS_WIDTH) / 2;
}

// Icon centred under keys a..b, below their arc
static void icon_under(gfx_t *g, const sprite_t *icon, int a, int b, int y) {
    int x = kb_x(g);
    int left = widget_key_x(x, a);
    int right = widget_key_x(x, b) + widget_key_width(b);
    gfx_blit(g, icon, (left + right - icon->width) / 2, y);
}

// Config mode, nothing adjusted yet: every function key with its icon
static void config_map(gfx_t *g, const ui_state_t *st) {
    int x = kb_x(g);
    widget_keyboard(g, x, 0, KB_H, st->keys, 0);
    for (int msg = CONFIG_MSG_BRIGHTNESS; msg <= CONFIG_MSG_BOOTSEL; msg++) {
        uint8_t a, b;
        config_mode_keys(msg, &a, &b);
        widget_arc(g, x, KB_H + 1, a, b);
        icon_under(g, CONFIG_ITEMS[msg].icon, a, b, KB_H + 5);
    }
}

// Config mode while a value changes: its keys, icon, value and a bar
static void config_value(gfx_t *g, const ui_state_t *st) {
    const config_item_t *item = &CONFIG_ITEMS[st->msg];
    int x = kb_x(g);
    uint8_t a, b;
    config_mode_keys(st->msg, &a, &b);
    widget_keyboard(g, x, 0, KB_H_SMALL, st->keys, 0);
    widget_arc(g, x, KB_H_SMALL + 1, a, b);
    gfx_blit(g, item->icon, 0, 17);

    char text[8];
    put_int(text, st->msg_value, st->msg == CONFIG_MSG_TRANSPOSE);
    int chars = 0;
    while (text[chars]) chars++;
    gfx_text_scaled(g, COLUMN_X - chars * 16, 14, text, 2);

    gfx_text(g, COLUMN_X, 13, item->name);
    widget_bar(g, COLUMN_X, 23, g->width - COLUMN_X, 8, st->msg_value,
               item->lo, item->hi);
}

// About to restart in USB flash mode: the whole screen for the message
static void config_bootsel(gfx_t *g) {
    gfx_blit(g, &SPRITE_FLASH, 4, (g->height - SPRITE_FLASH.height) / 2);
    gfx_text_scaled(g, 24, g->height / 2 - 16, "USB", 2);
    gfx_text_scaled(g, 24, g->height / 2, "FLASH", 2);
}

void ui_render(gfx_t *g, const ui_state_t *st) {
    char line[24];

    gfx_clear(g);
    if (st->config) {
        if (st->msg == CONFIG_MSG_TITLE) {
            config_map(g, st);
        } else if (st->msg == CONFIG_MSG_BOOTSEL) {
            config_bootsel(g);
        } else {
            config_value(g, st);
        }
        return;
    }

    widget_keyboard(g, kb_x(g), 0, KB_H, st->keys, st->marks);

    // Octave on the bottom line in both modes
    put_int(put_str(line, "Octava: "), st->octave, false);
    gfx_text(g, 0, TEXT_Y + 9, line);
    if (!st->chord_mode) {
        return;
    }

    // Chord: root and type above the octave, hold beside it
    char *p = line;
    if (st->root >= 0) {
        p = put_str(put_str(p, NOTE_NAMES[st->root % 12]), " ");
    }
    put_str(p, CHORDS[st->chord].name);
    gfx_text(g, 0, TEXT_Y, line);
    if (st->hold) {
        gfx_text(g, g->width - 4 * 8, TEXT_Y + 9, "HOLD");
    }
}
