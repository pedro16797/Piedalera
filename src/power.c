#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pll.h"
#include "hardware/rosc.h"
#include "hardware/xosc.h"
#include "midi.h"
#include "power.h"

// 48 MHz from the USB PLL is plenty for the 1 ms scan and 60 fps; the
// system PLL stays off.

#define VSYS_SAMPLES 16

void power_init(void) {
    set_sys_clock_48mhz();
    adc_init();
    adc_gpio_init(PICO_VSYS_PIN);
#if PICO_CYW43_SUPPORTED
    // W boards share the pin with the (unused) wireless chip; its chip select
    // high keeps it off the pin
    gpio_init(CYW43_DEFAULT_PIN_WL_CS);
    gpio_set_dir(CYW43_DEFAULT_PIN_WL_CS, GPIO_OUT);
    gpio_put(CYW43_DEFAULT_PIN_WL_CS, 1);
#endif
}

uint32_t power_vsys_mv(void) {
    adc_select_input(PICO_VSYS_PIN - ADC_BASE_PIN);
    uint32_t sum = 0;
    for (int i = 0; i < VSYS_SAMPLES; i++) {
        sum += adc_read();
    }
    // 12 bits of 3.3 V, and the divider is 1/3
    return sum * 3300 * 3 / (4096 * VSYS_SAMPLES);
}

void power_dormant(void) {
    midi_flush();
    // Everything onto the crystal, PLL off, then stop the crystal itself
    clock_configure_undivided(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLK_REF, 0, XOSC_HZ);
    clock_configure_undivided(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                              XOSC_HZ);
    pll_deinit(pll_usb);
    // Nothing runs from the ring oscillator once the crystal is up, but a
    // watchdog reboot (USB flash mode) keeps it as it is and restarts from
    // it, so it is only off while dormant
    uint32_t rosc = rosc_hw->ctrl;
    rosc_disable();
    xosc_dormant();
    rosc_write(&rosc_hw->ctrl, rosc);
    while (!(rosc_hw->status & ROSC_STATUS_STABLE_BITS)) {
        tight_loop_contents();
    }

    // Woken: back to 48 MHz; clk_peri returns to the rate MIDI was set up for
    pll_init(pll_usb, PLL_USB_REFDIV, PLL_USB_VCO_FREQ_HZ, PLL_USB_POSTDIV1,
             PLL_USB_POSTDIV2);
    set_sys_clock_48mhz();
}
