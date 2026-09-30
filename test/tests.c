#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "battery.h"
#include "config_mode.h"
#include "gfx.h"
#include "input.h"
#include "keyboard.h"
#include "midi.h"
#include "notes.h"
#include "octave.h"
#include "settings.h"
#include "splash.h"
#include "ui.h"
#include "widgets.h"

static int failures;

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("%s:%d: CHECK(%s) failed\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

// Fake MIDI: records messages as "on 48 95", "off 48", "cc 123 0"

static char sent[64][24];
static int sent_count;

static char *next_slot(uint8_t ch) {
    CHECK(ch == 0);
    CHECK(sent_count < 64);
    return sent[sent_count < 63 ? sent_count++ : 63];
}

void midi_note_on(uint8_t ch, uint8_t note, uint8_t velocity) {
    snprintf(next_slot(ch), sizeof(sent[0]), "on %d %d", note, velocity);
}

void midi_note_off(uint8_t ch, uint8_t note) {
    snprintf(next_slot(ch), sizeof(sent[0]), "off %d", note);
}

void midi_cc(uint8_t ch, uint8_t cc, uint8_t value) {
    snprintf(next_slot(ch), sizeof(sent[0]), "cc %d %d", cc, value);
}

static void clear_sent(void) {
    sent_count = 0;
}

// Checks the recorded messages against a NULL-terminated list, then clears
static void expect(const char *const *want, int line) {
    int n = 0;
    while (want[n]) n++;
    bool ok = n == sent_count;
    for (int i = 0; ok && i < n; i++) {
        ok = strcmp(want[i], sent[i]) == 0;
    }
    if (!ok) {
        printf("line %d: expected", line);
        for (int i = 0; i < n; i++) printf(" [%s]", want[i]);
        printf(", got");
        for (int i = 0; i < sent_count; i++) printf(" [%s]", sent[i]);
        printf("\n");
        failures++;
    }
    clear_sent();
}

#define EXPECT(...) expect((const char *const[]){ __VA_ARGS__, NULL }, __LINE__)
#define EXPECT_NONE() expect((const char *const[]){ NULL }, __LINE__)

// Settings

static void test_settings(void) {
    settings_t s, d;
    settings_defaults(&d);

    // Firmware defaults match config/piedalera.ini
    FILE *f = fopen(DEFAULT_INI, "r");
    CHECK(f);
    static char text[4096] = "# piedalera-config v1\n";
    size_t len = strlen(text);
    len += fread(text + len, 1, sizeof(text) - len - 1, f);
    fclose(f);
    CHECK(settings_parse(&s, text, len));
    CHECK(memcmp(&s, &d, sizeof(s)) == 0);

    // Round trip
    char out[SETTINGS_TEXT_MAX];
    s.midi_transpose = -7;
    s.keys_pull = PULL_UP;
    s.keys_active_low = false;
    s.octave_delay_ms = 800;
    s.power_battery = BATTERY_NIMH;
    settings_format(&s, out, sizeof(out));
    settings_t back;
    CHECK(settings_parse(&back, out, sizeof(out)));
    CHECK(memcmp(&s, &back, sizeof(s)) == 0);

    // Missing header: all defaults
    const char *bad = "midi.velocity = 10\n";
    CHECK(!settings_parse(&s, bad, strlen(bad)));
    CHECK(s.midi_velocity == d.midi_velocity);

    // Invalid values keep their default, comments and spaces are fine
    const char *mixed =
        "# piedalera-config v1\n"
        "midi.velocity = 0\n"
        "  midi.transpose=-3   # comment\n"
        "keys.pull = sideways\n"
        "display.width = 128\n"
        "display.col_offset = 4\n"
        "octave.min = 2\n"
        "octave.current = 1\n"
        "unknown.key = 5\n";
    CHECK(settings_parse(&s, mixed, strlen(mixed)));
    CHECK(s.midi_velocity == d.midi_velocity);
    CHECK(s.midi_transpose == -3);
    CHECK(s.keys_pull == d.keys_pull);
    CHECK(s.octave_min == 2 && s.octave_current == 2);

    // Heights must fill whole pages; the octave delay stays under 1 s
    const char *limits =
        "# piedalera-config v1\n"
        "display.height = 60\n"
        "octave.delay_ms = 1500\n";
    CHECK(settings_parse(&s, limits, strlen(limits)));
    CHECK(s.display_height == d.display_height);
    CHECK(s.octave_delay_ms == d.octave_delay_ms);

    // Erased flash
    char erased[16];
    memset(erased, 0xFF, sizeof(erased));
    CHECK(!settings_parse(&s, erased, sizeof(erased)));
}

// Debounce

static void test_debounce(void) {
    debounce_t d;
    debounce_init(&d, 0);
    input_state_t st = debounce_update(&d, 1, 0, 5);
    CHECK(st.pressed == 0 && st.down == 0);
    debounce_update(&d, 0, 2, 5);       // bounce restarts the count
    debounce_update(&d, 1, 3, 5);
    st = debounce_update(&d, 1, 7, 5);
    CHECK(st.pressed == 0);
    st = debounce_update(&d, 1, 8, 5);
    CHECK(st.pressed == 1 && st.down == 1);
    st = debounce_update(&d, 1, 9, 5);
    CHECK(st.down == 0);
    st = debounce_update(&d, 0, 10, 0); // no debounce: immediate
    CHECK(st.pressed == 0 && st.up == 1);
}

// Keyboard

static settings_t s;

static void setup(void) {
    settings_defaults(&s);
    notes_init(0);
    keyboard_init(&s);
    clear_sent();
}

static void test_normal(void) {
    setup();
    keyboard_press(0, 3);
    EXPECT("on 48 95");
    keyboard_release(0, 5);                 // note-off uses the press octave
    EXPECT("off 48");

    s.midi_transpose = 2;
    keyboard_press(19, 3);
    EXPECT("on 69 95");
    keyboard_release(19, 3);
    EXPECT("off 69");

    // Out of range notes are dropped
    s.midi_transpose = 12;
    keyboard_press(19, 8);
    keyboard_release(19, 8);
    EXPECT_NONE();

    // Same note from two keys: off only after both are released
    s.midi_transpose = 0;
    keyboard_press(12, 2);
    keyboard_press(0, 3);
    EXPECT("on 48 95", "on 48 95");
    keyboard_release(12, 2);
    EXPECT_NONE();
    keyboard_release(0, 3);
    EXPECT("off 48");

    // Keys held through a reset are ignored until pressed again
    keyboard_press(4, 3);
    keyboard_reset();
    EXPECT("on 52 95", "off 52");
    keyboard_release(4, 3);
    EXPECT_NONE();
}

static void test_chords(void) {
    setup();
    keyboard_set_chord_mode(true);

    keyboard_press(0, 3);                   // C major
    EXPECT("on 48 95", "on 52 95", "on 55 95");
    keyboard_press(5, 3);                   // F queued
    EXPECT_NONE();
    keyboard_press(15, 3);                  // minor, for the queued chord
    keyboard_release(15, 3);
    EXPECT_NONE();
    keyboard_release(0, 4);                 // F minor starts, octave now 4
    EXPECT("off 48", "off 52", "off 55", "on 65 95", "on 68 95", "on 72 95");
    keyboard_release(5, 4);
    EXPECT("off 65", "off 68", "off 72");

    // A queued root released before its turn is dropped
    keyboard_press(13, 3);                  // major
    keyboard_release(13, 3);
    keyboard_press(0, 3);
    keyboard_press(7, 3);
    keyboard_release(7, 3);
    clear_sent();
    keyboard_release(0, 3);
    EXPECT("off 48", "off 52", "off 55");

    // Latest press wins the queue
    keyboard_press(0, 3);
    keyboard_press(2, 3);
    keyboard_press(4, 3);
    clear_sent();
    keyboard_release(0, 3);
    EXPECT("off 48", "off 52", "off 55", "on 52 95", "on 56 95", "on 59 95");
}

static void test_hold(void) {
    setup();
    keyboard_set_chord_mode(true);
    keyboard_press(KEY_HOLD, 3);
    keyboard_release(KEY_HOLD, 3);
    CHECK(keyboard_hold());
    CHECK(!(keyboard_marks() & INPUT_BIT(KEY_HOLD)));  // shown as HOLD only

    keyboard_press(0, 3);
    keyboard_release(0, 3);
    EXPECT("on 48 95", "on 52 95", "on 55 95");
    keyboard_press(2, 3);                   // switches right away
    EXPECT("off 48", "off 52", "off 55", "on 50 95", "on 54 95", "on 57 95");
    keyboard_release(2, 3);
    EXPECT_NONE();

    keyboard_press(KEY_HOLD, 3);            // hold off stops the chord
    EXPECT("off 50", "off 54", "off 57");
    CHECK(!keyboard_hold());
}

// Octave buttons

// Holds the buttons from t0 to t1 at the 1 kHz scan rate; returns the
// time of each octave step
static int hold_octave(bool up, bool down, uint32_t t0, uint32_t t1,
                       uint32_t *steps) {
    int n = 0;
    for (uint32_t t = t0; t <= t1; t++) {
        if (octave_update(up, down, t) == OCTAVE_CHANGED) {
            steps[n++] = t;
        }
    }
    return n;
}

static void test_octave(void) {
    uint32_t steps[16];
    settings_defaults(&s);
    octave_init(&s);

    // First step after the delay, then one per repeat interval
    CHECK(hold_octave(true, false, 0, 1200, steps) == 3);
    CHECK(steps[0] == 100 && steps[1] == 600 && steps[2] == 1100);
    CHECK(s.octave_current == 6);
    CHECK(hold_octave(true, false, 1201, 3000, steps) == 1);  // stops at max
    CHECK(s.octave_current == 7);
    octave_update(false, false, 3001);

    // Repeat shorter than the delay
    s.octave_delay_ms = 300;
    s.octave_repeat_ms = 100;
    CHECK(hold_octave(false, true, 4000, 4500, steps) == 3);
    CHECK(steps[0] == 4300 && steps[1] == 4400 && steps[2] == 4500);
    CHECK(s.octave_current == 4);
    octave_update(false, false, 4501);
    settings_defaults(&s);
    s.octave_current = 6;

    // Both buttons: toggle, then config
    CHECK(octave_update(true, true, 4000) == OCTAVE_NONE);
    CHECK(octave_update(true, true, 4100) == OCTAVE_TOGGLE_CHORD);
    CHECK(octave_update(true, true, 4500) == OCTAVE_NONE);
    CHECK(octave_update(true, true, 5000) == OCTAVE_ENTER_CONFIG_UNDO);
    CHECK(octave_update(true, false, 5500) == OCTAVE_NONE);   // blocked
    CHECK(octave_update(true, false, 6000) == OCTAVE_NONE);
    octave_update(false, false, 6001);

    // A button left over after a toggle doesn't step
    octave_update(true, true, 7000);
    CHECK(octave_update(true, true, 7100) == OCTAVE_TOGGLE_CHORD);
    CHECK(octave_update(false, true, 7200) == OCTAVE_NONE);
    CHECK(octave_update(false, true, 8000) == OCTAVE_NONE);
    CHECK(s.octave_current == 6);
}

// App: config mode and saving

static input_state_t in;

static void tick(uint32_t pressed, uint32_t now) {
    in.down = pressed & ~in.pressed;
    in.up = in.pressed & ~pressed;
    in.pressed = pressed;
    app_update(&in, now);
}

static void test_app(void) {
    settings_defaults(&s);
    clear_sent();
    memset(&in, 0, sizeof(in));
    app_init(&s);
    EXPECT("cc 123 0", "cc 120 0");

    const uint32_t both = INPUT_BIT(INPUT_OCT_UP) | INPUT_BIT(INPUT_OCT_DOWN);
    ui_state_t ui;

    // Enter config: chord mode toggles at 100 ms and is undone at 1 s; the
    // border shows how far the hold is
    for (uint32_t t = 0; t <= 1000; t += 10) {
        tick(both, t);
        app_ui_state(&ui);
        if (t == 500) CHECK(ui.chord_mode && ui.progress == 127);
    }
    app_ui_state(&ui);
    CHECK(ui.config && !ui.chord_mode && ui.msg == CONFIG_MSG_TITLE);
    CHECK(ui.progress == 0);
    tick(0, 1100);

    // E: velocity down, auto-repeat after 200 ms then every 50 ms
    uint32_t e = INPUT_BIT(4);
    tick(e, 2000);
    CHECK(s.midi_velocity == 94);
    tick(e, 2199);
    CHECK(s.midi_velocity == 94);
    tick(e, 2200);
    tick(e, 2250);
    CHECK(s.midi_velocity == 92);
    tick(0, 2300);

    // G: bank down, sends bank select
    tick(INPUT_BIT(7), 2400);
    EXPECT("cc 0 121", "cc 32 11");
    tick(0, 2500);

    // Transpose clamps at +12
    for (int i = 0; i < 20; i++) {
        tick(INPUT_BIT(12), 3000 + i * 20);
        tick(0, 3010 + i * 20);
    }
    CHECK(s.midi_transpose == 12);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_TRANSPOSE && ui.msg_value == 12);

    // Any other key leaves on release and saves right away
    tick(INPUT_BIT(1), 4000);
    app_ui_state(&ui);
    CHECK(ui.config);
    tick(0, 4010);
    app_ui_state(&ui);
    CHECK(!ui.config);
    CHECK(app_save_due(4010));
    CHECK(!app_save_due(4011));

    // Keys play again, with the new transpose and velocity
    clear_sent();
    tick(INPUT_BIT(0), 5000);
    EXPECT("on 60 92");
    tick(0, 5010);
    clear_sent();

    // Octave changes are saved after 5 s
    tick(INPUT_BIT(INPUT_OCT_UP), 6000);
    tick(INPUT_BIT(INPUT_OCT_UP), 6100);
    tick(0, 6110);
    CHECK(s.octave_current == 4);
    CHECK(!app_save_due(11099));
    CHECK(app_save_pending() && !app_save_pending());   // taken early, e.g. to sleep
    CHECK(!app_save_due(11100));

    // Back into config: debounce on D'/E', then a short G' press leaves
    for (uint32_t t = 12000; t <= 13000; t += 10) tick(both, t);
    tick(0, 13100);
    tick(INPUT_BIT(16), 13200);
    tick(0, 13300);
    CHECK(s.keys_debounce_ms == 6);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_DEBOUNCE && ui.msg_value == 6);
    tick(INPUT_BIT(14), 13400);
    tick(INPUT_BIT(14), 13900);             // first repeat after 500 ms
    tick(0, 13910);
    CHECK(s.keys_debounce_ms == 4);

    // Untouched for 3 s, a setting's screen goes back to the map, showing
    // the last second on the border
    tick(0, 15899);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_DEBOUNCE && ui.progress == 0);
    tick(0, 16400);
    app_ui_state(&ui);
    CHECK(ui.progress == 127);
    tick(0, 16900);
    app_ui_state(&ui);
    CHECK(ui.config && ui.msg == CONFIG_MSG_TITLE && ui.progress == 0);
    tick(INPUT_BIT(16), 17000);             // and back to debounce
    tick(0, 17100);
    CHECK(s.keys_debounce_ms == 5);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_DEBOUNCE);
    // A G' tap goes back to the map from a setting, and leaves from the map
    tick(INPUT_BIT(KEY_BOOTSEL), 18000);
    tick(INPUT_BIT(KEY_BOOTSEL), 18999);
    CHECK(!app_bootsel());
    tick(0, 19000);
    app_ui_state(&ui);
    CHECK(ui.config && ui.msg == CONFIG_MSG_TITLE);
    tick(INPUT_BIT(KEY_BOOTSEL), 19100);
    tick(0, 19200);
    app_ui_state(&ui);
    CHECK(!ui.config && !app_bootsel());
    tick(0, 19201);                         // octave buttons unblock

    // Holding G' for a second asks for the bootloader
    for (uint32_t t = 20000; t <= 21000; t += 10) tick(both, t);
    tick(0, 21100);
    // USB FLASH shows after 0.1 s and the settings are saved then; the
    // reboot comes as the border closes
    tick(INPUT_BIT(KEY_BOOTSEL), 22000);
    tick(INPUT_BIT(KEY_BOOTSEL), 22099);
    app_ui_state(&ui);
    CHECK(ui.msg != CONFIG_MSG_BOOTSEL && !app_save_due(22099));
    tick(INPUT_BIT(KEY_BOOTSEL), 22100);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_BOOTSEL);
    CHECK(app_save_due(22100) && !app_save_due(22101));
    tick(INPUT_BIT(KEY_BOOTSEL), 22250);
    CHECK(!app_bootsel() && !app_save_due(22250));
    app_ui_state(&ui);
    CHECK(ui.progress == 63);
    tick(INPUT_BIT(KEY_BOOTSEL), 23000);
    CHECK(app_bootsel());
    app_ui_state(&ui);
    CHECK(ui.config && ui.msg == CONFIG_MSG_BOOTSEL && ui.progress == 255);
    tick(0, 23100);                         // stays put until the reboot
    CHECK(app_bootsel());
    CHECK(app_idle_ms(23600) == 500);       // since G' was released
}

