#include "octave.h"

typedef enum { IDLE, SINGLE, BOTH, BLOCKED } phase_t;

static settings_t *settings;
static phase_t phase;
static uint32_t since;      // when both buttons were pressed
static uint32_t next_step;  // single button: time of the next step
static bool toggled;

void octave_init(settings_t *s) {
    settings = s;
    phase = IDLE;
}

uint32_t octave_hold_ms(uint32_t now) {
    return phase == BOTH ? now - since : 0;
}

void octave_block(void) {
    phase = BLOCKED;
}

static bool step(bool up) {
    uint8_t *oct = &settings->octave_current;
    if (up && *oct < settings->octave_max) {
        (*oct)++;
        return true;
    }
    if (!up && *oct > settings->octave_min) {
        (*oct)--;
        return true;
    }
    return false;
}

octave_event_t octave_update(bool up, bool down, uint32_t now) {
    switch (phase) {
    case IDLE:
        if (up && down) {
            phase = BOTH;
            since = now;
            toggled = false;
        } else if (up || down) {
            phase = SINGLE;
            next_step = now + settings->octave_delay_ms;
        }
        return OCTAVE_NONE;

    case SINGLE:
        if (up && down) {
            phase = BOTH;
            since = now;
            toggled = false;
            return OCTAVE_NONE;
        }
        if (!up && !down) {
            phase = IDLE;
            return OCTAVE_NONE;
        }
        // First step after the delay, then every repeat interval
        if ((int32_t)(now - next_step) >= 0) {
            next_step += settings->octave_repeat_ms;
            return step(up) ? OCTAVE_CHANGED : OCTAVE_NONE;
        }
        return OCTAVE_NONE;

    case BOTH:
        if (!up || !down) {
            // A single button left over must not step the octave
            phase = (up || down) ? BLOCKED : IDLE;
            return OCTAVE_NONE;
        }
        if (now - since >= OCTAVE_CONFIG_HOLD_MS) {
            phase = BLOCKED;
            return toggled ? OCTAVE_ENTER_CONFIG_UNDO : OCTAVE_ENTER_CONFIG;
        }
        if (!toggled && now - since >= settings->octave_delay_ms) {
            toggled = true;
            return OCTAVE_TOGGLE_CHORD;
        }
        return OCTAVE_NONE;

    case BLOCKED:
        if (!up && !down) {
            phase = IDLE;
        }
        return OCTAVE_NONE;
    }
    return OCTAVE_NONE;
}
