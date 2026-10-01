#include <string.h>

#include "app.h"
#include "board.h"
#include "display.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "input.h"
#include "midi.h"
#include "pico/bootrom.h"
#include "pico/multicore.h"
#include "power.h"
#include "settings.h"
#include "splash.h"
#include "ui.h"

#define SCAN_PERIOD_US      1000
#define FRAME_PERIOD_US     16667   // 60 fps cap
#define DISPLAY_RETRY_MS    500
#define SAVE_WAIT_MS        2000
#define BOOTSEL_SHOW_WAIT_MS 50     // for the last frame to be drawn
#define FRAME_SEND_MS       13      // I2C transfer of a 128x32 frame
#define DIM_DIVISOR         4       // idle contrast is brightness / this
#define BATTERY_READ_MS     1000
#define PARK_WAIT_MS        200     // for core 1 to finish a frame and park
#define WAKE_GRACE_MS       100     // to see the press that woke it
#define SLEEP_RETRY_MS      1000
#define WATCHDOG_MS         3000    // longer than any wait, e.g. a flash save
#define CORE1_STALL_MS      2000    // core 1 silent this long: let it reset

static settings_t settings;

// Core 0 -> core 1 hand-over, guarded by a hardware spinlock
static spin_lock_t *lock;
static ui_state_t shared_ui;
static uint32_t ui_version;
static settings_t shared_save;
static bool save_requested;
static uint32_t saves_done;
static uint32_t ui_shown;   // last ui_version sent to the display
static volatile bool park_requested;    // core 0 wants to stop the clocks
static volatile bool parked;            // core 1 is idle for it
static volatile uint32_t core1_beats;   // core 1 loop count, for the watchdog

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
    ui_anim_t anim = { 0 };
    uint32_t seen = 0;
    bool redraw = true;
    bool asleep = false;
    int sent_contrast = -1;
    uint32_t retry_at = 0;

    // The splash starts once the display answers and ends early if the UI
    // changes (a button was used); not after a watchdog reset, mid-song
    bool splash = settings.display_splash && !watchdog_enable_caused_reboot();
    bool splash_started = false;
    uint32_t splash_at = 0, splash_version = 0;
    int splash_frame = -1;

    gfx_init(&gfx, settings.display_width, settings.display_height);
    absolute_time_t next = get_absolute_time();

    while (true) {
        next = delayed_by_us(next, FRAME_PERIOD_US);
        uint32_t now = to_ms_since_boot(get_absolute_time());
        core1_beats++;

        settings_t to_save;
        bool save = false;
        bool fresh = false;
        uint32_t irq = spin_lock_blocking(lock);
        if (ui_version != seen) {
            seen = ui_version;
            st = shared_ui;
            redraw = fresh = true;
        }
        if (save_requested) {
            to_save = shared_save;
            save_requested = false;
            save = true;
        }
        spin_unlock(lock, irq);

        if (save) {
            settings_save(&to_save);
            irq = spin_lock_blocking(lock);
            saves_done++;
            spin_unlock(lock, irq);
        }
        if (fresh) {
            ui_anim_update(&anim, &st, now);
        }

        // Deep sleep: screen off, then wait here while the clocks stop
        if (park_requested) {
            if (!asleep && display_ok()) {
                display_power(false);
            }
            asleep = true;
            parked = true;
            while (park_requested) {
                tight_loop_contents();
            }
            parked = false;
            next = get_absolute_time();
            continue;
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
            asleep = false;
        }

        if (splash) {
            if (!splash_started) {
                splash_started = true;
                splash_at = now;
                splash_version = seen;
            }
            uint32_t t = now - splash_at;
            if (seen == splash_version && splash_draw(&gfx, t)) {
                if ((int)(t / SPLASH_FRAME_MS) != splash_frame) {
                    splash_frame = t / SPLASH_FRAME_MS;
                    display_send(&gfx, st.brightness);
                }
                sleep_until(next);
                continue;
            }
            splash = false;
            redraw = true;
        }

        // Without input for a while the screen dims, then sleeps; any input
        // changes the snapshot and wakes it
        uint32_t idle = ui_idle_ms(&anim, now);
        bool off = settings.display_off_s && idle >= settings.display_off_s * 1000u;
        bool dim = settings.display_dim_s && idle >= settings.display_dim_s * 1000u;
        uint8_t contrast = dim ? st.brightness / DIM_DIVISOR : st.brightness;
        if (off != asleep) {
            display_power(!off);
            asleep = off;
            redraw = true;
        }
        if (asleep) {
            sleep_until(next);
            continue;
        }

        if (seen && (redraw || contrast != sent_contrast || ui_anim_running(&anim, now))) {
            ui_render(&gfx, &st, &anim, now);
            display_send(&gfx, contrast);
            sent_contrast = contrast;
            redraw = !display_ok();
            irq = spin_lock_blocking(lock);
            ui_shown = seen;
            spin_unlock(lock, irq);
        }
        sleep_until(next);
    }
}

