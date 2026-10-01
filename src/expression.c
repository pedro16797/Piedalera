#include "expression.h"

#define SMOOTHING   8   // each reading moves the average 1/8 of the way
#define SLACK       8   // counts past the travel before it widens
#define HYSTERESIS  12  // in 1/16 steps: a step and a half wide
#define SHOW_SMOOTHING 32   // the position shown while learning, ~32 ms
#define PROBE_SPAN  2048    // pulls moving it more than this: no pedal
#define PROBE_AGREE 3       // probes in a row before it counts

static bool started;
static int32_t filtered;    // ADC counts * 16
static int value;           // last returned, -1 if none
static int32_t shown;       // position while learning, 1/512 steps; -1 if none
static int shown_value;
static bool plugged;
static uint8_t agreeing;

void expression_init(void) {
    started = false;
    value = -1;
    shown = -1;
    plugged = false;
    agreeing = 0;
}

bool expression_probe(uint16_t up, uint16_t down) {
    bool held = up < down + PROBE_SPAN;
    if (held == plugged) {
        agreeing = 0;
        return false;
    }
    if (++agreeing < PROBE_AGREE) {
        return false;
    }
    bool was = plugged;
    expression_init();
    plugged = !was;
    return true;
}

bool expression_plugged(void) {
    return plugged;
}

// Keeps v unless pos (1/16 steps) moved past the hysteresis; returns it
static int settle(int v, int pos) {
    if (v >= 0 && pos - v * 16 <= HYSTERESIS && v * 16 - pos <= HYSTERESIS) {
        return v;
    }
    return (pos + 8) / 16;
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
    return shown < 0 ? -1 : shown_value;
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
        // Shown on screen, where the widening travel would make it jump
        int pos = position(s);
        if (pos < 0) {
            shown = -1;
        } else {
            if (shown < 0) {
                shown = pos * 32;
                shown_value = -1;
            }
            shown += (pos * 32 - shown) / SHOW_SMOOTHING;
            shown_value = settle(shown_value, shown / 32);
        }
        return -1;
    }
    int v = settle(value, position(s));
    if (v == value) {
        return -1;
    }
    value = v;
    return value;
}
