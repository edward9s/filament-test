#!/usr/bin/env python3
"""Fetch only the pinned arm64 Android validation layer; never alter normal builds."""
import hashlib
import io
from pathlib import Path
import urllib.request
import zipfile

VERSION = '1.4.363.0'
SHA256 = '9a3b8e32cb3330406dd4f3651594201635baee4528da8e2a5300e4f55e1a9cc7'
URL = (f'https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/download/'
       f'vulkan-sdk-{VERSION}/android-binaries-{VERSION}.zip')


def install(archive: bytes, destination: Path) -> None:
    if hashlib.sha256(archive).hexdigest() != SHA256:
        raise RuntimeError('Validation layer archive checksum mismatch')
    name = f'android-binaries-{VERSION}/arm64-v8a/libVkLayer_khronos_validation.so'
    with zipfile.ZipFile(io.BytesIO(archive)) as package:
        data = package.read(name)
    # Reject a wrong-architecture or damaged native library before packaging.
    if data[:6] != b'\x7fELF\x02\x01' or int.from_bytes(data[18:20], 'little') != 183:
        raise RuntimeError('Validation layer is not an AArch64 ELF library')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(data)
    print(f'Installed Vulkan validation {VERSION}: {len(data)} bytes')


if __name__ == '__main__':
    with urllib.request.urlopen(URL, timeout=120) as response:
        archive = response.read()
    install(archive, Path(__file__).resolve().parents[1] /
            '.deps/validation/arm64-v8a/libVkLayer_khronos_validation.so')
