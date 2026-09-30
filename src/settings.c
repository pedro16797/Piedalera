#include <string.h>

#include "input.h"
#include "settings.h"

#define HEADER "# piedalera-config v1"

typedef enum { T_U8, T_I8, T_U16, T_BOOL, T_PULL } type_t;

typedef struct {
    const char *name;
    uint16_t offset;
    uint8_t type;
    int16_t min, max, def;
} field_t;

#define F(key, member, type, min, max, def) \
    { key, offsetof(settings_t, member), type, min, max, def }

static const field_t FIELDS[] = {
    F("display.width",      display_width,      T_U8,   64,  128,  128),
    F("display.height",     display_height,     T_U8,   32,   64,   32),
    F("display.col_offset", display_col_offset, T_U8,    0,    4,    4),
    F("display.brightness", display_brightness, T_U8,    0,  255,   15),
    F("display.splash",     display_splash,     T_BOOL,  0,    1,    1),
    F("midi.channel",       midi_channel,       T_U8,    1,   16,    1),
    F("midi.velocity",      midi_velocity,      T_U8,    1,  127,   95),
    F("midi.transpose",     midi_transpose,     T_I8,  -12,   12,    0),
    F("midi.bank_lsb",      midi_bank_lsb,      T_U8,    0,  127,   12),
    F("octave.min",         octave_min,         T_U8,    0,    8,    0),
    F("octave.max",         octave_max,         T_U8,    0,    8,    7),
    F("octave.current",     octave_current,     T_U8,    0,    8,    3),
    F("octave.delay_ms",    octave_delay_ms,    T_U16,   0,  900,  100),
    F("octave.repeat_ms",   octave_repeat_ms,   T_U16,  50, 5000,  500),
    F("keys.active_low",    keys_active_low,    T_BOOL,  0,    1,    1),
    F("keys.pull",          keys_pull,          T_PULL,  0,    2, PULL_NONE),
    F("keys.debounce_ms",   keys_debounce_ms,   T_U8,    0,   50,    5),
};

#define FIELD_COUNT (sizeof(FIELDS) / sizeof(FIELDS[0]))

// Indexed by pull_t
static const char *const PULL_NAMES[] = { "none", "up", "down" };

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
    if (f->type == T_PULL) {
        for (int i = 0; i <= PULL_DOWN; i++) {
            if (equals(v, len, PULL_NAMES[i])) { *out = i; return true; }
        }
        return false;
    }
    return parse_int(v, len, out) && *out >= f->min && *out <= f->max;
}

static void parse_line(settings_t *s, const char *line, size_t len) {
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

    for (size_t pos = 0; pos < len;) {
        const char *nl = memchr(text + pos, '\n', len - pos);
        size_t line_len = nl ? (size_t)(nl - text) - pos : len - pos;
        parse_line(s, text + pos, line_len);
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
    return true;
}

static size_t append(char *out, size_t size, size_t pos, const char *str) {
    while (*str && pos + 1 < size) {
        out[pos++] = *str++;
    }
    return pos;
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
        } else if (f->type == T_PULL) {
            value = PULL_NAMES[v <= PULL_DOWN ? v : PULL_NONE];
        } else {
            // Right-aligned decimal
            char *p = num + sizeof(num) - 1;
            unsigned n = v < 0 ? -v : v;
            *p = '\0';
            do { *--p = '0' + n % 10; n /= 10; } while (n);
            if (v < 0) *--p = '-';
            value = p;
        }
        pos = append(out, size, pos, f->name);
        pos = append(out, size, pos, " = ");
        pos = append(out, size, pos, value);
        pos = append(out, size, pos, "\n");
    }
    out[pos] = '\0';
    return pos;
}
