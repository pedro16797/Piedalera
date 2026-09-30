#pragma once

#include <stdbool.h>
#include <stdint.h>

// Note on/off with a count per MIDI note, so a note shared by two keys only
// stops when the last one releases it. Out of range notes are ignored.
void notes_init(uint8_t channel);
void notes_on(int note, uint8_t velocity);
void notes_off(int note);

// Stop everything this module started
void notes_all_off(void);

// All Notes Off + All Sound Off, for whatever the synth may still play
void notes_panic(void);
