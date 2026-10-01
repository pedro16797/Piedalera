#include "expression.h"

#define SMOOTHING   8   // each reading moves the average 1/8 of the way
#define SLACK       8   // counts past the travel before it widens
#define HYSTERESIS  12  // in 1/16 steps: a step and a half wide

static bool started;
static int32_t filtered;    // ADC counts * 16
static int value;           // last returned, -1 if none

void expression_init(void) {
    started = false;
    value = -1;
}

void expression_forget(settings_t *s) {
    s->expression_min = 4095;
    s->expression_max = 0;
}

bool expression_ready(const settings_t *s) {
    return s->expression_max - s->expression_min >= EXPRESSION_MIN_SPAN;
}

// Position within the travel in 1/16 steps, 0-127 * 16; -1 with no travel.
// A little dead zone at both ends, so 0 and 127 are always reached.
static int position(const settings_t *s) {
    int span = s->expression_max - s->expression_min;
    if (span <= 0) {
        return -1;
    }
    int margin = span / 32;
    int lo = s->expression_min + margin;
    span -= 2 * margin;
    int pos = (filtered - lo * 16) * 127 / span;
    pos = pos < 0 ? 0 : pos > 127 * 16 ? 127 * 16 : pos;
    return s->expression_invert ? 127 * 16 - pos : pos;
}

int expression_position(const settings_t *s) {
    if (expression_ready(s) && value >= 0) {
        return value;
    }
    int pos = started ? position(s) : -1;
    return pos < 0 ? -1 : (pos + 8) / 16;
}

int expression_update(settings_t *s, uint16_t raw, bool learn) {
    int32_t in = raw * 16;
    filtered = started ? filtered + (in - filtered) / SMOOTHING : in;
    started = true;
    int now = filtered / 16;

    // A fresh travel (min above max) starts at the first reading
    if (learn) {
        if (s->expression_min > s->expression_max) {
            s->expression_min = s->expression_max = now;
        } else if (now + SLACK < s->expression_min) {
            s->expression_min = now;
        } else if (now > s->expression_max + SLACK) {
            s->expression_max = now;
        }
    }
    if (!expression_ready(s)) {
        return -1;
    }
    int pos = position(s);
    if (value >= 0 && pos - value * 16 <= HYSTERESIS && value * 16 - pos <= HYSTERESIS) {
        return -1;
    }
    value = (pos + 8) / 16;
    return value;
}
