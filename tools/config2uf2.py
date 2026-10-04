#!/usr/bin/env python3
"""Convert a Piedalera .ini settings file into a UF2 that only writes the
settings flash sector, leaving the firmware untouched.

Run without arguments (e.g. by double-clicking) to convert the
piedalera.ini next to this script into one UF2 per board."""

import argparse
import os
import struct
import sys

HEADER = "# piedalera-config v1\n"

FLASH_BASE = 0x10000000
SECTOR_SIZE = 4096

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY_ID = 0x00002000
RP2040_FAMILY_ID = 0xE48BFF56
ABSOLUTE_FAMILY_ID = 0xE48BFF57
UF2_PAYLOAD = 256

# board: (UF2 family, flash size). RP2350 uses the absolute family, which
# needs no RP2350-E10 workaround block.
BOARDS = {
    "pico":    (RP2040_FAMILY_ID, 2 * 1024 * 1024),
    "pico_w":  (RP2040_FAMILY_ID, 2 * 1024 * 1024),
    "pico2":   (ABSOLUTE_FAMILY_ID, 4 * 1024 * 1024),
    "pico2_w": (ABSOLUTE_FAMILY_ID, 4 * 1024 * 1024),
}

# key: (min, max) for integers, a tuple of strings for enums, bool for flags
SCHEMA = {
    "display.width":      (64, 128),
    "display.height":     (32, 64),
    "display.col_offset": (0, 4),
    "display.brightness": (0, 255),
    "display.splash":     bool,
    "display.dim_s":      (0, 3600),
    "display.off_s":      (0, 3600),
    "midi.channel":       (1, 16),
    "midi.velocity":      (1, 127),
    "midi.transpose":     (-12, 12),
    "midi.sound":         (0, 32),
    "octave.min":         (0, 8),
    "octave.max":         (0, 8),
    "octave.current":     (0, 8),
    "octave.delay_ms":    (0, 900),
    "octave.repeat_ms":   (50, 5000),
    "keys.active_low":    bool,
    "keys.pull":          ("up", "down", "none"),
    "keys.debounce_ms":   (0, 50),
    "keys.alternative":   ("chord", "scale"),
    "power.sleep_s":      (0, 7200),
    "power.battery":      ("none", "alkaline", "nimh", "li-ion"),
    "power.cells":        (1, 4),
    "power.drop_mv":      (0, 1000),
    "expression.enabled": bool,
    "expression.cc":      (0, 119),
    "expression.invert":  bool,
    "expression.min":     (0, 4095),
    "expression.max":     (0, 4095),
}

# "sound = msb lsb program name" lines, any number up to SOUNDS_MAX
SOUND_KEY = "sound"
SOUNDS_MAX = 32
SOUND_NAME_MAX = 15


def parse(path):
    values, sounds = {}, []
    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            if "=" not in line:
                raise ValueError(f"{path}:{lineno}: expected 'key = value'")
            key, value = (s.strip() for s in line.split("=", 1))
            if key == SOUND_KEY:
                sounds.append(check_sound(value, f"{path}:{lineno}"))
                continue
            if key not in SCHEMA:
                raise ValueError(f"{path}:{lineno}: unknown key '{key}'")
            values[key] = check(key, value, f"{path}:{lineno}")
    if len(sounds) > SOUNDS_MAX:
        raise ValueError(f"{path}: at most {SOUNDS_MAX} sounds")
    return values, sounds


def check_sound(value, where):
    parts = value.split(None, 3)
    usage = f"{where}: expected 'sound = msb lsb program name'"
    if len(parts) < 4:
        raise ValueError(usage)
    try:
        msb, lsb, program = (int(p) for p in parts[:3])
    except ValueError:
        raise ValueError(usage) from None
    name = parts[3]
    if not (0 <= msb <= 127 and 0 <= lsb <= 127):
        raise ValueError(f"{where}: bank numbers must be in 0..127")
    if not 1 <= program <= 128:
        raise ValueError(f"{where}: program must be in 1..128")
    if len(name) > SOUND_NAME_MAX or not all(" " <= c <= "~" for c in name):
        raise ValueError(f"{where}: name must be at most {SOUND_NAME_MAX} plain ASCII characters")
    return f"{msb} {lsb} {program} {name}"


