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
} config_msg_t;

typedef enum {
    CONFIG_STAY,
    CONFIG_EXIT,
    CONFIG_BOOTSEL,     // reboot into the USB bootloader to flash firmware
} config_result_t;

// Holding this key (G') leaves for the bootloader instead of just leaving
#define KEY_BOOTSEL         19
#define BOOTSEL_HOLD_MS     1000

// Function keys adjust settings while held; any other key leaves on release
void config_mode_enter(settings_t *s);

config_result_t config_mode_update(const input_state_t *in, uint32_t now_ms);

// Whether any setting changed since entering
bool config_mode_changed(void);

config_msg_t config_mode_msg(int *value);
