#include <string.h>

#include "battery.h"
#include "input.h"
#include "settings.h"

#define HEADER "# piedalera-config v1"

typedef enum { T_U8, T_I8, T_U16, T_BOOL, T_NAME } type_t;

typedef struct {
    const char *name;
    uint16_t offset;
    uint8_t type;
    int16_t min, max, def;
    const char *const *names;   // T_NAME: value names, 0 to max
} field_t;

#define F(key, member, type, min, max, def) \
    { key, offsetof(settings_t, member), type, min, max, def, NULL }

// A uint8_t stored as the index of its name
#define N(key, member, names, count, def) \
    { key, offsetof(settings_t, member), T_NAME, 0, (count) - 1, def, names }

// Indexed by pull_t
static const char *const PULL_NAMES[] = { "none", "up", "down" };

// Indexed by keys_alternative
static const char *const ALTERNATIVE_NAMES[] = { "chord", "scale" };

static const field_t FIELDS[] = {
    F("display.width",      display_width,      T_U8,   64,  128,  128),
    F("display.height",     display_height,     T_U8,   32,   64,   32),
    F("display.col_offset", display_col_offset, T_U8,    0,    4,    4),
    F("display.brightness", display_brightness, T_U8,    0,  255,   15),
    F("display.splash",     display_splash,     T_BOOL,  0,    1,    1),
    F("display.dim_s",      display_dim_s,      T_U16,   0, 3600,   60),
    F("display.off_s",      display_off_s,      T_U16,   0, 3600,  300),
    F("midi.channel",       midi_channel,       T_U8,    1,   16,    1),
    F("midi.velocity",      midi_velocity,      T_U8,    1,  127,   95),
    F("midi.transpose",     midi_transpose,     T_I8,  -12,   12,    0),
    F("midi.sound",         midi_sound,         T_U8,    0, SOUNDS_MAX, 0),
    F("octave.min",         octave_min,         T_U8,    0,    8,    0),
    F("octave.max",         octave_max,         T_U8,    0,    8,    7),
    F("octave.current",     octave_current,     T_U8,    0,    8,    3),
    F("octave.delay_ms",    octave_delay_ms,    T_U16,   0,  900,  100),
    F("octave.repeat_ms",   octave_repeat_ms,   T_U16,  50, 5000,  500),
    F("keys.active_low",    keys_active_low,    T_BOOL,  0,    1,    1),
    N("keys.pull",          keys_pull,          PULL_NAMES, 3, PULL_NONE),
    F("keys.debounce_ms",   keys_debounce_ms,   T_U8,    0,   50,    5),
    N("keys.alternative",   keys_alternative,   ALTERNATIVE_NAMES, 2, 0),
    F("power.sleep_s",      power_sleep_s,      T_U16,   0, 7200,  600),
    N("power.battery",      power_battery,      BATTERY_NAMES, BATTERY_TYPES, BATTERY_LIION),
    F("power.cells",        power_cells,        T_U8,    1,    4,    1),
    F("power.drop_mv",      power_drop_mv,      T_U16,   0, 1000,  300),
    F("expression.enabled", expression_enabled, T_BOOL,  0,    1,    0),
    F("expression.cc",      expression_cc,      T_U8,    0,  119,   11),
    F("expression.invert",  expression_invert,  T_BOOL,  0,    1,    0),
    F("expression.min",     expression_min,     T_U16,   0, 4095, 4095),
    F("expression.max",     expression_max,     T_U16,   0, 4095,    0),
};

#define FIELD_COUNT (sizeof(FIELDS) / sizeof(FIELDS[0]))

// General MIDI programs, which the Yamaha MU5 plays from bank 0
static const sound_t DEFAULT_SOUNDS[] = {
    { 0, 0,   1, "Grand Piano" },
    { 0, 0,   5, "E.Piano" },
    { 0, 0,   7, "Harpsichord" },
    { 0, 0,  22, "Accordion" },
    { 0, 0,  18, "Perc. Organ" },
    { 0, 0,  20, "Church Organ" },
    { 0, 0,  53, "Choir" },
    { 0, 0,  49, "Strings" },
    { 0, 0,  62, "Brass" },
    { 0, 0,  91, "Polysynth" },
    { 0, 0,  15, "Tubular Bells" },
    { 0, 0, 102, "Goblin" },
};

