#pragma once

#include <stdbool.h>
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

// Whether mv is too high for the batteries, so the board runs from USB or
// another supply: above full charge by this much
#define BATTERY_EXTERNAL_MV 300
bool battery_external(uint8_t type, uint8_t cells, uint32_t mv);

// Charge 0-255 from the battery voltage of `cells` cells in series
uint8_t battery_level(uint8_t type, uint8_t cells, uint32_t mv);
