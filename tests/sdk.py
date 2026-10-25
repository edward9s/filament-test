#!/usr/bin/env python3
"""Check the SDK trust boundary without network or GitHub credentials."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('download_filament',
    Path(__file__).resolve().parents[1] / 'scripts/download-filament.py')
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


class SdkDownload(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.config = {'repository': 'edward9s/filament', 'revision': 'a' * 40,
                       'version': '1.78.1', 'ndk': '30.0.16248370'}
        (self.root / 'filament.json').write_text(json.dumps(self.config))
        self.release = self.root / 'release'
        self.release.mkdir()
        self.packages = [('filament-linux.tgz', 'x86_64', '.deps/host/filament'),
                         ('filament-android-arm64.tgz', 'arm64-v8a', '.deps/filament')]
        self.patch_root = patch.object(sdk, 'ROOT', self.root)
        self.patch_root.start()
        self.addCleanup(self.patch_root.stop)

    def package(self, metadata=None, architecture=None):
        for name, arch, _ in self.packages:
            folder = self.root / name / 'filament'
            (folder / 'include/bluevk').mkdir(parents=True)
            (folder / 'include/bluevk/BlueVK.h').write_text('// header')
            (folder / 'lib' / (architecture or arch)).mkdir(parents=True)
            (folder / 'lib' / (architecture or arch) / 'libfilament.a').write_bytes(b'archive')
            (folder / 'SDK-INFO.json').write_text(json.dumps(
                {**self.config, 'archive': name, **(metadata or {})}))
            with tarfile.open(self.release / name, 'w:gz') as archive:
                archive.add(folder, arcname='filament')
        self.checksums()

    def checksums(self):
        (self.release / 'SHA256SUMS').write_text(''.join(
            hashlib.sha256((self.release / name).read_bytes()).hexdigest() + '  ' + name + '\n'
            for name, _, _ in self.packages))

    def download(self):
        def gh(args, check):
            self.assertTrue(check)
            name = args[args.index('--pattern') + 1]
            destination = Path(args[args.index('--dir') + 1])
            shutil.copyfile(self.release / name, destination / name)
        with patch.object(sys, 'argv', ['download-filament.py']), \
                patch.object(sdk.subprocess, 'run', side_effect=gh):
            sdk.main()

    def test_matching_complete_sdks_install_and_reuse(self):
        self.package()
        self.download()
        for name, _, destination in self.packages:
            self.assertTrue(sdk.ready(self.root / destination, self.config, name))
        with patch.object(sys, 'argv', ['download-filament.py']), \
                patch.object(sdk.subprocess, 'run', side_effect=AssertionError('Unexpected download')):
            sdk.main()

    def test_corrupt_archive_fails_before_replacement(self):
        self.package()
        destination = self.root / '.deps/host/filament'
        destination.mkdir(parents=True)
        sentinel = destination / 'existing'
        sentinel.write_text('keep')
        with (self.release / self.packages[0][0]).open('ab') as archive:
            archive.write(b'corrupt')
        with self.assertRaisesRegex(RuntimeError, 'checksum mismatch'):
            self.download()
        self.assertEqual(sentinel.read_text(), 'keep')

    def test_different_revision_is_rejected(self):
        self.package({'revision': 'b' * 40})
        with self.assertRaisesRegex(RuntimeError, 'source/architecture mismatch'):
            self.download()

    def test_different_ndk_is_rejected(self):
        self.package({'ndk': '29.0.0'})
        with self.assertRaisesRegex(RuntimeError, 'source/architecture mismatch'):
            self.download()

    def test_different_architecture_is_rejected(self):
        self.package(architecture='arm64-v8a')
        with self.assertRaisesRegex(RuntimeError, 'source/architecture mismatch'):
            self.download()

    def test_invalid_pin_fails_before_download(self):
        (self.root / 'filament.json').write_text(json.dumps({**self.config, 'revision': 'main'}))
        with self.assertRaisesRegex(RuntimeError, 'Invalid Filament source pin'):
            self.download()


if __name__ == '__main__':
    unittest.main()
