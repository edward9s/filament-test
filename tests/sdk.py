#!/usr/bin/env python3
"""Check SDK provenance, host selection and publication without network access."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

SCRIPTS = Path(__file__).resolve().parents[1] / 'scripts'
sys.path.insert(0, str(SCRIPTS))
import filament_sdk as common

spec = importlib.util.spec_from_file_location('download_filament', SCRIPTS / 'download-filament.py')
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
        self.info = {'body': ''.join(common.FIELDS[k] + '=' + v + '\n' for k, v in self.config.items()),
                     'isDraft': False, 'assets': [{'name': n, 'size': 1} for n in common.ASSETS]}
        self.patch_root = patch.object(sdk, 'ROOT', self.root)
        self.patch_root.start()
        self.addCleanup(self.patch_root.stop)
        self.host = patch.object(sdk, 'host_archive', return_value='filament-linux.tgz')
        self.host.start()
        self.addCleanup(self.host.stop)

    def package(self, metadata=None, architecture=None):
        for name, (arch, library, suffix) in common.LAYOUTS.items():
            folder = self.root / name / 'filament'
            (folder / 'include/bluevk').mkdir(parents=True)
            (folder / 'include/bluevk/BlueVK.h').write_text('// header')
            (folder / 'lib' / (architecture or arch)).mkdir(parents=True)
            (folder / 'lib' / (architecture or arch) / library).write_bytes(b'archive')
            if suffix is not None:
                (folder / 'bin').mkdir()
                for tool in ['matc', 'cmgen']:
                    (folder / 'bin' / (tool + suffix)).write_bytes(b'tool')
            (folder / 'SDK-INFO.json').write_text(json.dumps(
                {**self.config, 'archive': name, **(metadata or {})}))
            with tarfile.open(self.release / name, 'w:gz') as archive:
                archive.add(folder, arcname='filament')
        (self.release / 'SHA256SUMS').write_text(''.join(
            hashlib.sha256((self.release / name).read_bytes()).hexdigest() + '  ' + name + '\n'
            for name in common.LAYOUTS))

    def download(self):
        def gh(args, check, **kwargs):
            self.assertTrue(check)
            self.assertIn('filament-static', args)
            if args[2] == 'view':
                return subprocess.CompletedProcess(args, 0, stdout=json.dumps(self.info))
            name = args[args.index('--pattern') + 1]
            destination = Path(args[args.index('--dir') + 1])
            shutil.copyfile(self.release / name, destination / name)
        with patch.object(sys, 'argv', ['download-filament.py']), \
                patch.object(sdk.subprocess, 'run', side_effect=gh):
            sdk.main()

    def test_matching_complete_sdks_install_and_reuse(self):
        self.package()
        self.download()
        for name, destination in [('filament-linux.tgz', '.deps/host/filament'),
                                  ('filament-android-arm64.tgz', '.deps/filament')]:
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
        with (self.release / 'filament-linux.tgz').open('ab') as archive:
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

    def test_draft_release_cannot_be_consumed(self):
        self.info['isDraft'] = True
        with self.assertRaisesRegex(RuntimeError, 'not ready for the pinned source'):
            self.download()

    def test_other_source_release_cannot_be_consumed(self):
        self.info['body'] = self.info['body'].replace('a' * 40, 'b' * 40)
        with self.assertRaisesRegex(RuntimeError, 'not ready for the pinned source'):
            self.download()

    def test_incomplete_published_release_is_rejected(self):
        self.info['assets'] = self.info['assets'][:-1]
        with self.assertRaisesRegex(RuntimeError, 'release is incomplete'):
            self.download()

    def test_empty_published_asset_is_rejected(self):
        self.info['assets'][0]['size'] = 0
        with self.assertRaisesRegex(RuntimeError, 'release is incomplete'):
            self.download()

    def test_missing_release_metadata_is_rejected(self):
        self.info['body'] = ''
        with self.assertRaisesRegex(RuntimeError, 'missing source metadata'):
            self.download()

    def test_malformed_release_pin_is_rejected(self):
        self.info['body'] = self.info['body'].replace('a' * 40, 'main')
        with self.assertRaisesRegex(RuntimeError, 'Invalid Filament source pin'):
            self.download()

    def test_mac_host_tools_install(self):
        self.package()
        with patch.object(sdk, 'host_archive', return_value='filament-macos-universal.tgz'):
            self.download()
        self.assertTrue((self.root / '.deps/host/filament/lib/universal/libfilament.a').is_file())

    def test_windows_host_tools_install(self):
        self.package()
        with patch.object(sdk, 'host_archive', return_value='filament-windows-x64-msvc.tgz'):
            self.download()
        self.assertTrue((self.root / '.deps/host/filament/bin/matc.exe').is_file())
        self.assertTrue((self.root / '.deps/host/filament/lib/x86_64/filament.lib').is_file())

    def test_android_failure_does_not_replace_host_sdk(self):
        self.package()
        destination = self.root / '.deps/host/filament'
        destination.mkdir(parents=True)
        (destination / 'existing').write_text('keep')
        with (self.release / 'filament-android-arm64.tgz').open('ab') as archive:
            archive.write(b'corrupt')
        with self.assertRaisesRegex(RuntimeError, 'checksum mismatch'):
            self.download()
        self.assertEqual((destination / 'existing').read_text(), 'keep')


class Package(unittest.TestCase):
    def test_all_platform_archives_are_stamped_and_verified(self):
        spec=importlib.util.spec_from_file_location('package_filament',SCRIPTS / 'package-filament.py')
        package=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(package)
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            config={'repository':'edward9s/filament','revision':'a'*40,
                    'version':'1.78.1','ndk':'30.0.16248370'}
            (root/'filament.json').write_text(json.dumps(config))
            source=root/'source'
            for header in ['libs/bluevk/include/bluevk/BlueVK.h','libs/utils/include/utils/Mutex.h']:
                file=source/header
                file.parent.mkdir(parents=True,exist_ok=True)
                file.write_text('// header')
            for archive,(arch,library,suffix) in common.LAYOUTS.items():
                with self.subTest(archive=archive):
                    sdk=root/archive/'filament'
                    (sdk/'lib'/arch).mkdir(parents=True)
                    (sdk/'lib'/arch/library).write_bytes(b'static library')
                    if suffix is not None:
                        (sdk/'bin').mkdir()
                        for tool in ['matc','cmgen']:
                            (sdk/'bin'/(tool+suffix)).write_bytes(b'tool')
                    argv=['package-filament.py','--source',str(source),'--sdk',str(sdk),'--archive',archive]
                    with patch.object(package,'ROOT',root),patch.object(sys,'argv',argv):
                        package.main()
                    output=root/'.generated/sdk'/archive
                    package.verify(output,archive,config)
                    digest=hashlib.sha256(output.read_bytes()).hexdigest()
                    self.assertEqual((output.parent/(archive+'.sha256')).read_text(),digest+'  '+archive+'\n')
                    with self.assertRaisesRegex(RuntimeError,'mismatched SDK'):
                        package.verify(output,archive,{**config,'revision':'b'*40})


class HostSelection(unittest.TestCase):
    def test_supported_hosts(self):
        for system, machine, expected in [
            ('Linux', 'x86_64', 'filament-linux.tgz'),
            ('Darwin', 'arm64', 'filament-macos-universal.tgz'),
            ('Darwin', 'x86_64', 'filament-macos-universal.tgz'),
            ('Windows', 'AMD64', 'filament-windows-x64-msvc.tgz')]:
            with self.subTest(system=system, machine=machine), \
                    patch.object(common.platform, 'system', return_value=system), \
                    patch.object(common.platform, 'machine', return_value=machine):
                self.assertEqual(common.host_archive(), expected)

    def test_unsupported_host_fails(self):
        with patch.object(common.platform, 'system', return_value='Linux'), \
                patch.object(common.platform, 'machine', return_value='aarch64'), \
                self.assertRaisesRegex(RuntimeError, 'Unsupported SDK host'):
            common.host_archive()


if __name__ == '__main__':
    unittest.main()
