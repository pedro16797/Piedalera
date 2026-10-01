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

// A new VSYS reading, and whether USB is plugged in (false if the board
// can't tell); the battery voltage and charge follow it smoothly
void app_battery(uint32_t vsys_mv, bool usb);

// A new expression pedal reading (12-bit): sends its controller when the
// value changes, if enabled; a learnt wider travel is saved like the octave
void app_expression(uint16_t raw, uint32_t now_ms);

// Time since an input was last held or released
uint32_t app_idle_ms(uint32_t now_ms);

// True once config mode asked to reboot into the USB bootloader; the
// settings should be saved first
bool app_bootsel(void);

// True, once, if changed settings wait to be written, e.g. before sleeping
bool app_save_pending(void);

// True when changed settings should be written now: right after config
// mode, or once the octave or pedal travel has stayed put for a while
bool app_save_due(uint32_t now_ms);
