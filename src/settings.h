#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SOUNDS_MAX      32
#define SOUND_NAME_MAX  15

// A synth sound: bank select and program change
typedef struct {
    uint8_t msb, lsb;
    uint8_t program;            // 1-128, as synth manuals number them
    char name[SOUND_NAME_MAX + 1];
} sound_t;

// Keys and ranges must match tools/config2uf2.py and docs/configuration.md
typedef struct {
    uint8_t display_width;
    uint8_t display_height;
    uint8_t display_col_offset;
    uint8_t display_brightness;
    bool display_splash;
    uint16_t display_dim_s;     // 0: never
    uint16_t display_off_s;     // 0: never
    uint8_t midi_channel;       // 1-16
    uint8_t midi_velocity;
    int8_t midi_transpose;
    uint8_t midi_sound;         // 0: the synth's own, else sounds[n - 1]
    uint8_t octave_min;
    uint8_t octave_max;
    uint8_t octave_current;
    uint16_t octave_delay_ms;
    uint16_t octave_repeat_ms;
    bool keys_active_low;
    uint8_t keys_pull;          // pull_t
    uint8_t keys_debounce_ms;
    uint8_t keys_alternative;   // 0 chord, 1 mode
    uint16_t power_sleep_s;     // 0: never
    uint8_t power_battery;      // battery_t
    uint8_t power_cells;
    uint16_t power_drop_mv;     // across the supply diode, added to VSYS
    bool expression_enabled;
    uint8_t expression_cc;
    bool expression_invert;
    uint16_t expression_min;    // travel in ADC counts, learnt as it is played;
    uint16_t expression_max;    // min above max: not learnt yet
    uint8_t sound_count;        // "sound = msb lsb program name" lines
    sound_t sounds[SOUNDS_MAX];
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
