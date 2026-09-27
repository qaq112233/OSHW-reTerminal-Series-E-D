"""Source-level six-color contract (run after PlatformIO fetches Seeed_GFX)."""
from pathlib import Path
import re
import struct
import zlib

project = Path(__file__).resolve().parents[1]
repo = project.parents[2]
gfx = project / ".pio/libdeps/waveshare_photopainter/Seeed_GFX"
palette = (gfx / "TFT_eSPI.h").read_text()
driver = (gfx / "TFT_Drivers/ED2208_Defines.h").read_text()
wave = (repo / "references/ESP32-S3-PhotoPainter/01_Example/xiaozhi-esp32"
        / "components/port_bsp/display_bsp.h").read_text()

# Extract the actual macros rather than duplicating the Seeed_GFX lookup table.
gfx_indices = {}
for name in ("BLACK", "WHITE", "RED", "YELLOW", "BLUE", "GREEN"):
    match = re.search(r"#define\s+TFT_" + name + r"\s+0X([0-9A-F]+)", palette)
    assert match, f"missing Seeed Color6 {name}"
    gfx_indices[name] = int(match.group(1), 16)
lookups = {int(src, 16): int(dst, 16) for src, dst in re.findall(
    r"\(color\)\s*==\s*0x([0-9A-F]+)\s*\?\s*0x([0-9A-F]+)",
    driver.split("#define COLOR_GET(color)", 1)[1].split("#define EPD_PUSH_NEW_COLORS", 1)[0],
    re.IGNORECASE)}
assert len(lookups) == 6

# Waveshare enum has implicit increments, with ColorBlue explicitly set to 5.
match = re.search(r"enum ColorSelection\s*\{(.*?)\}", wave, re.DOTALL)
assert match
wave_indices = {}
value = -1
for item in match.group(1).split(","):
    item = item.strip()
    if not item or item.startswith("//"):
        continue
    name, *explicit = [x.strip() for x in item.split("=")]
    value = int(explicit[0]) if explicit else value + 1
    wave_indices[name.removeprefix("Color").upper()] = value
assert wave_indices == {"BLACK": 0, "WHITE": 1, "YELLOW": 2,
                        "RED": 3, "BLUE": 5, "GREEN": 6}
for color, index in gfx_indices.items():
    assert lookups[index] == wave_indices[color], (color, index, lookups[index])

# The emulator's real downloaded artifacts are ignored by Git; use them if present.
samples = list((repo / "tools/sensecraft-emulator/downloads").glob("*.epd"))
for sample in samples:
    data = sample.read_bytes()
    assert data[:4] == b"EPD0" and (data[5], data[6]) == (4, 2)
    w, h = struct.unpack(">II", data[8:16])
    raw = zlib.decompress(data[16:])
    assert (w, h, len(raw)) == (800, 480, 192000)
    assert {n for byte in raw for n in (byte >> 4, byte & 15)} <= set(lookups)
print(f"Six colors map to Waveshare ED2208 indices; {len(samples)} local EPD samples checked")
