#!/usr/bin/env python3
"""Consume the complete fork SDK matching filament.json; never mix official libraries."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import tarfile
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def lock():
    config = json.loads((ROOT / 'filament.json').read_text())
    if config['repository'] != 'edward9s/filament' or not re.fullmatch('[0-9a-f]{40}', config['revision']):
        raise RuntimeError('Invalid Filament source pin')
    if not re.fullmatch(r'\d+\.\d+\.\d+', config['ndk']):
        raise RuntimeError('Invalid Filament NDK pin')
    return config


def ready(path, config, archive):
    info = path / 'SDK-INFO.json'
    try:
        metadata = json.loads(info.read_text())
    except (OSError, ValueError):
        return False
    architecture = 'x86_64' if archive == 'filament-linux.tgz' else 'arm64-v8a'
    return (all(metadata.get(key) == value for key, value in config.items())
            and metadata.get('archive') == archive
            and (path / 'include/bluevk/BlueVK.h').is_file()
            and (path / 'lib' / architecture / 'libfilament.a').is_file())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--info', action='store_true')
    args = parser.parse_args()
    config = lock()
    revision = config['revision']
    tag = 'filament-sdk-' + revision[:12]
    if args.info:
        for k, v in {**config, 'tag': tag}.items():
            print(f'{k}={v}')
        return
    import os
    repository = os.environ.get('GITHUB_REPOSITORY', 'edward9s/filament-test')
    archives = {'filament-linux.tgz': ROOT / '.deps/host/filament',
                'filament-android-arm64.tgz': ROOT / '.deps/filament'}
    if all(ready(path, config, name) for name, path in archives.items()):
        return
    with tempfile.TemporaryDirectory() as temporary:
        temporary = Path(temporary)
        for name in [*archives, 'SHA256SUMS']:
            subprocess.run(['gh', 'release', 'download', tag, '--repo', repository,
                            '--pattern', name, '--dir', str(temporary)], check=True)
        sums = {}
        for line in (temporary / 'SHA256SUMS').read_text().splitlines():
            digest, name = line.split()
            sums[name] = digest
        for name, destination in archives.items():
            archive = temporary / name
            if hashlib.sha256(archive.read_bytes()).hexdigest() != sums[name]:
                raise RuntimeError(f'Filament SDK checksum mismatch: {name}')
            extracted = temporary / name.removesuffix('.tgz')
            with tarfile.open(archive) as package:
                package.extractall(extracted, filter='data')
            sdk = extracted / 'filament'
            if not ready(sdk, config, name):
                raise RuntimeError(f'Filament SDK source/architecture mismatch: {name}')
            destination.parent.mkdir(parents=True, exist_ok=True)
            if destination.exists():
                shutil.rmtree(destination)
            shutil.move(sdk, destination)
    print(f'Filament fork SDK ready: {revision}')


if __name__ == '__main__':
    main()
