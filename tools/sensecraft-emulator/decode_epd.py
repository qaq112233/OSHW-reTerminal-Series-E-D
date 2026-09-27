#!/usr/bin/env python3
"""Decode a SenseCraft .epd file (4-bit indexed Spectra-6 palette) to PNG.

Usage: uv run --with pillow python decode_epd.py <file.epd> [out.png]
Palette mirrors MAP_COLOR6 in upstream src/utils/epd_color_map.h
(commit 8d97d89): 0x00=white, 0x0F=black, 0x06=red, 0x02=green,
0x0D=blue, 0x0B=yellow. Do NOT assume 0=black -- Color6 index order is
not the natural RGB order.
Header layout mirrors downloader.parse_epd_header() / web_tools/hmi_image_get_epd.html.
"""
import struct
import sys
import zlib

from PIL import Image

PALETTE = {
    0x00: (255, 255, 255),  # white  (MAP_COLOR6, epd_color_map.h)
    0x0F: (0, 0, 0),        # black
    0x06: (255, 0, 0),      # red
    0x02: (0, 255, 0),      # green
    0x0D: (0, 0, 255),      # blue
    0x0B: (255, 255, 0),    # yellow
}

def main() -> None:
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else src.rsplit(".", 1)[0] + ".png"
    with open(src, "rb") as f:
        data = f.read()
    if data[:4] != b"EPD0":
        raise SystemExit(f"not an EPD file: {src}")
    bit_depth, mode = data[5], data[6]
    w, h = struct.unpack(">II", data[8:16])
    raw = zlib.decompress(data[16:])
    if len(raw) != w * h // 2:
        raise SystemExit(f"payload size mismatch: {len(raw)} != {w * h // 2}")
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        row = y * w
        for x in range(0, w, 2):
            b = raw[(row + x) // 2]
            px[x, y] = PALETTE.get(b >> 4, (128, 128, 128))
            if x + 1 < w:
                px[x + 1, y] = PALETTE.get(b & 0xF, (128, 128, 128))
    img.save(out)
    print(f"{src} -> {out}  ({w}x{h}, bit_depth={bit_depth}, mode={mode}, payload={len(raw)}B)")

if __name__ == "__main__":
    main()
