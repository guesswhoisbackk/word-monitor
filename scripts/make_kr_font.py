#!/usr/bin/env python3
"""Generate the LVGL Korean font subset for the word card UI.

Extracts every Hangul syllable used by the current wordbook meanings plus the
ASCII range from Malgun Gothic and compiles them with lv_font_conv (npx) into
``src/font_kr_16.c``. Meanings served later that introduce syllables outside
this subset render as blanks, so re-run this script after adding new Korean
text and rebuild the firmware.

Usage (from the repository root, with Node.js available):
    python scripts\\make_kr_font.py
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VOCAB = ROOT / "content" / "illustrated-vocabulary" / "unique_words.json"
SOURCE_FONT = Path(r"C:\Windows\Fonts\malgun.ttf")
OUTPUT = ROOT / "src" / "font_kr_16.c"

FONT_SIZE = 16
BPP = 4


def collect_ranges() -> list[tuple[int, int]]:
    """ASCII plus every Hangul syllable present in the current meanings."""
    data = json.loads(VOCAB.read_text(encoding="utf-8"))
    needed = set(range(0x20, 0x7F))
    for item in data:
        for ch in item["ko"]:
            if 0xAC00 <= ord(ch) <= 0xD7A3:
                needed.add(ord(ch))
            elif not (0x20 <= ord(ch) <= 0x7E):
                raise SystemExit(
                    f"meaning of {item['word']!r} uses non-covered character "
                    f"{ch!r} (U+{ord(ch):04X}); extend the font generator"
                )
    points = sorted(needed)
    ranges: list[tuple[int, int]] = []
    for point in points:
        if ranges and point == ranges[-1][1] + 1:
            ranges[-1] = (ranges[-1][0], point)
        else:
            ranges.append((point, point))
    return ranges


def main() -> int:
    if not SOURCE_FONT.exists():
        print(f"source font missing: {SOURCE_FONT}", file=sys.stderr)
        return 1
    ranges = collect_ranges()
    syllables = sum(hi - lo + 1 for lo, hi in ranges if lo >= 0xAC00)
    range_args = [f"0x{lo:X}-0x{hi:X}" if lo != hi else f"0x{lo:X}"
                  for lo, hi in ranges]
    print(f"{syllables} hangul syllables, {len(range_args)} ranges")

    npx = shutil.which("npx.cmd") or shutil.which("npx")
    if not npx:
        print("npx not found; Node.js is required", file=sys.stderr)
        return 1
    cmd = [npx, "--yes", "lv_font_conv",
           "--font", str(SOURCE_FONT),
           "--size", str(FONT_SIZE),
           "--bpp", str(BPP),
           "--format", "lvgl",
           "--no-compress", "--no-prefilter", "--no-kerning",
           "--lv-include", "lvgl.h",
           "--lv-font-name", "font_kr_16"]
    for spec in range_args:
        cmd += ["-r", spec]
    cmd += ["-o", str(OUTPUT)]
    subprocess.run(cmd, check=True, cwd=ROOT)
    size = OUTPUT.stat().st_size
    print(f"wrote {OUTPUT} ({size:,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
