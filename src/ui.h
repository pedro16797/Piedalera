#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx.h"
#include "settings.h"

// Snapshot of what the display shows, published by core 0
typedef struct {
    uint32_t keys;      // pressed inputs: keys 0-19, then the octave buttons
    uint32_t marks;     // keys to mark, see keyboard_marks()
    uint8_t octave;
    bool chord_mode;
    uint8_t chord;
    int8_t root;        // sounding chord root, -1 if none
    bool hold;
    bool config;
    uint8_t msg;        // config_msg_t
    int16_t msg_value;
    uint8_t brightness;
    uint8_t progress;   // of a key hold towards an action, 0-255
    bool battery;           // running on batteries: show the gauge
    uint8_t battery_level;  // 0-255
    uint8_t battery_type;   // battery_t
    uint8_t battery_cells;
    uint16_t battery_mv;
    bool battery_external;  // running from USB or another supply instead
    bool expression;        // pedal enabled
    bool expression_ready;  // and its travel learnt
    uint8_t sound_count;
    char sound[SOUND_NAME_MAX + 1];    // msg_value's name in config mode, if any
} ui_state_t;

// Transitions played over the snapshots, and when an input last changed
typedef struct {
    ui_state_t last;
    uint32_t changed_at;    // any change of the snapshot
    uint32_t mode_at;       // chord mode toggled: banner
    uint32_t octave_at;     // octave changed: the number slides
    uint8_t octave_from;
    uint32_t value_at;      // config value changed: the same
    int16_t value_from;
    bool started;
} ui_anim_t;

#define UI_BANNER_MS    800
#define UI_SLIDE_MS     150
#define UI_BLINK_MS     500
#define UI_BATTERY_LOW  24      // level below which the gauge warns

// Notes what changed since the previous snapshot
void ui_anim_update(ui_anim_t *a, const ui_state_t *st, uint32_t now_ms);

// True while a transition needs a new frame every period
bool ui_anim_running(const ui_anim_t *a, uint32_t now_ms);

// Time since the snapshot last changed, i.e. since the last input
uint32_t ui_idle_ms(const ui_anim_t *a, uint32_t now_ms);

// a may be NULL for no transitions
void ui_render(gfx_t *g, const ui_state_t *st, const ui_anim_t *a, uint32_t now_ms);