static uint32_t get_saves_done(void) {
    uint32_t irq = spin_lock_blocking(lock);
    uint32_t n = saves_done;
    spin_unlock(lock, irq);
    return n;
}

static bool ui_up_to_date(void) {
    uint32_t irq = spin_lock_blocking(lock);
    bool done = ui_shown == ui_version;
    spin_unlock(lock, irq);
    return done;
}

static void save_and_wait(void) {
    uint32_t before = get_saves_done();
    request_save(&settings);
    absolute_time_t timeout = make_timeout_time_ms(SAVE_WAIT_MS);
    while (get_saves_done() == before && !time_reached(timeout)) {
        tight_loop_contents();
    }
}

// Save the settings (already done while USB FLASH showed, unless they changed
// since), let the screen show the full border, then reboot into the USB
// bootloader so new firmware can be copied onto the drive
static void reboot_to_bootsel(void) {
    save_and_wait();
    absolute_time_t timeout = make_timeout_time_ms(BOOTSEL_SHOW_WAIT_MS);
    while (!ui_up_to_date() && !time_reached(timeout)) {
        tight_loop_contents();
    }
    sleep_ms(FRAME_SEND_MS);
    // Or it would reset the bootloader while the drive is open
    watchdog_disable();
    reset_usb_boot(0, 0);
}

// Save what's pending, let core 1 turn the screen off and park, then stop
// the clocks until an input is pressed. False if core 1 didn't park.
static bool deep_sleep(void) {
    if (app_save_pending()) {
        save_and_wait();
    }
    park_requested = true;
    absolute_time_t timeout = make_timeout_time_ms(PARK_WAIT_MS);
    while (!parked && !time_reached(timeout)) {
        tight_loop_contents();
    }
    bool ok = parked;
    if (ok) {
        // Armed before checking, so a press from here on wakes it at once
        input_wake(true);
        ok = input_read() == 0;
        if (ok) {
            power_dormant();
        }
        input_wake(false);
    }
    park_requested = false;
    return ok;
}

// Core 0: inputs, note logic and MIDI, every millisecond
int main(void) {
    power_init();
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
    uint32_t battery_at = 0;
    uint32_t retry_at = 0;      // next time to check whether to sleep

    // Resets the Pico if either core hangs; it stands still in deep sleep,
    // and a reset starts with a MIDI panic, so no note is left hanging
    uint32_t beats = core1_beats, core1_at = 0;
    watchdog_enable(WATCHDOG_MS, true);

    while (true) {
        next = delayed_by_us(next, SCAN_PERIOD_US);
        uint32_t now = to_ms_since_boot(get_absolute_time());

        input_state_t in = debounce_update(&debounce, input_read(), now,
                                           settings.keys_debounce_ms);
        app_update(&in, now);
        if (settings.expression_enabled) {
            app_expression(input_expression(), now);
        }
        // After power.sleep_s without input, sleep until the next press. The
        // timer stood still meanwhile, so give the waking press a moment to
        // show before sleeping again.
        if (settings.power_sleep_s && (int32_t)(now - retry_at) >= 0 &&
            app_idle_ms(now) >= settings.power_sleep_s * 1000u) {
            retry_at = now + (deep_sleep() ? WAKE_GRACE_MS : SLEEP_RETRY_MS);
            next = get_absolute_time();
        }
        if (now - battery_at >= BATTERY_READ_MS) {
            battery_at = now;
            app_battery(power_vsys_mv());
        }

        app_ui_state(&ui);
        if (memcmp(&ui, &last_ui, sizeof(ui)) != 0) {
            publish_ui(&ui);
            last_ui = ui;
        }
        if (app_save_due(now)) {
            request_save(&settings);
        }
        if (app_bootsel()) {
            reboot_to_bootsel();
        }
        if (core1_beats != beats) {
            beats = core1_beats;
            core1_at = now;
        }
        if (now - core1_at < CORE1_STALL_MS) {
            watchdog_update();
        }
        sleep_until(next);
    }
}
