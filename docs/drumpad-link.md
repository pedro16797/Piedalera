# Drumpad link (planned)

A wireless link between the pedalboard and a second Pico so the musician on
the pedalboard chooses the chords and a MIDI drumpad decides when and how
they are played: bass notes, arpeggios, strums, timing.

This is an early design note, parked until the keyboard itself works;
nothing is implemented yet. The legacy
experiment is in [`legacy/wifi-tests/`](../legacy/wifi-tests/).

## Roles

```
 MIDI drumpad ──▶ link Pico (W) ── wireless ──▶ pedalboard Pico (W) ──▶ MIDI out ──▶ synth
                  forwards pad hits             chord state + note engine
```

- **Pedalboard** keeps doing what it does now and stays the only MIDI output
  to the synth. It knows the chord, octave and chord type, so it turns each
  incoming pad hit into notes.
- **Link Pico** only reads the drumpad and forwards hits (pad number and
  velocity). It needs no knowledge of chords, so it stays simple.

Keeping all note generation on the pedalboard means one MIDI stream, one
place that tracks sounding notes (no stuck notes across the link), and the
pedalboard still works on its own when the link is off.

## Pad actions

Each pad (by its MIDI note number) maps to an action in the settings file,
for example:

| Action     | What a hit plays |
|------------|------------------|
| `bass`     | Chord root, one or two octaves down |
| `chord`    | The whole chord (strum optional) |
| `arp_up`, `arp_down`, `arp_random` | The next chord tone in that order |
| `fifth`    | Root and fifth (power chord) |

Pad velocity becomes note velocity. Notes last until the pad's note-off, or
for a configurable length if the drumpad only sends note-ons.

## Transport

The legacy test used TCP with a new connection per message, which adds a
handshake to every hit; any design needs a persistent link.

| Option | Latency | Notes |
|--------|---------|-------|
| **BLE MIDI** (recommended) | ~7.5 ms connection interval | Standard protocol: phones, computers and commercial BLE-MIDI pads work for testing or even replace the link Pico. BTstack is in the Pico SDK. |
| Wi-Fi AP + UDP | ~2–5 ms, occasional spikes | Lower latency but a heavier stack; Wi-Fi power saving must be off. |

Both run on the Pico W and Pico 2 W, and neither needs any extra GPIO: the
wireless chip uses internal pins only.

## Firmware impact

- The pedalboard must be a W board (`pico_w` or `pico2_w` build). Plain
  `pico` / `pico2` builds leave the feature out.
- The wireless stack runs on **core 1** next to the display, with its
  interrupts pinned there. Pad hits reach core 0 through a second
  `queue_t`, so note timing on core 0 is unaffected.
- The link Pico firmware becomes a second executable in this repository,
  sharing the MIDI and settings code; CI builds it for the W boards.

## Open questions

- How does the drumpad connect to the link Pico: 5-pin MIDI out, USB-MIDI
  (needs USB host on the Pico), or pads wired to the Pico directly?
- Which actions and pad mappings are needed first?
- When the link is active, should pedal presses still sound on their own, or
  only select the chord and wait for a pad hit?
- Is MIDI clock (tempo-synced arpeggios) wanted, or is every note triggered
  by a hit?