#define DEFAULT_SOUND_COUNT (sizeof(DEFAULT_SOUNDS) / sizeof(DEFAULT_SOUNDS[0]))

static void set(settings_t *s, const field_t *f, int v) {
    uint8_t *p = (uint8_t *)s + f->offset;
    switch (f->type) {
    case T_U16:  *(uint16_t *)p = (uint16_t)v; break;
    case T_I8:   *(int8_t *)p = (int8_t)v; break;
    case T_BOOL: *(bool *)p = v != 0; break;
    default:     *p = (uint8_t)v; break;
    }
}

static int get(const settings_t *s, const field_t *f) {
    const uint8_t *p = (const uint8_t *)s + f->offset;
    switch (f->type) {
    case T_U16:  return *(const uint16_t *)p;
    case T_I8:   return *(const int8_t *)p;
    case T_BOOL: return *(const bool *)p;
    default:     return *p;
    }
}

void settings_defaults(settings_t *s) {
    memset(s, 0, sizeof(*s));
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        set(s, &FIELDS[i], FIELDS[i].def);
    }
    s->sound_count = DEFAULT_SOUND_COUNT;
    memcpy(s->sounds, DEFAULT_SOUNDS, sizeof(DEFAULT_SOUNDS));
}

static bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static bool equals(const char *a, size_t len, const char *b) {
    return strlen(b) == len && memcmp(a, b, len) == 0;
}

// Decimal integer with optional sign, nothing else
static bool parse_int(const char *v, size_t len, int *out) {
    size_t i = 0;
    bool neg = false;
    if (len && (v[0] == '-' || v[0] == '+')) {
        neg = v[0] == '-';
        i = 1;
    }
    if (i == len || len - i > 5) {
        return false;
    }
    int n = 0;
    for (; i < len; i++) {
        if (v[i] < '0' || v[i] > '9') {
            return false;
        }
        n = n * 10 + (v[i] - '0');
    }
    *out = neg ? -n : n;
    return true;
}

static bool parse_value(const field_t *f, const char *v, size_t len, int *out) {
    if (f->type == T_BOOL) {
        if (equals(v, len, "true"))  { *out = 1; return true; }
        if (equals(v, len, "false")) { *out = 0; return true; }
        return false;
    }
    if (f->type == T_NAME) {
        for (int i = 0; i <= f->max; i++) {
            if (equals(v, len, f->names[i])) { *out = i; return true; }
        }
        return false;
    }
    return parse_int(v, len, out) && *out >= f->min && *out <= f->max;
}

// "msb lsb program name", the name being the rest of the line
static bool parse_sound(sound_t *out, const char *v, size_t len) {
    static const int16_t LO[3] = { 0, 0, 1 }, HI[3] = { 127, 127, 128 };
    int n[3];
    for (int i = 0; i < 3; i++) {
        size_t word = 0;
        while (word < len && !is_space(v[word])) word++;
        if (!parse_int(v, word, &n[i]) || n[i] < LO[i] || n[i] > HI[i]) {
            return false;
        }
        while (word < len && is_space(v[word])) word++;
        v += word;
        len -= word;
    }
    if (len == 0 || len > SOUND_NAME_MAX) {
        return false;
    }
    out->msb = n[0];
    out->lsb = n[1];
    out->program = n[2];
    memset(out->name, 0, sizeof(out->name));
    memcpy(out->name, v, len);
    return true;
}

