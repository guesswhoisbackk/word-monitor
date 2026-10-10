"""WMR1 card-art format shared by the wordbook content scripts.

A WMR1 file is an 8-byte header (``WMR1`` magic, uint16le width, uint16le
height) followed by RGB565 little-endian pixels, row-major, already
alpha-blended over the card color. The firmware (``Wordbook::loadArt`` in
``src/wordbook.cpp``) reads the pixels straight into its shared buffer, so
serving art this way needs no image decoder or large runtime allocation on
the device. Limits mirror ``include/app_config.hpp``.
"""

from __future__ import annotations

import struct

from PIL import Image

MAGIC = b"WMR1"
MAX_ART_BYTES = 64 * 1024
MAX_ART_WIDTH = 120
MAX_ART_HEIGHT = 88

# Card background the art is blended onto. Must match kCard (0x0D1B2D) in
# src/word_ui.cpp; changing either side requires regenerating the art.
CARD_RGB = (0x0D, 0x1B, 0x2D)


def render_wmr(image: Image.Image) -> bytes:
    """Blend RGBA art over the card color and pack it as a WMR1 payload."""
    background = Image.new("RGB", image.size, CARD_RGB)
    background.paste(image, (0, 0), image)
    width, height = image.size
    out = bytearray(MAGIC + struct.pack("<HH", width, height))
    pixels = bytearray(width * height * 2)
    offset = 0
    for r, g, b in background.getdata():
        value = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        pixels[offset] = value & 0xFF
        pixels[offset + 1] = value >> 8
        offset += 2
    out += pixels
    return bytes(out)
