# Roadmap

## To check on the hardware

- [ ] Current draw playing, with the screen off and in deep sleep (crystal,
  PLL and ring oscillator all stopped)
- [ ] Battery voltage against a multimeter (`power.drop_mv`), also on a W board
- [ ] Expression probe: an empty jack found within a second, a plugged pedal
  never taken for one at any position, on Pico and Pico 2
- [ ] The screen no longer going dark for a moment while playing
- [ ] The Yamaha MU5 keeping its own sound by default, and taking each
  listed sound when picked and at power-up
- [ ] Scale mode: H held switching to it and back, kept after a power cycle,
  and the first pedal after a mode key setting the tonic

## Later

- [ ] Narrow keyboard mode with no chord/scale modes and alternative narrow
  Config, single key per config category and the octave buttons for up/down
- [ ] Drumpad link: a second Pico forwards hits from a MIDI drumpad over
  BLE MIDI, and the pedalboard turns them into bass notes, strums or
  arpeggios of the current chord. Needs a W board.
