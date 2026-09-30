# Setup guide

Step-by-step instructions for putting Piedalera on a pedalboard. No
programming needed.

> The C firmware hasn't been tried on a pedalboard yet, and no version has
> been released. Until the repository is made public, the Releases page is
> only visible to its members.

## What you need

- The pedalboard, with a Raspberry Pi Pico inside.
- A computer (Windows, macOS or Linux).
- A micro-USB cable. It must be a **data** cable; some cheap cables only
  charge.
- For changing settings: [Python 3](https://www.python.org/downloads/)
  (free). On Windows, tick **"Add python.exe to PATH"** during installation.

## 1. Find out which board you have

Look at the board and the square black chip in its middle.

| Printed on the board | Chip says | Silver metal box on the board? | Your board |
|----------------------|-----------|--------------------------------|------------|
| Pico                 | RP2040    | No                             | `pico`     |
| Pico W               | RP2040    | Yes                            | `pico_w`   |
| Pico 2               | RP2350    | No                             | `pico2`    |
| Pico 2 W             | RP2350    | Yes                            | `pico2_w`  |

Remember the name in the last column; the files you download are named after
it. An "H" at the end of the name (e.g. "Pico H") just means it came with
pins soldered on and doesn't change anything.

## 2. Download the files

Go to the [Releases page](https://github.com/pedro16797/Piedalera/releases)
and open the newest release. Download:

- `piedalera-<board>.uf2`: the firmware.
- `piedalera-<board>-config.uf2`: the default settings.
- `piedalera.ini` and `config2uf2.py`: only if you want to change settings.

For example, for a Pico 2 you need `piedalera-pico2.uf2` and
`piedalera-pico2-config.uf2`. You don't need the `.elf` files.

## 3. Install the firmware

1. Unplug the Pico from everything.
2. Press and **hold** the small white **BOOTSEL** button on the board.
3. While holding it, plug the USB cable into the Pico and then into the
   computer.
4. Let go of the button. A new drive appears, like a USB stick, called
   **RPI-RP2** (Pico, Pico W) or **RP2350** (Pico 2, Pico 2 W).
5. Drag `piedalera-<board>.uf2` onto that drive.
6. The drive disappears by itself after a few seconds and the pedalboard
   starts. This is normal. If your computer says the drive wasn't ejected
   properly, ignore it.

Updating to a newer version later works the same way, and your settings are
kept. Once the pedalboard runs this firmware you don't need the BOOTSEL
button any more: with the USB cable plugged into the computer, enter config
mode (hold both octave buttons for a second) and hold **G'** until the line
round the screen closes. The screen shows `USB FLASH` and the drive
appears.

## 4. Install the settings

The first time, do the same as in step 3 but drag
`piedalera-<board>-config.uf2` instead (or use config mode and G' as
described there). This loads the default settings.

## 5. Change settings (optional)

1. Put `piedalera.ini` and `config2uf2.py` in the same folder.
2. Open `piedalera.ini` with a plain text editor (Notepad on Windows,
   TextEdit on macOS). Change only the numbers or words after the `=` signs.
   Lines starting with `#` are notes and are ignored.
3. Save the file.
4. Run `config2uf2.py`:
   - **Windows:** double-click it.
   - **macOS:** right-click it, choose **Open With → Python Launcher**.
   - **Linux:** open a terminal in the folder and type `python3 config2uf2.py`.

   A window lists the files it created: one `piedalera-<board>-config.uf2`
   per board type, next to `piedalera.ini`. If you made a typo, it says which
   line is wrong instead; fix it and run it again.
5. Install your board's config file as in step 4.

The settings you are most likely to change:

| Setting              | What it does |
|----------------------|--------------|
| `display.width`, `display.height` | Size of your screen in pixels, e.g. 128 and 32 |
| `display.brightness` | Screen brightness, 0 (dim) to 255 (bright) |
| `midi.channel`       | MIDI channel, 1 to 16. Must match your synth |
| `midi.velocity`      | How hard notes are played, 1 to 127 |
| `octave.current`     | Starting octave |
| `keys.debounce_ms`   | Raise it (e.g. to 10) if pedals sometimes play twice |

The full list is in [`configuration.md`](configuration.md).

To go back to the default settings, install the original
`piedalera-<board>-config.uf2` from the release again.

## 6. Wiring (new builds only)

Skip this if your pedalboard is already built. New builds use the
**Picórgano** board; its design files are in `hardware/picorgano/` and the
schematic is `Picorgano-schematic.pdf` in the same folder.

1. Plug the Pico into the two long sockets. Its pin 1 (next to the USB
   socket, labelled on the underside of the Pico) goes into pin 1 of the
   left socket, MCU3, which has a square pad.
2. Connect the parts to the board's headers. On every header, pin 1 is the
   one with the square pad.

| Header | Connect | Pin 1 | Pin 2 | Pin 3 | Pin 4 |
|--------|---------|-------|-------|-------|-------|
| PWR1   | 5 V power supply | − | + | | |
| OCT1   | Octave up button | either wire | other wire | | |
| OCT2   | Octave down button | either wire | other wire | | |
| RST1   | Reset button (optional) | either wire | other wire | | |
| DSP1   | Screen | VCC | GND | SCL | SDA |
| MIDI1  | MIDI OUT socket (5-pin DIN) | socket pin 4 | socket pin 2 | socket pin 5 | |
| EXP1   | Expression pedal (coming soon) | | | | |

Screen modules print their pin names next to the pins, often in a different
order (e.g. GND, VCC, SCL, SDA). Match the names, not the positions.

Instead of OCT1 and OCT2, both octave buttons can share the 3-pin OCTS
header: pin 1 to one wire of each button, pin 2 to the other wire of the
up button, pin 3 to the other wire of the down button.

**Pedals** connect through the 40-pin flat cable (KEY1). Every even pin is
ground; each pedal switch goes between its pin below and any even pin.

| Pedal | Pin | Pedal | Pin | Pedal | Pin | Pedal | Pin |
|-------|-----|-------|-----|-------|-----|-------|-----|
| C     | 39  | E     | 31  | A♭    | 23  | D'    | 11  |
| D♭    | 37  | F     | 29  | A     | 21  | E'♭   | 09  |
| D     | 35  | G♭    | 27  | B♭    | 19  | E'    | 01  |
| E♭    | 33  | G     | 25  | B     | 17  | F'    | 03  |
|       |     |       |     | C'    | 15  | G'♭   | 05  |
|       |     |       |     | D'♭   | 13  | G'    | 07  |

The last four pedals (E' to G') really do go on pins 01, 03, 05, 07 in that
order; this is how the existing pedalboard is wired.

## 7. Play

See [playing the pedalboard](playing.md) for what each pedal and button does
in normal, chord and config mode.

## Troubleshooting

| Problem | Try this |
|---------|----------|
| No drive appears in step 3 | Use another USB cable (it may be charge-only). Make sure you hold BOOTSEL *before* plugging in. |
| Nothing plays on the synth | Check the MIDI cable goes from the pedalboard's MIDI OUT to the synth's MIDI IN, and that `midi.channel` matches the synth. |
| Screen is dark | On the current board the screen only gets power from the power supply, not from USB. Connect the power supply. |
| Screen is cut off, shifted or garbled | Check `display.width`, `display.height` and `display.col_offset`. |
| A pedal sometimes plays twice, or notes play by themselves | Raise `keys.debounce_ms`. See [`hardware.md`](hardware.md#switch-wiring) if it continues. |
| Pedals do the opposite (sound when released) | Change `keys.active_low` from `true` to `false`, or the other way round. |
