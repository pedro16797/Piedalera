#pragma once

#include <stdbool.h>
#include <stdint.h>

// Input bits: keys 0-19 from C upwards, then the octave buttons
#define KEY_COUNT       20
#define INPUT_OCT_UP    20
#define INPUT_OCT_DOWN  21
#define INPUT_COUNT     22

#define INPUT_BIT(i)    (1u << (i))
#define KEYS_MASK       (INPUT_BIT(KEY_COUNT) - 1)

typedef struct {
    uint32_t pressed;   // debounced state
    uint32_t down;      // became pressed this scan
    uint32_t up;        // became released this scan
} input_state_t;

typedef struct {
    uint32_t stable;
    uint32_t since[INPUT_COUNT];    // ms when a bit started to differ
    uint32_t differing;
} debounce_t;

// Debounce raw pressed bits: a change counts once stable for debounce_ms
void debounce_init(debounce_t *d, uint32_t raw);
input_state_t debounce_update(debounce_t *d, uint32_t raw, uint32_t now_ms,
                              uint32_t debounce_ms);

// Hardware side, see input.c
typedef enum { PULL_NONE, PULL_UP, PULL_DOWN } pull_t;

void input_init(pull_t pull, bool active_low);
uint32_t input_read(void);

// Expression pedal position, 12 bits; only at full speed, as power_vsys_mv
uint16_t input_expression(void);

// Its readings with the pin's pull-up, then its pull-down; no pull after
void input_expression_probe(uint16_t *up, uint16_t *down);

// Arm (or disarm) every input to wake the chip from dormant when pressed
void input_wake(bool on);
