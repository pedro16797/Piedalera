#include <string.h>

#include "app.h"
#include "config_mode.h"
#include "keyboard.h"
#include "notes.h"
#include "octave.h"

// Octave changes are saved once the octave stays put this long
#define OCTAVE_SAVE_DELAY_MS 5000

static settings_t *settings;
static bool config;
static bool bootsel;
static bool save_pending;
static uint32_t save_at;

static void request_save(uint32_t now, uint32_t delay) {
    save_pending = true;
    save_at = now + delay;
}

void app_init(settings_t *s) {
    settings = s;
    config = false;
    bootsel = false;
    save_pending = false;
    notes_init(s->midi_channel - 1);
    notes_panic();
    keyboard_init(s);
    octave_init(s);
}

static void enter_config(bool undo_toggle) {
    if (undo_toggle) {
        keyboard_set_chord_mode(!keyboard_chord_mode());
    } else {
        keyboard_reset();
    }
    config = true;
    config_mode_enter(settings);
}

void app_update(const input_state_t *in, uint32_t now) {
    if (bootsel) {
        return;
    }
    if (config) {
        switch (config_mode_update(in, now)) {
        case CONFIG_EXIT:
            config = false;
            octave_block();
            if (config_mode_changed()) {
                request_save(now, 0);
            }
            break;
        case CONFIG_BOOTSEL:
            bootsel = true;
            break;
        default:
            break;
        }
        return;
    }

    bool up = in->pressed & INPUT_BIT(INPUT_OCT_UP);
    bool down = in->pressed & INPUT_BIT(INPUT_OCT_DOWN);
    octave_event_t event = octave_update(up, down, now);
    switch (event) {
    case OCTAVE_CHANGED:
        request_save(now, OCTAVE_SAVE_DELAY_MS);
        break;
    case OCTAVE_TOGGLE_CHORD:
        keyboard_set_chord_mode(!keyboard_chord_mode());
        break;
    case OCTAVE_ENTER_CONFIG:
    case OCTAVE_ENTER_CONFIG_UNDO:
        enter_config(event == OCTAVE_ENTER_CONFIG_UNDO);
        return;
    default:
        break;
    }

    uint8_t octave = settings->octave_current;
    for (uint32_t b = in->up & KEYS_MASK; b; b &= b - 1) {
        keyboard_release(__builtin_ctz(b), octave);
    }
    for (uint32_t b = in->down & KEYS_MASK; b; b &= b - 1) {
        keyboard_press(__builtin_ctz(b), octave);
    }
}

void app_ui_state(ui_state_t *out) {
    // Zeroed padding too, so snapshots can be compared with memcmp
    memset(out, 0, sizeof(*out));
    out->octave = settings->octave_current;
    out->chord_mode = keyboard_chord_mode();
    out->chord = keyboard_chord();
    out->hold = keyboard_hold();
    out->config = config;
    out->brightness = settings->display_brightness;
    if (config) {
        int value;
        out->msg = config_mode_msg(&value);
        out->msg_value = value;
    }
}

bool app_bootsel(void) {
    return bootsel;
}

bool app_save_due(uint32_t now) {
    if (!save_pending || config || (int32_t)(now - save_at) < 0) {
        return false;
    }
    save_pending = false;
    return true;
}
