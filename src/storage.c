#include <string.h>

#include "board.h"
#include "hardware/flash.h"
#include "settings.h"

// The firmware runs from RAM (copy_to_ram), so erasing flash here never
// stalls the other core. Only core 1 writes; flash is only read here.
static const char *const stored = (const char *)(XIP_BASE + CONFIG_FLASH_OFFSET);

void settings_load(settings_t *s) {
    settings_parse(s, stored, FLASH_SECTOR_SIZE);
}

bool settings_save(const settings_t *s) {
    static uint8_t sector[FLASH_SECTOR_SIZE];

    memset(sector, 0xFF, sizeof(sector));
    settings_format(s, (char *)sector, sizeof(sector));
    if (memcmp(sector, stored, sizeof(sector)) == 0) {
        return true;
    }
    flash_range_erase(CONFIG_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(CONFIG_FLASH_OFFSET, sector, FLASH_SECTOR_SIZE);
    return memcmp(sector, stored, sizeof(sector)) == 0;
}
