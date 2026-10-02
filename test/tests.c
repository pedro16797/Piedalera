#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "battery.h"
#include "config_mode.h"
#include "expression.h"
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

// Fake MIDI: records messages as "on 48 95", "off 48", "cc 123 0", "pc 0"

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

void midi_program(uint8_t ch, uint8_t program) {
    snprintf(next_slot(ch), sizeof(sent[0]), "pc %d", program);
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

    // Any sound line replaces the default list; bad ones are skipped, and
    // a choice past the end means the synth's own
    const char *sounds =
        "# piedalera-config v1\n"
        "midi.sound = 3\n"
        "sound = 0 0 34 Fingered Bass   # comment\n"
        "sound = 0 0 129 Too High\n"
        "sound = 0 0 1\n"
        "sound = 0 0 1 Name Much Too Long\n"
        "sound = 64 3  12  Bells  &  more\n";
    CHECK(settings_parse(&s, sounds, strlen(sounds)));
    CHECK(s.sound_count == 2 && s.midi_sound == 0);
    CHECK(s.sounds[0].msb == 0 && s.sounds[0].program == 34);
    CHECK(strcmp(s.sounds[0].name, "Fingered Bass") == 0);
    CHECK(s.sounds[1].msb == 64 && s.sounds[1].lsb == 3 && s.sounds[1].program == 12);
    CHECK(strcmp(s.sounds[1].name, "Bells  &  more") == 0);
    s.midi_sound = 2;
    settings_format(&s, out, sizeof(out));
    CHECK(strstr(out, "\nsound = 64 3 12 Bells  &  more\n"));
    CHECK(settings_parse(&back, out, sizeof(out)));
    CHECK(memcmp(&s, &back, sizeof(s)) == 0);

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
    for (int i = 0; i < CHORD_COUNT; i++) {
        CHECK(strlen(CHORDS[i].name) <= 13);    // "Db " before it fills the line
    }
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
    CHECK(!keyboard_marks());               // type and hold shown as text only

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

// Ionian on a new tonic, with nothing sounding after
static void pick_tonic(int key) {
    keyboard_press(12, 3);
    keyboard_press(key, 3);
    keyboard_release(key, 3);
    keyboard_release(12, 3);
    clear_sent();
}

static void test_modes(void) {
    setup();
    s.keys_alternative = 1;
    keyboard_set_chord_mode(true);

    keyboard_press(2, 3);                   // C Ionian: D minor 7th
    EXPECT("on 50 95", "on 53 95", "on 57 95", "on 60 95");
    keyboard_release(2, 3);
    clear_sent();
    keyboard_press(11, 3);                  // B half-diminished 7th
    EXPECT("on 59 95", "on 62 95", "on 65 95", "on 69 95");
    keyboard_release(11, 3);
    clear_sent();
    keyboard_press(1, 3);                   // outside: C major triad
    EXPECT("on 48 95", "on 52 95", "on 55 95");
    CHECK(keyboard_root() == 0 && keyboard_marks() == 0x91);
    keyboard_release(1, 3);

    keyboard_press(13, 3);                  // Dorian
    keyboard_release(13, 3);
    clear_sent();
    keyboard_press(0, 3);                   // C minor 7th
    EXPECT("on 48 95", "on 51 95", "on 55 95", "on 58 95");
    keyboard_release(0, 3);

    // A pedal pressed with a mode key held picks the tonic, silently
    keyboard_press(13, 3);
    clear_sent();
    keyboard_press(2, 3);
    keyboard_release(2, 3);
    keyboard_release(13, 3);
    EXPECT_NONE();
    CHECK(keyboard_tonic() == 2 && keyboard_mode() == 1);
    keyboard_press(0, 3);                   // D Dorian: C major 7th
    EXPECT("on 48 95", "on 52 95", "on 55 95", "on 59 95");
    keyboard_release(0, 3);

    // Outside the scale, the scale note of the same letter: in D Ionian
    // (sharps) C is C sharp diminished, and C sharp itself in G Ionian is C
    keyboard_press(12, 3);
    keyboard_release(12, 3);
    clear_sent();
    keyboard_press(0, 3);
    EXPECT("on 49 95", "on 52 95", "on 55 95");
    CHECK(keyboard_root() == 1);
    keyboard_release(0, 3);
    pick_tonic(7);
    keyboard_press(1, 3);
    EXPECT("on 48 95", "on 52 95", "on 55 95");
    keyboard_release(1, 3);
    keyboard_press(5, 3);                   // F: F sharp diminished
    EXPECT("off 48", "off 52", "off 55", "on 54 95", "on 57 95", "on 60 95");
    keyboard_release(5, 3);

    // F Ionian (flats): D flat is D minor, B is B flat major
    pick_tonic(5);
    keyboard_press(1, 3);
    EXPECT("on 50 95", "on 53 95", "on 57 95");
    keyboard_release(1, 3);
    clear_sent();
    keyboard_press(11, 3);
    EXPECT("on 58 95", "on 62 95", "on 65 95");
    keyboard_release(11, 3);

    // G flat Ionian: C is C flat, B major below the keyboard
    pick_tonic(6);
    keyboard_press(0, 3);
    EXPECT("on 47 95", "on 51 95", "on 54 95");
    CHECK(keyboard_root() == -1 && keyboard_marks() == 0x48);
    keyboard_release(0, 3);
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
    s.midi_sound = 6;                       // a chosen sound follows the panic
    app_init(&s);
    EXPECT("cc 123 0", "cc 120 0", "cc 0 0", "cc 32 0", "pc 19");
    s.midi_sound = 0;                       // the synth's own: nothing more
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
    EXPECT("cc 123 0", "cc 120 0");         // entering it silences the synth
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

    // G: the synth's own sound already, nothing sent; A picks the first
    // of the list, G goes back without sending
    tick(INPUT_BIT(7), 2400);
    EXPECT_NONE();
    tick(0, 2410);
    tick(INPUT_BIT(9), 2420);
    EXPECT("cc 0 0", "cc 32 0", "pc 0");
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_SOUND && ui.msg_value == 1);
    CHECK(strcmp(ui.sound, "Grand Piano") == 0 && ui.sound_count == 12);
    tick(0, 2430);
    tick(INPUT_BIT(7), 2440);
    tick(0, 2450);
    CHECK(s.midi_sound == 0);
    EXPECT_NONE();

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
    // USB FLASH and the border show after 0.2 s and the settings are saved
    // then; the reboot comes as the border closes
    tick(INPUT_BIT(KEY_BOOTSEL), 22000);
    tick(INPUT_BIT(KEY_BOOTSEL), 22199);
    app_ui_state(&ui);
    CHECK(ui.msg != CONFIG_MSG_BOOTSEL && ui.progress == 0 && !app_save_due(22199));
    tick(INPUT_BIT(KEY_BOOTSEL), 22200);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_BOOTSEL);
    CHECK(app_save_due(22200) && !app_save_due(22201));
    tick(INPUT_BIT(KEY_BOOTSEL), 22600);
    CHECK(!app_bootsel() && !app_save_due(22600));
    app_ui_state(&ui);
    CHECK(ui.progress == 127);
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

