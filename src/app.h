#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "input.h"
#include "settings.h"
#include "ui.h"

// Core 0 logic: routes debounced inputs to the octave buttons, keyboard and
// config mode. Sends a MIDI panic on init.
void app_init(settings_t *s);
void app_update(const input_state_t *in, uint32_t now_ms);

void app_ui_state(ui_state_t *out);

// True when changed settings should be written now: right after config
// mode, or once the octave has stayed put for a while
bool app_save_due(uint32_t now_ms);
