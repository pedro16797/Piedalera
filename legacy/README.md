# Legacy firmware

The original MicroPython code, kept unmodified as the reference for the C
port. It is not built or maintained.

| File | What it is |
|------|------------|
| `main.py` | **The firmware running on the pedalboard** and the reference for the port; settings in `config.txt` on the Pico |
| `Piedalera.py` | An older version: no transpose or hold, octave buttons swapped, different config keys |
| `ssd1305.py` | Display driver both import (128×32 SSD1305, column offset 4), same as on the pedalboard |
| `displayTest.py` | Display test: text and a 32×32 logo |
| `wifi-tests/` | Pico W access point (`testing.py`) and client (`testclient.py`), an experiment for the [drumpad link](../docs/drumpad-link.md); the Wi-Fi password is redacted |

- Behaviour: [`../docs/legacy-behaviour.md`](../docs/legacy-behaviour.md)
- Known issues: [`../docs/legacy-issues.md`](../docs/legacy-issues.md)
