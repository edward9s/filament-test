#!/usr/bin/env python3
"""Build the macOS universal or Windows x64 SDK using native compilers."""
import argparse
from pathlib import Path
import platform
import subprocess
import sys

from filament_sdk import lock

ROOT = Path(__file__).resolve().parents[1]


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('platform', choices=('macos-universal', 'windows-x64-msvc'))
    args = parser.parse_args()
    source = args.source.resolve()
    config = lock(ROOT)
    revision = subprocess.check_output(['git', '-C', str(source), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != config['revision']:
        raise RuntimeError('Fork checkout does not match filament.json')
    windows = args.platform == 'windows-x64-msvc'
    if platform.system() != ('Windows' if windows else 'Darwin'):
        raise RuntimeError('This SDK requires its native build runner')
    sdk = source / 'out' / ('sdk-' + args.platform) / 'filament'
    common = ['-DFILAMENT_SKIP_SAMPLES=ON', '-DFILAMENT_SKIP_SDL2=ON',
              '-DFILAMENT_SUPPORTS_VULKAN=ON']
    if windows:
        # Match the dynamic MSVC CRT used by the repository's static SDL3 SDK.
        common += ['-G', 'Visual Studio 17 2022', '-A', 'x64', '-DUSE_STATIC_CRT=OFF',
                   '-DFILAMENT_WINDOWS_CI_BUILD=ON']
    else:
        common += ['-G', 'Ninja', '-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64',
                   '-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0']
    debug = source / 'out' / ('sdk-debug-' + args.platform)
    run('cmake', '-S', source, '-B', debug, '-DCMAKE_BUILD_TYPE=Debug', *common)
    run('cmake', '--build', debug, '--config', 'Debug', '--target',
        'test_vulkan_pipeline', 'test_utils', '--parallel', '2')
    component = Path('Debug') if windows else Path()
    suffix = '.exe' if windows else ''
    run(debug / 'filament/backend' / component / ('test_vulkan_pipeline' + suffix))
    run(debug / 'libs/utils' / component / ('test_utils' + suffix))
    build = source / 'out' / ('sdk-release-' + args.platform)
    run('cmake', '-S', source, '-B', build, '-DCMAKE_BUILD_TYPE=Release',
        f'-DCMAKE_INSTALL_PREFIX={sdk}', '-DFILAMENT_BUILD_TESTING=OFF', *common)
    run('cmake', '--build', build, '--config', 'Release', '--target', 'install', '--parallel', '2')
    if not windows:
        archs = subprocess.check_output(['lipo', '-archs', str(sdk / 'lib/universal/libfilament.a')], text=True)
        if set(archs.split()) != {'x86_64', 'arm64'}:
            raise RuntimeError('macOS SDK must contain both x86_64 and arm64')
    run(sys.executable, ROOT / 'scripts/package-filament.py', '--source', source,
        '--sdk', sdk, '--archive', 'filament-' + args.platform + '.tgz')


if __name__ == '__main__':
    main()
