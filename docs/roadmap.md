# Roadmap

Everything the MicroPython firmware did is implemented and running on the
pedalboard, plus a keyboard screen, start-up animation, mode and octave
transitions, idle dimming, a debounce setting and USB flash mode from config
mode.

## Next

- [ ] MIDI panic on demand
- [ ] Watchdog
- [ ] Expression pedal on GP28 / ADC2 (CC11, calibration)
- [ ] Instrument presets (`instList` in `legacy/Piedalera.py`)
- [ ] Wear levelling for the settings sector, if octave saves turn out
  frequent

## Later

- **Drumpad link:** a second Pico forwards hits from a MIDI drumpad over
  BLE MIDI, and the pedalboard turns them into bass notes, strums or
  arpeggios of the current chord. Needs a W board; parked until the
  keyboard works.

## Open questions

- Which Roland module is the target, and which bank/program list applies?
- Changing the bank sends bank select without a program change, which the
  synth may ignore until the next one. Should the firmware send a program
  change too, and the bank at power-up?
- DIY expression pedal design.
