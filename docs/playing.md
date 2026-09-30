# Playing the pedalboard

At power-up the screen plays a short start-up animation (it can be turned
off with `display.splash`). Pedals play straight away; using any pedal or
button skips the rest of it.

Every screen shows the 20 pedals as a keyboard along the top, with the
pedals you hold drawn hollow. While you hold a pedal or buttons for a
second to do something, a line runs clockwise round the edge of the screen
from the top middle; the action happens when it closes. The diagrams below
are drawn by the firmware's own screen code
(`cmake --build build-preview --target diagrams`), so they look like the
display.

## Normal mode

Every pedal plays its own note. At octave 3 (the default), C is MIDI note 48.
The screen shows the octave below the keyboard, in the same place as in
chord mode.

![Normal mode](images/keys-normal.png)

- **▲ / ▼** (one octave button): one octave up or down; hold to repeat.
- **▲ + ▼ briefly** (the note stack): chord mode on or off.
- **▲ + ▼ for a second** (the gear): config mode.

## Chord mode

The lower twelve pedals (under the arc) play a chord rooted on their note;
the upper eight pick the chord type and switch hold:

| Label | Pedal | Chord |
|-------|-------|-------|
| M7 | C'  | Major 7th |
| M  | D'♭ | Major |
| m7 | D'  | Minor 7th |
| m  | E'♭ | Minor |
| d7 | E'  | Diminished 7th |
| h7 | F'  | Half-diminished 7th |
| 7  | G'♭ | Seventh |
| H  | G'  | Hold on/off |

![Chord mode](images/keys-chord.png)

The screen marks the sounding chord's notes, the selected chord type and
hold with a dotted fill, and shows the chord's name, the octave and `HOLD`
when it is on.

- **One chord at a time.** Pressing another root while a chord sounds
  prepares it: it starts the moment you release the current one, with the
  chord type and octave selected at that moment. Releasing a prepared root
  before its turn cancels it.
- **Hold** keeps each chord sounding after you release it, until the next
  root replaces it. Pressing G' again turns hold off and stops the chord.
- The octave buttons work as in normal mode.

## Config mode

The screen shows each setting's icon under the pair of pedals that turns it
down (left) and up (right):

![Config mode](images/keys-config.png)

| Icon | Pedals | Setting |
|------|--------|---------|
| Bulb | C / D | Screen brightness (`Shine`) |
| p f | E / F | Velocity |
| Note | G / A | Sound variation (bank) |
| ♭− ♯+ | B / C' | Transpose (`Transp.`) |
| Stopwatch | D' / E' | Debounce time |
| Flash | G' | Hold 1 s: USB flash mode |

- While you change a value, the screen shows its icon, the value, the
  setting's name and a bar.
  Holding the pedal keeps changing it.
- Values stop at their limits: brightness 0–16 steps, velocity 1–127, bank
  0–127, transpose −12 to +12 semitones, debounce 0–50 ms.
- **Debounce** is how long a pedal must stay still before a press or release
  counts. Raise it if pedals sometimes play twice; lower it for a quicker
  response.
- Changing the bank sends the new sound variation to the synth straight
  away.
- Pressing and releasing any other pedal, or tapping G', leaves config mode
  and saves the settings, so they survive a power cycle. The octave is saved
  too, a few seconds after you last change it.
- **Holding G' for a second** restarts the Pico in USB flash mode, so new
  firmware can be copied onto it without opening the pedalboard and pressing
  BOOTSEL. `USB FLASH` shows once you have held it for a moment, and the
  Pico restarts as soon as the line round the screen closes. Letting go
  before then just leaves config mode. See the [setup guide](setup-guide.md#3-install-the-firmware).
- The octave buttons do nothing in config mode.
