#pragma once

#include <stdint.h>

// Channels are 0-15 here, 1-16 in the settings

// Hardware side, see midi.c: UART0 with an interrupt-driven TX buffer
void midi_init(void);
void midi_note_on(uint8_t ch, uint8_t note, uint8_t velocity);
void midi_note_off(uint8_t ch, uint8_t note);
void midi_cc(uint8_t ch, uint8_t cc, uint8_t value);

// Wait until everything queued is sent, e.g. before a clock change
void midi_flush(void);

#define MIDI_CC_BANK_MSB        0
#define MIDI_CC_BANK_LSB        32
#define MIDI_CC_ALL_SOUND_OFF   120
#define MIDI_CC_ALL_NOTES_OFF   123

// Roland GM2 melodic bank
#define MIDI_BANK_MSB_GM2       121
