"""Offline vprot installer, launcher and exact patch-helper regression tests."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('vprot', ROOT / 'scripts/install-wine-vprot-runtime.py')
vprot = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vprot)


def checksum(filename):
    return hashlib.sha256(filename.read_bytes()).hexdigest()


class RuntimeFixture(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.runtime = self.root / 'runtime with spaces'
        self.unix = self.runtime / 'lib/wine/x86_64-unix'
        self.unix.mkdir(parents=True)
        for name in vprot.BINARIES:
            (self.unix / name).write_text('fixture: ' + name)
            (self.unix / name).chmod(0o755)
        self.system = self.root / 'system'
        self.system.mkdir()
        self.dependencies = []
        for name in ('ntdll.so', 'wineserver', 'win32u.so'):
            dependency = self.system / name
            dependency.write_text('system fixture: ' + name)
            self.dependencies.append(str(dependency))
        self.manifest = self.root / 'runtime.sha256'
        rows = [checksum(self.unix / name) + '  lib/wine/x86_64-unix/' + name
                for name in vprot.BINARIES]
        rows += [checksum(Path(filename)) + '  ' + filename for filename in self.dependencies]
        self.manifest.write_text('\n'.join(rows) + '\n')


class InstallerTests(RuntimeFixture):
    def setUp(self):
        super().setUp()
        self.output = self.root / 'installed'
        self.pe = self.root / 'pe'
        for name in ('x86_64-windows', 'i386-windows'):
            (self.pe / name).mkdir(parents=True)
        self.share = self.root / 'share'
        self.share.mkdir()
        for name, value in [('MANIFEST', self.manifest), ('SYSTEM_FILES', tuple(self.dependencies)),
                            ('UNIX', self.system), ('PE', self.pe), ('SHARE', self.share)]:
            context = patch.object(vprot, name, value)
            context.start()
            self.addCleanup(context.stop)
        context = patch.object(vprot.subprocess, 'check_output', return_value='wine 11.19-1\n')
        self.package = context.start()
        self.addCleanup(context.stop)

    def test_copies_loader_and_preloader_but_links_system_modules(self):
        vprot.install(self.runtime, self.output)
        unix = self.output / 'lib/wine/x86_64-unix'
        for name in vprot.BINARIES:
            self.assertFalse((unix / name).is_symlink())
            self.assertEqual((unix / name).read_bytes(), (self.unix / name).read_bytes())
        self.assertEqual((unix / 'win32u.so').resolve(), self.system / 'win32u.so')
        self.assertEqual((unix.parent / 'i386-windows').resolve(), self.pe / 'i386-windows')
        vprot.verify(self.output, self.output / 'runtime.sha256')

    def test_existing_destination_and_dangling_symlink_are_not_overwritten(self):
        self.output.symlink_to(self.root / 'missing')
        with self.assertRaisesRegex(ValueError, 'already exists'):
            vprot.install(self.runtime, self.output)
        self.assertTrue(self.output.is_symlink())
        self.package.assert_not_called()

    def test_unknown_version_or_hash_is_refused(self):
        self.package.return_value = 'wine 11.20-1\n'
        with self.assertRaisesRegex(ValueError, '11.19-1'):
            vprot.install(self.runtime, self.output)
        self.package.return_value = 'wine 11.19-1\n'
        (self.unix / 'ntdll.so').write_text('different')
        with self.assertRaisesRegex(ValueError, 'Hash mismatch'):
            vprot.install(self.runtime, self.output)
        self.assertFalse(self.output.exists())

    def test_changed_system_dependency_is_refused(self):
        Path(self.dependencies[1]).write_text('upgraded server')
        with self.assertRaisesRegex(ValueError, 'Hash mismatch'):
            vprot.install(self.runtime, self.output)
        self.assertFalse(self.output.exists())

    def test_symlinked_preloader_is_refused_even_with_matching_bytes(self):
        preloader = self.unix / 'wine-preloader'
        copied = self.root / 'preloader'
        preloader.rename(copied)
        preloader.symlink_to(copied)
        with self.assertRaisesRegex(ValueError, 'copied regular binary'):
            vprot.install(self.runtime, self.output)

    def test_incomplete_manifest_is_refused(self):
        self.manifest.write_text(self.manifest.read_text().splitlines()[0] + '\n')
        with self.assertRaisesRegex(ValueError, 'Incomplete'):
            vprot.install(self.runtime, self.output)

    def test_copy_failure_removes_only_new_output(self):
        with patch.object(vprot.shutil, 'copyfile', side_effect=OSError('simulated copy failure')):
            with self.assertRaisesRegex(OSError, 'copy failure'):
                vprot.install(self.runtime, self.output)
        self.assertFalse(self.output.exists())
        self.assertTrue(self.runtime.is_dir())


class LauncherRuntimeTests(RuntimeFixture):
    def setUp(self):
        super().setUp()
        self.prefix = self.root / 'prefix with spaces'
        self.scripts = self.prefix / 'wecom-compat/scripts'
        self.scripts.mkdir(parents=True)
        self.configs = self.scripts.parent / 'configs'
        self.configs.mkdir()
        shutil.copyfile(self.manifest, self.configs / 'wine-vprot-runtime.sha256')
        (self.configs / 'dll-overrides.txt').write_text('winemenubuilder.exe=d')
        (self.prefix / 'system.reg').touch()
        app = self.prefix / 'drive_c/Program Files (x86)/WXWork/WXWork.exe'
        app.parent.mkdir(parents=True)
        app.touch()
        shutil.copyfile(ROOT / 'scripts/launch-wecom.sh', self.scripts / 'launch-wecom.sh')
        self.bin = self.root / 'bin'
        self.bin.mkdir()
        (self.bin / 'pacman').write_text('#!/bin/bash\nprintf "%s\\n" "${MOCK_WINE_PACKAGE:-wine 11.19-1}"\n')
        (self.bin / 'env').write_text('#!' + sys.executable + '\nimport json, sys\nprint(json.dumps(sys.argv[1:]))\n')
        for filename in self.bin.iterdir():
            filename.chmod(0o755)

    def launch(self, **settings):
        environment = dict(os.environ, WECOM_PREFIX=str(self.prefix), WECOM_VPROT_RUNTIME=str(self.runtime),
                           WECOM_VPROT_FIX='1', WECOM_TITLEBAR_FIX='0', WECOM_TRAY_ICON_FIX='0', WECOM_IME_FIX='0',
                           PATH=str(self.bin) + os.pathsep + os.environ['PATH'])
        environment.update(settings)
        result = subprocess.run(['/bin/bash', str(self.scripts / 'launch-wecom.sh')],
                                env=environment, capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout), result.stderr

    def test_candidate_and_titlebar_use_same_runtime(self):
        (self.scripts / 'titlebar-managed.exe').touch()
        arguments, warning = self.launch(WECOM_TITLEBAR_FIX='1')
        loader = str(self.unix / 'wine')
        self.assertIn(loader, arguments)
        self.assertIn('WINESERVER=/usr/bin/wineserver', arguments)
        self.assertIn('WINEDLLPATH=/usr/lib/wine', arguments)
        self.assertIn('WINELOADERNOEXEC', arguments)
        self.assertEqual(warning, '')
        log = self.prefix / 'wecom-titlebar.log'
        for attempt in range(100):
            if log.exists() and log.read_text().strip():
                break
            time.sleep(0.01)
        self.assertIn(loader, json.loads(log.read_text()))

    def test_disabled_uses_system_wine(self):
        arguments, warning = self.launch(WECOM_VPROT_FIX='0')
        self.assertIn('/usr/bin/wine', arguments)
        self.assertNotIn('WINEDLLPATH=/usr/lib/wine', arguments)
        self.assertEqual(warning, '')

    def test_default_missing_runtime_is_silent(self):
        arguments, warning = self.launch(WECOM_VPROT_RUNTIME='', XDG_DATA_HOME=str(self.root / 'empty'))
        self.assertIn('/usr/bin/wine', arguments)
        self.assertEqual(warning, '')

    def test_custom_missing_or_relative_runtime_warns_and_falls_back(self):
        for directory in (str(self.root / 'absent'), 'relative-runtime'):
            with self.subTest(directory=directory):
                arguments, warning = self.launch(WECOM_VPROT_RUNTIME=directory)
                self.assertIn('/usr/bin/wine', arguments)
                self.assertIn('using system Wine', warning)

    def test_default_location_uses_xdg_data_home(self):
        data = self.root / 'xdg data'
        target = data / 'wecom-runtimes/wine-11.19-vprot-e38a364b'
        target.parent.mkdir(parents=True)
        shutil.copytree(self.runtime, target)
        arguments, _ = self.launch(WECOM_VPROT_RUNTIME='', XDG_DATA_HOME=str(data))
        self.assertIn(str(target / 'lib/wine/x86_64-unix/wine'), arguments)

    def test_changed_package_or_missing_manifest_falls_back(self):
        arguments, warning = self.launch(MOCK_WINE_PACKAGE='wine 11.20-1')
        self.assertIn('/usr/bin/wine', arguments)
        self.assertIn('using system Wine', warning)
        (self.configs / 'wine-vprot-runtime.sha256').unlink()
        arguments, warning = self.launch()
        self.assertIn('/usr/bin/wine', arguments)
        self.assertIn('using system Wine', warning)

    def test_changed_runtime_or_system_hash_falls_back(self):
        for filename in (self.unix / 'ntdll.so', Path(self.dependencies[1])):
            original = filename.read_bytes()
            filename.write_bytes(b'changed')
            arguments, warning = self.launch()
            self.assertIn('/usr/bin/wine', arguments)
            self.assertIn('using system Wine', warning)
            filename.write_bytes(original)

    def test_symlinked_preloader_falls_back(self):
        preloader = self.unix / 'wine-preloader'
        original = self.root / 'original-preloader'
        preloader.rename(original)
        preloader.symlink_to(original)
        arguments, warning = self.launch()
        self.assertIn('/usr/bin/wine', arguments)
        self.assertIn('using system Wine', warning)


class PatchHelperTests(unittest.TestCase):
    def test_exact_patch_helper_preserves_neighbor_bytes_and_failure_fallback(self):
        source = (ROOT / 'experiments/wine-vprot/wine-11.19.patch').read_text()
        additions = '\n'.join(line[1:] for line in source.splitlines()
                              if line.startswith('+') and not line.startswith('+++')) + '\n'
        helper = re.search(r'^static void set_page_vprot_range\(.*?^}\n', additions, re.M | re.S).group()
        self.assertEqual(hashlib.sha256(helper.encode()).hexdigest(),
                         '0b2fa2a16d814ec6babd4c62c4d32b7a1930fbfa9da08b2c1a7aedc346172dc8')
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            (temporary / 'vprot-range.inc').write_text(helper)
            binary = temporary / 'range-unit'
            subprocess.run(['gcc', '-Wall', '-Wextra', '-Werror', '-O2', '-I' + directory,
                            str(ROOT / 'experiments/wine-vprot/range-unit.c'), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.stdout.strip(),
                             'PASS cases=221184 madvise_success=16387 injected_failures=16387')


if __name__ == '__main__':
    unittest.main()
