#include "input.h"
#include "keyboard.h"
#include "notes.h"

const chord_t CHORDS[CHORD_COUNT] = {
    { "Major",     3, { 0, 4, 7 } },
    { "Minor",     3, { 0, 3, 7 } },
    { "Major 7th", 4, { 0, 4, 7, 11 } },
    { "Minor 7th", 4, { 0, 3, 7, 10 } },
    { "Sev (7)",   4, { 0, 4, 7, 10 } },
    { "Dim 7th",   4, { 0, 3, 6, 9 } },
    { "H-dim 7th", 4, { 0, 3, 6, 10 } },
};

// Chord selected by keys 12-18 (C' to G'b)
static const uint8_t SELECTOR[7] = { 2, 0, 3, 1, 5, 6, 4 };

#define NO_KEY   (-1)

static const settings_t *settings;
static bool chord_mode;
static bool hold;
static uint8_t chord;

// Keys whose press was taken in the current mode
static uint32_t active;
// Normal mode: note each key started
static int16_t key_note[KEY_COUNT];

// Chord mode: the sounding chord and the root queued behind it
static int sounding_root = NO_KEY;
static int queued_root = NO_KEY;
static int sounding_notes[4];
static uint8_t sounding_count;

static int base_note(int key, uint8_t octave) {
    return key + settings->midi_transpose + 12 * (octave + 1);
}

static void chord_start(int root, uint8_t octave) {
    const chord_t *c = &CHORDS[chord];
    int base = base_note(root, octave);
    sounding_root = root;
    sounding_count = c->count;
    for (int i = 0; i < c->count; i++) {
        sounding_notes[i] = base + c->tones[i];
        notes_on(sounding_notes[i], settings->midi_velocity);
    }
}

static void chord_stop(void) {
    for (int i = 0; i < sounding_count; i++) {
        notes_off(sounding_notes[i]);
    }
    sounding_count = 0;
    sounding_root = NO_KEY;
}

void keyboard_init(const settings_t *s) {
    settings = s;
    chord_mode = false;
    hold = false;
    chord = 0;
    keyboard_reset();
}

void keyboard_reset(void) {
    notes_all_off();
    active = 0;
    sounding_root = NO_KEY;
    queued_root = NO_KEY;
    sounding_count = 0;
}

static void chord_press(int key, uint8_t octave) {
    if (key >= 12) {
        if (key == KEY_HOLD) {
            hold = !hold;
            if (!hold) {
                chord_stop();
                queued_root = NO_KEY;
            }
        } else {
            chord = SELECTOR[key - 12];
        }
        return;
    }

    if (hold || sounding_root == NO_KEY) {
        chord_stop();
        chord_start(key, octave);
    } else {
        queued_root = key;
    }
}

// The queued chord starts with the octave and chord type current now
static void chord_release(int key, uint8_t octave) {
    if (hold || key >= 12) {
        return;
    }
    if (key == sounding_root) {
        chord_stop();
        if (queued_root != NO_KEY) {
            chord_start(queued_root, octave);
            queued_root = NO_KEY;
        }
    } else if (key == queued_root) {
        queued_root = NO_KEY;
    }
}

void keyboard_press(int key, uint8_t octave) {
    active |= INPUT_BIT(key);
    if (chord_mode) {
        chord_press(key, octave);
    } else {
        key_note[key] = base_note(key, octave);
        notes_on(key_note[key], settings->midi_velocity);
    }
}

void keyboard_release(int key, uint8_t octave) {
    if (!(active & INPUT_BIT(key))) {
        return;
    }
    active &= ~INPUT_BIT(key);
    if (chord_mode) {
        chord_release(key, octave);
    } else {
        notes_off(key_note[key]);
    }
}

void keyboard_set_chord_mode(bool on) {
    keyboard_reset();
    chord_mode = on;
}

bool keyboard_chord_mode(void) { return chord_mode; }
uint8_t keyboard_chord(void) { return chord; }
bool keyboard_hold(void) { return hold; }
