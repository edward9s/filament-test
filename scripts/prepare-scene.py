#!/usr/bin/env python3
"""Fetch hash-pinned CC0 sources and build assets using the matching Filament tools."""
import argparse
import hashlib
import json
import os
import pathlib
import shutil
import subprocess
import tempfile
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[1]


def download(url, path, digest):
    if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest() == digest:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    partial = path.with_suffix(path.suffix + '.partial')
    try:
        subprocess.run(['curl', '-fLsS', '--retry', '3', '--connect-timeout', '20',
                        '--max-time', '300', url, '-o', str(partial)], check=True)
        if hashlib.sha256(partial.read_bytes()).hexdigest() != digest:
            raise RuntimeError(f'Checksum mismatch: {url}')
        partial.replace(path)
    finally:
        partial.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tools', type=pathlib.Path, help='Filament 1.78.1 bin directory')
    args = parser.parse_args()
    tools = args.tools
    if tools is None:
        subprocess.run(['python3', str(ROOT / 'scripts/download-filament.py')], check=True)
        tools = ROOT / '.deps/host/filament/bin'
    suffix = '.exe' if os.name == 'nt' else ''
    matc, cmgen = tools / ('matc' + suffix), tools / ('cmgen' + suffix)
    for tool in [matc, cmgen]:
        if not tool.is_file():
            raise RuntimeError(f'Missing Filament tool: {tool}')
    destination = ROOT / '.generated/assets/forest'
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Publish only after every download, conversion and shader compilation succeeds.
    with tempfile.TemporaryDirectory(dir=destination.parent) as temporary:
        output = pathlib.Path(temporary)
        for entry in json.loads((ROOT / 'assets/sources.json').read_text()):
            source = ROOT / '.deps/scene' / (entry['name'] + pathlib.Path(entry['url']).suffix)
            download(entry['url'], source, entry['sha256'])
            if entry['name'] == 'environment':
                subprocess.run([str(cmgen), '-q', '-s', '256', '-f', 'ktx',
                                '--ibl-samples=128', '-x', str(output / 'environment'), str(source)], check=True)
                for suffix in ['ibl', 'skybox']:
                    generated = output / 'environment' / f'environment_{suffix}.ktx'
                    shutil.move(generated, output / generated.name)
                shutil.rmtree(output / 'environment')
            else:
                with Image.open(source) as image:
                    if image.size != (1024, 1024):
                        raise RuntimeError(f'Unexpected source dimensions: {source}')
                    image = image.convert('RGBA')
                    if 'crop' in entry:
                        image = image.crop(entry['crop']).resize((1024, 1024), Image.Resampling.LANCZOS)
                    (output / (entry['name'] + '.rgba')).write_bytes(image.tobytes())
            print(f"Prepared {entry['name']}", flush=True)
        for material in ['surface', 'foliage', 'canopy', 'hud', 'effects', 'water']:
            subprocess.run([str(matc), '-a', 'opengl', '-a', 'vulkan', '-p', 'all',
                            '-o', str(output / (material + '.filamat')),
                            str(ROOT / 'materials' / (material + '.mat'))], check=True)
        shutil.copy(ROOT / 'assets/CREDITS.md', output / 'CREDITS.md')
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(output, destination)
    print(f'Scene ready: {destination}')


if __name__ == '__main__':
    main()

