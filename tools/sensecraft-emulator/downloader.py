"""Image download and format detection.

Mirrors app_download.cpp::__fetch_psram_image() (download + magic detection)
and web_tools/hmi_image_get_epd.html (EPD header layout).
"""

import os
import struct
import sys

import requests

import protocol as P
from manifest import DOWNLOADS_DIR

PNG_MAGIC = b"\x89PNG\r\n\x1a\n"


def detect_format(head: bytes) -> str:
    """Mirrors app_download.cpp::__image_format_from_buffer()."""
    if len(head) >= 2 and head[0:2] == b"BM":
        return "bmp"
    if len(head) >= 8 and head[0:8] == PNG_MAGIC:
        return "png"
    if len(head) >= 4 and head[0:4] == P.EPD_MAGIC:
        return "epd"
    return "unknown"


def safe_filename(image_id: str) -> str:
    """Keep only filename-safe characters (no path traversal)."""
    keep = "-_."
    out = "".join(c if (c.isalnum() or c in keep) else "_" for c in image_id)
    out = out.strip("._") or "image"
    return out[:180]


def parse_epd_header(data: bytes) -> dict | None:
    """Parse the 16-byte EPD header (web_tools/hmi_image_get_epd.html).

    layout: "EPD0" | u8 reserved | u8 bit_depth | u8 mode | u8 reserved |
            u32 width (BE) | u32 height (BE) | zlib payload from offset 16
    """
    if len(data) < 16 or data[0:4] != P.EPD_MAGIC:
        return None
    bit_depth = data[5]
    mode = data[6]
    width, height = struct.unpack(">II", data[8:16])
    return {
        "magic": "EPD0",
        "bit_depth": bit_depth,
        "mode": mode,
        "mode_name": P.EPD_MODE_NAMES.get(mode, f"mode {mode}"),
        "width": width,
        "height": height,
        "payload_bytes": max(0, len(data) - 16),
    }


def png_dimensions(data: bytes) -> tuple[int, int] | None:
    if len(data) < 24 or data[0:8] != PNG_MAGIC:
        return None
    width, height = struct.unpack(">II", data[16:24])
    return width, height


def bmp_dimensions(data: bytes) -> tuple[int, int] | None:
    if len(data) < 26 or data[0:2] != b"BM":
        return None
    width, height = struct.unpack("<ii", data[18:26])
    return width, abs(height)


def describe_image(path: str, fmt: str, size: int) -> str:
    """Best-effort metadata line for the downloaded file (no Pillow required)."""
    with open(path, "rb") as f:
        head = f.read(32)
    dims = None
    if fmt == "epd":
        with open(path, "rb") as f:
            hdr = parse_epd_header(f.read(16))
        if hdr:
            extra = (f" epd[{hdr['width']}x{hdr['height']} "
                     f"{hdr['mode_name']} {hdr['bit_depth']}bit]")
            return f"[DOWNLOAD] {path} ({size} bytes, {fmt}){extra}"
        return f"[DOWNLOAD] {path} ({size} bytes, {fmt}, header < 16 bytes)"
    if fmt == "png":
        dims = png_dimensions(head + b"\0" * 32)
    elif fmt == "bmp":
        with open(path, "rb") as f:
            dims = bmp_dimensions(f.read(32))
    if dims:
        expected = dims == (P.BOARD_WIDTH, P.BOARD_HEIGHT)
        flag = "matches" if expected else "DIFFERS from"
        return (f"[DOWNLOAD] {path} ({size} bytes, {fmt}, {dims[0]}x{dims[1]} "
                f"{flag} board {P.BOARD_WIDTH}x{P.BOARD_HEIGHT})")
    return f"[DOWNLOAD] {path} ({size} bytes, {fmt})"


def download_image(url: str, image_id: str, *, cloud_token: str, insecure: bool = False,
                   debug: bool = False, timeout: float = 60.0,
                   on_progress=None, session: requests.Session | None = None) -> tuple[str, str, int]:
    """Stream-download one image; detect format from magic bytes.

    on_progress(percent: int) is called at 20/40/60/80/100 percent of bytes.
    Returns (saved_path, format, size).
    """
    http = session or requests.Session()
    headers = {}
    if cloud_token:
        headers["authorization"] = cloud_token
    if debug:
        print(f"[DOWNLOAD] GET {url}")

    with http.get(url, headers=headers, timeout=timeout, verify=not insecure, stream=True) as resp:
        if resp.status_code in (401, 403):
            raise PermissionError(f"HTTP {resp.status_code}: cloud token rejected for image")
        if resp.status_code != 200:
            raise RuntimeError(f"HTTP {resp.status_code} fetching image {url}")

        total = int(resp.headers.get("Content-Length", "0") or 0)
        fmt = "unknown"
        os.makedirs(DOWNLOADS_DIR, exist_ok=True)
        tmp_path = os.path.join(DOWNLOADS_DIR, safe_filename(image_id) + ".part")
        received = 0
        next_report = 20
        with open(tmp_path, "wb") as f:
            for chunk in resp.iter_content(chunk_size=64 * 1024):
                if not chunk:
                    continue
                f.write(chunk)
                received += len(chunk)
                if fmt == "unknown":
                    fmt = detect_format(chunk[:16])
                if total and on_progress is not None:
                    pct = received * 100 // total
                    while pct >= next_report and next_report <= 80:
                        on_progress(next_report)
                        next_report += 20

        if fmt == "unknown":
            with open(tmp_path, "rb") as f:
                fmt = detect_format(f.read(16))
        if fmt == "unknown":
            print(f"[DOWNLOAD] WARNING: unknown image format for {image_id}; "
                  f"Content-Type={resp.headers.get('Content-Type')}", file=sys.stderr)
            fmt = "bin"

        ext = {"bmp": ".bmp", "png": ".png", "epd": ".epd"}.get(fmt, ".bin")
        final_path = tmp_path[: -len(".part")] + ext
        os.replace(tmp_path, final_path)

    if total and on_progress is not None:
        on_progress(100)
    return final_path, fmt, received
