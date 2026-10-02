# Hardware improvements

Ideas for future board revisions; the firmware targets the current one.

- **Display on USB power:** feed DSP1's VCC from VSYS (after the diode) so
  the screen works on USB alone. Check the module accepts ~4.7 V, or use 3V3.
- **Key16–Key19 labels:** the schematic numbers them in connector order,
  the reverse of their notes. Relabel them or swap the four traces.
- **Fewer resistors:** `keys.pull = up` could replace the 10 kΩ pull-ups,
  at some cost in noise immunity on the long flat cable. Avoid pull-down
  designs (RP2350-E9).
- **Expression input:** EXP1's wiper goes straight to GP28. A ~1 kΩ series
  resistor and a low-capacitance ESD diode to GNDA would absorb the static
  a plug brings in, and ~1 MΩ to GNDA would hold an empty jack at 0. No
  capacitor on the board side: it would hide an empty jack from the
  firmware's plug probe.
- **3V3_EN:** tied to VSYS but already pulled up in the Pico; left free it
  could take a power switch.
