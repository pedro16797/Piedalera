# Piedalera

Firmware for a Raspberry Pi Pico MIDI pedalboard: 20 foot keys, two octave
buttons, a small OLED display and a 5-pin DIN MIDI output. Written in C on
the [Pico SDK](https://github.com/raspberrypi/pico-sdk).

- **Install it:** [setup guide](docs/setup-guide.md)
- **Play it:** [what each pedal does](docs/playing.md)
- **Change settings:** [configuration](docs/configuration.md)

The original MicroPython firmware is kept in [`legacy/`](legacy/) for
reference; this firmware supersedes it.

## Building

Needs the Pico SDK and an `arm-none-eabi` toolchain.

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -B build -DPICO_BOARD=pico   # or pico_w, pico2, pico2_w
cmake --build build
```

Host tests (any C compiler):

```sh
cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test
```

Display preview, rendered with the firmware's own drawing code; open
`build-preview/preview.html` afterwards:

```sh
cmake -S tools/preview -B build-preview
cmake --build build-preview --target preview-run
```

CI builds the firmware and default settings for all four boards on every
push to `master`; tagging `v*` publishes a release.

## Layout

| Path | Contents |
|------|----------|
| `src/` | Firmware ([architecture](docs/architecture.md)) |
| `test/` | Host tests |
| `tools/` | `config2uf2.py` (settings to UF2), `sprites.py` (PNG assets to C), `preview/` |
| `assets/` | Sprites for the splash, config icons and diagrams |
| `config/` | Default settings |
| `hardware/picorgano/` | KiCad board ([hardware](docs/hardware.md)) |
| `legacy/` | Original MicroPython firmware |

Plans are in the [roadmap](docs/roadmap.md).
