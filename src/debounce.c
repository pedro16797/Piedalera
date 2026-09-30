#include "input.h"

void debounce_init(debounce_t *d, uint32_t raw) {
    d->stable = raw;
    d->differing = 0;
}

input_state_t debounce_update(debounce_t *d, uint32_t raw, uint32_t now_ms,
                              uint32_t debounce_ms) {
    uint32_t diff = raw ^ d->stable;
    uint32_t flip = 0;

    // Bits back at their stable value stop counting
    d->differing &= diff;

    for (uint32_t pending = diff; pending; pending &= pending - 1) {
        uint32_t i = __builtin_ctz(pending);
        uint32_t bit = 1u << i;
        if (!(d->differing & bit)) {
            d->differing |= bit;
            d->since[i] = now_ms;
        }
        if (now_ms - d->since[i] >= debounce_ms) {
            flip |= bit;
        }
    }

    d->differing &= ~flip;
    d->stable ^= flip;
    return (input_state_t){
        .pressed = d->stable,
        .down = flip & d->stable,
        .up = flip & ~d->stable,
    };
}
