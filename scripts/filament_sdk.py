"""Source identity and layout of the four supported static SDKs."""
import json
import platform
import re

TAG = 'filament-static'
LAYOUTS = {
    'filament-linux.tgz': ('x86_64', 'libfilament.a', ''),
    'filament-android-arm64.tgz': ('arm64-v8a', 'libfilament.a', None),
    'filament-macos-universal.tgz': ('universal', 'libfilament.a', ''),
    'filament-windows-x64-msvc.tgz': ('x86_64', 'filament.lib', '.exe'),
}
ASSETS = set(LAYOUTS) | {'SHA256SUMS'}
FIELDS = {'repository': 'SOURCE_REPOSITORY', 'revision': 'SOURCE_COMMIT',
          'version': 'SOURCE_VERSION', 'ndk': 'SOURCE_NDK', 'lto': 'SDK_LTO'}


def validate(config):
    if set(config) != set(FIELDS) or not re.fullmatch(r'\d+\.\d+\.\d+', config['version']):
        raise RuntimeError('Invalid Filament SDK configuration')
    if config['repository'] != 'edward9s/filament' or not re.fullmatch('[0-9a-f]{40}', config['revision']):
        raise RuntimeError('Invalid Filament source pin')
    if not re.fullmatch(r'\d+\.\d+\.\d+', config['ndk']):
        raise RuntimeError('Invalid Filament NDK pin')
    if config['lto'] not in ('ON', 'OFF'):
        raise RuntimeError('Invalid Filament LTO setting')
    return config


def lock(root):
    return validate(json.loads((root / 'filament.json').read_text()))


def ready(path, config, archive):
    try:
        metadata = json.loads((path / 'SDK-INFO.json').read_text())
    except (OSError, ValueError):
        return False
    arch, library, suffix = LAYOUTS[archive]
    files = ['include/bluevk/BlueVK.h', f'lib/{arch}/{library}']
    if suffix is not None:
        files += [f'bin/matc{suffix}', f'bin/cmgen{suffix}']
    return (isinstance(metadata, dict)
            and all(metadata.get(key) == value for key, value in config.items())
            and metadata.get('archive') == archive
            and all((path / file).is_file() and (path / file).stat().st_size > 0 for file in files))


def release_ready(release, config):
    notes = dict(line.split('=', 1) for line in (release.get('body') or '').splitlines() if '=' in line)
    if not all(key in notes for key in FIELDS.values()):
        raise RuntimeError('Filament release is missing source metadata')
    validate({key: notes[field] for key, field in FIELDS.items()})
    if release['isDraft'] or any(notes[FIELDS[key]] != value for key, value in config.items()):
        return False
    found = {asset['name'] for asset in release['assets'] if asset['size'] > 0}
    if found != ASSETS or len(release['assets']) != len(ASSETS):
        raise RuntimeError('Published Filament SDK release is incomplete')
    return True


def host_archive():
    system, machine = platform.system(), platform.machine().lower()
    if system == 'Darwin' and machine in ('arm64', 'x86_64'):
        return 'filament-macos-universal.tgz'
    if system in ('Linux', 'Windows') and machine in ('amd64', 'x86_64'):
        return 'filament-linux.tgz' if system == 'Linux' else 'filament-windows-x64-msvc.tgz'
    raise RuntimeError(f'Unsupported SDK host: {system} {machine}')

