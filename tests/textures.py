"""Exercise texture metadata validation before real payloads reach Filament."""
from pathlib import Path
import struct
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from textures import IDENTIFIER, LEVELS, SIZE, validate


def fixture(srgb=True):
    descriptor = 80 + LEVELS * 24
    start = descriptor + 16
    data = bytearray(start + LEVELS * 8)
    data[:12] = IDENTIFIER
    struct.pack_into('<9I', data, 12, 0, 1, SIZE, SIZE, 0, 0, 1, LEVELS, 2)
    struct.pack_into('<2I', data, 48, descriptor, 16)
    data[descriptor + 12] = 166
    data[descriptor + 14] = 2 if srgb else 1
    for level in range(LEVELS):
        dimension = max(1, SIZE >> level)
        struct.pack_into('<3Q', data, 80 + level * 24, start + level * 8, 8,
                         max(1, (dimension + 3) // 4) ** 2 * 16)
    return data


class Textures(unittest.TestCase):
    def test_valid_layouts(self):
        validate(fixture(True), True)
        validate(fixture(False), False)

    def test_wrong_transfer(self):
        with self.assertRaises(ValueError):
            validate(fixture(False), True)

    def test_missing_mips(self):
        data = fixture()
        struct.pack_into('<I', data, 40, 1)
        with self.assertRaises(ValueError):
            validate(data, True)

    def test_wrong_decoded_size(self):
        data = fixture()
        struct.pack_into('<Q', data, 96, 1)
        with self.assertRaises(ValueError):
            validate(data, True)

    def test_overlapping_mips(self):
        data = fixture()
        data[104:112] = data[80:88]
        with self.assertRaises(ValueError):
            validate(data, True)

    def test_truncated(self):
        with self.assertRaises(ValueError):
            validate(fixture()[:-1], True)


if __name__ == '__main__':
    unittest.main()
