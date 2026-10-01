# Roadmap

Everything the MicroPython firmware did is implemented and running on the
pedalboard, plus a keyboard screen, start-up animation, mode and octave
transitions, a debounce setting, USB flash mode and a MIDI panic from
config mode, a watchdog, an expression pedal calibrated in config mode, a
sound list editable in the settings file (or the synth's own sound), and
for battery use: charge monitoring, idle dimming, screen off and deep sleep.

## To check on the hardware

- [ ] Current draw playing, with the screen off and in deep sleep (crystal,
  PLL and ring oscillator all stopped)
- [ ] Waking from deep sleep on every board, and the press playing
- [ ] USB flash mode after having been in deep sleep
- [ ] No watchdog resets in normal use, including settings saves and deep sleep
- [ ] Battery voltage against a multimeter (`power.drop_mv`), also on a W board
- [ ] USB power shown on every board (GPIO24 on Pico and Pico 2, the
  voltage alone on W boards)
- [ ] Expression pedal: no CC sent while it stands still, the full 0–127
  after one sweep on its page, the travel kept after a power cycle, and
  learnt again after turning it off and on
- [ ] Expression probe: an empty jack found within a second, a plugged pedal
  never taken for one at any position, on Pico and Pico 2
- [ ] The screen no longer going dark for a moment while playing
- [ ] The Yamaha MU5 keeping its own sound by default, and taking each
  listed sound when picked and at power-up

## Next

Each in its own pull request:

- [ ] Mode mode: a third mode where the upper keys pick a musical mode
  (Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian) instead
  of a chord type

## Later

- [ ] Drumpad link: a second Pico forwards hits from a MIDI drumpad over
  BLE MIDI, and the pedalboard turns them into bass notes, strums or
  arpeggios of the current chord. Needs a W board.

## Open questions

- Whether the default sound list should use any of the MU5's banks beyond
  General MIDI.
- What the lower keys play in mode mode: the root's diatonic chord, or the
  scale's notes from the root?
- DIY expression pedal design.
