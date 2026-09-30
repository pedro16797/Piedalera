# Legacy behaviour

What `legacy/main.py` does: the MicroPython firmware running on the
pedalboard today, and the functional reference for the C port. The older
`legacy/Piedalera.py` differs in places (noted below). Deviations that look
accidental are listed in [`legacy-issues.md`](legacy-issues.md) rather than
here.

## Threads

- **Main thread (core 0):** fixed-period loop (`PERIOD`, 10 ms) that scans the
  keys and octave buttons and writes MIDI to UART0.
- **Display thread (core 1):** redraws the OLED every 100 ms.

At start-up the script launches and immediately exits a dummy thread to clear
a display thread left over from a previous run (a MicroPython soft reset
quirk). If the main loop stops with an exception, the display thread stops
too.

## Settings

Stored as whitespace-separated `Name: value` pairs in `config.txt` and read
by position. A missing or short file (< 20 tokens) is replaced with the
defaults.

| Key             | Default  | Pedalboard | Meaning |
|-----------------|----------|------------|---------|
| `Shine`         | 0        | 15         | Display contrast (0–255) |
| `Channel`       | 0        | 12         | Roland sound variation: sent as bank select LSB (CC32), **not** the MIDI channel |
| `Velocity`      | 127      | 95         | Note-on velocity |
| `Transpose`     | 0        | 0          | Semitones added to every note (−12–12) |
| `Min_Oct`       | 0        | 0          | Lowest octave offset |
| `Max_Oct`       | 7        | 7          | Highest octave offset |
| `Oct_Offset`    | 3        | 3          | Octave offset at power-up |
| `Oct_Delay`     | 100000   | 100000     | µs a button must be held before it acts |
| `Oct_Iteration` | 500000   | 500000     | µs between auto-repeats while held |
| `Period`        | 10000    | 10000      | µs per main loop iteration |

`Piedalera.py` has no `Transpose` and expects 18 tokens.

## Normal mode

Each key plays one note, always on MIDI channel 1:

```
note = key_index + transpose + 12 * (octave + 1)
```

With the default octave 3 and no transpose, key 0 is MIDI 48 (C3) and key 19
is MIDI 67 (G4). Note-on uses `Velocity`; note-off is `0x80 note 0`. The
octave in effect when a key is pressed is remembered, so the matching
note-off is correct even if the octave changes while the key is held.

Key changes are detected against the last reading of each pin, so a key
triggers once per press.

## Octave buttons

`OCT_UP` is GP17 and `OCT_DOWN` is GP18 (swapped from `Piedalera.py`).

- **Up / Down alone:** after being held for `Oct_Delay`, shifts the octave by
  one within `[Min_Oct, Max_Oct]`, then auto-repeats every `Oct_Iteration`.
- **Both held > `Oct_Delay`:** all notes off, chord mode toggles.
- **Both held > 1 s:** the toggle above is undone and config mode starts.

The octave is not saved; every power-up starts at `Oct_Offset`.

## Chord mode

Keys 0–11 (C to B) play a chord rooted on that key. Keys 12–19 stop playing
notes and become chord selectors, plus a hold toggle:

| Key | Note | Function       | Intervals (semitones) |
|-----|------|----------------|-----------------------|
| 13  | D'♭  | Major          | 0 4 7                 |
| 15  | E'♭  | Minor          | 0 3 7                 |
| 12  | C'   | Major 7th      | 0 4 7 11              |
| 14  | D'   | Minor 7th      | 0 3 7 10              |
| 18  | G'♭  | Seventh (7)    | 0 4 7 10              |
| 16  | E'   | Diminished 7th | 0 3 6 9               |
| 17  | F'   | Half-dim 7th   | 0 3 6 10              |
| 19  | G'   | **Hold** on/off |                      |

Selection persists until changed (Major by default) but not across power
cycles. `Piedalera.py` had an eighth chord (Min-Maj 7th) on F' and no hold.

- **Hold off:** pressing a root plays its chord; pressing another root while
  one is held layers both chords. Releasing any root sends note-off for
  **every** sounding note.
- **Hold on:** pressing a root first stops whatever sounds, then plays the
  new chord. Releasing does nothing, so the chord sustains until the next
  root is pressed. Turning hold off stops all notes.

Hold is not reset when chord mode is left and is not saved.

## Config mode

Entered by holding both octave buttons (see above); the screen shows
`CONFIG`. The function keys below act while held, and config mode stays open
between them. Pressing any other key waits for its release, saves the
settings to `config.txt` and leaves config mode.

| Key | Note | Action                          | First step, then repeat |
|-----|------|---------------------------------|-------------------------|
| 0   | C    | Brightness −16 (wraps to 255)   | 500 ms, 200 ms |
| 2   | D    | Brightness +16 (wraps to 0)     | 500 ms, 200 ms |
| 4   | E    | Velocity −1 (wraps to 127)      | 200 ms, 50 ms  |
| 5   | F    | Velocity +1 (wraps to 0)        | 200 ms, 50 ms  |
| 7   | G    | "Channel" −1 (wraps 0–15), sends CC0 = 121 and CC32 = value | 500 ms, 200 ms |
| 9   | A    | "Channel" +1 (wraps 0–15), same messages | 500 ms, 200 ms |
| 11  | B    | Transpose −1 (wraps to +12)     | 500 ms, 200 ms |
| 12  | C'   | Transpose +1 (wraps to −12)     | 200 ms, 100 ms |

CC0 = 121 selects the GM2 melodic bank on the Roland; CC32 picks the variation.
No program change follows and the values are not sent at power-up.

`Piedalera.py` used different keys, left config mode after one action, and
had a program-change browser and a commented-out instrument preset table
(`instList`).

## Display

Three text rows at y = 0, 12 and 24, redrawn every 100 ms with the current
brightness:

1. `Octava: <n>`
2. Selected chord name, only in chord mode.
3. `HOLD: ON` / `HOLD: OFF` in chord mode, otherwise the last config message
   (`CONFIG`, `Brightness n`, `Velocity n`, `Ch n`, `Transpose n`), cleared
   when config mode ends.
