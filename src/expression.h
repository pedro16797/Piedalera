#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

// Expression pedal: smooths the ADC reading, learns the pedal's travel while
// allowed and turns its position into a controller value 0-127

// Travel needed before any value is sent, in ADC counts
#define EXPRESSION_MIN_SPAN 256

void expression_init(void);

// Forget the learnt travel, so it is learnt again when next allowed
void expression_forget(settings_t *s);

// Whether the travel is wide enough to send values
bool expression_ready(const settings_t *s);

// A new 12-bit reading, every scan. Returns the new value, or -1 if it
// hasn't changed or the travel isn't ready. With learn, widens
// s->expression_min / max when the pedal goes past them.
int expression_update(settings_t *s, uint16_t raw, bool learn);

// The value last sent; before the travel is ready, the position 0-127
// within what is learnt so far. -1 before a travel or a reading.
int expression_position(const settings_t *s);
