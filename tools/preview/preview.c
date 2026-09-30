// Renders UI scenes with the firmware's own drawing code (gfx.c, ui.c) and
// writes preview.html (playable, OLED-styled) plus a PNG per scene, or the
// pedal diagrams in docs/images/.
//
//   preview [output dir] [--size WxH]
//   preview --diagrams docs/images

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_mode.h"
#include "gfx.h"
#include "midi.h"
#include "diagram_sprites.h"
#include "icons_sprites.h"
#include "splash.h"
#include "ui.h"
#include "widgets.h"

// ui.c pulls in the chord table from keyboard.c, which needs a MIDI output
void midi_note_on(uint8_t ch, uint8_t note, uint8_t velocity) { (void)ch; (void)note; (void)velocity; }
void midi_note_off(uint8_t ch, uint8_t note) { (void)ch; (void)note; }
void midi_cc(uint8_t ch, uint8_t cc, uint8_t value) { (void)ch; (void)cc; (void)value; }

// Scenes

typedef struct {
    const char *name;
    uint32_t duration_ms;   // 0: a single still frame
    uint16_t fps;
    void (*state)(ui_state_t *st, uint32_t t_ms);
    bool (*draw)(gfx_t *g, uint32_t t_ms);  // instead of state + ui_render
} scene_t;

static void base(ui_state_t *st) {
    memset(st, 0, sizeof(*st));
    st->octave = 3;
    st->brightness = 15;
    st->root = -1;
}

#define KEY(k) (1u << (k))

// C, then C-E, C-E-G, then D'b
static void normal(ui_state_t *st, uint32_t t) {
    static const uint32_t STEPS[] = { 0, KEY(0), KEY(0) | KEY(4), KEY(0) | KEY(4) | KEY(7), KEY(13) };
    base(st);
    st->keys = STEPS[(t / 400) % 5];
}

static void chord(ui_state_t *st, uint32_t t) {
    (void)t;
    base(st);
    st->chord_mode = true;
    st->marks = KEY(13);    // Major selected
}

// C major 7th sounding with hold; root and chord notes marked
static void chord_hold(ui_state_t *st, uint32_t t) {
    (void)t;
    base(st);
    st->chord_mode = true;
    st->chord = 2;
    st->hold = true;
    st->root = 0;
    st->keys = KEY(0);
    st->marks = KEY(0) | KEY(4) | KEY(7) | KEY(11) | KEY(12) | KEY(19);
}

static void config_title(ui_state_t *st, uint32_t t) {
    (void)t;
    base(st);
    st->config = true;
    st->msg = CONFIG_MSG_TITLE;
}

static void config_brightness(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->keys = KEY(2);
    st->msg = CONFIG_MSG_BRIGHTNESS;
    st->msg_value = 7;
}

static void config_velocity(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->keys = KEY(5);
    st->msg = CONFIG_MSG_VELOCITY;
    st->msg_value = 95;
}

static void config_transpose(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->keys = KEY(11);
    st->msg = CONFIG_MSG_TRANSPOSE;
    st->msg_value = -3;
}

static void config_debounce(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->keys = KEY(16);
    st->msg = CONFIG_MSG_DEBOUNCE;
    st->msg_value = 5;
}

// Both octave buttons held towards config mode, 60 % of the way
static void normal_hold(ui_state_t *st, uint32_t t) {
    (void)t;
    base(st);
    st->progress = 153;
}

// G' held for a second: USB FLASH from 0.1 s, reboot when the border closes
static void config_hold(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->keys = KEY(19);
    st->progress = t < 1000 ? t * 255 / 1000 : 255;
    if (t >= 100) {
        st->msg = CONFIG_MSG_BOOTSEL;
    }
}

static void config_bootsel(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->keys = KEY(19);
    st->msg = CONFIG_MSG_BOOTSEL;
}