// Chord mode, then H held for a second: the tap's hold is undone and mode
// mode takes over, saved like the octave
static void test_switch(void) {
    settings_defaults(&s);
    memset(&in, 0, sizeof(in));
    app_init(&s);
    const uint32_t both = INPUT_BIT(INPUT_OCT_UP) | INPUT_BIT(INPUT_OCT_DOWN);
    ui_state_t ui;
    tick(both, 0);
    tick(both, 100);
    tick(0, 110);
    uint32_t h = INPUT_BIT(KEY_HOLD);
    for (uint32_t t = 200; t <= 1200; t += 10) {
        tick(h, t);
        app_ui_state(&ui);
        if (t == 700) CHECK(ui.hold && !ui.modes && ui.progress == 127);
    }
    tick(h, 1201);
    app_ui_state(&ui);
    CHECK(ui.chord_mode && ui.modes && !ui.hold && ui.progress == 0);
    CHECK(s.keys_alternative == 1);
    tick(0, 1210);
    tick(h, 1300);                         // a tap still toggles hold
    tick(0, 1310);
    CHECK(keyboard_hold());
    tick(h, 1400);
    tick(0, 1410);
    CHECK(app_save_pending());
}

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
    app_battery(3300, false);
    app_ui_state(&ui);
    CHECK(ui.battery && ui.battery_mv == 3600 && ui.battery_cells == 3);
    app_battery(3380, false);
    app_ui_state(&ui);
    CHECK(ui.battery_mv == 3610 && !ui.battery_external);

    // Plugged in: USB sensed, or a voltage above full charge
    app_battery(3380, true);
    app_ui_state(&ui);
    CHECK(ui.battery_external && ui.battery_level == 255);
    CHECK(battery_external(BATTERY_LIION, 1, 4700) && !battery_external(BATTERY_LIION, 1, 4200));
    CHECK(!battery_external(BATTERY_NONE, 1, 5000));
    app_battery(3380, false);

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

    // A new reading isn't input, so the screen still dims, nor is the first
    ui_anim_t a = { 0 };
    ui.battery = false;
    ui_anim_update(&a, &ui, 0);
    ui.battery = true;
    ui.battery_type = BATTERY_LIION;
    ui.battery_cells = 1;
    ui.battery_mv = 4000;
    ui_anim_update(&a, &ui, 1000);
    ui.battery_mv += 10;
    ui.battery_level++;
    ui_anim_update(&a, &ui, 5000);
    CHECK(ui_idle_ms(&a, 5000) == 5000);
}

