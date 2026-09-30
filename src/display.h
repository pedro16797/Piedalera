#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx.h"

// SSD1305 over I2C, frames sent by DMA so core 1 can draw the next one
// meanwhile. Returns false if the display doesn't answer (e.g. unpowered).
bool display_init(uint8_t height, uint8_t col_offset);

// Waits for the previous frame, then starts sending this one
void display_send(const gfx_t *g, uint8_t contrast);

// Puts the panel to sleep (off) or wakes it; frames sent while asleep are
// kept for when it wakes
void display_power(bool on);

// Notices a frame that wasn't acknowledged; call every frame period
void display_poll(void);

// False once a transfer failed; call display_init again
bool display_ok(void);
