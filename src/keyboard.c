#include "input.h"
#include "keyboard.h"
#include "notes.h"

const chord_t CHORDS[CHORD_COUNT] = {
    { "Major",        3, { 0, 4, 7 } },
    { "Minor",        3, { 0, 3, 7 } },
    { "Major 7th",    4, { 0, 4, 7, 11 } },
    { "Minor 7th",    4, { 0, 3, 7, 10 } },
    { "Dominant 7th", 4, { 0, 4, 7, 10 } },
    { "Dim 7th",      4, { 0, 3, 6, 9 } },
    { "Half-dim 7th", 4, { 0, 3, 6, 10 } },
};

// Chord selected by keys 12-18 (C' to G'b)
static const uint8_t SELECTOR[7] = { 2, 0, 3, 1, 5, 6, 4 };

// Modes on the same keys, each a step further up the major scale
const char *const MODES[MODE_COUNT] = {
    "Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian", "Locrian",
};

static const uint8_t MAJOR[7] = { 0, 2, 4, 5, 7, 9, 11 };

#define NO_KEY   (-1)

static settings_t *settings;
static bool chord_mode;
static bool hold;
static uint8_t chord;
static uint8_t mode;
static uint8_t tonic;
static bool tonic_next;     // the next pedal sets the tonic

// Keys whose press was taken in the current mode
static uint32_t active;
// Normal mode: note each key started
static int16_t key_note[KEY_COUNT];

// Chord and scale mode: the sounding chord and the root queued behind it
static int sounding_root = NO_KEY;
static int queued_root = NO_KEY;
static int sounding_notes[4];
static uint8_t sounding_count;
static int chord_root;      // key of the sounding chord's root, may differ

static bool scales(void) {
    return settings->keys_alternative == 1;
}

static int base_note(int key, uint8_t octave) {
    return key + settings->midi_transpose + 12 * (octave + 1);
}

// Semitones from the tonic to a degree of the mode, counting on upwards
static int step(int degree) {
    int i = mode + degree;
    return MAJOR[i % 7] + 12 * (i / 7) - MAJOR[mode];
}

// Degree of a note semitones above the tonic, -1 if outside the mode
static int degree_of(int rel) {
    for (int degree = 0; degree < 7; degree++) {
        if (step(degree) == rel) {
            return degree;
        }
    }
    return -1;
}

// Semitones from b to a, -6 to 5
static int interval(int a, int b) {
    return ((a - b) % 12 + 18) % 12 - 6;
}

// Letter of a note (0 C to 6 B), black keys spelt sharp (sign 1) or flat
static int letter(int note, int sign) {
    for (int pass = 0; pass < 2; pass++, note = (note - sign + 12) % 12) {
        for (int i = 0; i < 7; i++) {
            if (MAJOR[i] == note) {
                return i;
            }
        }
    }
    return 0;
}

// Sharp (1) or flat (-1) scale, spelt from the tonic's name as on screen
// (flats for black keys); 0 if all naturals
static int scale_sign(void) {
    int first = letter(tonic, -1);
    for (int degree = 0; degree < 7; degree++) {
        int accidental = interval(tonic + step(degree), MAJOR[(first + degree) % 7]);
        if (accidental) {
            return accidental > 0 ? 1 : -1;
        }
    }
    return 0;
}

// A note outside the scale, spelt like the scale, is the scale note of the
// same letter altered; that note is a semitone above or below. Without
// sharps or flats, or out of reach, the one below.
static int neighbour(int key) {
    int sign = scale_sign();
    if (sign) {
        int degree = (letter(key % 12, sign) - letter(tonic, -1) + 7) % 7;
        int shift = interval(tonic + step(degree), key);
        if (shift == 1 || shift == -1) {
            return shift;
        }
    }
    return -1;
}

// Scale mode: a key in the scale plays its seventh chord, built from the
// scale; one outside it, the triad of its neighbour in the scale. Returns
// the root.
static int scale_chord(int key, chord_t *c) {
    int rel = (key - tonic + 12) % 12;
    int degree = degree_of(rel);
    c->count = 4;
    if (degree < 0) {
        // Scale steps are one or two semitones, so both neighbours are in it
        int shift = neighbour(key);
        key += shift;
        degree = degree_of((rel + shift + 12) % 12);
        c->count = 3;
    }
    for (int i = 0; i < c->count; i++) {
        c->tones[i] = step(degree + 2 * i) - step(degree);
    }
    return key;
}

static void chord_start(int key, uint8_t octave) {
    chord_t c = CHORDS[chord];
    chord_root = scales() ? scale_chord(key, &c) : key;
    int base = base_note(chord_root, octave);
    sounding_root = key;
    sounding_count = c.count;
    for (int i = 0; i < c.count; i++) {
        sounding_notes[i] = base + c.tones[i];
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

void keyboard_init(settings_t *s) {
    settings = s;
    chord_mode = false;
    hold = false;
    chord = 0;
    mode = 0;
    tonic = 0;
    tonic_next = true;
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
        } else if (scales()) {
            mode = key - 12;
            tonic_next = true;
        } else {
            chord = SELECTOR[key - 12];
        }
        return;
    }
    // First pedal after picking a mode: its tonic, and plays as such
    if (scales() && tonic_next) {
        tonic = key;
        tonic_next = false;
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

void keyboard_switch_alternative(void) {
    keyboard_reset();
    hold = !hold;
    settings->keys_alternative = !settings->keys_alternative;
    tonic_next = true;
}

int keyboard_root(void) {
    return chord_mode && sounding_root != NO_KEY && chord_root >= 0 ? chord_root : -1;
}

uint32_t keyboard_marks(void) {
    if (!chord_mode) {
        return 0;
    }
    uint32_t marks = 0;
    if (sounding_root != NO_KEY) {
        for (int i = 0; i < sounding_count; i++) {
            int key = chord_root + sounding_notes[i] - sounding_notes[0];
            if (key >= 0 && key < KEY_COUNT) {
                marks |= INPUT_BIT(key);
            }
        }
    }
    return marks;
}

bool keyboard_chord_mode(void) { return chord_mode; }
uint8_t keyboard_chord(void) { return chord; }
bool keyboard_hold(void) { return hold; }
bool keyboard_scales(void) { return scales(); }
uint8_t keyboard_mode(void) { return mode; }
int keyboard_tonic(void) { return tonic_next ? -1 : tonic; }
