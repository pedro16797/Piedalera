#pragma once

#include <stdint.h>

#include "gfx.h"

// Keyboard strip of the 20 pedals: 12 white keys WIDGET_KEY_STEP wide, black
// keys as notches over the gaps. Keys are outlines, filled while pressed;
// marked keys (e.g. chord tones) are dotted.
#define WIDGET_KEY_STEP 10
#define WIDGET_KEYS_WIDTH (12 * WIDGET_KEY_STEP)

void widget_keyboard(gfx_t *g, int x, int y, int h, uint32_t pressed,
                     uint32_t marked);

// Left edge of a key (0-19) in a strip drawn at x, and its width
int widget_key_x(int x, int key);
int widget_key_width(int key);

// "\___/" pointer under keys a..b (inclusive) of a strip drawn at x
void widget_arc(gfx_t *g, int x, int y, int key_a, int key_b);

// 3x5 text for key labels, lit or dark; digits, A-H, M O, c d h m o r s t,
// + - and space only
void widget_tiny_text(gfx_t *g, int x, int y, const char *str, bool on);
int widget_tiny_width(const char *str);

// Vertical battery, WIDGET_BATTERY_W x WIDGET_BATTERY_H, filled solid in
// proportion to level (0-255) and emptying from the top a row at a time;
// warn draws an exclamation mark inside
#define WIDGET_BATTERY_W    5
#define WIDGET_BATTERY_H    10
#define WIDGET_BATTERY_ROWS 7
void widget_battery(gfx_t *g, int x, int y, uint8_t level, bool warn);

// Screen border inverted clockwise from the top middle, progress/255 of the
// way round, while a key is held towards an action
void widget_hold_border(gfx_t *g, uint8_t progress);

// Horizontal bar, filled in proportion to value within [lo, hi]; from the
// middle when the range spans zero
void widget_bar(gfx_t *g, int x, int y, int w, int h, int value, int lo, int hi);
