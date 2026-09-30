# Configuration

Settings are plain text stored in their own flash sector, so they can be
changed without rebuilding the firmware. Edit
[`config/piedalera.ini`](../config/piedalera.ini) and convert it:

```sh
python3 tools/config2uf2.py --board pico config/piedalera.ini piedalera-config.uf2
```

`--board` is `pico`, `pico_w`, `pico2` or `pico2_w`; without arguments it
converts the `piedalera.ini` next to it for every board. It rejects unknown
keys and values out of range. Install the UF2 like firmware (see the
[setup guide](setup-guide.md)); it only replaces the settings, and firmware
updates keep them. Changes made on the pedalboard (config mode, octave) are
saved to the same place.

| Key                  | Default | Range              | Meaning |
|----------------------|---------|--------------------|---------|
| `display.width`      | 128     | 64–128             | Display width in pixels |
| `display.height`     | 32      | 32–64, multiple of 8 | Display height in pixels |
| `display.col_offset` | 4       | 0–4                | First visible RAM column; width + offset ≤ 132 |
| `display.brightness` | 15      | 0–255              | Display contrast |
| `display.splash`     | true    | true / false       | Play the start-up animation |
| `midi.channel`       | 1       | 1–16               | MIDI channel |
| `midi.velocity`      | 95      | 1–127              | Note velocity |
| `midi.transpose`     | 0       | −12–12             | Semitones added to every note |
| `midi.bank_lsb`      | 12      | 0–127              | Sound variation: bank select CC0 = 121, CC32 = value |
| `octave.min`         | 0       | 0–8                | Lowest octave |
| `octave.max`         | 7       | 0–8                | Highest octave |
| `octave.current`     | 3       | `min`–`max`        | Octave at power-up; saved as you play |
| `octave.delay_ms`    | 100     | 0–900              | Hold time before an octave button acts |
| `octave.repeat_ms`   | 500     | 50–5000            | Auto-repeat interval while held |
| `keys.active_low`    | true    | true / false       | Pressed reads low |
| `keys.pull`          | none    | up / down / none   | Internal pull resistor on the inputs |
| `keys.debounce_ms`   | 5       | 0–50               | Time an input must be stable to count |

Octave `n` puts C on MIDI note `12 * (n + 1) + transpose`. Missing keys take
the default.

## Flash format

`key = value` lines after a `# piedalera-config v1` header, NUL-terminated
and padded with `0xFF` to 4096 bytes, in the second to last flash sector
(`0x101FE000` on 2 MB boards, `0x103FE000` on 4 MB ones). The last sector is
left free for the RP2350-E10 bootrom workaround, and RP2350 config UF2s use
the absolute UF2 family. An erased or unrecognised sector means defaults.