// Display

static bool pixel(const gfx_t *g, int x, int y) {
    return g->buf[(y >> 3) * g->width + x] >> (y & 7) & 1;
}

// Any pixel lit in [x0, x1) x [y0, y1)
static bool lit(const gfx_t *g, int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (pixel(g, x, y)) return true;
        }
    }
    return false;
}

// Battery

static void test_battery(void) {
    // Charge along each curve, per cell
    CHECK(battery_level(BATTERY_LIION, 1, 4200) == 255);
    CHECK(battery_level(BATTERY_LIION, 1, 3800) == 102);         // 40 %
    CHECK(battery_level(BATTERY_LIION, 1, 3200) == 0);
    CHECK(battery_level(BATTERY_NIMH, 3, 3600) == 63);          // 25 %
    CHECK(battery_level(BATTERY_NONE, 3, 4000) == 0);
    CHECK(!strcmp(battery_label(BATTERY_ALKALINE), "Alk."));

    // The reading adds the diode drop and is smoothed
    settings_defaults(&s);
    s.power_battery = BATTERY_NIMH;
    s.power_cells = 3;
    memset(&in, 0, sizeof(in));
    app_init(&s);
    ui_state_t ui;
    app_ui_state(&ui);
    CHECK(!ui.battery);                     // until the first reading
    app_battery(3300);
    app_ui_state(&ui);
    CHECK(ui.battery && ui.battery_mv == 3600 && ui.battery_cells == 3);
    app_battery(3380);
    app_ui_state(&ui);
    CHECK(ui.battery_mv == 3610);

    // F' opens the battery page from the config map, G' goes back
    const uint32_t both = INPUT_BIT(INPUT_OCT_UP) | INPUT_BIT(INPUT_OCT_DOWN);
    for (uint32_t t = 0; t <= 1000; t += 10) tick(both, t);
    tick(0, 1100);
    tick(INPUT_BIT(KEY_BATTERY), 1200);
    tick(0, 1300);
    app_ui_state(&ui);
    CHECK(ui.config && ui.msg == CONFIG_MSG_BATTERY);
    tick(INPUT_BIT(KEY_BOOTSEL), 1400);
    tick(0, 1500);
    app_ui_state(&ui);
    CHECK(ui.config && ui.msg == CONFIG_MSG_TITLE);

    // Without a battery F' just leaves, as any other key
    s.power_battery = BATTERY_NONE;
    tick(INPUT_BIT(KEY_BATTERY), 1600);
    tick(0, 1700);
    app_ui_state(&ui);
    CHECK(!ui.config);

    // A new reading isn't input, so the screen still dims
    ui_anim_t a = { 0 };
    ui_anim_update(&a, &ui, 0);
    ui.battery_mv += 10;
    ui.battery_level++;
    ui_anim_update(&a, &ui, 5000);
    CHECK(ui_idle_ms(&a, 5000) == 5000);
}

