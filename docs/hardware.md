# Hardware

The current build is the **Picórgano** board (rev 0.2.0, ePick!): a carrier
PCB for a Raspberry Pi Pico. KiCad sources are in
[`hardware/picorgano/`](../hardware/picorgano/), with a PDF export of the
schematic ([`Picorgano-schematic.pdf`](../hardware/picorgano/Picorgano-schematic.pdf)).
The firmware defaults target this board. Ideas for future revisions are in
[`hardware-improvements.md`](hardware-improvements.md).

## Components

| Part            | Details |
|-----------------|---------|
| MCU             | Raspberry Pi Pico in two 1×20 sockets (MCU3 left, MCU2 right) |
| Note keys       | 20 foot switches on a 40-pin flat cable (KEY1, Samtec EHF-120) |
| Octave buttons  | 2 buttons on OCT1 / OCT2, or both on the 3-pin OCTS header |
| Display         | 128×32 SSD1305 OLED on I2C1, address `0x3C` (DSP1) |
| MIDI out        | 3-pin header to a 5-pin DIN socket (MIDI1) |
| Expression      | 3-pin header (EXP1), for the planned DIY pedal |
| Reset           | 2-pin header to the Pico's RUN pin (RST1), for a reset button |
| Power           | 2-pin header (PWR1), through a Schottky diode (CR1) into VSYS |
| Target synth    | A Roland GM2 module (exact model **TBC**) |

## Headers

All headers are JST-EH (2.5 mm). Pin 1 is GND unless stated.

| Header | Pin 1 | Pin 2 | Pin 3 | Pin 4 |
|--------|-------|-------|-------|-------|
| OCT1   | GND   | Octave up (GP17) | | |
| OCT2   | GND   | Octave down (GP18) | | |
| OCTS   | GND   | Octave up (GP17) | Octave down (GP18) | |
| RST1   | GND   | RUN (Pico pin 30) | | |
| EXP1   | AGND  | GP28 / ADC2 | ADC_VREF (Pico pin 35) | |
| MIDI1  | 33 Ω to 3V3 (DIN pin 4) | GND (DIN pin 2) | 10 Ω to GP16 (DIN pin 5) | |
| DSP1   | VCC (from PWR1, before CR1) | GND | SCL (GP27) | SDA (GP26) |
| PWR1   | GND   | Supply in | | |

KEY1: odd pins carry the keys, every even pin is GND.

| KEY1 pins   | Schematic label | GPIO     | Note (legacy code) |
|-------------|-----------------|----------|--------------------|
| 39, 37 … 09 | Key0 … Key15    | GP0–GP15 | C … E'♭            |
| 07          | Key16           | GP22     | G'                 |
| 05          | Key17           | GP21     | G'♭                |
| 03          | Key18           | GP20     | F'                 |
| 01          | Key19           | GP19     | E'                 |

The schematic numbers the top four keys along the connector, while the
code's note order follows the GPIO numbers, so Key16–Key19 map to notes in
reverse. The pedal harness compensates: every pedal plays the right note on
the current build, so the table above is the working mapping.

## Switch wiring

Each key and octave input has a 10 kΩ pull-up to 3V3 (R0–R19, Ro1, Ro2) and
its switch shorts it to GND. Pressed reads low. Defaults:
`keys.active_low = true` and `keys.pull = none`, since the external
resistors already pull up.

## Power

- PWR1 feeds VSYS through CR1 (MBRA210 Schottky), so USB and the external
  supply can be connected at the same time.
- The display's VCC comes from PWR1 **before** the diode, so the display
  stays dark when the board is powered from USB alone.
- The Pico's 3V3_EN (pin 37) is tied to VSYS, keeping the regulator always
  on (it is also pulled up inside the Pico).
- 3V3 has 10 µF + 100 nF decoupling. The MIDI source and TX lines each go
  through a ferrite bead.

## Display

