# Setup guide

How to install Piedalera on the pedalboard. No programming needed. You need
a computer and a micro-USB **data** cable (some cheap cables only charge).

## 1. Find your board

Look at the Pico board inside the pedalboard:

| Printed on the board | Silver metal box on it? | Your board |
|----------------------|-------------------------|------------|
| Pico                 | No                      | `pico`     |
| Pico W               | Yes                     | `pico_w`   |
| Pico 2               | No                      | `pico2`    |
| Pico 2 W             | Yes                     | `pico2_w`  |

## 2. Download

Get the files for your board from the newest release on the
[Releases page](https://github.com/pedro16797/Piedalera/releases), or from
the `piedalera-<board>` download of a build on the
[Actions page](https://github.com/pedro16797/Piedalera/actions). There are
three:

| File | What it is | When to install it |
|------|------------|--------------------|
| `piedalera-<board>.uf2` | The firmware | Always |
| `piedalera-<board>-config.uf2` | Default settings | First install only. It resets your settings |
| `piedalera-<board>.elf` | For developers | Never; ignore it |

## 3a. First install

For a pedalboard still running the old firmware, or a new Pico.

1. Hold the white **BOOTSEL** button on the Pico and plug it into the
   computer. Let go: a drive called **RPI-RP2** or **RP2350** appears.
2. Drag `piedalera-<board>.uf2` onto it. The drive disappears and the
   pedalboard starts.
3. Do step 1 again and drag `piedalera-<board>-config.uf2` onto the drive.

## 3b. Updating

For a pedalboard that already runs Piedalera, to install a newer version or
a modified build. No need to open it.

1. Plug the pedalboard into the computer, with its power supply connected
   too so the screen lights up.
2. Hold both octave buttons for a second to enter config mode, then hold
   **G'** until the line round the screen closes. The screen shows
   `USB FLASH` and the drive appears.
3. Drag `piedalera-<board>.uf2` onto it. Your settings are kept.

## 4. Change settings (optional)

Download `piedalera.ini` and `config2uf2.py` from the release into one
folder, edit the numbers after the `=` signs in `piedalera.ini` with a plain
text editor, then run `config2uf2.py` (it needs
[Python 3](https://www.python.org/downloads/); double-click it on Windows,
"Open With → Python Launcher" on macOS, `python3 config2uf2.py` on Linux).
It creates a `piedalera-<board>-config.uf2` for each board; install yours
like the firmware. What each setting does: [configuration](configuration.md).

## Troubleshooting

| Problem | Try this |
|---------|----------|
| No drive appears | Try another USB cable; hold BOOTSEL *before* plugging in. |
| The synth plays nothing | The MIDI cable must go from the pedalboard's MIDI OUT to the synth's MIDI IN, and `midi.channel` must match the synth. |
| The screen is dark | It only gets power from the power supply, not from USB. |
| A pedal plays twice | Raise `keys.debounce_ms`, or use D'/E' in config mode. |

Next: [what each pedal does](playing.md).
