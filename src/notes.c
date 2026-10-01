#include "midi.h"
#include "notes.h"

static uint8_t channel;
static uint8_t count[128];
static uint32_t sounding;

void notes_init(uint8_t ch) {
    channel = ch;
    notes_all_off();
}

void notes_on(int note, uint8_t velocity) {
    if (note < 0 || note > 127) {
        return;
    }
    // Repeated notes re-attack; the count keeps the note-off for the last
    midi_note_on(channel, note, velocity);
    if (count[note]++ == 0) {
        sounding++;
    }
}

void notes_off(int note) {
    if (note < 0 || note > 127 || count[note] == 0) {
        return;
    }
    if (--count[note] == 0) {
        sounding--;
        midi_note_off(channel, note);
    }
}

void notes_all_off(void) {
    for (int n = 0; sounding && n < 128; n++) {
        if (count[n]) {
            count[n] = 0;
            sounding--;
            midi_note_off(channel, n);
        }
    }
}

void notes_panic(void) {
    notes_all_off();
    midi_cc(channel, MIDI_CC_ALL_NOTES_OFF, 0);
    midi_cc(channel, MIDI_CC_ALL_SOUND_OFF, 0);
}

void notes_sound(const sound_t *sound) {
    midi_cc(channel, MIDI_CC_BANK_MSB, sound->msb);
    midi_cc(channel, MIDI_CC_BANK_LSB, sound->lsb);
    midi_program(channel, sound->program - 1);
}
