#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx.h"

// Snapshot of what the display shows, published by core 0
typedef struct {
    uint32_t keys;      // pressed keys
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
} ui_state_t;

void ui_render(gfx_t *g, const ui_state_t *st);
