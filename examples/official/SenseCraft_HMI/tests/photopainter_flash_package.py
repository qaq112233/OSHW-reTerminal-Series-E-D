"""Host checks for initial-flash packaging; does not test hardware."""
from pathlib import Path
import hashlib
import struct
import sys
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "tools"))
from photopainter_flash_package import (
    FLASH_BYTES, partition_table, validate_image, validate_ota_init, verify_merged,
)


def fixture(rows=None):
    if rows is None:
        rows = [("otadata", 1, 0, 0x8D000, 0x2000),
                ("app0", 0, 0x10, 0x90000, 0x500000),
                ("spiffs", 1, 0x82, 0xA90000, 0x500000)]
    data = b"".join(struct.pack("<HBBII16sI", 0x50AA, kind, sub, off, size,
                                name.encode(), 0) for name, kind, sub, off, size in rows)
    return data + b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(data).digest() + b"\xff" * 64


class FlashPackageTests(unittest.TestCase):
    def test_real_layout(self):
        entries = partition_table(fixture())
        self.assertEqual(entries[0]["offset"], 0x8D000)
        self.assertEqual(entries[1]["offset"], 0x90000)

    def test_bad_md5(self):
        data = bytearray(fixture()); data[12] ^= 1
        with self.assertRaises(ValueError): partition_table(bytes(data))

    def test_default_ota_address_rejected(self):
        with self.assertRaises(ValueError):
            partition_table(fixture([("otadata", 1, 0, 0xE000, 0x2000),
                                     ("app0", 0, 0x10, 0x90000, 0x500000),
                                     ("spiffs", 1, 0x82, 0xA90000, 0x500000)]))

    def test_overlap_rejected(self):
        with self.assertRaises(ValueError):
            partition_table(fixture([("otadata", 1, 0, 0x8D000, 0x2000),
                                     ("app0", 0, 0x10, 0x90000, 0xB00000),
                                     ("spiffs", 1, 0x82, 0xA90000, 0x500000)]))

    def test_flash_overflow_rejected(self):
        with self.assertRaises(ValueError):
            partition_table(fixture([("otadata", 1, 0, 0x8D000, 0x2000),
                                     ("app0", 0, 0x10, 0x90000, 0x500000),
                                     ("spiffs", 1, 0x82, 0xA90000, 0x600000)]))

    def test_chip_and_flash_header(self):
        data = bytearray(24); data[0] = 0xE9; data[3] = 0x4F; data[12] = 9
        validate_image(bytes(data), "fixture")
        data[12] = 0
        with self.assertRaises(ValueError): validate_image(bytes(data), "wrong chip")
        data[12] = 9; data[3] = 0x2F
        with self.assertRaises(ValueError): validate_image(bytes(data), "wrong flash size")

    def test_ota_initializer(self):
        data = bytearray(b"\xff" * 0x2000)
        struct.pack_into("<I", data, 0, 1)
        struct.pack_into("<I", data, 28, zlib.crc32(data[:4], 0xFFFFFFFF))
        validate_ota_init(bytes(data), 0x2000)
        data[28] ^= 1
        with self.assertRaises(ValueError): validate_ota_init(bytes(data), 0x2000)

    def test_full_merge_and_erased_regions(self):
        data = bytearray(b"\xff" * FLASH_BYTES)
        segments = [(0, b"boot"), (0x8000, b"table"), (0x8D000, b"ota"), (0x90000, b"app")]
        for offset, payload in segments: data[offset:offset + len(payload)] = payload
        verify_merged(bytes(data), segments)
        data[0xA90000] = 0
        with self.assertRaises(ValueError): verify_merged(bytes(data), segments)
        with self.assertRaises(ValueError): verify_merged(bytes(data[:-1]), segments)


if __name__ == "__main__":
    unittest.main()
