#include "config_mode.h"
#include "midi.h"

typedef enum { BRIGHTNESS, VELOCITY, BANK, TRANSPOSE, DEBOUNCE } target_t;

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
    {  7, BANK,        -1, 500, 200 },  // G
    {  9, BANK,         1, 500, 200 },  // A
    { 11, TRANSPOSE,   -1, 500, 200 },  // B
    { 12, TRANSPOSE,    1, 200, 100 },  // C'
    { 14, DEBOUNCE,    -1, 500, 100 },  // D'
    { 16, DEBOUNCE,     1, 500, 100 },  // E'
};

#define FUNCTION_COUNT (sizeof(FUNCTIONS) / sizeof(FUNCTIONS[0]))
#define NO_KEY 0xFF

static settings_t *settings;
static uint32_t next_step[FUNCTION_COUNT];
static uint32_t repeating;  // function keys pressed since entering
static uint8_t exit_key;
static uint32_t exit_down_at;
static bool changed;
static config_msg_t msg;
static int msg_value;

void config_mode_enter(settings_t *s) {
    settings = s;
    exit_key = NO_KEY;
    repeating = 0;
    changed = false;
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
    case BANK:
        before = s->midi_bank_lsb;
        after = msg_value = s->midi_bank_lsb = clamp(before + f->delta, 0, 127);
        msg = CONFIG_MSG_BANK;
        if (after != before) {
            uint8_t ch = s->midi_channel - 1;
            midi_cc(ch, MIDI_CC_BANK_MSB, MIDI_BANK_MSB_GM2);
            midi_cc(ch, MIDI_CC_BANK_LSB, after);
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

    // Any other key: leave when it is released
    if (down && exit_key == NO_KEY) {
        exit_key = __builtin_ctz(down);
        exit_down_at = now;
    }
    if (exit_key == NO_KEY) {
        return CONFIG_STAY;
    }
    if (in->up & INPUT_BIT(exit_key)) {
        return CONFIG_EXIT;
    }
    if (exit_key == KEY_BOOTSEL && now - exit_down_at >= BOOTSEL_HOLD_MS) {
        msg = CONFIG_MSG_BOOTSEL;
        return CONFIG_BOOTSEL;
    }
    return CONFIG_STAY;
}

bool config_mode_changed(void) {
    return changed;
}

bool config_mode_keys(config_msg_t msg, uint8_t *down, uint8_t *up) {
    if (msg == CONFIG_MSG_BOOTSEL) {
        *down = *up = KEY_BOOTSEL;
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

config_msg_t config_mode_msg(int *value) {
    *value = msg_value;
    return msg;
}