// The first sound line replaces the default list
static void parse_line(settings_t *s, const char *line, size_t len, bool *own_sounds) {
    // Strip comments and surrounding spaces
    const char *hash = memchr(line, '#', len);
    if (hash) {
        len = hash - line;
    }
    const char *eq = memchr(line, '=', len);
    if (!eq) {
        return;
    }
    const char *k = line, *k_end = eq;
    const char *v = eq + 1, *v_end = line + len;
    while (k < k_end && is_space(*k)) k++;
    while (k_end > k && is_space(k_end[-1])) k_end--;
    while (v < v_end && is_space(*v)) v++;
    while (v_end > v && is_space(v_end[-1])) v_end--;

    sound_t sound;
    if (equals(k, k_end - k, "sound") && parse_sound(&sound, v, v_end - v)) {
        if (!*own_sounds) {
            *own_sounds = true;
            s->sound_count = 0;
        }
        if (s->sound_count < SOUNDS_MAX) {
            s->sounds[s->sound_count++] = sound;
        }
        return;
    }
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        int value;
        if (equals(k, k_end - k, FIELDS[i].name) &&
            parse_value(&FIELDS[i], v, v_end - v, &value)) {
            set(s, &FIELDS[i], value);
            return;
        }
    }
}

bool settings_parse(settings_t *s, const char *text, size_t len) {
    settings_defaults(s);
    const char *end = memchr(text, '\0', len);
    if (end) {
        len = end - text;
    }
    size_t header = strlen(HEADER);
    if (len < header || memcmp(text, HEADER, header) != 0) {
        return false;
    }

    bool own_sounds = false;
    for (size_t pos = 0; pos < len;) {
        const char *nl = memchr(text + pos, '\n', len - pos);
        size_t line_len = nl ? (size_t)(nl - text) - pos : len - pos;
        parse_line(s, text + pos, line_len, &own_sounds);
        pos += line_len + 1;
    }

    // Cross-field rules, same as the tool
    settings_t d;
    settings_defaults(&d);
    if (s->display_height % 8) {
        s->display_height = d.display_height;
    }
    if (s->display_width + s->display_col_offset > 132) {
        s->display_width = d.display_width;
        s->display_col_offset = d.display_col_offset;
    }
    if (s->octave_min > s->octave_max) {
        s->octave_min = d.octave_min;
        s->octave_max = d.octave_max;
    }
    if (s->octave_current < s->octave_min) s->octave_current = s->octave_min;
    if (s->octave_current > s->octave_max) s->octave_current = s->octave_max;
    if (s->midi_sound > s->sound_count) {
        s->midi_sound = 0;
    }
    return true;
}

static size_t append(char *out, size_t size, size_t pos, const char *str) {
    while (*str && pos + 1 < size) {
        out[pos++] = *str++;
    }
    return pos;
}

// Decimal, right-aligned in num; returns where it starts
static const char *decimal(char num[8], int v) {
    char *p = num + 7;
    unsigned n = v < 0 ? -v : v;
    *p = '\0';
    do { *--p = '0' + n % 10; n /= 10; } while (n);
    if (v < 0) *--p = '-';
    return p;
}

size_t settings_format(const settings_t *s, char *out, size_t size) {
    size_t pos = append(out, size, 0, HEADER "\n");
    for (size_t i = 0; i < FIELD_COUNT; i++) {
        const field_t *f = &FIELDS[i];
        int v = get(s, f);
        char num[8];
        const char *value = num;
        if (f->type == T_BOOL) {
            value = v ? "true" : "false";
        } else if (f->type == T_NAME) {
            value = f->names[v <= f->max ? v : f->def];
        } else {
            value = decimal(num, v);
        }
        pos = append(out, size, pos, f->name);
        pos = append(out, size, pos, " = ");
        pos = append(out, size, pos, value);
        pos = append(out, size, pos, "\n");
    }
    for (int i = 0; i < s->sound_count; i++) {
        const sound_t *snd = &s->sounds[i];
        char num[8];
        pos = append(out, size, pos, "sound = ");
        pos = append(out, size, pos, decimal(num, snd->msb));
        pos = append(out, size, pos, " ");
        pos = append(out, size, pos, decimal(num, snd->lsb));
        pos = append(out, size, pos, " ");
        pos = append(out, size, pos, decimal(num, snd->program));
        pos = append(out, size, pos, " ");
        pos = append(out, size, pos, snd->name);
        pos = append(out, size, pos, "\n");
    }
    out[pos] = '\0';
    return pos;
}