// Add a scene for every screen or animation being worked on
static const scene_t SCENES[] = {
    { "normal",            2000, 10, normal, NULL },
    { "normal-hold",       0,    0,  normal_hold, NULL },
    { "chord",             0,    0,  chord, NULL },
    { "chord-hold",        0,    0,  chord_hold, NULL },
    { "config",            0,    0,  config_title, NULL },
    { "config-brightness", 0,    0,  config_brightness, NULL },
    { "config-velocity",   0,    0,  config_velocity, NULL },
    { "config-transpose",  0,    0,  config_transpose, NULL },
    { "config-debounce",   0,    0,  config_debounce, NULL },
    { "config-hold",       1200, 30, config_hold, NULL },
    { "config-bootsel",    0,    0,  config_bootsel, NULL },
    { "splash", SPLASH_FRAMES * SPLASH_FRAME_MS, 1000 / SPLASH_FRAME_MS, NULL, splash_draw },
};

#define SCENE_COUNT (sizeof(SCENES) / sizeof(SCENES[0]))

// PNG output: 8-bit RGB, stored (uncompressed) deflate blocks

static uint32_t crc_table[256];

static uint32_t crc32(uint32_t crc, const uint8_t *p, size_t n) {
    if (!crc_table[1]) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_table[i] = c;
        }
    }
    crc = ~crc;
    while (n--) crc = crc_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static void chunk(FILE *f, const char *type, const uint8_t *data, uint32_t len) {
    uint8_t head[8];
    put32(head, len);
    memcpy(head + 4, type, 4);
    fwrite(head, 1, 8, f);
    fwrite(data, 1, len, f);
    uint32_t crc = crc32(crc32(0, (const uint8_t *)type, 4), data, len);
    uint8_t tail[4];
    put32(tail, crc);
    fwrite(tail, 1, 4, f);
}

