# Configuration

Settings live as plain text in a 4 KB flash sector of the Pico, so they can
be changed without rebuilding the firmware.

## Changing settings

1. Edit [`config/piedalera.ini`](../config/piedalera.ini).
2. Convert it:
   ```sh
   python3 tools/config2uf2.py --board pico config/piedalera.ini piedalera-config.uf2
   ```
   `--board` is `pico`, `pico_w`, `pico2` or `pico2_w`. Without arguments it
   converts the `piedalera.ini` next to it for every board. The tool checks
   every key and range, so typos are caught here and not on the pedalboard.
3. Hold BOOTSEL, plug the Pico in and copy `piedalera-config.uf2` to the
   `RPI-RP2` (or `RP2350`) drive.

The config UF2 only touches the settings sector; the firmware stays as it is.
Flashing a new firmware UF2 doesn't touch the settings either. CI attaches a
config UF2 built from the default `piedalera.ini` for each board to every
build.

Settings changed on the pedalboard itself (config mode, octave) are written
back to the same sector, so flashing a config UF2 overrides them.

## Keys

Missing keys take the default. Keys out of range are rejected by the tool and
reset to the default by the firmware.

| Key                  | Default | Range              | Meaning |
|----------------------|---------|--------------------|---------|
| `display.width`      | 128     | 64–128             | Display width in pixels |
| `display.height`     | 32      | 32–64, multiple of 8 | Display height in pixels |
| `display.col_offset` | 4       | 0–4                | First visible RAM column; width + offset ≤ 132 |
| `display.brightness` | 15      | 0–255              | Display contrast |
| `midi.channel`       | 1       | 1–16               | MIDI channel for all messages |
| `midi.velocity`      | 95      | 1–127              | Note-on velocity |
| `midi.transpose`     | 0       | −12–12             | Semitones added to every note |
| `midi.bank_lsb`      | 12      | 0–127              | Sound variation on the Roland: bank select CC0 = 121, CC32 = value, sent when changed in config mode |
| `octave.min`         | 0       | 0–8                | Lowest octave |
| `octave.max`         | 7       | 0–8                | Highest octave |
| `octave.current`     | 3       | `min`–`max`        | Octave at power-up; updated as you play |
| `octave.delay_ms`    | 100     | 0–900              | Hold time before an octave button acts; under the 1 s config hold so both buttons can still toggle chord mode |
| `octave.repeat_ms`   | 500     | 50–5000            | Auto-repeat interval while held |
| `keys.active_low`    | true    | true / false       | Pressed reads low (`true`) or high (`false`) |
| `keys.pull`          | none    | up / down / none   | Internal pull resistor on key and octave inputs |
| `keys.debounce_ms`   | 5       | 0–50               | Time an input must be stable to count; also adjustable in config mode |

Octave `n` puts key 0 (C) on MIDI note `12 * (n + 1) + transpose`, so the
default 3 is C3 (MIDI 48). Notes outside MIDI 0–127 are not sent.

The defaults match the pedalboard's current `config.txt` (`Shine: 15`,
`Channel: 12`, `Velocity: 95`, `Transpose: 0`, same octave timings). The
legacy `Channel` is really a bank select value (see
[`legacy-behaviour.md`](legacy-behaviour.md#config-mode)), so it maps to
`midi.bank_lsb`; notes have always gone out on MIDI channel 1.

## Flash format

```
# piedalera-config v1
display.width = 128
...
<NUL> then 0xFF padding to 4096 bytes
```

The first line identifies the format; an erased or unrecognised sector means
all defaults. The sector is the second to last one,
`PICO_FLASH_SIZE_BYTES - 8192`: `0x101FE000` on a Pico / Pico W (2 MB),
`0x103FE000` on a Pico 2 / Pico 2 W (4 MB). The last sector is left free
because the RP2350-E10 bootrom workaround uses it.

RP2350 config UF2s use the absolute UF2 family, so they don't need the
workaround block firmware UF2s carry.
