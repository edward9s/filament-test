#!/usr/bin/env python3
"""Check the bundled scene, not merely the existence of an APK."""
import sys
import zipfile
from textures import validate as validate_texture

with zipfile.ZipFile(sys.argv[1]) as apk:
    libraries = {name for name in apk.namelist() if name.startswith('lib/')}
    if 'lib/arm64-v8a/libmain.so' not in libraries:
        raise RuntimeError('Missing libmain.so')
    if any(name.endswith('/libSDL3.so') for name in libraries):
        raise RuntimeError('SDL3 must be statically linked; unexpected libSDL3.so')
    for name in ['surface.filamat', 'foliage.filamat', 'canopy.filamat', 'hud.filamat', 'effects.filamat', 'water.filamat', 'environment_ibl.ktx', 'environment_skybox.ktx']:
        if apk.getinfo('assets/forest/' + name).file_size == 0:
            raise RuntimeError(f'Empty asset: {name}')
    for material in ['ground', 'stone', 'bark']:
        name = f'assets/forest/{material}_normal.rgba'
        if apk.getinfo(name).file_size != 1024 * 1024 * 4:
            raise RuntimeError(f'Invalid normal texture size: {name}')
        for kind in ['albedo', 'arm']:
            name = f'assets/forest/{material}_{kind}.ktx2'
            validate_texture(apk.read(name), kind == 'albedo')
    validate_texture(apk.read('assets/forest/canopy.ktx2'), True)
    if not apk.read('assets/forest/CREDITS.md'):
        raise RuntimeError('Missing asset credits')
print('APK contains static SDL3, six materials, both environment maps, ten textures and credits.')