// Expression pedal

static uint32_t pedal_at;

// Holds the pedal at raw for 100 ms, long past the smoothing; returns the
// last value sent, -1 if none
static int pedal(uint16_t raw) {
    clear_sent();
    for (int i = 0; i < 100; i++) {
        app_expression(raw, pedal_at++);
    }
    int cc = -1, value = -1;
    if (sent_count) {
        sscanf(sent[sent_count - 1], "cc %d %d", &cc, &value);
        CHECK(cc == s.expression_cc);
    }
    clear_sent();
    return value;
}

// A few probes of the pin, with a pedal holding it or an empty jack
static void probe(bool plugged) {
    for (int i = 0; i < 3; i++) {
        app_expression_probe(plugged ? 2100 : 4095, plugged ? 1900 : 0);
    }
}

// Into config mode with both octave buttons
static void pedal_config(void) {
    const uint32_t both = INPUT_BIT(INPUT_OCT_UP) | INPUT_BIT(INPUT_OCT_DOWN);
    for (uint32_t t = 0; t <= 1000; t += 10) tick(both, pedal_at + t);
    pedal_at += 1100;
    tick(0, pedal_at);
}

// Out of it with another key, saving
static void pedal_leave(void) {
    tick(INPUT_BIT(1), pedal_at);
    tick(0, pedal_at + 100);
    tick(0, pedal_at + 101);                // octave buttons unblock
    pedal_at += 200;
}

