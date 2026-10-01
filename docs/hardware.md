# Hardware

The **Picórgano** board carries the Pico. KiCad sources and a schematic PDF
are in [`hardware/picorgano/`](../hardware/picorgano/); ideas for later
revisions are in [`hardware-improvements.md`](hardware-improvements.md).

## Wiring

Plug the Pico into the two long sockets with its pin 1 (next to the USB
socket) in pin 1 of MCU3, the one with the square pad. On every header, pin
1 has the square pad. All headers are JST-EH.

| Header | Connect | Pin 1 | Pin 2 | Pin 3 | Pin 4 |
|--------|---------|-------|-------|-------|-------|
| PWR1 | 5 V supply | − | + | | |
| OCT1 | Octave up button | GND | GP17 | | |
| OCT2 | Octave down button | GND | GP18 | | |
| OCTS | Both octave buttons (instead of OCT1/OCT2) | GND | GP17 | GP18 | |
| RST1 | Reset button (optional) | GND | RUN | | |
| DSP1 | SSD1305 128×32 OLED, I2C `0x3C` | VCC | GND | SCL (GP27) | SDA (GP26) |
| MIDI1 | 5-pin DIN socket | DIN pin 4 | DIN pin 2 | DIN pin 5 | |
| EXP1 | Expression pedal | AGND | GP28 / ADC2 | ADC_VREF | |

Screen modules often order their pins differently; match the names.

**Pedals** use the 40-pin flat cable (KEY1): each switch goes between its
pin and any even pin (ground).

| Pedal | Pin | GPIO | Pedal | Pin | GPIO | Pedal | Pin | GPIO |
|-------|-----|------|-------|-----|------|-------|-----|------|
| C   | 39 | GP0 | G    | 25 | GP7  | D'   | 11 | GP14 |
| D♭  | 37 | GP1 | A♭   | 23 | GP8  | E'♭  | 09 | GP15 |
| D   | 35 | GP2 | A    | 21 | GP9  | E'   | 01 | GP19 |
| E♭  | 33 | GP3 | B♭   | 19 | GP10 | F'   | 03 | GP20 |
| E   | 31 | GP4 | B    | 17 | GP11 | G'♭  | 05 | GP21 |
| F   | 29 | GP5 | C'   | 15 | GP12 | G'   | 07 | GP22 |
| G♭  | 27 | GP6 | D'♭  | 13 | GP13 |      |    |      |

E' to G' really are on pins 01, 03, 05, 07 in that order.

## Notes

- Every input has a 10 kΩ pull-up and its switch shorts it to ground, so
  the defaults are `keys.active_low = true` and `keys.pull = none`.
- PWR1 feeds VSYS through a Schottky diode, so USB and the supply can be
  connected together. The display is powered from PWR1 before the diode, so
  it stays dark on USB power alone.
- **Battery:** one 18650 Li-ion cell through a TC4056 charger module with
  protection (DW01A): cell on B+/B−, OUT+/OUT− to PWR1, charged through the
  module's USB socket, also while playing. The DW01A cuts the load at about
  2.4 V; the firmware warns long before. A switch between OUT+ and PWR1
  stops the ~1 mA drain in storage and still lets it charge. While charging,
  the battery page shows the charging voltage. The display gets the cell
  voltage directly, so check it still lights near 3.3 V.
- MIDI goes out on UART0 TX (GP16). There is no MIDI in: GP17, UART0's RX,
  is the octave up button.
- All four boards (Pico, Pico W, Pico 2, Pico 2 W) share this pin map. On a
  Pico 2, avoid `keys.pull = down` (RP2350-E9).
- For the expression pedal, a 10–50 kΩ linear potentiometer across pins 1
  and 3 with its wiper on pin 2 will do; ~1 kΩ in series and ~100 nF to AGND
  on the wiper filter noise. It is off until turned on in config mode (see
  [playing](playing.md)), as a pin left open floats.
