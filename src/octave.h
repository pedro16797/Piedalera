#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

typedef enum {
    OCTAVE_NONE,
    OCTAVE_CHANGED,
    OCTAVE_TOGGLE_CHORD,        // both buttons held past the delay
    OCTAVE_ENTER_CONFIG,        // both held 1 s
    OCTAVE_ENTER_CONFIG_UNDO,   // same, after a toggle that must be undone
} octave_event_t;

#define OCTAVE_CONFIG_HOLD_MS 1000

// Current octave lives in s->octave_current
void octave_init(settings_t *s);
octave_event_t octave_update(bool up, bool down, uint32_t now_ms);

// How long both buttons have been held towards config mode, 0 if not
uint32_t octave_hold_ms(uint32_t now_ms);

// Ignore the buttons until both are released
void octave_block(void);
