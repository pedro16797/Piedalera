#pragma once

#include <stdbool.h>
#include <stdint.h>

// System clock at 48 MHz, and dormant sleep. Core 0 only.
void power_init(void);

// Stop every clock until an armed input wakes the chip (see input_wake),
// then run at 48 MHz again. RAM and pins keep their state; the millisecond
// timer stands still meanwhile. Core 1 must be idle.
void power_dormant(void);

// VSYS in mV, read through the Pico's divider on GPIO29; only at full speed,
// since the ADC is clocked by the USB PLL
uint32_t power_vsys_mv(void);

// USB plugged in, on boards that sense VBUS on a GPIO (not the W ones)
bool power_vbus(void);
