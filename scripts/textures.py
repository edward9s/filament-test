"""Validate the UASTC KTX2 payloads shared by asset preparation and APK checks."""
import struct

SIZE = 1024
LEVELS = 11
IDENTIFIER = b'\xabKTX 20\xbb\r\n\x1a\n'


def validate(data, srgb):
    if len(data) < 80 + LEVELS * 24 or data[:12] != IDENTIFIER:
        raise ValueError('Invalid KTX2 header')
    vk_format, _, width, height, depth, layers, faces, levels, compression = struct.unpack_from('<9I', data, 12)
    if (vk_format, width, height, depth, layers, faces, levels, compression) != (0, SIZE, SIZE, 0, 0, 1, LEVELS, 2):
        raise ValueError('Expected a complete 1024x1024 UASTC/Zstd mip chain')
    offset, length = struct.unpack_from('<2I', data, 48)
    if length < 16 or offset < 80 + LEVELS * 24 or offset + length > len(data):
        raise ValueError('Invalid KTX2 data format descriptor')
    # Khronos data format descriptor: UASTC color model; explicit sRGB/linear transfer.
    if data[offset + 12] != 166 or data[offset + 14] != (2 if srgb else 1):
        raise ValueError('Wrong UASTC model or texture transfer function')
    regions = []
    for level in range(LEVELS):
        start, size, decoded = struct.unpack_from('<3Q', data, 80 + level * 24)
        dimension = max(1, SIZE >> level)
        expected = max(1, (dimension + 3) // 4) ** 2 * 16
        if not size or start < offset + length or start + size > len(data) or decoded != expected:
            raise ValueError(f'Invalid KTX2 mip level {level}')
        regions.append((start, start + size))
    regions.sort()
    if any(a[1] > b[0] for a, b in zip(regions, regions[1:])):
        raise ValueError('Overlapping KTX2 mip payloads')
