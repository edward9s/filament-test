#!/usr/bin/env python3
"""Check the bundled scene, not merely the existence of an APK."""
import sys
import zipfile

with zipfile.ZipFile(sys.argv[1]) as apk:
    for name in ['surface.filamat', 'foliage.filamat', 'canopy.filamat', 'hud.filamat', 'environment_ibl.ktx', 'environment_skybox.ktx']:
        if apk.getinfo('assets/forest/' + name).file_size == 0:
            raise RuntimeError(f'Empty asset: {name}')
    for material in ['ground', 'stone', 'bark']:
        for kind in ['albedo', 'normal', 'arm']:
            name = f'assets/forest/{material}_{kind}.rgba'
            if apk.getinfo(name).file_size != 1024 * 1024 * 4:
                raise RuntimeError(f'Invalid texture size: {name}')
    if apk.getinfo('assets/forest/canopy.rgba').file_size != 1024 * 1024 * 4:
        raise RuntimeError('Invalid canopy texture size')
    if not apk.read('assets/forest/CREDITS.md'):
        raise RuntimeError('Missing asset credits')
print('APK contains four materials, both environment maps, ten textures and credits.')