From `legacy/ssd1305.py` (identical to the copy on the pedalboard): 128×32
panel, I2C address `0x3C`, 2.2 kΩ pull-ups to 3V3. The controller has 132
columns of RAM and the panel starts at column 4, so every frame is written
to columns 4–131. Init sequence, in order: display off, clock divide
`0x80`, segment remap on, multiplex `height - 1`, display offset 0, master
config `0x8E`, area colour `0x05`, horizontal addressing, start line 0,
scroll off, COM scan decrement, COM pins `0x12`, contrast `0xFF`,
precharge `0xD2`, VCOMH `0x34`, normal display, charge pump `0x14`,
display on.

The legacy firmware ran the bus at 200 kHz; the C firmware uses 400 kHz
(I2C fast mode, supported by the SSD1305).

## Pin map

| GPIO | Pico pin | Function      | Note              |
|------|----------|---------------|-------------------|
| GP0  | 1        | Key 0         | C                 |
| GP1  | 2        | Key 1         | D♭                |
| GP2  | 4        | Key 2         | D                 |
| GP3  | 5        | Key 3         | E♭                |
| GP4  | 6        | Key 4         | E                 |
| GP5  | 7        | Key 5         | F                 |
| GP6  | 9        | Key 6         | G♭                |
| GP7  | 10       | Key 7         | G                 |
| GP8  | 11       | Key 8         | A♭                |
| GP9  | 12       | Key 9         | A                 |
| GP10 | 14       | Key 10        | B♭                |
| GP11 | 15       | Key 11        | B                 |
| GP12 | 16       | Key 12        | C'                |
| GP13 | 17       | Key 13        | D'♭               |
| GP14 | 19       | Key 14        | D'                |
| GP15 | 20       | Key 15        | E'♭               |
| GP16 | 21       | UART0 TX      | MIDI out          |
| GP17 | 22       | Octave up     |                   |
| GP18 | 24       | Octave down   |                   |
| GP19 | 25       | Key 16        | E'                |
| GP20 | 26       | Key 17        | F'                |
| GP21 | 27       | Key 18        | G'♭               |
| GP22 | 29       | Key 19        | G'                |
| GP26 | 31       | I2C1 SDA      | Display           |
| GP27 | 32       | I2C1 SCL      | Display, 400 kHz  |
| GP28 | 34       | Expression    | ADC2              |

GP23–GP25 are used internally by the Pico board (SMPS mode, VBUS sense, LED).

## Supported boards

| Board     | Chip   | Flash | `PICO_BOARD` |
|-----------|--------|-------|--------------|
| Pico      | RP2040 | 2 MB  | `pico`       |
| Pico W    | RP2040 | 2 MB  | `pico_w`     |
| Pico 2    | RP2350 | 4 MB  | `pico2`      |
| Pico 2 W  | RP2350 | 4 MB  | `pico2_w`    |

All four share the pin map above.

- **W boards:** GP23, GP24, GP25 and GP29 drive the wireless chip instead.
  None of them is used here. The onboard LED is behind the wireless chip, not
  on GP25. A plain `pico` build runs on a Pico W too, but the `pico_w` build
  is needed for any wireless feature.
- **Pico 2:** RP2350-E9 makes the internal pull-downs unreliable. The
  Picórgano board uses pull-ups, so it isn't affected; keep `keys.pull` at
  `none` or `up` on a Pico 2.

## Free resources

- Every usable GPIO is taken; GP28 / ADC2 goes to the expression header.
- UART0 RX would be GP17 (octave up), so there is no MIDI in.
- UART0 is MIDI and the SDK's default stdio pins GP0/GP1 are keys, so the
  firmware uses no stdio.

## Expression pedal (planned)

The pedal will be DIY; its design isn't settled yet. EXP1 provides AGND,
ADC_VREF and the ADC input, so a linear potentiometer (10–50 kΩ) with its
ends on pins 1 and 3 and its wiper on pin 2 is enough. Powering it from
ADC_VREF makes the reading ratiometric: supply noise and the small voltage
drop on ADC_VREF cancel out.

- Pedal travel doesn't need to use the full range; the firmware will
  calibrate its minimum and maximum.
- A series resistor (~1 kΩ) and a capacitor to AGND (~100 nF) on the wiper
  are recommended to filter noise and protect the pin when a jack is
  plugged in live.
