#include "battery.h"
#include "config_mode.h"
#include "expression.h"
#include "notes.h"

typedef enum { BRIGHTNESS, VELOCITY, SOUND, TRANSPOSE, DEBOUNCE } target_t;

typedef struct {
    uint8_t key;
    uint8_t target;
    int8_t delta;
    uint16_t first_ms, repeat_ms;   // first auto-repeat, then each one after
} function_t;

// Legacy keys and timings, plus debounce on D' and E'
static const function_t FUNCTIONS[] = {
    {  0, BRIGHTNESS, -16, 500, 200 },  // C
    {  2, BRIGHTNESS,  16, 500, 200 },  // D
    {  4, VELOCITY,    -1, 200,  50 },  // E
    {  5, VELOCITY,     1, 200,  50 },  // F
    {  7, SOUND,       -1, 500, 200 },  // G
    {  9, SOUND,        1, 500, 200 },  // A
    { 11, TRANSPOSE,   -1, 500, 200 },  // B
    { 12, TRANSPOSE,    1, 200, 100 },  // C'
    { 14, DEBOUNCE,    -1, 500, 100 },  // D'
    { 16, DEBOUNCE,     1, 500, 100 },  // E'
};

#define FUNCTION_COUNT (sizeof(FUNCTIONS) / sizeof(FUNCTIONS[0]))
#define NO_KEY 0xFF
#define VELOCITY_KEYS (INPUT_BIT(KEY_VELOCITY_DOWN) | INPUT_BIT(KEY_VELOCITY_UP))

static settings_t *settings;
static uint32_t next_step[FUNCTION_COUNT];
static uint32_t repeating;  // function keys pressed since entering
static uint8_t exit_key;
static uint32_t exit_down_at;
static bool exit_to_map;    // G' pressed on a setting's screen
static uint32_t touched_at; // last time a key was held
static bool changed;
static bool pair_held;      // both velocity keys
static bool pair_done;      // and the pedal toggled
static uint32_t pair_at;
static uint8_t velocity_before; // restored when the second one joins
static config_msg_t msg;
static int msg_value;

void config_mode_enter(settings_t *s) {
    settings = s;
    exit_key = NO_KEY;
    repeating = 0;
    changed = false;
    pair_held = false;
    msg = CONFIG_MSG_TITLE;
}

