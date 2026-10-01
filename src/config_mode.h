#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "input.h"
#include "settings.h"

typedef enum {
    CONFIG_MSG_TITLE,
    CONFIG_MSG_BRIGHTNESS,
    CONFIG_MSG_VELOCITY,
    CONFIG_MSG_BANK,
    CONFIG_MSG_TRANSPOSE,
    CONFIG_MSG_DEBOUNCE,
    CONFIG_MSG_BOOTSEL,
    CONFIG_MSG_BATTERY,
    CONFIG_MSG_EXPRESSION,
} config_msg_t;

typedef enum {
    CONFIG_STAY,
    CONFIG_EXIT,
    CONFIG_BOOTSEL,     // reboot into the USB bootloader to flash firmware
} config_result_t;

// Holding this key (G') shows USB FLASH after BOOTSEL_SHOW_MS, then leaves
// for the bootloader. A tap goes back to the map from a setting, and
// leaves from the map.
#define KEY_BOOTSEL         19
#define BOOTSEL_SHOW_MS     100
#define BOOTSEL_HOLD_MS     1000

// With a battery set up, this key (F') shows its charge and voltage
#define KEY_BATTERY         17

// A setting's screen goes back to the map after this long untouched; the
// last CONFIG_IDLE_BORDER_MS show on the border
#define CONFIG_IDLE_MS          3000
#define CONFIG_IDLE_BORDER_MS   1000

// Function keys adjust settings while held; any other key leaves on release
void config_mode_enter(settings_t *s);

config_result_t config_mode_update(const input_state_t *in, uint32_t now_ms);

// The expression pedal moved to value: shown from the map, and its page
// stays while it moves
void config_mode_expression(uint8_t value, uint32_t now_ms);

// Whether any setting changed since entering
bool config_mode_changed(void);

config_msg_t config_mode_msg(int *value);

// How long G' has been held towards USB flash mode, 0 if not
uint32_t config_mode_hold_ms(uint32_t now_ms);

// How long a setting's screen has been untouched, 0 on the map
uint32_t config_mode_idle_ms(uint32_t now_ms);

// Keys that turn a setting down and up (the same key twice for bootsel);
// false for CONFIG_MSG_TITLE and CONFIG_MSG_EXPRESSION
bool config_mode_keys(config_msg_t msg, uint8_t *down, uint8_t *up);
