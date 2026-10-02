#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

#define CHORD_COUNT 7
#define MODE_COUNT  7

// Chord and mode mode: keys 12-18 select a chord type or a mode, 19 toggles
// hold, and held this long switches between the two
#define KEY_HOLD    19
#define KEY_HOLD_SWITCH_MS 1000

typedef struct {
    const char *name;   // up to 13 characters, after the root on screen
    uint8_t count;
    uint8_t tones[4];   // semitones above the root, root included
} chord_t;

extern const chord_t CHORDS[CHORD_COUNT];
extern const char *const MODES[MODE_COUNT];

// Settings are read live, so config mode changes apply to the next note
void keyboard_init(settings_t *s);

void keyboard_press(int key, uint8_t octave);
void keyboard_release(int key, uint8_t octave);

// Stops everything; keys held now are ignored until pressed again
void keyboard_reset(void);

// Chord mode, or mode mode with s->keys_alternative
void keyboard_set_chord_mode(bool on);

// Undoes the hold toggle of the press on H, then switches chord and mode
// mode, saved in s->keys_alternative
void keyboard_switch_alternative(void);

// Chord and mode mode: root key of the sounding chord (-1 if none or off
// the keyboard), and keys to mark on screen: the sounding chord's notes
int keyboard_root(void);
uint32_t keyboard_marks(void);
bool keyboard_chord_mode(void);
uint8_t keyboard_chord(void);
bool keyboard_hold(void);

// Mode mode: the mode and the key of its tonic (0-11), -1 until the first
// pedal after picking a mode or switching to mode mode, which sets it
bool keyboard_modes(void);
uint8_t keyboard_mode(void);
int keyboard_tonic(void);