static void test_ui(void) {
    static gfx_t g;
    gfx_init(&g, 128, 32);
    int kx = (128 - WIDGET_KEYS_WIDTH) / 2;

    // Keyboard on top: keys are outlines that wrap around the black keys,
    // filled while pressed
    ui_state_t st = { .octave = 3, .keys = 1u << 0, .root = -1 };
    ui_render(&g, &st, NULL, 0);
    CHECK(pixel(&g, kx + 3, 9));                        // C, pressed
    CHECK(pixel(&g, kx + 10, 9) && !pixel(&g, kx + 13, 9));  // D outline
    int notch = widget_key_x(kx, 1);                    // Db
    CHECK(pixel(&g, notch, 3) && !pixel(&g, notch + 1, 3));  // its outline
    CHECK(!pixel(&g, notch - 1, 3));                    // margin stays dark
    CHECK(pixel(&g, notch + 4, 3) && pixel(&g, notch + 2, 8));  // D wraps it

    // Chord mode: chord line and octave line under the keyboard
    st = (ui_state_t){ .octave = 3, .chord_mode = true, .chord = 2, .root = 0,
                       .hold = true };
    ui_render(&g, &st, NULL, 0);
    int text = 0;
    for (int x = 0; x < 128; x++) text |= g.buf[2 * 128 + x] | g.buf[3 * 128 + x];
    CHECK(text);

    // Config map and value screens draw something below the keyboard
    st = (ui_state_t){ .config = true, .msg = CONFIG_MSG_TITLE, .root = -1 };
    ui_render(&g, &st, NULL, 0);
    CHECK(pixel(&g, widget_key_x(kx, 0) + 1, 13));      // arc under C-D
    st.msg = CONFIG_MSG_TRANSPOSE;
    st.msg_value = -3;
    ui_render(&g, &st, NULL, 0);
    CHECK(pixel(&g, 64, 23));                           // bar frame

    // Hold border: inverted from the top middle, clockwise
    st.progress = 64;   // a quarter: the top right and part of the right
    ui_render(&g, &st, NULL, 0);
    CHECK(pixel(&g, 64, 23));                           // untouched
    CHECK(pixel(&g, 127, 5) && !pixel(&g, 0, 5));
    CHECK(!pixel(&g, 70, 0));                           // lit key, inverted
    st.progress = 255;
    ui_render(&g, &st, NULL, 0);
    CHECK(pixel(&g, 0, 5) && pixel(&g, 5, 31));

    // Transitions: a mode banner, then an octave slide clipped to its line;
    // idle time counts from the last change
    ui_anim_t a = { 0 };
    st = (ui_state_t){ .octave = 3, .root = -1 };
    ui_anim_update(&a, &st, 1000);
    CHECK(!ui_anim_running(&a, 1000));
    st.chord_mode = true;
    ui_anim_update(&a, &st, 2000);
    CHECK(ui_anim_running(&a, 2000 + UI_BANNER_MS - 1));
    CHECK(!ui_anim_running(&a, 2000 + UI_BANNER_MS));
    ui_render(&g, &st, &a, 2400);
    CHECK(lit(&g, 80, 15, 104, 29));                    // "CHORD", large
    ui_render(&g, &st, &a, 2800);
    CHECK(!lit(&g, 80, 15, 104, 29));
    st.octave = 4;
    ui_anim_update(&a, &st, 3000);
    CHECK(ui_anim_running(&a, 3100) && !ui_anim_running(&a, 3000 + UI_SLIDE_MS));
    ui_render(&g, &st, &a, 3075);
    CHECK(lit(&g, 64, 23, 72, 32) && !lit(&g, 64, 14, 72, 23));
    CHECK(ui_idle_ms(&a, 5000) == 2000);

    // Battery page: voltage left of the column, type above the charge bar
    st = (ui_state_t){ .config = true, .msg = CONFIG_MSG_BATTERY, .root = -1, .battery = true,
                       .battery_level = 128, .battery_mv = 4120, .battery_type = BATTERY_LIION,
                       .battery_cells = 1 };
    ui_render(&g, &st, NULL, 0);
    CHECK(lit(&g, 0, 14, 64, 28) && lit(&g, 64, 13, 128, 21) && pixel(&g, 64, 23));

    // Battery: only on the config map, filling from the bottom; nearly empty
    // it blinks an exclamation mark, so frames keep coming
    st = (ui_state_t){ .octave = 3, .root = -1, .battery = true, .battery_level = 255 };
    ui_render(&g, &st, NULL, 0);
    CHECK(!lit(&g, 100, 13, 114, 32));                  // not while playing
    st.config = true;
    ui_render(&g, &st, NULL, 0);
    CHECK(lit(&g, 107, 19, 110, 21));                   // full to the top
    st.battery_level = 30;
    ui_render(&g, &st, NULL, 0);
    CHECK(!lit(&g, 107, 19, 110, 20));
    st.battery_level = 10;
    ui_anim_t b = { 0 };
    ui_anim_update(&b, &st, 0);
    CHECK(ui_anim_running(&b, 0));
    ui_render(&g, &st, &b, 0);
    CHECK(lit(&g, 107, 19, 110, 22));                   // "!" on
    ui_render(&g, &st, &b, UI_BLINK_MS);
    CHECK(!lit(&g, 107, 19, 110, 22));                  // and off

    // Text at y = 12 straddles pages 1 and 2
    gfx_clear(&g);
    gfx_text(&g, 0, 12, "|");
    CHECK(g.buf[1 * 128 + 3] == 0xF0 && g.buf[2 * 128 + 3] == 0x0F);
    // Tall glyphs reach a 9th row
    gfx_clear(&g);
    gfx_text(&g, 0, 0, "j");
    CHECK(pixel(&g, 3, 8) && !pixel(&g, 3, 9));
    // Clipped text doesn't write out of bounds
    gfx_text(&g, 120, 28, "WWj");
    gfx_text(&g, -4, -4, "Wj");
}

