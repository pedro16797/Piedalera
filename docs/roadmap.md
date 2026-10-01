# Roadmap

Everything the MicroPython firmware did is implemented and running on the
pedalboard, plus a keyboard screen, start-up animation, mode and octave
transitions, a debounce setting, USB flash mode and a MIDI panic from
config mode, a watchdog, an expression pedal that learns its travel, and
for battery use: charge monitoring, idle dimming, screen off and deep sleep.

## To check on the hardware

- [ ] Current draw playing, with the screen off and in deep sleep (crystal,
  PLL and ring oscillator all stopped)
- [ ] Waking from deep sleep on every board, and the press playing
- [ ] USB flash mode after having been in deep sleep
- [ ] No watchdog resets in normal use, including settings saves and deep sleep
- [ ] Battery voltage against a multimeter (`power.drop_mv`), also on a W board
- [ ] Expression pedal: no CC sent while it stands still, the full 0–127
  after one sweep, and the learnt travel kept after a power cycle

## Next

Each in its own pull request:

- [ ] Instrument presets (`instList` in `legacy/Piedalera.py`)
- [ ] Drumpad link: a second Pico forwards hits from a MIDI drumpad over
  BLE MIDI, and the pedalboard turns them into bass notes, strums or
  arpeggios of the current chord. Needs a W board.

## Open questions

- Which Roland module is the target, and which bank/program list applies?
- Changing the bank sends bank select without a program change, which the
  synth may ignore until the next one. Should the firmware send a program
  change too, and the bank at power-up?
- DIY expression pedal design.
