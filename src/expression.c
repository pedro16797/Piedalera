#include "expression.h"

#define SMOOTHING   8   // readings in the moving average, about 8 ms
#define SLACK       8   // counts past the travel before it widens
#define HYSTERESIS  12  // in 1/16 steps: a step and a half wide

static uint32_t filtered;   // ADC counts * 16, 0 until the first reading
static int value;           // last returned, -1 if none

void expression_init(void) {
    filtered = 0;
    value = -1;
}

int expression_update(settings_t *s, uint16_t raw) {
    uint32_t in = raw * 16u + 1;    // + 1: never 0 once read
    filtered = filtered ? filtered + ((int32_t)in - (int32_t)filtered) / SMOOTHING : in;
    int now = filtered / 16;

    // A fresh range (min above max) starts at the first reading
    if (now + SLACK < s->expression_min || s->expression_min > s->expression_max) {
        s->expression_min = now;
    }
    if (now > s->expression_max + SLACK) {
        s->expression_max = now;
    }
    int span = s->expression_max - s->expression_min;
    if (span < EXPRESSION_MIN_SPAN) {
        return -1;
    }

    // A little dead zone at both ends, so 0 and 127 are always reached
    int margin = span / 32;
    int lo = s->expression_min + margin;
    span -= 2 * margin;
    int pos = ((int)filtered - lo * 16) * 127 / span;     // 1/16 steps
    pos = pos < 0 ? 0 : pos > 127 * 16 ? 127 * 16 : pos;
    if (s->expression_invert) {
        pos = 127 * 16 - pos;
    }
    if (value >= 0 && pos - value * 16 <= HYSTERESIS && value * 16 - pos <= HYSTERESIS) {
        return -1;
    }
    value = (pos + 8) / 16;
    return value;
}