// Splash: every frame matches assets/splash/reference.gif (FNV-1a of the
// framebuffer, taken from the GIF)

static const uint32_t SPLASH_REFERENCE[SPLASH_FRAMES] = {
    0x4d7705c5, 0xa886fb54, 0xdfba8a63, 0x13f9e619, 0x9e153a4c, 0x7670d45d,
    0x1bb57122, 0xa62bda29, 0x0633cef4, 0x748b2488, 0xd0958f99, 0xfa7ba644,
    0x9669c004, 0x1a4ea9d3, 0xfc291715, 0xd70281a9, 0xc2d66b25, 0x8ff4094f,
    0xeae6c826, 0x50e93764, 0xa47650e2, 0x0a3d51ef, 0x451aa479, 0x1f3e413f,
    0x2372db1b, 0xc24692c9, 0xc24692c9, 0xc24692c9, 0xc24692c9, 0xd1ae3359,
    0x335df499, 0xc8c87eb5, 0x27ce0105, 0x4d7705c5,
};

static uint32_t fnv1a(const uint8_t *p, size_t n) {
    uint32_t h = 0x811c9dc5;
    while (n--) h = (h ^ *p++) * 0x01000193;
    return h;
}

static void test_splash(void) {
    static gfx_t g;
    gfx_init(&g, 128, 32);
    for (int f = 0; f < SPLASH_FRAMES; f++) {
        CHECK(splash_draw(&g, f * SPLASH_FRAME_MS + SPLASH_FRAME_MS / 2));
        if (fnv1a(g.buf, 128 * 4) != SPLASH_REFERENCE[f]) {
            printf("splash frame %d differs from the reference\n", f);
            failures++;
        }
    }
    CHECK(!splash_draw(&g, SPLASH_FRAMES * SPLASH_FRAME_MS));

    // Centred on a taller display, nothing drawn outside
    gfx_init(&g, 128, 64);
    splash_draw(&g, 26 * SPLASH_FRAME_MS);
    for (int x = 0; x < 128; x++) {
        CHECK(g.buf[x] == 0 && g.buf[7 * 128 + x] == 0);
    }
}

int main(void) {
    test_settings();
    test_debounce();
    test_normal();
    test_chords();
    test_hold();
    test_octave();
    test_app();
    test_battery();
    test_ui();
    test_splash();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    printf("all tests passed\n");
    return EXIT_SUCCESS;
}
