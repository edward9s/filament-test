#!/usr/bin/env python3
"""Package or verify a complete static Filament SDK."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tarfile
import tempfile

from filament_sdk import LAYOUTS, lock, ready

ROOT = Path(__file__).resolve().parents[1]


def verify(archive, name, config):
    with tempfile.TemporaryDirectory() as temporary:
        with tarfile.open(archive) as package:
            package.extractall(temporary, filter='data')
        if not ready(Path(temporary) / 'filament', config, name):
            raise RuntimeError(f'Incomplete or mismatched SDK: {name}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path)
    parser.add_argument('--sdk', type=Path)
    parser.add_argument('--archive', choices=LAYOUTS)
    parser.add_argument('--verify', type=Path)
    args = parser.parse_args()
    config = lock(ROOT)
    if args.verify:
        verify(args.verify, args.verify.name, config)
        return
    if not all((args.source, args.sdk, args.archive)):
        parser.error('--source, --sdk and --archive are required to package an SDK')
    shutil.copytree(args.source / 'libs/bluevk/include', args.sdk / 'include', dirs_exist_ok=True)
    shutil.copytree(args.source / 'libs/utils/include/utils', args.sdk / 'include/utils', dirs_exist_ok=True)
    (args.sdk / 'SDK-INFO.json').write_text(json.dumps({**config, 'archive': args.archive}, indent=2) + '\n')
    if not ready(args.sdk, config, args.archive):
        raise RuntimeError('SDK is missing libraries, tools or headers')
    output = ROOT / '.generated/sdk'
    output.mkdir(parents=True, exist_ok=True)
    archive = output / args.archive
    with tarfile.open(archive, 'w:gz') as package:
        package.add(args.sdk, arcname='filament')
    verify(archive, args.archive, config)
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    (output / (args.archive + '.sha256')).write_text(f'{digest}  {args.archive}\n')


if __name__ == '__main__':
    main()
