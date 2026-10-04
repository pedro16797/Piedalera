# Roadmap

## To check on the hardware

- [ ] Current draw playing, with the screen off and in deep sleep (crystal,
  PLL and ring oscillator all stopped)
- [ ] Battery voltage against a multimeter (`power.drop_mv`), also on a W board

## Later

- [ ] Narrow keyboard mode with no chord/scale modes and alternative narrow
  Config, single key per config category and the octave buttons for up/down
- [ ] Drumpad link: a second Pico forwards hits from a MIDI drumpad over
  BLE MIDI, and the pedalboard turns them into bass notes, strums or
  arpeggios of the current chord. Needs a W board.
