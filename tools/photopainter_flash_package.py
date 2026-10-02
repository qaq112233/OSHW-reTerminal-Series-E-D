#!/usr/bin/env python3
"""Package freshly built PhotoPainter V1 binaries; never touches a serial port."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
from datetime import datetime
from zoneinfo import ZoneInfo
import zipfile
import zlib

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "examples/official/SenseCraft_HMI"
FLASH_BYTES = 16 * 1024 * 1024
PARTITION_OFFSET = 0x8000  # pinned Arduino platformio-build.py
TARGETS = {
    "waveshare_photopainter_bringup": "photopainter-v1-bringup-full.bin",
    "waveshare_photopainter": "photopainter-v1-sensecraft-full.bin",
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def partition_table(data: bytes) -> list[dict]:
    """Read ESP-IDF 32-byte partition records, including their MD5 guard."""
    entries = []
    for pos in range(0, len(data), 32):
        row = data[pos:pos + 32]
        if len(row) != 32:
            raise ValueError("Truncated partition table")
        magic = struct.unpack_from("<H", row)[0]
        if magic == 0xEBEB:
            if row[16:] != hashlib.md5(data[:pos], usedforsecurity=False).digest():
                raise ValueError("Partition table MD5 mismatch")
            if any(byte != 0xFF for byte in data[pos + 32:]):
                raise ValueError("Unexpected bytes after partition table MD5")
            break
        if magic != 0x50AA:
            raise ValueError("Missing partition table MD5 or invalid partition record")
        _, kind, subtype, offset, size, name, flags = struct.unpack("<HBBII16sI", row)
        if flags or size <= 0 or offset < PARTITION_OFFSET + 0x1000:
            raise ValueError("Encrypted, empty, or unsafe partition is unsupported")
        if offset % 0x1000 or size % 0x1000 or offset + size > FLASH_BYTES:
            raise ValueError("Partition alignment/16MB boundary violation")
        entries.append(dict(name=name.rstrip(b"\x00").decode("ascii"), type=kind,
                            subtype=subtype, offset=offset, size=size))
    else:
        raise ValueError("No partition table MD5 record")
    ordered = sorted(entries, key=lambda item: item["offset"])
    if len({item["name"] for item in entries}) != len(entries):
        raise ValueError("Duplicate partition name")
    for left, right in zip(ordered, ordered[1:]):
        if left["offset"] + left["size"] > right["offset"]:
            raise ValueError("Overlapping partitions")
    by_name = {item["name"]: item for item in entries}
    expected = {"app0": (0, 0x10, 0x90000), "otadata": (1, 0, 0x8D000),
                "spiffs": (1, 0x82, 0xA90000)}
    for name, values in expected.items():
        entry = by_name.get(name)
        if entry is None or (entry["type"], entry["subtype"], entry["offset"]) != values:
            raise ValueError(f"Unexpected PhotoPainter partition: {name}")
    return entries


def validate_image(data: bytes, name: str) -> None:
    if len(data) < 24 or data[0] != 0xE9:
        raise ValueError(f"Invalid ESP image: {name}")
    if struct.unpack_from("<H", data, 12)[0] != 9:
        raise ValueError(f"Not an ESP32-S3 image: {name}")
    if data[3] >> 4 != 4:  # esptool flash-size code 4 is 16 MB
        raise ValueError(f"Image is not configured for 16MB flash: {name}")


def validate_ota_init(data: bytes, size: int) -> None:
    # Arduino boot_app0.bin initializes sequence=1 (OTA slot 0). Its record
    # contains no absolute flash addresses, so place it at the actual otadata.
    if len(data) != size or size != 0x2000:
        raise ValueError("OTA initializer does not match the actual OTA data partition")
    seq, = struct.unpack_from("<I", data)
    crc, = struct.unpack_from("<I", data, 28)
    if seq != 1 or crc != zlib.crc32(data[:4], 0xFFFFFFFF):
        raise ValueError("OTA initializer does not select app0 or has an invalid CRC")


def verify_merged(data: bytes, segments: list[tuple[int, bytes]]) -> None:
    if len(data) != FLASH_BYTES:
        raise ValueError("Merged image must cover exactly 16MB")
    end = 0
    for offset, payload in sorted(segments):
        if offset < end or offset + len(payload) > len(data):
            raise ValueError("Merged segment overlap or overflow")
        if any(byte != 0xFF for byte in data[end:offset]):
            raise ValueError("Unexpected contents in an erased gap")
        if data[offset:offset + len(payload)] != payload:
            raise ValueError(f"Merged contents mismatch at {offset:#x}")
        end = offset + len(payload)
    if any(byte != 0xFF for byte in data[end:]):
        raise ValueError("Unwritten flash is not initialized to erased bytes")


def command_log(command: list[str], destination: Path) -> str:
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, check=False)
    destination.write_text(result.stdout, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(f"Command failed; see {destination}")
    return result.stdout


def package(core: Path, output: Path, build_log: Path | None = None) -> Path:
    source_paths = [str(PROJECT / p) for p in
                    ("src", "lib", "include", "platformio.ini", "partitions_diykit_new.csv",
                     "littlefsbuilder.py")]
    subprocess.run(["git", "diff", "--exit-code", "HEAD", "--", *source_paths],
                   cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    untracked = subprocess.check_output(
        ["git", "ls-files", "--others", "--exclude-standard", "--", *source_paths], cwd=ROOT)
    if untracked.strip():
        raise ValueError("Uncommitted firmware inputs: commit/review before packaging")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT).decode().strip()
    esptool = core / "packages/tool-esptoolpy/esptool.py"
    ota = core / "packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
    if not esptool.is_file() or not ota.is_file():
        raise ValueError("Build tool packages missing; provide the PlatformIO core used to build")
    # Validate everything before creating a new release directory.
    prepared = []
    for target, name in TARGETS.items():
        build = PROJECT / ".pio/build" / target
        paths = [build / "bootloader.bin", build / "partitions.bin", ota, build / "firmware.bin"]
        payloads = [path.read_bytes() for path in paths]
        validate_image(payloads[0], f"{target}/bootloader")
        validate_image(payloads[3], f"{target}/firmware")
        parts = partition_table(payloads[1])
        indexed = {item["name"]: item for item in parts}
        validate_ota_init(payloads[2], indexed["otadata"]["size"])
        if len(payloads[0]) > PARTITION_OFFSET or len(payloads[1]) > 0x1000:
            raise ValueError("Bootloader or partition table overflow")
        if len(payloads[3]) > indexed["app0"]["size"]:
            raise ValueError("Application exceeds app0 partition")
        offsets = [0, PARTITION_OFFSET, indexed["otadata"]["offset"], indexed["app0"]["offset"]]
        prepared.append((target, name, paths, payloads, offsets, parts))
    output.mkdir(parents=True, exist_ok=False)  # preserve existing release artifacts
    records = []
    for target, name, paths, payloads, offsets, parts in prepared:
        merged = output / name
        command = [sys.executable, str(esptool), "--chip", "esp32s3", "merge_bin",
                   "--output", str(merged), "--target-offset", "0", "--flash_mode", "keep",
                   "--flash_freq", "keep", "--flash_size", "keep", "--fill-flash-size", "16MB"]
        for offset, path in zip(offsets, paths):
            command.extend([hex(offset), str(path)])
        command_log(command, output / f"{target}-merge.txt")
        verify_merged(merged.read_bytes(), list(zip(offsets, payloads)))
        # Independent official esptool image inspection (not an on-device test).
        for kind, path in (("bootloader", paths[0]), ("application", paths[3])):
            info = command_log([sys.executable, str(esptool), "--chip", "esp32s3",
                                "image_info", "--version", "2", str(path)],
                               output / f"{target}-{kind}-info.txt")
            if "invalid" in info.lower():
                raise ValueError(f"esptool reports an invalid image: {path}")
        records.append(dict(environment=target, filename=name, flash_address="0x0",
                            size=merged.stat().st_size, sha256=digest(merged.read_bytes()),
                            partitions=parts,
                            segments=[dict(offset=hex(offset), filename=path.name,
                                           size=len(payload), sha256=digest(payload))
                                      for offset, path, payload in zip(offsets, paths, payloads)]))
    manifest = dict(schema=1, created_at=datetime.now(ZoneInfo("Asia/Hong_Kong")).isoformat(),
                    git_revision=revision, board="Waveshare ESP32-S3-PhotoPainter V1",
                    chip="esp32s3", flash_bytes=FLASH_BYTES, initial_flash_resets_all_flash=True,
                    hardware_tested=False, sd_card_modified_by_package=False,
                    filesystem="No preloaded assets; firmware uses LittleFS.begin(true)", images=records)
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n")
    shutil.copyfile(ROOT / "docs/photopainter/flashing.md", output / "FLASHING_zh.md")
    if build_log is not None:
        shutil.copyfile(build_log, output / "build-log.txt")
    files = sorted(path for path in output.iterdir() if path.is_file())
    (output / "SHA256SUMS.txt").write_text("".join(
        f"{digest(path.read_bytes())}  {path.name}\n" for path in files))
    archive = output / "PhotoPainter-V1-flash-package.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for path in sorted(output.iterdir()):
            if path.is_file() and path != archive:
                bundle.write(path, path.name)
    # Verify the archive against the files on disk, not merely ZIP member names.
    with zipfile.ZipFile(archive) as bundle:
        if bundle.testzip() is not None:
            raise ValueError("ZIP CRC validation failed")
        for name in bundle.namelist():
            if bundle.read(name) != (output / name).read_bytes():
                raise ValueError("ZIP readback mismatch")
    (output / (archive.name + ".sha256")).write_text(f"{digest(archive.read_bytes())}  {archive.name}\n")
    return archive


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--build-log", type=Path)
    args = parser.parse_args()
    print(package(args.core_dir.resolve(), args.output_dir.resolve(), args.build_log))


if __name__ == "__main__":
    main()
