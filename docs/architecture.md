# Architecture

How the C firmware in `src/` is organised. Keep this file in sync with the
code.

## Core split

```
            core 0 (real time)                        core 1
 ┌──────────────────────────────────┐     ┌──────────────────────────────┐
 │ every 1 ms:                      │     │ every 16.7 ms (≤ 60 fps):    │
 │  ├─ read all GPIOs at once       │     │  ├─ take UI snapshot         │
 │  ├─ debounce                     │     │  ├─ draw frame if changed    │
 │  ├─ octave buttons, config mode, │ ──▶ │  ├─ send it by I2C DMA       │
 │  │  keyboard and chords          │     │  └─ write settings to flash  │
 │  └─ queue MIDI ─▶ UART TX IRQ    │     │     when asked               │
 └──────────────────────────────────┘     └──────────────────────────────┘
              UI snapshot + settings to save, under a hardware spinlock
```

- **Core 0** owns the inputs, all playing state and MIDI output. Its loop
  takes a few microseconds of each millisecond, leaving room for the
  expression pedal and more.
- **Core 1** owns the display and flash writes. It only reads what core 0
  publishes and never touches playing state.
- **Hand-over:** core 0 publishes a small `ui_state_t` snapshot, only when it
  changes, and a copy of the settings when they need saving. Both sit behind
  one hardware spinlock held for a struct copy.
- **The whole firmware runs from RAM** (`copy_to_ram`). There are no flash
  cache misses, and erasing flash on core 1 can't stall core 0. Flash is only
  read at boot and written by core 1.
- **No stdio**, so no USB interrupts on core 0.

## Modules

Hardware-independent modules are also built into the host tests (`test/`);
`gfx.c` and `ui.c` also run in the display preview (`tools/preview`).

| Module          | Core | Host | Responsibility |
|-----------------|------|------|----------------|
| `board.h`       | –    |      | Pin map and hardware constants |
| `input.c`       | 0    |      | GPIO setup (pulls, polarity), one read of all inputs |
| `debounce.c`    | 0    | ✓    | Per-input debounce: a change counts once stable for `keys.debounce_ms` |
| `midi.c`        | 0    |      | UART0 at 31250 baud, 256-byte ring drained by the TX interrupt, running status |
| `notes.c`       | 0    | ✓    | Note on/off with a count per MIDI note, range checks, power-up panic |
| `keyboard.c`    | 0    | ✓    | Normal mode, chord mode with queue and hold, chord table |
| `octave.c`      | 0    | ✓    | Octave buttons, auto-repeat, both-buttons gesture |
| `config_mode.c` | 0    | ✓    | Config mode keys, auto-repeat, clamping |
| `app.c`         | 0    | ✓    | Routes inputs to the above, UI snapshot, save timing |
| `settings.c`    | –    | ✓    | Settings table, text parse and format |
| `storage.c`     | 0, 1 |      | Load at boot (core 0), save (core 1) |
| `gfx.c`         | 1    | ✓    | 1-bit framebuffer in SSD1305 layout, 8×8 text |
| `ui.c`          | 1    | ✓    | Draws the three-row screen from a snapshot |
| `display.c`     | 1    |      | SSD1305 init and DMA frame transfer |
| `main.c`        | 0, 1 |      | Start-up, both core loops, hand-over |

## Key decisions

- **Timing** uses 32-bit millisecond timestamps compared by unsigned
  difference, which stays correct across wrap-around (fixes legacy issue #1).
- **Debounce** per input, 5 ms by default, at a 1 kHz scan.
- **Note bookkeeping:** each key remembers the exact notes it started, so
  octave or transpose changes never leave notes hanging. A MIDI note shared
  by two keys stops only when both are released. Keys held through a mode
  change are ignored until pressed again (fixes #7, #8).
- **Chord mode is monophonic with a queued next chord.** One chord sounds at
  a time. A root pressed while a chord sounds is queued (the latest press
  wins) and starts in the same scan the sounding root is released, with no
  gap. Releasing a queued root before its turn cancels it. Chord type and
  octave are taken when the chord starts, so they can be prepared while the
  previous chord still sounds. Nothing is cut or re-attacked (fixes #2).
- **Hold** (G' in chord mode, as in the legacy firmware) latches the chord:
  pressing a root switches to its chord immediately, releasing does nothing,
  and turning hold off stops the chord.
- **Both octave buttons** toggle chord mode after `octave.delay_ms` and enter
  config mode after 1 s (undoing the toggle), as before. Afterwards the
  buttons are ignored until both are released, so a button left over
  doesn't step the octave.
- **MIDI output** never blocks: messages go into a ring buffer drained by
  the UART TX interrupt. Note-offs are sent as note-on with velocity 0 so
  every note message shares the running status, which saves a third of the
  bytes. The status byte is repeated after 1 s of silence. Notes outside
  0–127 are dropped and data bytes are masked to 7 bits (fixes #3, #15).
- **Power-up** sends All Notes Off and All Sound Off.
- **Config mode** keeps the legacy keys and timings, clamps instead of
  wrapping (fixes #4, #11) and leaves when any other key is released.
- **Settings** live in the second to last flash sector as `key = value` text
  (see [`configuration.md`](configuration.md)), so they can be flashed as a
  separate UF2 without rebuilding. Invalid values fall back to defaults key
  by key (fixes #6).
- **Saving** happens right after config mode or 5 s after the last octave
  change, and only if the text differs from what is stored (fixes #14).
- **Display:** the frame is drawn into a RAM framebuffer, then sent as one
  DMA transfer of 16-bit I2C commands (window and contrast commands, then
  the pixels) at 400 kHz, about 12 ms for 128×32. Core 1 is free while it
  goes out. Frames are only sent when the snapshot changes. A missing or
  unpowered display is retried every 500 ms. Size and column offset come
  from the settings.
- The fixed-rate loop with `sleep_until`, the new default brightness and
  explicit pad setup (`keys.pull`, which also clears the power-on
  pull-downs) take care of legacy issues #9, #10, #12 and #13.
- **Expression pedal (planned):** sampled at the scan rate, low-pass
  filtered, mapped through min/max calibration to 0–127 and sent as CC11
  only when the value changes by more than a hysteresis threshold.
