#include "battery.h"

const char *const BATTERY_NAMES[BATTERY_TYPES] = { "none", "alkaline", "nimh", "li-ion" };

static const char *const LABELS[BATTERY_TYPES] = { "None", "Alk.", "NiMH", "Li-ion" };

// Volts per cell at 100, 80, 60, 40, 20, 5 and 0 % under a light load
#define POINTS 7
static const uint8_t PERCENT[POINTS] = { 100, 80, 60, 40, 20, 5, 0 };
static const uint16_t CURVES[BATTERY_TYPES][POINTS] = {
    [BATTERY_ALKALINE] = { 1550, 1450, 1350, 1270, 1180, 1080, 1000 },
    [BATTERY_NIMH]     = { 1380, 1300, 1260, 1230, 1190, 1120, 1000 },
    [BATTERY_LIION]     = { 4150, 4000, 3880, 3800, 3730, 3620, 3300 },
};

const char *battery_label(uint8_t type) {
    return LABELS[type < BATTERY_TYPES ? type : BATTERY_NONE];
}

uint8_t battery_level(uint8_t type, uint8_t cells, uint32_t mv) {
    if (type == BATTERY_NONE || type >= BATTERY_TYPES || cells == 0) {
        return 0;
    }
    const uint16_t *curve = CURVES[type];
    uint32_t cell = mv / cells;
    if (cell >= curve[0]) {
        return 255;
    }
    for (int i = 1; i < POINTS; i++) {
        if (cell >= curve[i]) {
            // Linear between the two points around it
            uint32_t pct = PERCENT[i] * 100 + (PERCENT[i - 1] - PERCENT[i]) * 100 *
                           (cell - curve[i]) / (curve[i - 1] - curve[i]);
            return pct * 255 / 10000;
        }
    }
    return 0;
}
