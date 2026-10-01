#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "gfx.h"

#define SPLASH_FRAME_MS     50
#define SPLASH_FRAMES       50

// Draws the start-up animation at t_ms after it started, centred on the
// screen. Returns false once it has finished.
bool splash_draw(gfx_t *g, uint32_t t_ms);
