#include <string.h>

#include "app.h"
#include "board.h"
#include "display.h"
#include "hardware/sync.h"
#include "midi.h"
#include "pico/multicore.h"
#include "settings.h"
#include "ui.h"

#define SCAN_PERIOD_US      1000
#define FRAME_PERIOD_US     16667   // 60 fps cap
#define DISPLAY_RETRY_MS    500

static settings_t settings;

// Core 0 -> core 1 hand-over, guarded by a hardware spinlock
static spin_lock_t *lock;
static ui_state_t shared_ui;
static uint32_t ui_version;
static settings_t shared_save;
static bool save_requested;

static void publish_ui(const ui_state_t *st) {
    uint32_t irq = spin_lock_blocking(lock);
    shared_ui = *st;
    ui_version++;
    spin_unlock(lock, irq);
}

static void request_save(const settings_t *s) {
    uint32_t irq = spin_lock_blocking(lock);
    shared_save = *s;
    save_requested = true;
    spin_unlock(lock, irq);
}

// Core 1: display and flash writes, neither of which may delay core 0
static void core1_main(void) {
    static gfx_t gfx;
    ui_state_t st;
    uint32_t seen = 0;
    bool redraw = true;
    uint32_t retry_at = 0;

    gfx_init(&gfx, settings.display_width, settings.display_height);
    absolute_time_t next = get_absolute_time();

    while (true) {
        next = delayed_by_us(next, FRAME_PERIOD_US);
        uint32_t now = to_ms_since_boot(get_absolute_time());

        settings_t to_save;
        bool save = false;
        uint32_t irq = spin_lock_blocking(lock);
        if (ui_version != seen) {
            seen = ui_version;
            st = shared_ui;
            redraw = true;
        }
        if (save_requested) {
            to_save = shared_save;
            save_requested = false;
            save = true;
        }
        spin_unlock(lock, irq);

        if (save) {
            settings_save(&to_save);
        }

        display_poll();
        if (!display_ok()) {
            if ((int32_t)(now - retry_at) < 0) {
                sleep_until(next);
                continue;
            }
            retry_at = now + DISPLAY_RETRY_MS;
            if (!display_init(settings.display_height,
                              settings.display_col_offset)) {
                sleep_until(next);
                continue;
            }
            redraw = true;
        }

        if (redraw && seen) {
            ui_render(&gfx, &st);
            display_send(&gfx, st.brightness);
            redraw = !display_ok();
        }
        sleep_until(next);
    }
}

// Core 0: inputs, note logic and MIDI, every millisecond
int main(void) {
    settings_load(&settings);
    input_init(settings.keys_pull, settings.keys_active_low);
    midi_init();
    app_init(&settings);

    lock = spin_lock_init(spin_lock_claim_unused(true));
    ui_state_t ui, last_ui;
    app_ui_state(&ui);
    publish_ui(&ui);
    last_ui = ui;
    multicore_launch_core1(core1_main);

    debounce_t debounce;
    debounce_init(&debounce, input_read());
    absolute_time_t next = get_absolute_time();

    while (true) {
        next = delayed_by_us(next, SCAN_PERIOD_US);
        uint32_t now = to_ms_since_boot(get_absolute_time());

        input_state_t in = debounce_update(&debounce, input_read(), now,
                                           settings.keys_debounce_ms);
        app_update(&in, now);

        app_ui_state(&ui);
        if (memcmp(&ui, &last_ui, sizeof(ui)) != 0) {
            publish_ui(&ui);
            last_ui = ui;
        }
        if (app_save_due(now)) {
            request_save(&settings);
        }
        sleep_until(next);
    }
}
