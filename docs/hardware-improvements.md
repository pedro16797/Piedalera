# Hardware improvements

Ideas to simplify or improve future builds. The firmware supports the
current hardware first ([`hardware.md`](hardware.md)); nothing here is
required.

## Power the display from USB too

DSP1's VCC is taken from PWR1 before CR1, so the display is dark when only
USB is connected (e.g. while flashing or testing on a desk). Feeding it from
VSYS (after CR1) instead powers it from either source. Check the display
module accepts ~4.7 V first; if it runs at 3.3 V, use 3V3.

## Leave 3V3_EN unconnected

3V3_EN is tied to VSYS, which is harmless but redundant: the Pico already
pulls it up internally. Left unconnected (or wired to a switch to GND), it
could become a soft power switch.

## Fix the Key16–Key19 numbering

The schematic numbers Key16–Key19 in connector order, which is the reverse
of their GPIO (and note) order. Relabelling them, or swapping the four
traces so the connector runs in note order, avoids confusion when wiring
pedals.

## Drop the switch pull-ups (optional)

Every GPIO has an internal pull-up (~50–80 kΩ) that the firmware can enable
with `keys.pull = up`, so R0–R19, Ro1 and Ro2 could go. The external 10 kΩ
resistors are stiffer, though, and a 40-pin flat cable can pick up noise, so
this saves parts at some cost in robustness. If tried, raise
`keys.debounce_ms` first if phantom notes appear.

Avoid pull-down designs: RP2350-E9 makes internal pull-downs unreliable on
the Pico 2.

## Free up GPIOs

All GPIOs except GP28 are in use, which blocks MIDI in and further inputs.
If more are needed, the 20 keys could move to a key matrix (5×4 = 9 pins
instead of 20, needs a diode per switch) or to I2C/SPI shift registers or
port expanders.

## MIDI

- UART0 RX (GP17) is taken by octave up, so there is no MIDI in. Moving the
  octave button would allow MIDI in or MIDI thru with an optocoupler.
- USB MIDI needs no extra hardware; the firmware can add it later.
