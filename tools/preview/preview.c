// Renders UI scenes with the firmware's own drawing code (gfx.c, ui.c) and
// writes preview.html (playable, OLED-styled) plus a PNG per scene.
//
//   preview [output dir] [--size WxH]

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config_mode.h"
#include "gfx.h"
#include "midi.h"
#include "ui.h"

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
} scene_t;

static void base(ui_state_t *st) {
    memset(st, 0, sizeof(*st));
    st->octave = 3;
    st->brightness = 15;
}

static void normal(ui_state_t *st, uint32_t t) {
    (void)t;
    base(st);
}

static void chord(ui_state_t *st, uint32_t t) {
    (void)t;
    base(st);
    st->chord_mode = true;
}

static void chord_hold(ui_state_t *st, uint32_t t) {
    (void)t;
    base(st);
    st->chord_mode = true;
    st->chord = 3;
    st->hold = true;
}

static void config_title(ui_state_t *st, uint32_t t) {
    (void)t;
    base(st);
    st->config = true;
    st->msg = CONFIG_MSG_TITLE;
}

static void config_velocity(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->msg = CONFIG_MSG_VELOCITY;
    st->msg_value = 95;
}

static void config_transpose(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->chord_mode = true;
    st->msg = CONFIG_MSG_TRANSPOSE;
    st->msg_value = -3;
}

static void config_debounce(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->msg = CONFIG_MSG_DEBOUNCE;
    st->msg_value = 5;
}

static void config_bootsel(ui_state_t *st, uint32_t t) {
    config_title(st, t);
    st->msg = CONFIG_MSG_BOOTSEL;
}

// Shows playback works; replace with real animations as they are written
static void octave_sweep(ui_state_t *st, uint32_t t) {
    base(st);
    st->octave = (t / 500) % 8;
}

// Add a scene for every screen or animation being worked on
static const scene_t SCENES[] = {
    { "normal",           0,    0,  normal },
    { "chord",            0,    0,  chord },
    { "chord-hold",       0,    0,  chord_hold },
    { "config",           0,    0,  config_title },
    { "config-velocity",  0,    0,  config_velocity },
    { "config-transpose", 0,    0,  config_transpose },
    { "config-debounce",  0,    0,  config_debounce },
    { "config-bootsel",   0,    0,  config_bootsel },
    { "octave-sweep",     4000, 30, octave_sweep },
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
// pixels, dark background
#define PNG_SCALE 4

static void frame_png(const char *path, const gfx_t *g) {
    int w = g->width * PNG_SCALE, h = g->height * PNG_SCALE;
    uint8_t *rgb = malloc((size_t)w * h * 3);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int px = x / PNG_SCALE, py = y / PNG_SCALE;
            bool on = g->buf[(py >> 3) * g->width + px] >> (py & 7) & 1;
            bool gap = x % PNG_SCALE == PNG_SCALE - 1 || y % PNG_SCALE == PNG_SCALE - 1;
            uint8_t *p = rgb + ((size_t)y * w + x) * 3;
            if (on && !gap) { p[0] = 0xE8; p[1] = 0xF0; p[2] = 0xFF; }
            else            { p[0] = 0x0B; p[1] = 0x0D; p[2] = 0x12; }
        }
    }
    write_png(path, rgb, w, h);
    free(rgb);
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
        for (uint32_t f = 0; f < frames; f++) {
            ui_state_t st;
            sc->state(&st, sc->fps ? f * 1000 / sc->fps : 0);
            ui_render(&g, &st);
            fputs("    \"", html);
            for (size_t i = 0; i < bytes; i++) fprintf(html, "%02x", g.buf[i]);
            fputs("\",\n", html);
            if (f == 0) {
                char png[1024];
                snprintf(png, sizeof(png), "%s/%s.png", out_dir, sc->name);
                frame_png(png, &g);
            }
        }
        fputs("  ] },\n", html);
    }
    fputs("] }", html);
    fputs(mark + strlen("/*SCENES*/"), html);
    fclose(html);
    free(tmpl);

    printf("%s: %zu scenes at %dx%d\n", path, SCENE_COUNT, width, height);
    return 0;
}