static void write_png(const char *path, const uint8_t *rgb, int w, int h) {
    size_t raw_len = (size_t)(w * 3 + 1) * h;
    uint8_t *raw = malloc(raw_len);
    for (int y = 0; y < h; y++) {
        raw[y * (w * 3 + 1)] = 0;   // no filter
        memcpy(raw + y * (w * 3 + 1) + 1, rgb + (size_t)y * w * 3, w * 3);
    }

    size_t blocks = raw_len / 65535 + 1;
    uint8_t *z = malloc(raw_len + blocks * 5 + 6);
    size_t zl = 0;
    z[zl++] = 0x78; z[zl++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t pos = 0; pos < raw_len;) {
        size_t n = raw_len - pos > 65535 ? 65535 : raw_len - pos;
        z[zl++] = pos + n == raw_len;
        z[zl++] = n; z[zl++] = n >> 8; z[zl++] = ~n; z[zl++] = ~n >> 8;
        memcpy(z + zl, raw + pos, n);
        zl += n;
        for (size_t i = 0; i < n; i++) {
            a = (a + raw[pos + i]) % 65521;
            b = (b + a) % 65521;
        }
        pos += n;
    }
    put32(z + zl, (b << 16) | a);
    zl += 4;

    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    static const uint8_t sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    put32(ihdr, w); put32(ihdr + 4, h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    chunk(f, "IDAT", z, zl);
    chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
}

// Scaled-up OLED look: lit pixels in the panel colour, a thin gap between
// pixels, dark background. Animations become a grid of frames.
#define PNG_SCALE   4
#define PNG_COLS    4
#define PNG_BORDER  8

static void frames_png(const char *path, uint8_t *const *bufs, int n,
                       int width, int height) {
    int cols = n < PNG_COLS ? n : PNG_COLS, rows = (n + cols - 1) / cols;
    int fw = width * PNG_SCALE, fh = height * PNG_SCALE;
    int w = cols * fw + (cols - 1) * PNG_BORDER;
    int h = rows * fh + (rows - 1) * PNG_BORDER;
    uint8_t *rgb = malloc((size_t)w * h * 3);
    memset(rgb, 0x40, (size_t)w * h * 3);
    for (int i = 0; i < n; i++) {
        int x0 = (i % cols) * (fw + PNG_BORDER), y0 = (i / cols) * (fh + PNG_BORDER);
        for (int y = 0; y < fh; y++) {
            for (int x = 0; x < fw; x++) {
                int px = x / PNG_SCALE, py = y / PNG_SCALE;
                bool on = bufs[i][(py >> 3) * width + px] >> (py & 7) & 1;
                bool gap = x % PNG_SCALE == PNG_SCALE - 1 || y % PNG_SCALE == PNG_SCALE - 1;
                uint8_t *p = rgb + ((size_t)(y0 + y) * w + x0 + x) * 3;
                if (on && !gap) { p[0] = 0xE8; p[1] = 0xF0; p[2] = 0xFF; }
                else            { p[0] = 0x0B; p[1] = 0x0D; p[2] = 0x12; }
            }
        }
    }
    write_png(path, rgb, w, h);
    free(rgb);
}

// Pedal diagrams for the docs, drawn like the display

#define KB_X ((128 - WIDGET_KEYS_WIDTH) / 2)
#define DIAGRAM_KB_H 12

static bool is_white(int key) {
    return widget_key_width(key) != widget_key_width(1);
}

// Label centred under a white key (row 0 or 1) or over a black one
static void key_label(gfx_t *g, int kb_y, int key, const char *label, int row) {
    int x = widget_key_x(KB_X, key) + (widget_key_width(key) - widget_tiny_width(label)) / 2;
    int y = is_white(key) ? kb_y + DIAGRAM_KB_H + 4 + row * 7 : kb_y - 6;
    widget_tiny_text(g, x, y, label, true);
}

// Sprite with its left edge at x, centred vertically on y
static void blit_centred(gfx_t *g, const sprite_t *s, int x, int y) {
    gfx_blit(g, s, x, y - s->height / 2);
}

static void diagram_normal(gfx_t *g) {
    static const char *const NOTES[12] = { "C", "D", "E", "F", "G", "A", "B", "C", "D", "E", "F", "G" };
    gfx_clear(g);
    widget_keyboard(g, KB_X, 0, DIAGRAM_KB_H, 0, 0);
    for (int key = 0, n = 0; key < 20; key++) {
        if (is_white(key)) {
            key_label(g, 0, key, NOTES[n++], 0);
        }
    }
    // One button: one octave up or down
    blit_centred(g, &SPRITE_BUTTON_UP, 12, 28);
    widget_tiny_text(g, 26, 26, "Oct +1", true);
    blit_centred(g, &SPRITE_BUTTON_DOWN, 68, 28);
    widget_tiny_text(g, 82, 26, "Oct -1", true);
    // Both: chord mode briefly, config mode after a second
    blit_centred(g, &SPRITE_BUTTON_UP, 12, 41);
    blit_centred(g, &SPRITE_BUTTON_DOWN, 25, 41);
    widget_tiny_text(g, 40, 39, "Chord", true);
    blit_centred(g, &SPRITE_BUTTON_UP, 68, 41);
    blit_centred(g, &SPRITE_BUTTON_DOWN, 81, 41);
    widget_tiny_text(g, 96, 39, "1s", true);
    blit_centred(g, &SPRITE_SETTINGS, 106, 41);
}

static void diagram_chord(gfx_t *g) {
    static const char *const LABELS[8] = { "M7", "M", "m7", "m", "d7", "h7", "7", "H" };
    int kb_y = 6;
    gfx_clear(g);
    widget_keyboard(g, KB_X, kb_y, DIAGRAM_KB_H, 0, 0);
    // Chord types and hold on the upper eight keys; labels under neighbouring
    // white keys alternate rows, as they are as wide as the keys
    for (int key = 12, white = 0; key < 20; key++) {
        key_label(g, kb_y, key, LABELS[key - 12], is_white(key) ? white++ & 1 : 0);
    }
    // Roots: one arc under all of them
    int y = kb_y + DIAGRAM_KB_H + 1;
    widget_arc(g, KB_X, y, 0, 11);
    int left = widget_key_x(KB_X, 0), right = widget_key_x(KB_X, 11) + widget_key_width(11);
    widget_tiny_text(g, (left + right - widget_tiny_width("Chord")) / 2, y + 4, "Chord", true);
}

static void write_diagrams(const char *dir) {
    static gfx_t g;
    char path[1024];
    uint8_t *buf[1] = { g.buf };

    gfx_init(&g, 128, 48);
    diagram_normal(&g);
    snprintf(path, sizeof(path), "%s/keys-normal.png", dir);
    frames_png(path, buf, 1, g.width, g.height);

    gfx_init(&g, 128, 40);
    diagram_chord(&g);
    snprintf(path, sizeof(path), "%s/keys-chord.png", dir);
    frames_png(path, buf, 1, g.width, g.height);

    gfx_init(&g, 128, 32);
    ui_state_t st;
    memset(&st, 0, sizeof(st));
    st.root = -1;
    st.config = true;
    st.msg = CONFIG_MSG_TITLE;
    ui_render(&g, &st);
    snprintf(path, sizeof(path), "%s/keys-config.png", dir);
    frames_png(path, buf, 1, g.width, g.height);
    printf("%s: pedal diagrams\n", dir);
}

// HTML output: the template with the frames injected as hex strings

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc(n + 1);
    s[fread(s, 1, n, f)] = '\0';
    fclose(f);
    return s;
}

