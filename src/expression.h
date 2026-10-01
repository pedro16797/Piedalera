#pragma once

#include <stdint.h>

#include "settings.h"

// Expression pedal: smooths the ADC reading, learns the pedal's travel as it
// is played and turns its position into a controller value 0-127

// Travel needed before any value is sent, in ADC counts
#define EXPRESSION_MIN_SPAN 256

void expression_init(void);

// Forget the learnt travel, so it is learnt again from the next reading
void expression_forget(settings_t *s);

// A new 12-bit reading, every scan. Returns the new value, or -1 if it
// hasn't changed. Widens s->expression_min / max when the pedal goes past
// them.
int expression_update(settings_t *s, uint16_t raw);