static void test_expression(void) {
    settings_defaults(&s);
    memset(&in, 0, sizeof(in));
    app_init(&s);
    pedal_at = 0;
    ui_state_t ui;

    // Off by default
    CHECK(pedal(1000) == -1);
    s.expression_enabled = true;

    // Until a probe finds a pedal, readings are ignored, even in config
    // mode where they would be learnt; two probes aren't enough
    pedal_config();
    CHECK(pedal(3000) == -1 && s.expression_min > s.expression_max);
    app_expression_probe(2100, 1900);
    app_expression_probe(2100, 1900);
    CHECK(!expression_plugged());
    app_expression_probe(2100, 1900);
    CHECK(expression_plugged());
    pedal_leave();

    // Never learnt, it sends nothing and learns nothing while playing
    CHECK(pedal(3000) == -1 && pedal(1000) == -1);
    CHECK(s.expression_min > s.expression_max);

    // In config mode the travel starts at the first reading, and widening
    // it opens the pedal's page from the map: On, with the position so far
    pedal_config();
    pedal(1000);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_TITLE && s.expression_min == 1000 && s.expression_max == 1000);
    CHECK(pedal(1100) == -1);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_EXPRESSION && ui.expression && !ui.expression_ready);
    CHECK(ui.msg_value == 127);
    pedal(1050);                            // the bar follows inside it
    app_ui_state(&ui);
    CHECK(ui.msg_value >= 55 && ui.msg_value <= 72);

    // A slow sweep keeps the page open past its 3 s; values are sent once
    // the travel is wide enough
    int last = -1;
    for (int raw = 1060; raw <= 3000; raw += 20) {
        int v = pedal(raw);
        if (v >= 0) last = v;
        tick(0, pedal_at);
    }
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_EXPRESSION && ui.expression_ready);
    CHECK(last == 127 && ui.msg_value == 127);
    CHECK(s.expression_min == 1000 && s.expression_max >= 2990);
    CHECK(pedal(1000) == 0);
    int mid = pedal(2000);
    CHECK(mid >= 62 && mid <= 65);

    // Noise of a few counts settles it a step at most, then sends nothing
    for (int i = 0; i < 200; i++) {
        if (i == 100) clear_sent();
        app_expression(i & 1 ? 2010 : 1990, pedal_at++);
    }
    EXPECT_NONE();

    // The learnt travel is saved once it stops widening
    pedal_leave();
    app_ui_state(&ui);
    CHECK(!ui.config && !app_save_due(pedal_at));
    CHECK(app_save_due(pedal_at + 5000));

    // While playing it never widens, e.g. with the pedal unplugged
    CHECK(pedal(0) == 0 && pedal(4095) == 127);
    CHECK(s.expression_min == 1000 && s.expression_max <= 3000);
    CHECK(!app_save_due(pedal_at + 5000));

    // Reversed, and on another controller
    s.expression_invert = true;
    CHECK(pedal(3000) == 0 && pedal(1000) == 127);
    s.expression_invert = false;
    s.expression_cc = 7;
    CHECK(pedal(3000) == 127);
    s.expression_cc = 11;

    // Learnt, moving it opens its page from the map with the value; the page
    // is left with G' like any other, and doesn't cover another setting's
    pedal_config();
    pedal(2000);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_EXPRESSION && ui.msg_value >= 62 && ui.msg_value <= 65);
    uint8_t down, up;
    CHECK(config_mode_keys(CONFIG_MSG_EXPRESSION, &down, &up) &&
          down == KEY_VELOCITY_DOWN && up == KEY_VELOCITY_UP);
    tick(INPUT_BIT(KEY_BOOTSEL), pedal_at);
    tick(0, pedal_at + 100);
    app_ui_state(&ui);
    CHECK(ui.config && ui.msg == CONFIG_MSG_TITLE);
    tick(INPUT_BIT(4), pedal_at + 200);
    tick(0, pedal_at + 300);
    pedal_at += 400;
    pedal(3000);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_VELOCITY);
    CHECK(s.expression_max <= 3000);        // nor learns there

    // E and F held together: the first step is undone, nothing repeats,
    // the border fills and at 1 s the pedal turns off, back to full
    // expression, forgetting its travel; once until they are released
    const uint32_t ef = INPUT_BIT(KEY_VELOCITY_DOWN) | INPUT_BIT(KEY_VELOCITY_UP);
    uint32_t t0 = pedal_at + 1000;
    uint8_t velocity = s.midi_velocity;
    tick(INPUT_BIT(KEY_VELOCITY_DOWN), t0);
    CHECK(s.midi_velocity == velocity - 1);
    tick(ef, t0 + 100);
    app_ui_state(&ui);
    CHECK(s.midi_velocity == velocity && ui.msg == CONFIG_MSG_EXPRESSION);
    clear_sent();
    tick(ef, t0 + 600);
    app_ui_state(&ui);
    CHECK(ui.progress == 127 && ui.expression && s.midi_velocity == velocity);
    tick(ef, t0 + 1100);
    app_ui_state(&ui);
    CHECK(!s.expression_enabled && !ui.expression && ui.progress == 0);
    CHECK(s.expression_min > s.expression_max);
    tick(ef, t0 + 1101);
    EXPECT("cc 11 127");
    tick(ef, t0 + 3000);
    CHECK(!s.expression_enabled && s.midi_velocity == velocity);

    // On again, it learns afresh on its page, here from the very bottom;
    // leaving saves it
    tick(0, t0 + 3100);
    tick(ef, t0 + 3200);
    tick(ef, t0 + 4200);
    CHECK(s.expression_enabled);
    pedal_at = t0 + 4300;
    tick(0, pedal_at);
    probe(true);
    CHECK(pedal(0) == -1 && s.expression_min == 0 && s.expression_max == 0);
    app_ui_state(&ui);
    CHECK(ui.msg == CONFIG_MSG_EXPRESSION && ui.expression && !ui.expression_ready);
    CHECK(ui.msg_value == -1);              // On, empty bar
    CHECK(pedal(1000) == 127 && pedal(0) == 0);
    pedal_leave();
    CHECK(app_save_due(pedal_at));

    // Pulled out: 127 once, then its noise is ignored and doesn't keep the
    // board awake; plugged back in, its value is sent again
    clear_sent();
    probe(false);
    EXPECT("cc 11 127");
    uint32_t idle_from = pedal_at;
    CHECK(pedal(0) == -1 && pedal(1000) == -1);
    CHECK(app_idle_ms(pedal_at) >= pedal_at - idle_from);
    probe(true);
    CHECK(pedal(500) == 64);
}

