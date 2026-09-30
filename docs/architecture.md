# Architecture

## Cores

```
 core 0, every 1 ms                      core 1, up to 60 fps
 read inputs, debounce                   take the UI snapshot
 octave buttons, config mode,     ──▶    draw the frame if it changed
 keyboard and chords                     send it by I2C DMA
 queue MIDI → UART TX interrupt          write settings to flash
```

- Core 0 owns inputs, playing state and MIDI. Core 1 owns the display and
  flash writes and never touches playing state.
- Core 0 publishes a small `ui_state_t` snapshot when it changes, and a copy
  of the settings when they need saving, both behind one hardware spinlock.
- The firmware runs from RAM (`copy_to_ram`), so erasing flash on core 1
  can't stall core 0. There is no stdio.

## Modules

Modules marked ✓ have no SDK dependencies and are built into the host tests;
the drawing modules also run in the display preview (`tools/preview`).

| Module          | Core | Host | Does |
|-----------------|------|------|------|
| `board.h`       | –    |      | Pin map |
| `input.c`       | 0    |      | GPIO setup, one read of all inputs |
| `debounce.c`    | 0    | ✓    | Per-input debounce |
| `midi.c`        | 0    |      | UART at 31250 baud, ring buffer drained by the TX interrupt |
| `notes.c`       | 0    | ✓    | Note on/off with a count per note, power-up panic |
| `keyboard.c`    | 0    | ✓    | Normal and chord mode, chord table |
| `octave.c`      | 0    | ✓    | Octave buttons, auto-repeat, both-buttons gesture |
| `config_mode.c` | 0    | ✓    | Config keys, auto-repeat, clamping, G' hold |
| `app.c`         | 0    | ✓    | Routes inputs, UI snapshot, save timing |
| `settings.c`    | –    | ✓    | Settings table, parse and format |
| `storage.c`     | 0, 1 |      | Load at boot, save on core 1 |
| `gfx.c`         | 1    | ✓    | 1-bit framebuffer in SSD1305 layout, text, sprites |
| `widgets.c`     | 1    | ✓    | Keyboard strip, arcs, bars, hold border, 3×5 labels |
| `ui.c`          | 1    | ✓    | Screens drawn from a snapshot |
| `splash.c`      | 1    | ✓    | Start-up animation |
| `display.c`     | 1    |      | SSD1305 init and DMA frame transfer |
| `main.c`        | 0, 1 |      | Start-up, core loops, hand-over |

## Behaviour

- **Timing:** 32-bit millisecond timestamps compared by unsigned
  difference, so wrap-around is harmless.
- **Notes:** each key remembers the notes it started, so octave or
  transpose changes never leave notes hanging. A note shared by two keys
  stops when both are released. Keys held through a mode change are ignored
  until pressed again.
- **Chord mode** is monophonic: a root pressed while a chord sounds is
  queued (latest wins) and starts in the scan the sounding root is
  released. Chord type and octave are read when the chord starts. Hold
  switches chords on press and ignores releases.
- **Octave buttons:** both together toggle chord mode after
  `octave.delay_ms` and enter config mode after 1 s (undoing the toggle);
  the buttons are then ignored until both are released.
- **MIDI** never blocks. Note-offs are sent as note-on with velocity 0 to
  share running status; the status byte is repeated after 1 s of silence.
  Power-up sends All Notes Off and All Sound Off.
- **Config mode** clamps values and leaves when any other key is released.
- **Holds:** while both octave buttons or G' are held towards their 1 s
  action, the snapshot carries the progress and core 1 inverts that share of
  the screen border, clockwise from the top middle.
- **USB flash mode:** G' shows `USB FLASH` after 100 ms and saves the
  settings; at 1 s core 0 waits for any further save and for the full-border
  frame, then calls `reset_usb_boot()`.
- **Saving** happens after config mode or 5 s after the last octave change,
  and only when the text differs from what is stored.
- **Display:** frames go out as one DMA transfer of I2C commands at
  400 kHz (about 12 ms for 128×32), only when the snapshot changes. A
  missing display is retried every 500 ms.
- **Splash:** 34 frames at 20 fps from sprites stored as one 32-bit mask per
  column, starting once the display answers and ending early if the UI
  changes. A host test pins every frame to `assets/splash/reference.gif`.
