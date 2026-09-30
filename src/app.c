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
static bool bootsel_saved;  // saved while USB FLASH shows
static bool save_pending;
static uint32_t save_at;
static uint32_t pressed;    // last debounced inputs, for the screen
static uint8_t progress;    // of the current key hold, for the screen

static void request_save(uint32_t now, uint32_t delay) {
    save_pending = true;
    save_at = now + delay;
}

void app_init(settings_t *s) {
    settings = s;
    config = false;
    bootsel = false;
    pressed = 0;
    progress = 0;
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
    bootsel_saved = false;
    config_mode_enter(settings);
}

static uint8_t hold_progress(uint32_t held, uint32_t total) {
    return held >= total ? 255 : held * 255 / total;
}

void app_update(const input_state_t *in, uint32_t now) {
    pressed = in->pressed;
    if (bootsel) {
        return;
    }
    progress = 0;
    if (config) {
        config_result_t result = config_mode_update(in, now);
        if (result != CONFIG_EXIT) {
            progress = hold_progress(config_mode_hold_ms(now), BOOTSEL_HOLD_MS);
        }
        // Save as soon as USB FLASH shows, so the reboot needn't wait
        int value;
        if (config_mode_msg(&value) == CONFIG_MSG_BOOTSEL && !bootsel_saved) {
            bootsel_saved = true;
            request_save(now, 0);
        }
        switch (result) {
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
    progress = hold_progress(octave_hold_ms(now), OCTAVE_CONFIG_HOLD_MS);
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
    out->keys = pressed & KEYS_MASK;
    out->marks = keyboard_marks();
    out->root = keyboard_root();
    out->octave = settings->octave_current;
    out->chord_mode = keyboard_chord_mode();
    out->chord = keyboard_chord();
    out->hold = keyboard_hold();
    out->config = config;
    out->brightness = settings->display_brightness;
    out->progress = progress;
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
    if (!save_pending || (config && !bootsel_saved) || (int32_t)(now - save_at) < 0) {
        return false;
    }
    save_pending = false;
    return true;
}
