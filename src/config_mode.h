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
} config_msg_t;

// Function keys adjust settings while held; any other key leaves on release
void config_mode_enter(settings_t *s);

// Returns false once config mode ends
bool config_mode_update(const input_state_t *in, uint32_t now_ms);

// Whether any setting changed since entering
bool config_mode_changed(void);

config_msg_t config_mode_msg(int *value);
