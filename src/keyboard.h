#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "settings.h"

#define CHORD_COUNT 7

// Chord mode: keys 12-18 select a chord, 19 toggles hold
#define KEY_HOLD    19

typedef struct {
    const char *name;   // up to 13 characters, after the root on screen
    uint8_t count;
    uint8_t tones[4];   // semitones above the root, root included
} chord_t;

extern const chord_t CHORDS[CHORD_COUNT];

// Settings are read live, so config mode changes apply to the next note
void keyboard_init(const settings_t *s);

void keyboard_press(int key, uint8_t octave);
void keyboard_release(int key, uint8_t octave);

// Stops everything; keys held now are ignored until pressed again
void keyboard_reset(void);

void keyboard_set_chord_mode(bool on);

// Chord mode: root key of the sounding chord (-1 if none), and keys to mark
// on screen: the sounding chord's notes
int keyboard_root(void);
uint32_t keyboard_marks(void);
bool keyboard_chord_mode(void);
uint8_t keyboard_chord(void);
bool keyboard_hold(void);