static int clamp(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static void apply(const function_t *f) {
    settings_t *s = settings;
    int before, after;
    switch (f->target) {
    case BRIGHTNESS:
        before = s->display_brightness;
        after = s->display_brightness = clamp(before + f->delta, 0, 255);
        msg = CONFIG_MSG_BRIGHTNESS;
        msg_value = (after + 8) / 16;
        break;
    case VELOCITY:
        before = s->midi_velocity;
        after = msg_value = s->midi_velocity = clamp(before + f->delta, 1, 127);
        msg = CONFIG_MSG_VELOCITY;
        break;
    case SOUND:
        // 0 sends nothing: the synth's own choice applies from then on
        before = s->midi_sound;
        after = msg_value = s->midi_sound = clamp(before + f->delta, 0, s->sound_count);
        msg = CONFIG_MSG_SOUND;
        if (after != before && after) {
            notes_sound(&s->sounds[after - 1]);
        }
        break;
    case TRANSPOSE:
        before = s->midi_transpose;
        after = msg_value = s->midi_transpose = clamp(before + f->delta, -12, 12);
        msg = CONFIG_MSG_TRANSPOSE;
        break;
    default:
        before = s->keys_debounce_ms;
        after = msg_value = s->keys_debounce_ms = clamp(before + f->delta, 0, 50);
        msg = CONFIG_MSG_DEBOUNCE;
        break;
    }
    changed |= after != before;
}

config_result_t config_mode_update(const input_state_t *in, uint32_t now) {
    uint32_t down = in->down & KEYS_MASK;
    repeating &= in->pressed;
    if (in->pressed & KEYS_MASK) {
        touched_at = now;
    }

    // Both velocity keys: what the first one changed is undone, nothing
    // repeats, and after a second the expression pedal turns on or off
    if (!(in->pressed & ~in->down & VELOCITY_KEYS)) {
        velocity_before = settings->midi_velocity;
    }
    if ((in->pressed & VELOCITY_KEYS) == VELOCITY_KEYS) {
        down &= ~VELOCITY_KEYS;
        if (!pair_held) {
            pair_held = true;
            pair_done = false;
            pair_at = now;
            repeating &= ~VELOCITY_KEYS;
            settings->midi_velocity = velocity_before;
            msg = CONFIG_MSG_EXPRESSION;
            msg_value = -1;
        }
        if (!pair_done && now - pair_at >= EXPRESSION_HOLD_MS) {
            pair_done = true;
            settings->expression_enabled = !settings->expression_enabled;
            if (!settings->expression_enabled) {
                expression_forget(settings);    // maybe another pedal next
            }
            changed = true;
            msg = CONFIG_MSG_EXPRESSION;
            msg_value = -1;
        }
    } else {
        pair_held = false;
    }

    for (unsigned i = 0; i < FUNCTION_COUNT; i++) {
        const function_t *f = &FUNCTIONS[i];
        uint32_t bit = INPUT_BIT(f->key);
        if (down & bit) {
            down &= ~bit;
            repeating |= bit;
            apply(f);
            next_step[i] = now + f->first_ms;
        } else if ((repeating & bit) && (int32_t)(now - next_step[i]) >= 0) {
            apply(f);
            next_step[i] += f->repeat_ms;
        }
    }

    if ((down & INPUT_BIT(KEY_BATTERY)) && settings->power_battery != BATTERY_NONE) {
        down &= ~INPUT_BIT(KEY_BATTERY);
        msg = CONFIG_MSG_BATTERY;
    }

    // Any other key: leave when it is released
    if (down && exit_key == NO_KEY) {
        exit_key = __builtin_ctz(down);
        exit_down_at = now;
        exit_to_map = exit_key == KEY_BOOTSEL && msg != CONFIG_MSG_TITLE;
    }
    if (exit_key == NO_KEY) {
        if (config_mode_idle_ms(now) >= CONFIG_IDLE_MS) {
            msg = CONFIG_MSG_TITLE;
        }
        return CONFIG_STAY;
    }
    if (in->up & INPUT_BIT(exit_key)) {
        exit_key = NO_KEY;
        if (exit_to_map) {
            msg = CONFIG_MSG_TITLE;
            return CONFIG_STAY;
        }
        return CONFIG_EXIT;
    }
    if (exit_key == KEY_BOOTSEL && now - exit_down_at >= BOOTSEL_SHOW_MS) {
        msg = CONFIG_MSG_BOOTSEL;
        if (now - exit_down_at >= BOOTSEL_HOLD_MS) {
            return CONFIG_BOOTSEL;
        }
    }
    return CONFIG_STAY;
}

void config_mode_expression(int value, bool moved, uint32_t now) {
    if (moved && msg == CONFIG_MSG_TITLE) {
        msg = CONFIG_MSG_EXPRESSION;
    }
    if (msg == CONFIG_MSG_EXPRESSION) {
        msg_value = value;
        if (moved) {
            touched_at = now;
        }
    }
}

bool config_mode_changed(void) {
    return changed;
}

bool config_mode_keys(config_msg_t msg, uint8_t *down, uint8_t *up) {
    if (msg == CONFIG_MSG_EXPRESSION) {
        *down = KEY_VELOCITY_DOWN;
        *up = KEY_VELOCITY_UP;
        return true;
    }
    if (msg == CONFIG_MSG_BOOTSEL || msg == CONFIG_MSG_BATTERY) {
        *down = *up = msg == CONFIG_MSG_BOOTSEL ? KEY_BOOTSEL : KEY_BATTERY;
        return true;
    }
    bool found = false;
    for (unsigned i = 0; i < FUNCTION_COUNT; i++) {
        // Messages follow the target order, after the title
        if ((config_msg_t)(FUNCTIONS[i].target + CONFIG_MSG_BRIGHTNESS) == msg) {
            *(FUNCTIONS[i].delta < 0 ? down : up) = FUNCTIONS[i].key;
            found = true;
        }
    }
    return found;
}

uint32_t config_mode_idle_ms(uint32_t now) {
    return msg == CONFIG_MSG_TITLE ? 0 : now - touched_at;
}

uint32_t config_mode_hold_ms(uint32_t now, uint32_t *total) {
    if (pair_held && !pair_done) {
        *total = EXPRESSION_HOLD_MS;
        return now - pair_at;
    }
    *total = BOOTSEL_HOLD_MS - BOOTSEL_SHOW_MS;
    uint32_t held = exit_key == KEY_BOOTSEL ? now - exit_down_at : 0;
    return held > BOOTSEL_SHOW_MS ? held - BOOTSEL_SHOW_MS : 0;
}

config_msg_t config_mode_msg(int *value) {
    *value = msg_value;
    return msg;
}
