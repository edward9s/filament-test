#!/usr/bin/env python3
"""Download the pinned static SDK from the single Filament dependency release."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

from filament_sdk import TAG, host_archive, lock as read_lock, ready, release_ready

ROOT = Path(__file__).resolve().parents[1]


def lock():
    return read_lock(ROOT)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--info', action='store_true')
    args = parser.parse_args()
    config = lock()
    if args.info:
        for key, value in {**config, 'tag': TAG}.items():
            print(f'{key}={value}')
        return
    archives = {host_archive(): ROOT / '.deps/host/filament',
                'filament-android-arm64.tgz': ROOT / '.deps/filament'}
    if all(ready(path, config, name) for name, path in archives.items()):
        return
    repository = os.environ.get('GITHUB_REPOSITORY', 'edward9s/filament-test')
    result = subprocess.run(['gh', 'release', 'view', TAG, '--repo', repository,
                             '--json', 'body,assets,isDraft'], check=True, capture_output=True, text=True)
    if not release_ready(json.loads(result.stdout), config):
        raise RuntimeError('Filament release is not ready for the pinned source; run build_filament.yml')
    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        for name in [*archives, 'SHA256SUMS']:
            subprocess.run(['gh', 'release', 'download', TAG, '--repo', repository,
                            '--pattern', name, '--dir', str(temporary)], check=True)
        sums = {}
        for line in (temporary / 'SHA256SUMS').read_text().splitlines():
            digest, name = line.split()
            sums[name] = digest
        # Validate both archives before replacing an installed SDK.
        extracted = {}
        for name in archives:
            archive = temporary / name
            if hashlib.sha256(archive.read_bytes()).hexdigest() != sums[name]:
                raise RuntimeError(f'Filament SDK checksum mismatch: {name}')
            folder = temporary / name.removesuffix('.tgz')
            with tarfile.open(archive) as package:
                package.extractall(folder, filter='data')
            sdk = folder / 'filament'
            if not ready(sdk, config, name):
                raise RuntimeError(f'Filament SDK source/architecture mismatch: {name}')
            extracted[name] = sdk
        for name, destination in archives.items():
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists():
                shutil.rmtree(destination)
            shutil.move(extracted[name], destination)
    print(f'Filament fork SDK ready: {config["revision"]}')


if __name__ == '__main__':
    main()
