#include "board.h"
#include "input.h"

static uint32_t invert;

void input_init(pull_t pull, bool active_low) {
    for (int i = 0; i < INPUT_COUNT; i++) {
        uint pin = INPUT_PINS[i];
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_set_pulls(pin, pull == PULL_UP, pull == PULL_DOWN);
    }
    invert = active_low ? ~0u : 0;
}

// Pressed inputs as input bits, from a single read of all GPIOs
uint32_t input_read(void) {
    uint32_t gpio = gpio_get_all() ^ invert;
    uint32_t bits = 0;
    for (int i = 0; i < INPUT_COUNT; i++) {
        bits |= ((gpio >> INPUT_PINS[i]) & 1u) << i;
    }
    return bits;
}
