# Roadmap

## 0. Repository setup ✅

- Legacy firmware under source control, behaviour and issues documented.
- CI building firmware and default config UF2s for Pico, Pico W, Pico 2 and
  Pico 2 W, plus host tests; releases on `v*` tags.
- Settings file format and `tools/config2uf2.py`.
- Display preview on the computer (`tools/preview`).

## 1. Parity port ✅ (untested on hardware)

Reproduces `legacy/main.py` in C with the bugs in
[`legacy-issues.md`](legacy-issues.md) fixed. See
[`architecture.md`](architecture.md).

- [x] GPIO input with polarity and pulls from settings, and debounce
- [x] MIDI out over UART with a non-blocking TX buffer
- [x] Normal mode notes with per-key octave memory
- [x] Octave buttons with delay and auto-repeat
- [x] Chord mode (monophonic with a queued chord), chord selection and hold
- [x] SSD1305 driver and the legacy three-row screen on core 1
- [x] Settings read from and saved to flash ([`configuration.md`](configuration.md))
- [x] Config mode
- [ ] Try it on the pedalboard

## 2. Hardening

- [x] Configurable MIDI channel
- [x] Settings validation
- [x] MIDI panic at power-up
- [ ] MIDI panic on demand
- [ ] Watchdog
- [ ] Wear levelling for the settings sector, if octave saves turn out frequent

## 3. New features

- [ ] Display animations for notes, octave shifts, chord changes and mode changes
- [ ] Expression pedal on GP28 / ADC2 (CC11, calibration)
- [ ] Instrument presets (`instList` from the older `Piedalera.py`, with
  0-based program numbers)
- [ ] Config mode as a menu with clear navigation and an exit key

## Later

- Drumpad link: pad hits from a second Pico trigger bass, chords and
  arpeggios over BLE MIDI ([`drumpad-link.md`](drumpad-link.md)). Parked
  until the keyboard works.

## Decisions

- Display size is a runtime setting, not a build option.
- Chord mode is monophonic: a root pressed while a chord sounds is queued and
  plays as soon as the sounding chord is released. Hold latches chords as in
  the legacy firmware.
- Config mode values clamp at their limits.
- The current octave is saved across power cycles.
- The firmware and defaults target the current hardware; simplifications
  go in [`hardware-improvements.md`](hardware-improvements.md).
- The repository stays private until the first working version.

## Open questions

- What does changing "Ch" (now "Bank") in config mode do on the synth? It
  only sends bank select without a program change (legacy issue #5), so the
  Roland may ignore it until the next program change. Should the firmware
  send it (and a program) at power-up?
- DIY expression pedal design.
- Which Roland module is the target, and which bank/program list applies?