def check(key, value, where):
    rule = SCHEMA[key]
    if rule is bool:
        if value not in ("true", "false"):
            raise ValueError(f"{where}: {key} must be true or false")
    elif isinstance(rule[0], str):
        if value not in rule:
            raise ValueError(f"{where}: {key} must be one of {', '.join(rule)}")
    else:
        try:
            number = int(value)
        except ValueError:
            raise ValueError(f"{where}: {key} must be an integer") from None
        if not rule[0] <= number <= rule[1]:
            raise ValueError(f"{where}: {key} must be in {rule[0]}..{rule[1]}")
        value = str(number)
    return value


def validate(values, sounds):
    if int(values.get("display.height", 32)) % 8:
        raise ValueError("display.height must be a multiple of 8")
    width = int(values.get("display.width", 128))
    offset = int(values.get("display.col_offset", 4))
    if width + offset > 132:
        raise ValueError("display.width + display.col_offset must be at most 132")
    lo = int(values.get("octave.min", 0))
    hi = int(values.get("octave.max", 7))
    cur = int(values.get("octave.current", 3))
    if not lo <= cur <= hi:
        raise ValueError("octave.current must be between octave.min and octave.max")
    if sounds and int(values.get("midi.sound", 0)) > len(sounds):
        raise ValueError("midi.sound is past the end of the sound list")


def to_uf2(data, address, family):
    blocks = [data[i:i + UF2_PAYLOAD] for i in range(0, len(data), UF2_PAYLOAD)]
    out = bytearray()
    for n, chunk in enumerate(blocks):
        out += struct.pack("<8I", UF2_MAGIC_START0, UF2_MAGIC_START1,
                           UF2_FLAG_FAMILY_ID, address + n * UF2_PAYLOAD,
                           UF2_PAYLOAD, n, len(blocks), family)
        out += chunk.ljust(476, b"\0")
        out += struct.pack("<I", UF2_MAGIC_END)
    return bytes(out)


def build(values, sounds, board):
    text = HEADER + "".join(f"{k} = {v}\n" for k, v in values.items())
    text += "".join(f"{SOUND_KEY} = {s}\n" for s in sounds)
    blob = text.encode("ascii") + b"\0"
    if len(blob) > SECTOR_SIZE:
        raise ValueError("settings don't fit in one flash sector")
    blob = blob.ljust(SECTOR_SIZE, b"\xff")

    # Second to last sector: the last one is kept free for RP2350-E10
    family, flash_size = BOARDS[board]
    return to_uf2(blob, FLASH_BASE + flash_size - 2 * SECTOR_SIZE, family)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("ini", nargs="?", default=os.path.join(here, "piedalera.ini"),
                    help="settings file (default: piedalera.ini next to this script)")
    ap.add_argument("uf2", nargs="?",
                    help="output file (default: piedalera-<board>-config.uf2 next to the ini)")
    ap.add_argument("--board", choices=BOARDS,
                    help="target board (default: all boards)")
    args = ap.parse_args()
    if args.uf2 and not args.board:
        ap.error("--board is required when naming the output file")

    try:
        values, sounds = parse(args.ini)
        validate(values, sounds)
        for board in [args.board] if args.board else BOARDS:
            out = args.uf2 or os.path.join(os.path.dirname(os.path.abspath(args.ini)),
                                           f"piedalera-{board}-config.uf2")
            with open(out, "wb") as f:
                f.write(build(values, sounds, board))
            print(f"{out}: {len(values)} settings and {len(sounds)} sounds for {board}")
    except (OSError, ValueError) as e:
        print(f"error: {e}")
        return 1
    return 0


if __name__ == "__main__":
    status = main()
    # Keep the window open when started by double-click
    if len(sys.argv) == 1:
        input("Press Enter to close.")
    sys.exit(status)
