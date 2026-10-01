#pragma once

#include <stdint.h>

// Battery chemistries for power.battery. The charge is estimated from the
// voltage per cell along a typical discharge curve.
// Li-ion covers LiPo cells too.
typedef enum { BATTERY_NONE, BATTERY_ALKALINE, BATTERY_NIMH, BATTERY_LIION } battery_t;

#define BATTERY_TYPES 4

// Setting names, indexed by battery_t
extern const char *const BATTERY_NAMES[BATTERY_TYPES];

// Short name for the screen, e.g. "NiMH"
const char *battery_label(uint8_t type);

// Charge 0-255 from the battery voltage of `cells` cells in series
uint8_t battery_level(uint8_t type, uint8_t cells, uint32_t mv);