static void test_ui(void) {
    static gfx_t g;
    gfx_init(&g, 128, 32);
    int kx = (128 - WIDGET_KEYS_WIDTH) / 2;

    // Keyboard on top; a pressed white key is an outline that wraps around
    // the black key next to it
    ui_state_t st = { .octave = 3, .keys = 1u << 0, .root = -1 };
    ui_render(&g, &st, NULL, 0);
    CHECK(pixel(&g, kx, 0) && pixel(&g, kx, 11));      // C outline
    CHECK(!pixel(&g, kx + 3, 9));                       // C inside
    CHECK(pixel(&g, kx + 12, 9));                       // D, not pressed
    int notch = widget_key_x(kx, 1);                    // Db
    CHECK(pixel(&g, notch - 2, 3));                     // outline beside it
    CHECK(pixel(&g, notch - 1, 8));                     // and below it
    CHECK(!pixel(&g, notch - 1, 3));                    // margin stays dark

    // On hold the root's mark is solid, the other notes' dotted
    st = (ui_state_t){ .octave = 3, .chord_mode = true, .chord = 2, .root = 0,
                       .hold = true, .marks = 0x891 };
    ui_render(&g, &st, NULL, 0);
    CHECK(!pixel(&g, kx + 1, 8) && !pixel(&g, kx + 2, 8));       // C
    CHECK(!pixel(&g, kx + 21, 8) && pixel(&g, kx + 22, 8));      // E
    st.root = 3;
    st.marks = 0x891 << 3;
    ui_render(&g, &st, NULL, 0);
    notch = widget_key_x(kx, 3);                        // Eb: an outline
    CHECK(pixel(&g, notch, 3) && !pixel(&g, notch + 1, 3) && pixel(&g, notch + 2, 3));
    notch = widget_key_x(kx, 10);                       // Bb: dotted
    CHECK(pixel(&g, notch, 0) && !pixel(&g, notch + 1, 0));

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

    // A setting's value slides the same way, clipped to its rows, but the
    // expression pedal's doesn't
    st = (ui_state_t){ .config = true, .msg = CONFIG_MSG_VELOCITY, .msg_value = 95,
                       .root = -1 };
    ui_anim_update(&a, &st, 6000);
    st.msg_value = 96;
    ui_anim_update(&a, &st, 6100);
    CHECK(ui_anim_running(&a, 6100) && !ui_anim_running(&a, 6100 + UI_SLIDE_MS));
    ui_render(&g, &st, &a, 6175);
    CHECK(lit(&g, 32, 14, 64, 23) && lit(&g, 32, 23, 64, 32));  // old above, new below
    ui_render(&g, &st, &a, 6100 + UI_SLIDE_MS);
    CHECK(!lit(&g, 32, 30, 64, 32));                    // settled
    st.msg = CONFIG_MSG_EXPRESSION;
    ui_anim_update(&a, &st, 6300);
    st.msg_value = 50;
    ui_anim_update(&a, &st, 6400);
    CHECK(!ui_anim_running(&a, 6400));

    // Battery page: voltage left of the column, type above the charge bar
    st = (ui_state_t){ .config = true, .msg = CONFIG_MSG_BATTERY, .root = -1, .battery = true,
                       .battery_level = 128, .battery_mv = 4120, .battery_type = BATTERY_LIION,
                       .battery_cells = 1 };
    ui_render(&g, &st, NULL, 0);
    CHECK(lit(&g, 0, 14, 64, 28) && lit(&g, 64, 13, 128, 21) && pixel(&g, 64, 23));

    // Battery while playing: in the bottom right corner, HOLD beside it
    st = (ui_state_t){ .octave = 3, .root = -1, .battery = true, .battery_level = 255,
                       .chord_mode = true, .hold = true };
    ui_render(&g, &st, NULL, 0);
    CHECK(pixel(&g, 122, 30) && pixel(&g, 126, 23) && !lit(&g, 122, 13, 127, 21));
    CHECK(!lit(&g, 121, 21, 122, 32) && lit(&g, 88, 23, 120, 32));
    st.battery_external = true;                         // full, a dark bolt
    ui_render(&g, &st, NULL, 0);
    CHECK(pixel(&g, 123, 23) && pixel(&g, 125, 29) && !pixel(&g, 124, 25));
    st.battery_external = false;
    st.battery_level = 10;
    ui_anim_t c = { 0 };
    ui_anim_update(&c, &st, 0);
    CHECK(!ui_anim_running(&c, 0));                     // "!" steady

    // On the config map, under F', filling from the bottom; nearly empty
    // it blinks an exclamation mark, so frames keep coming
    st = (ui_state_t){ .octave = 3, .root = -1, .battery = true, .battery_level = 255 };
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
    test_modes();
    test_octave();
    test_app();
    test_switch();
    test_battery();
    test_expression();
    test_ui();
    test_splash();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return EXIT_FAILURE;
    }
    printf("all tests passed\n");
    return EXIT_SUCCESS;
}