int main(int argc, char **argv) {
    const char *out_dir = ".";
    int width = 128, height = 32;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--diagrams") && i + 1 < argc) {
            write_diagrams(argv[++i]);
            return 0;
        }
        if (!strcmp(argv[i], "--size") && i + 1 < argc) {
            if (sscanf(argv[++i], "%dx%d", &width, &height) != 2 ||
                width < 8 || width > GFX_MAX_WIDTH ||
                height < 8 || height > GFX_MAX_HEIGHT || height % 8) {
                fprintf(stderr, "--size must be WxH, up to %dx%d, height a multiple of 8\n",
                        GFX_MAX_WIDTH, GFX_MAX_HEIGHT);
                return 1;
            }
        } else {
            out_dir = argv[i];
        }
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/preview.html", out_dir);
    FILE *html = fopen(path, "w");
    if (!html) { perror(path); return 1; }

    char *tmpl = read_file(PREVIEW_TEMPLATE);
    char *mark = strstr(tmpl, "/*SCENES*/");
    if (!mark) { fprintf(stderr, "template has no /*SCENES*/ marker\n"); return 1; }
    fwrite(tmpl, 1, mark - tmpl, html);

    static gfx_t g;
    gfx_init(&g, width, height);
    size_t bytes = (size_t)width * (height / 8);

    fprintf(html, "{ width: %d, height: %d, scenes: [\n", width, height);
    for (size_t s = 0; s < SCENE_COUNT; s++) {
        const scene_t *sc = &SCENES[s];
        uint32_t frames = sc->duration_ms ? sc->duration_ms * sc->fps / 1000 : 1;
        fprintf(html, "  { name: \"%s\", fps: %u, frames: [\n", sc->name, sc->fps);
        uint8_t **bufs = malloc(frames * sizeof(*bufs));
        for (uint32_t f = 0; f < frames; f++) {
            uint32_t t = sc->fps ? f * 1000 / sc->fps : 0;
            if (sc->draw) {
                sc->draw(&g, t);
            } else {
                ui_state_t st;
                sc->state(&st, t);
                ui_render(&g, &st);
            }
            fputs("    \"", html);
            for (size_t i = 0; i < bytes; i++) fprintf(html, "%02x", g.buf[i]);
            fputs("\",\n", html);
            bufs[f] = malloc(bytes);
            memcpy(bufs[f], g.buf, bytes);
        }
        char png[1024];
        snprintf(png, sizeof(png), "%s/%s.png", out_dir, sc->name);
        frames_png(png, bufs, frames, width, height);
        for (uint32_t f = 0; f < frames; f++) free(bufs[f]);
        free(bufs);
        fputs("  ] },\n", html);
    }
    fputs("] }", html);
    fputs(mark + strlen("/*SCENES*/"), html);
    fclose(html);
    free(tmpl);

    printf("%s: %zu scenes at %dx%d\n", path, SCENE_COUNT, width, height);
    return 0;
}
