# Legacy issues

Bugs and weak spots found in `legacy/main.py`, the firmware running on the
pedalboard. Each one should be fixed (or deliberately kept) in the C port.
Line numbers refer to `legacy/main.py`.

Severity: **High** breaks playing, **Medium** wrong output or state,
**Low** cosmetic or latent.

## Bugs

| # | Sev. | Lines | Issue |
|---|------|-------|-------|
| 1 | High | 175–246, 331 | Raw `ticks_us()` arithmetic without `ticks_diff()`. MicroPython ticks wrap every 2³⁰ µs (~17.9 min), so octave timing, the both-buttons timer and the loop sleep misbehave at each wrap. `octUpCount = start + OCT_ITERT - OCT_DELAY` can even be past the wrap point. |
| 2 | Medium | 209–212 | With hold off, releasing one chord root calls `allNotesOff()`, which also silences any other root still held. |
| 3 | Medium | 158–207 | No check that `note + transpose + tone + 12 * (octave + 1)` stays within 0–127. At octave 7 with transpose or a chord it goes past 127 (up to 138), and a negative transpose at octave 0 goes below 0; `pack("b")` then emits a status byte instead of a note. |
| 4 | Medium | 269–282 | Velocity wraps down to 0, and a note-on with velocity 0 means note-off, so every key goes silent. |
| 5 | Medium | 283–300 | The "Channel" setting isn't the MIDI channel: it sends bank select (CC0 = 121, CC32 = value) on channel 1, with no program change after it, so most synths only apply it at the next program change. It isn't sent at power-up either. All notes go out on channel 1. |
| 6 | Medium | 35–54 | A partially bad `config.txt` leaves some settings loaded and others at default, then `secago()` overwrites the file with defaults. Values are not range-checked (e.g. `Min_Oct > Max_Oct`, `Period = 0`). |
| 7 | Medium | 65–84, 205–208 | Each key's last reading starts as `False` (pressed), so the first scan treats every released key as a release and sends 20 stray note-offs. They target MIDI notes 0–19 (plus transpose), far below the played range, so they can't clear stuck notes; with a negative transpose some are out of range (see #3). The port sends All Notes Off / All Sound Off at power-up instead. |
| 8 | Low | 183–192 | A chord selector or hold press `break`s the key loop, delaying the keys after it by one scan. Selectors also set their octave to 0 and never reset it, so `allNotesOff()` later sends stray note-offs for them. |
| 9 | Low | 253–321 | Config mode busy-waits with no sleep and no debounce. The `iterating` flag is shared by all keys in a pass, so a second key held in the same pass skips its initial delay. |
| 10 | Low | 331 | `PERIOD + min(0, start - now)` goes negative when an iteration overruns; should clamp at 0. |
| 11 | Low | 259–312 | Brightness, velocity, "Channel" and transpose wrap around instead of clamping, so overshooting a hold jumps from max to min. |
| 12 | Low | 19 | Default brightness is 0 (minimum contrast). |
| 13 | Low | 59–84 | Pins are created without `Pin.IN` / `Pin.PULL_UP`. Works on the Picórgano board thanks to its 10 kΩ external pull-ups, but the pads' power-on pull-downs stay enabled against them. |
| 14 | Low | 323–326 | Every config exit rewrites `config.txt`, even with no changes (flash wear). |
| 15 | Low | 158–207 | `pack("bbb", 0x90, ...)` packs unsigned bytes as signed; it only works because MicroPython doesn't range-check. |

## Fixed in `main.py`

Problems in the older `Piedalera.py` that `main.py` already solves:
brightness changes no longer crash the main loop (the display thread applies
the contrast), a held chord selector no longer re-triggers every scan,
releasing a chord no longer re-attacks other held roots, the "Channel" value
is bounded, config mode stays open between adjustments, the brightness
display is consistent and the config message is cleared on exit.

## Design weaknesses

- **No debounce.** Keys rely on the 10 ms scan period; switch bounce can
  produce double notes.
- **No MIDI panic.** Mode changes send individual note-offs, never
  *All Notes Off* (CC123), so a stuck note on the synth can't be cleared.
- **Blocking UART writes** in the scan loop add jitter proportional to how
  many notes change at once (each 3-byte message takes ~1 ms on the wire).
- **Shared globals between threads** without any synchronisation; tolerable
  under the MicroPython GIL but not in C.
- **Chord mode takes over keys 12–19**, so chord mode has only one octave of
  roots and no bass notes above B.
- **A held chord can only be stopped** by turning hold off or leaving chord
  mode; pressing its root again re-attacks it.
- **Dead code:** unused imports (`SPI`, `os`, `framebuf`), `configMode`,
  the `dot` blink variable and the commented-out logo.
