#include "config_mode.h"
#include "keyboard.h"
#include "ui.h"

// Legacy layout: three rows 12 px apart
#define ROW(n) ((n) * 12)

// Write a signed decimal at p; returns the new end
static char *put_int(char *p, int v) {
    char tmp[6];
    int n = 0;
    unsigned u = v < 0 ? -v : v;
    if (v < 0) {
        *p++ = '-';
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

static const char *const MSG_LABELS[] = {
    [CONFIG_MSG_TITLE]      = "CONFIG",
    [CONFIG_MSG_BRIGHTNESS] = "Brightness ",
    [CONFIG_MSG_VELOCITY]   = "Velocity ",
    [CONFIG_MSG_BANK]       = "Bank ",
    [CONFIG_MSG_TRANSPOSE]  = "Transpose ",
};

void ui_render(gfx_t *g, const ui_state_t *st) {
    char line[24];

    gfx_clear(g);
    put_int(put_str(line, "Octava: "), st->octave);
    gfx_text(g, 0, ROW(0), line);

    if (st->chord_mode) {
        gfx_text(g, 0, ROW(1), CHORDS[st->chord].name);
    }

    if (st->config) {
        char *p = put_str(line, MSG_LABELS[st->msg]);
        if (st->msg != CONFIG_MSG_TITLE) {
            put_int(p, st->msg_value);
        }
        gfx_text(g, 0, ROW(2), line);
    } else if (st->chord_mode) {
        gfx_text(g, 0, ROW(2), st->hold ? "HOLD: ON" : "HOLD: OFF");
    }
}
