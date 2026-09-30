#include <string.h>

#include "battery.h"
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
    [CONFIG_MSG_BRIGHTNESS] = { &SPRITE_BULB,      "Contrast",   0,  16 },
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

static bool battery_warns(const ui_state_t *st) {
    return st->battery && st->battery_level < UI_BATTERY_LOW;
}

// Config mode, nothing adjusted yet: every function key with its icon, and
// with batteries their charge under F'. Nearly empty, it blinks an
// exclamation mark.
static void config_map(gfx_t *g, const ui_state_t *st, const ui_anim_t *a, uint32_t now) {
    int x = kb_x(g);
    widget_keyboard(g, x, 0, KB_H, st->keys, 0);
    for (int msg = CONFIG_MSG_BRIGHTNESS; msg <= CONFIG_MSG_BOOTSEL; msg++) {
        uint8_t k0, k1;
        config_mode_keys(msg, &k0, &k1);
        widget_arc(g, x, KB_H + 1, k0, k1);
        icon_under(g, CONFIG_ITEMS[msg].icon, k0, k1, KB_H + 5);
    }
    if (st->battery) {
        int left = widget_key_x(x, KEY_BATTERY);
        bool blink_on = !a || (now / UI_BLINK_MS) % 2 == 0;
        widget_arc(g, x, KB_H + 1, KEY_BATTERY, KEY_BATTERY);
        widget_battery(g, left + (widget_key_width(KEY_BATTERY) - WIDGET_BATTERY_W) / 2,
                       KB_H + 5, st->battery_level, battery_warns(st) && blink_on);
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

// Battery page (F'): the voltage, the battery type and a bar for the charge
static void config_battery(gfx_t *g, const ui_state_t *st) {
    int x = kb_x(g);
    widget_keyboard(g, x, 0, KB_H_SMALL, st->keys, 0);
    widget_arc(g, x, KB_H_SMALL + 1, KEY_BATTERY, KEY_BATTERY);

    // "4.12V", proportionally spaced to fit left of the column
    char text[12];
    char *p = put_str(put_int(text, st->battery_mv / 1000, false), ".");
    p = put_int(p, st->battery_mv / 100 % 10, false);
    p = put_str(put_int(p, st->battery_mv / 10 % 10, false), "V");
    int w = gfx_text_ink(NULL, 0, 0, text, 2, 2);
    gfx_text_ink(g, COLUMN_X - 2 - w, 14, text, 2, 2);

    // "3x NiMH", or just "Li-ion" for one cell
    p = text;
    if (st->battery_cells > 1) {
        p = put_str(put_int(p, st->battery_cells, false), "x ");
    }
    put_str(p, battery_label(st->battery_type));
    gfx_text(g, COLUMN_X, 13, text);
    widget_bar(g, COLUMN_X, 23, g->width - COLUMN_X, 8, st->battery_level, 0, 255);
}

// About to restart in USB flash mode: "USB FLASH" at double size under the
// keyboard, letters 3 px apart instead of 4 so it fits on one line
#define FLASH_ADVANCE   15      // 12 px of ink and the gap
#define FLASH_WIDTH     120     // 8 letters, 7 gaps, 3 px more between words

static void config_bootsel(gfx_t *g, const ui_state_t *st) {
    widget_keyboard(g, kb_x(g), 0, KB_H, st->keys, 0);
    // Ink starts 2 px into each 16 px cell and is 14 px tall
    int x = (g->width - FLASH_WIDTH) / 2 - 2;
    int y = KB_H + (g->height - KB_H - 14) / 2;
    gfx_text_spaced(g, x, y, "USB", 2, FLASH_ADVANCE);
    gfx_text_spaced(g, x + 3 * FLASH_ADVANCE + 3, y, "FLASH", 2, FLASH_ADVANCE);
}

void ui_anim_update(ui_anim_t *a, const ui_state_t *st, uint32_t now) {
    if (!a->started) {
        a->started = true;
        a->last = *st;
        a->changed_at = now;
        a->mode_at = now - UI_BANNER_MS;
        a->octave_at = now - UI_SLIDE_MS;
        return;
    }
    // Battery readings change on their own; only the rest means input
    ui_state_t cmp = *st;
    cmp.battery_level = a->last.battery_level;
    cmp.battery_mv = a->last.battery_mv;
    if (memcmp(&cmp, &a->last, sizeof(cmp)) != 0) {
        a->changed_at = now;
    }
    // Only while playing; entering config mode toggles and restores it
    if (!st->config && !a->last.config) {
        if (st->chord_mode != a->last.chord_mode) {
            a->mode_at = now;
        }
        if (st->octave != a->last.octave) {
            a->octave_from = a->last.octave;
            a->octave_at = now;
        }
    }
    a->last = *st;
}

static bool active(uint32_t at, uint32_t now, uint32_t len) {
    return now - at < len;
}

bool ui_anim_running(const ui_anim_t *a, uint32_t now) {
    const ui_state_t *st = &a->last;
    bool blinking = st->config && st->msg == CONFIG_MSG_TITLE && battery_warns(st);
    return blinking || active(a->mode_at, now, UI_BANNER_MS) ||
           active(a->octave_at, now, UI_SLIDE_MS);
}

uint32_t ui_idle_ms(const ui_anim_t *a, uint32_t now) {
    return now - a->changed_at;
}

// "Octave: n"; a changed number slides in from below when going up, from
// above when going down, pushing the old one out
static void octave_line(gfx_t *g, const ui_state_t *st, const ui_anim_t *a, uint32_t now) {
    char num[4];
    int y = TEXT_Y + 9, x = 8 * 8;
    gfx_text(g, 0, y, "Octave: ");
    put_int(num, st->octave, false);
    if (!a || !active(a->octave_at, now, UI_SLIDE_MS)) {
        gfx_text(g, x, y, num);
        return;
    }
    char old[4];
    put_int(old, a->octave_from, false);
    int dir = st->octave > a->octave_from ? 1 : -1;
    int d = (now - a->octave_at) * 9 / UI_SLIDE_MS;
    gfx_text_clipped(g, x, y - dir * d, old, y, y + 9);
    gfx_text_clipped(g, x, y + dir * (9 - d), num, y, y + 9);
}

static void screen(gfx_t *g, const ui_state_t *st, const ui_anim_t *a, uint32_t now) {
    char line[24];

    if (st->config) {
        if (st->msg == CONFIG_MSG_TITLE) {
            config_map(g, st, a, now);
        } else if (st->msg == CONFIG_MSG_BOOTSEL) {
            config_bootsel(g, st);
        } else if (st->msg == CONFIG_MSG_BATTERY) {
            config_battery(g, st);
        } else {
            config_value(g, st);
        }
        return;
    }

    widget_keyboard(g, kb_x(g), 0, KB_H, st->keys, st->marks);

    // Mode just toggled: its name, large, for a moment
    if (a && active(a->mode_at, now, UI_BANNER_MS)) {
        const char *name = st->chord_mode ? "CHORD" : "NORMAL";
        int len = (int)strlen(name);
        gfx_text_scaled(g, (g->width - len * 16) / 2, KB_H + (g->height - KB_H - 14) / 2,
                        name, 2);
        return;
    }

    // Octave on the bottom line in both modes
    octave_line(g, st, a, now);
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

void ui_render(gfx_t *g, const ui_state_t *st, const ui_anim_t *a, uint32_t now) {
    gfx_clear(g);
    screen(g, st, a, now);
    widget_hold_border(g, st->progress);
}
