# Setup guide

How to install Piedalera on the pedalboard. No programming needed.

You need a computer and a micro-USB **data** cable (some cheap cables only
charge).

## 1. Find your board

Look at the Pico board inside the pedalboard:

| Printed on the board | Silver metal box on it? | Your board |
|----------------------|-------------------------|------------|
| Pico                 | No                      | `pico`     |
| Pico W               | Yes                     | `pico_w`   |
| Pico 2               | No                      | `pico2`    |
| Pico 2 W             | Yes                     | `pico2_w`  |

## 2. Download

From the newest release on the
[Releases page](https://github.com/pedro16797/Piedalera/releases), download
`piedalera-<board>.uf2` and `piedalera-<board>-config.uf2` for your board.

## 3. Install

1. Hold the white **BOOTSEL** button on the Pico and plug it into the
   computer. Let go: a drive called **RPI-RP2** or **RP2350** appears.
2. Drag `piedalera-<board>.uf2` onto it. The drive disappears and the
   pedalboard starts.
3. The first time only, repeat with `piedalera-<board>-config.uf2` to load
   the default settings.

To update later, do the same with the new `piedalera-<board>.uf2`; your
settings are kept. Instead of pressing BOOTSEL you can enter config mode
(hold both octave buttons for a second) and hold **G'** until the line round
the screen closes.

## 4. Change settings (optional)

Download `piedalera.ini` and `config2uf2.py` from the same release into one
folder, edit the numbers after the `=` signs in `piedalera.ini` with a plain
text editor, then run `config2uf2.py` (it needs
[Python 3](https://www.python.org/downloads/); double-click it on Windows,
"Open With → Python Launcher" on macOS, `python3 config2uf2.py` on Linux).
It creates a `piedalera-<board>-config.uf2` for each board; install yours
as in step 3. What each setting does: [configuration](configuration.md).

## Troubleshooting

| Problem | Try this |
|---------|----------|
| No drive appears | Try another USB cable; hold BOOTSEL *before* plugging in. |
| The synth plays nothing | The MIDI cable must go from the pedalboard's MIDI OUT to the synth's MIDI IN, and `midi.channel` must match the synth. |
| The screen is dark | It only gets power from the power supply, not from USB. |
| A pedal plays twice | Raise `keys.debounce_ms`, or use D'/E' in config mode. |

Next: [what each pedal does](playing.md).
