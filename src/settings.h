#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Keys and ranges must match tools/config2uf2.py and docs/configuration.md
typedef struct {
    uint8_t display_width;
    uint8_t display_height;
    uint8_t display_col_offset;
    uint8_t display_brightness;
    uint8_t midi_channel;       // 1-16
    uint8_t midi_velocity;
    int8_t midi_transpose;
    uint8_t midi_bank_lsb;
    uint8_t octave_min;
    uint8_t octave_max;
    uint8_t octave_current;
    uint16_t octave_delay_ms;
    uint16_t octave_repeat_ms;
    bool keys_active_low;
    uint8_t keys_pull;          // pull_t
    uint8_t keys_debounce_ms;
} settings_t;

#define SETTINGS_TEXT_MAX 4096

void settings_defaults(settings_t *s);

// Parse settings text (NUL or len terminated). Unknown keys are ignored and
// invalid values keep their default. Returns false if the header is missing,
// in which case everything is default.
bool settings_parse(settings_t *s, const char *text, size_t len);

// Write settings text including the NUL; returns its length without the NUL
size_t settings_format(const settings_t *s, char *out, size_t size);

// Hardware side, see storage.c
void settings_load(settings_t *s);
bool settings_save(const settings_t *s);
