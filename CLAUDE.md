# Piedalera

C firmware (Pico SDK) for a Raspberry Pi Pico MIDI pedalboard. It supersedes
the MicroPython firmware in `legacy/`.

- `legacy/` is read-only reference; never edit it, and don't document its history.
- Current hardware: `hardware/picorgano/` (KiCad), described in `docs/hardware.md`; defaults target it.
- Keep `docs/playing.md`, `docs/architecture.md` and `docs/roadmap.md` in sync with `src/`.
- Docs are for users first: short, no history, no detail they don't need.
- Core 0 is real time (input, notes, MIDI); core 1 does the display and flash writes. The firmware runs from RAM (`copy_to_ram`); nothing may read flash after boot except `storage.c` on core 1.
- Logic modules stay free of Pico SDK includes so the host tests in `test/` can build them.
- Settings keys and ranges are defined in both `tools/config2uf2.py` and `src/settings.c`; change them together with `docs/configuration.md` and `config/piedalera.ini`.
- Build: `PICO_SDK_PATH=... cmake -B build -DPICO_BOARD=pico && cmake --build build`. CI builds pico, pico_w, pico2 and pico2_w; keep all four working.
- Tests: `cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test`.
- Display preview: `cmake -S tools/preview -B build-preview && cmake --build build-preview --target preview-run`, then open `build-preview/preview.html` (PNGs alongside). Add a scene in `tools/preview/preview.c` for new screens or animations and check the PNGs.
- Pedal diagrams in `docs/images/` are drawn by `tools/preview` (`--target diagrams`); update them when key functions change (CI checks they are current).
- `src/<set>_sprites.h` are generated from `assets/<set>/*.png` by `tools/sprites.py` (needs Pillow); never edit them by hand. The splash test in `test/tests.c` pins every frame to `assets/splash/reference.gif`.
- Keep comments short, in the style of existing files.
