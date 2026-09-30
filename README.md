# Piedalera

Firmware for a Raspberry Pi Pico–based MIDI pedalboard: 20 foot keys, two
octave buttons, a small OLED display and a 5-pin DIN MIDI output.

The original firmware is a MicroPython script (kept in [`legacy/`](legacy/)).
This repository reworks it in C on top of the
[Pico SDK](https://github.com/raspberrypi/pico-sdk), with the goals of:

- Lowering the per-scan compute cost and latency.
- Using both cores properly (real-time input/MIDI on core 0, display on core 1).
- Fixing the bugs found in the legacy script and hardening its logic.
- Adding new features: display animations, an expression pedal input, and more.

**Just want to install it?** Follow the [setup guide](docs/setup-guide.md),
then see [playing the pedalboard](docs/playing.md) for what each pedal does.

## Repository layout

```
.
├── CMakeLists.txt       Pico SDK build
├── .github/workflows/   CI: builds the UF2 files, runs the host tests
├── src/                 C firmware (see docs/architecture.md)
├── assets/              Sprites: splash/ (start-up animation), icons/
├── test/                Host tests for the hardware-independent modules
├── config/
│   └── piedalera.ini    Default settings
├── tools/
│   ├── config2uf2.py    Turns a settings file into a flashable UF2
│   ├── sprites.py       Converts assets/<set>/ PNGs into src/<set>_sprites.h
│   └── preview/         Renders the display on the computer, and the pedal
│                        diagrams in docs/images/
├── hardware/
│   └── picorgano/       KiCad project of the Picórgano board, schematic PDF
├── legacy/              Original MicroPython firmware (reference only)
└── docs/
    ├── setup-guide.md       Step-by-step install for non-programmers
    ├── playing.md           What each pedal does in each mode (images/)
    ├── hardware.md          Board, headers, pin map, power
    ├── hardware-improvements.md  Ideas for simpler future builds
    ├── configuration.md     Settings file and how to flash it
    ├── legacy-behaviour.md  What the MicroPython firmware does
    ├── legacy-issues.md     Bugs and weak spots found in it
    ├── architecture.md      C firmware design
    ├── drumpad-link.md      Planned wireless link to a MIDI drumpad
    └── roadmap.md           Milestones and open questions
```

## Building

Requires the Pico SDK and an `arm-none-eabi` toolchain.

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -B build -DPICO_BOARD=pico   # or pico_w, pico2, pico2_w
cmake --build build
```

Flash `build/piedalera.uf2` by holding BOOTSEL while plugging the Pico in and
copying the file to the drive that appears.

The host tests only need a C compiler:

```sh
cmake -S test -B build-test
cmake --build build-test
ctest --test-dir build-test
```

## Previewing the display

`tools/preview` runs the firmware's own drawing code on the computer and
renders a list of scenes, so screens and animations can be iterated on
without flashing the Pico:

```sh
cmake -S tools/preview -B build-preview
cmake --build build-preview --target preview-run
```

Open `build-preview/preview.html` in a browser: it plays the scenes with the
OLED's pixel grid, zoom and panel colour. A PNG of each scene is written next
to it. Scenes are defined at the top of `tools/preview/preview.c`; re-run the
second command and reload the page after each change. CI also uploads the
preview as a `display-preview` artifact.

## CI

Every push is built by GitHub Actions for all four boards and runs the host
tests; download `piedalera-<board>.uf2` and `piedalera-<board>-config.uf2`
from the run's artifacts. Pushing a `v*` tag also publishes them as a GitHub
release.

## Settings

Display size, MIDI channel, velocity, octave range, switch polarity and more
are set in [`config/piedalera.ini`](config/piedalera.ini) and flashed as a
separate UF2, no rebuild needed. See
[`docs/configuration.md`](docs/configuration.md).

## Documentation

Start with [`docs/legacy-behaviour.md`](docs/legacy-behaviour.md) for how the
pedalboard is played, [`docs/architecture.md`](docs/architecture.md) for how
the firmware works and [`docs/roadmap.md`](docs/roadmap.md) for where it is
going.
