"""Add or update a wordbook entry from an SVG illustration.

Usage (from the project root):
  # word with artwork: renders apple.svg -> docs/wordbook-sample/apple.png
  .venv/Scripts/python.exe scripts/wordbook_art.py docs/apple.svg \
      --word apple --meaning "a round red fruit" --example "She ate an apple."

  # text-only word
  .venv/Scripts/python.exe scripts/wordbook_art.py \
      --word resilient --meaning "bounces back quickly" \
      --example "The little shop survived it all."

The art PNG keeps its alpha channel; the device blends it onto the card
background when decoding. The words.jsonl entry is upserted by word.
"""

import argparse
import json
import pathlib
import sys

from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).parent))
from svg_to_header import render_svg  # noqa: E402


def upsert(wordbook_dir: pathlib.Path, entry: dict) -> None:
    manifest = wordbook_dir / "words.jsonl"
    lines: list[str] = []
    if manifest.exists():
        lines = [
            line
            for line in manifest.read_text(encoding="utf-8").splitlines()
            if line.strip()
            and json.loads(line).get("w") != entry["w"]
        ]
    lines.append(json.dumps(entry, ensure_ascii=False, separators=(",", ":")))
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"words.jsonl: {len(lines)} entries (upserted '{entry['w']}')")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", nargs="?", type=pathlib.Path, help=".svg input")
    parser.add_argument("--word", required=True)
    parser.add_argument("--meaning", default="")
    parser.add_argument("--example", default="")
    parser.add_argument("--dir", type=pathlib.Path,
                        default=pathlib.Path("docs/wordbook-sample"))
    parser.add_argument("--max-w", type=int, default=80)
    parser.add_argument("--max-h", type=int, default=88)
    args = parser.parse_args()

    entry = {"w": args.word, "m": args.meaning, "e": args.example}

    if args.source is not None:
        png_path = args.source.resolve().with_name(
            args.source.stem + "_render.png")
        if args.source.suffix.lower() == ".svg":
            render_svg(args.source.resolve(), png_path)
        image = Image.open(png_path).convert("RGBA")
        image = image.crop(image.getchannel("A").getbbox())
        scale = min(args.max_w / image.width, args.max_h / image.height, 1.0)
        image = image.resize(
            (round(image.width * scale), round(image.height * scale)),
            Image.LANCZOS,
        )
        args.dir.mkdir(parents=True, exist_ok=True)
        art_name = f"{args.word}.png"
        image.save(args.dir / art_name, optimize=True)
        entry["a"] = art_name
        print(f"art: {args.dir / art_name} ({image.width}x{image.height})")

    upsert(args.dir, entry)


if __name__ == "__main__":
    main()
