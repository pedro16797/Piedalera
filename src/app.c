#include <string.h>

#include "app.h"
#include "battery.h"
#include "config_mode.h"
#include "expression.h"
#include "keyboard.h"
#include "midi.h"
#include "notes.h"
#include "octave.h"

// Octave and pedal travel changes are saved once they stay put this long
#define SAVE_DELAY_MS 5000

static settings_t *settings;
static bool config;
static bool bootsel;
static bool bootsel_saved;  // saved while USB FLASH shows
static bool save_pending;
static uint32_t save_at;
static uint32_t pressed;    // last debounced inputs, for the screen
static uint8_t progress;    // of the current key hold, for the screen
static uint32_t input_at;   // last time an input was held or released
static uint32_t battery_mv; // filtered, 0 until the first reading
static bool vbus;           // USB plugged in, where the board can tell
static bool expression_on;  // readings coming in since it was enabled

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
    input_at = 0;
    battery_mv = 0;
    vbus = false;
    expression_on = false;
    save_pending = false;
    notes_init(s->midi_channel - 1);
    notes_panic();
    keyboard_init(s);
    octave_init(s);
}

// Entering config mode also silences the synth, for any note left stuck
static void enter_config(bool undo_toggle) {
    if (undo_toggle) {
        keyboard_set_chord_mode(!keyboard_chord_mode());
    }
    keyboard_reset();
    notes_panic();
    config = true;
    bootsel_saved = false;
    config_mode_enter(settings);
}

static uint8_t hold_progress(uint32_t held, uint32_t total) {
    return held >= total ? 255 : held * 255 / total;
}

// Turned off in config mode: back to full expression, so the synth isn't
// left quiet
static void expression_off(void) {
    if (expression_on && !settings->expression_enabled) {
        expression_on = false;
        midi_cc(settings->midi_channel - 1, settings->expression_cc, 127);
    }
}

void app_update(const input_state_t *in, uint32_t now) {
    pressed = in->pressed;
    expression_off();
    if (in->pressed || in->up) {
        input_at = now;
    }
    if (bootsel) {
        return;
    }
    progress = 0;
    if (config) {
        config_result_t result = config_mode_update(in, now);
        if (result != CONFIG_EXIT) {
            progress = hold_progress(config_mode_hold_ms(now), BOOTSEL_HOLD_MS);
            // Last stretch before a setting's screen goes back to the map
            uint32_t idle = config_mode_idle_ms(now);
            if (idle > CONFIG_IDLE_MS - CONFIG_IDLE_BORDER_MS) {
                progress = hold_progress(idle - (CONFIG_IDLE_MS - CONFIG_IDLE_BORDER_MS),
                                         CONFIG_IDLE_BORDER_MS);
            }
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
        request_save(now, SAVE_DELAY_MS);
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
    out->keys = pressed & (KEYS_MASK | INPUT_BIT(INPUT_OCT_UP) | INPUT_BIT(INPUT_OCT_DOWN));
    out->marks = keyboard_marks();
    out->root = keyboard_root();
    out->octave = settings->octave_current;
    out->chord_mode = keyboard_chord_mode();
    out->chord = keyboard_chord();
    out->hold = keyboard_hold();
    out->config = config;
    out->brightness = settings->display_brightness;
    out->progress = progress;
    out->expression = settings->expression_enabled;
    out->expression_ready = expression_ready(settings);
    if (settings->power_battery != BATTERY_NONE && battery_mv) {
        out->battery = true;
        out->battery_type = settings->power_battery;
        out->battery_cells = settings->power_cells;
        out->battery_mv = (battery_mv + 5) / 10 * 10;     // calmer on screen
        out->battery_external = vbus || battery_external(settings->power_battery,
                                                         settings->power_cells, out->battery_mv);
        out->battery_level = out->battery_external ? 255 :
            battery_level(settings->power_battery, settings->power_cells, out->battery_mv);
    }
    if (config) {
        int value;
        out->msg = config_mode_msg(&value);
        out->msg_value = value;
    }
}

void app_battery(uint32_t vsys_mv, bool usb) {
    vbus = usb;
    uint32_t mv = vsys_mv + settings->power_drop_mv;
    // Average over about 8 readings
    battery_mv = battery_mv ? battery_mv + ((int32_t)mv - (int32_t)battery_mv) / 8 : mv;
}

void app_expression(uint16_t raw, uint32_t now) {
    if (!settings->expression_enabled) {
        return;
    }
    if (!expression_on) {
        expression_on = true;
        expression_init();
    }
    // The travel is only learnt in config mode, on the map or the pedal's
    // page, so a pedal unplugged while playing can't spoil it
    int unused;
    config_msg_t msg = config_mode_msg(&unused);
    bool learn = config && (msg == CONFIG_MSG_TITLE || msg == CONFIG_MSG_EXPRESSION);
    uint16_t lo = settings->expression_min, hi = settings->expression_max;
    int value = expression_update(settings, raw, learn);
    bool widened = settings->expression_min != lo || settings->expression_max != hi;
    if (widened) {
        request_save(now, SAVE_DELAY_MS);
    }
    if (value >= 0) {
        midi_cc(settings->midi_channel - 1, settings->expression_cc, value);
    }
    // A fresh travel starting isn't a movement
    bool moved = value >= 0 || (widened && lo <= hi);
    if (moved) {
        input_at = now;
    }
    if (config) {
        config_mode_expression(expression_position(settings), moved, now);
    }
}

uint32_t app_idle_ms(uint32_t now) {
    return now - input_at;
}

bool app_bootsel(void) {
    return bootsel;
}

bool app_save_pending(void) {
    bool pending = save_pending;
    save_pending = false;
    return pending;
}

bool app_save_due(uint32_t now) {
    if (!save_pending || (config && !bootsel_saved) || (int32_t)(now - save_at) < 0) {
        return false;
    }
    save_pending = false;
    return true;
}
