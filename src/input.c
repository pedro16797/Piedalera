#include "board.h"
#include "hardware/adc.h"
#include "input.h"

#define EXPRESSION_SAMPLES 8

static uint32_t invert;

void input_init(pull_t pull, bool active_low) {
    for (int i = 0; i < INPUT_COUNT; i++) {
        uint pin = INPUT_PINS[i];
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_set_pulls(pin, pull == PULL_UP, pull == PULL_DOWN);
    }
    invert = active_low ? ~0u : 0;
    adc_gpio_init(PIN_EXPRESSION);
}

void input_wake(bool on) {
    uint32_t edge = invert ? GPIO_IRQ_EDGE_FALL : GPIO_IRQ_EDGE_RISE;
    for (int i = 0; i < INPUT_COUNT; i++) {
        // Clear old edges first, or they would wake it straight away
        gpio_acknowledge_irq(INPUT_PINS[i], edge);
        gpio_set_dormant_irq_enabled(INPUT_PINS[i], edge, on);
    }
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

// Average of a few readings, about 20 us; the ADC must be running (power.c)
uint16_t input_expression(void) {
    adc_select_input(ADC_EXPRESSION);
    uint32_t sum = 0;
    for (int i = 0; i < EXPRESSION_SAMPLES; i++) {
        sum += adc_read();
    }
    return sum / EXPRESSION_SAMPLES;
}
