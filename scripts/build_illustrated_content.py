#!/usr/bin/env python3
"""Convert the illustrated vocabulary material into device wordbook content.

Reads ``content/illustrated-vocabulary/unique_words.json`` (893 words with
Korean meanings and RGBA source art) and writes a deployable wordbook folder:

* ``words.jsonl`` -- one ``{"w","m","a"}`` line per word (``m`` is the Korean
  meaning; ``e``/``s`` are left for a later content pass).
* ``w_<slug>.wmr`` -- pre-rendered card art in the device's raw format: an
  8-byte header (``WMR1`` magic, uint16le width, uint16le height) followed by
  RGB565 little-endian pixels already alpha-blended over the card color. The
  firmware copies this straight into its pixel buffer, so the device needs no
  PNG/zlib decoder (the old PNG path needed a 46KiB contiguous heap block
  that heap fragmentation after TLS could not provide) and no per-art RAM
  beyond the shared pixel buffer.

The card blend color must match ``kCard`` (0x0D1B2D) in ``src/word_ui.cpp``.
Art obeys the limits in ``include/app_config.hpp`` (max 120x88, 64KiB) and
the firmware's ``validArtName`` (ASCII alnum plus ``_-.``, 40 chars max, no
``/``), so files stay flat in the folder.

The originals under ``content/`` are never modified; converted art goes to a
separate output directory (default ``build/illustrated-device``).

Usage (from the repository root):
    python scripts\\build_illustrated_content.py [--out DIR] [--limit N]
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from time import monotonic

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from wmr import MAX_ART_BYTES, MAX_ART_HEIGHT, MAX_ART_WIDTH, render_wmr  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
VOCAB = ROOT / "content" / "illustrated-vocabulary" / "unique_words.json"

MAX_ART_NAME = 40  # includes the .wmr suffix


def slugify(word: str, used: set[str]) -> str:
    slug = word.replace(" ", "_")
    if not slug.replace("_", "").replace("-", "").isalnum():
        raise SystemExit(f"word needs manual art naming: {word!r}")
    stem = slug[: MAX_ART_NAME - 4]
    candidate = stem
    suffix = 1
    while f"w_{candidate}.wmr" in used:
        suffix += 1
        tail = f"_{suffix}"
        candidate = stem[: MAX_ART_NAME - 4 - len(tail)] + tail
    used.add(f"w_{candidate}.wmr")
    return candidate


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", default=str(ROOT / "build" / "illustrated-device"),
                        help="output directory (created if missing)")
    parser.add_argument("--limit", type=int, default=0,
                        help="convert only the first N words (verification bundles)")
    args = parser.parse_args()

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)
    for stale in out_dir.glob("w_*"):
        stale.unlink()
    (out_dir / "words.jsonl").unlink(missing_ok=True)

    data = json.loads(VOCAB.read_text(encoding="utf-8"))
    if args.limit:
        data = data[: args.limit]

    started = monotonic()
    used_names: set[str] = set()
    lines: list[str] = []
    total = 0
    largest = (0, "")
    for item in data:
        slug = slugify(item["word"], used_names)
        art_name = f"w_{slug}.wmr"
        src = ROOT / "content" / "illustrated-vocabulary" / item["image"]
        with Image.open(src) as im:
            im = im.convert("RGBA")
            im.thumbnail((MAX_ART_WIDTH, MAX_ART_HEIGHT), Image.LANCZOS)
            if im.width > MAX_ART_WIDTH or im.height > MAX_ART_HEIGHT:
                raise SystemExit(f"{item['word']}: art still {im.size}")
            payload = render_wmr(im)
        if len(payload) > MAX_ART_BYTES:
            raise SystemExit(f"{item['word']}: art {len(payload)} bytes exceeds limit")
        (out_dir / art_name).write_bytes(payload)
        size = len(payload)
        total += size
        largest = max(largest, (size, art_name))
        entry = {"w": item["word"], "m": item["ko"], "a": art_name}
        line = json.dumps(entry, ensure_ascii=False, separators=(",", ":"))
        if len(line.encode("utf-8")) > 512:
            raise SystemExit(f"{item['word']}: manifest line exceeds 512 bytes")
        lines.append(line)

    (out_dir / "words.jsonl").write_text("\n".join(lines) + "\n", encoding="utf-8")
    manifest_bytes = (out_dir / "words.jsonl").stat().st_size
    print(f"words: {len(lines)}")
    print(f"manifest: {manifest_bytes:,} bytes")
    print(f"art total: {total:,} bytes, avg {total // max(len(lines), 1):,}, "
          f"largest {largest[0]:,} ({largest[1]}), {monotonic() - started:.0f}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
