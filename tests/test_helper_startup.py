"""Deterministic helper lifecycle checks; no Wine process or X display needed."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('tray_icon', ROOT / 'scripts/tray-icon.py')
tray = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(tray)


class Clock:
    def __init__(self):
        self.now = 0

    def monotonic(self):
        return self.now

    def select(self, readers, writers, errors, timeout):
        self.now += timeout
        if self.now > 400:
            raise AssertionError('helper did not stop after application exit')


class IdleX:
    @staticmethod
    def XPending(display):
        return 0


class TrayLifecycleTests(unittest.TestCase):
    def run_helper(self, alive, window=lambda now: 0):
        clock, scans, attached = Clock(), [], []
        helper = object.__new__(tray.X11)
        helper.x, helper.display, helper.fd = IdleX(), None, -1
        helper.target = lambda prefix: (scans.append(clock.now) or False, window(clock.now))
        helper.attach = attached.append
        with patch.object(tray.time, 'monotonic', clock.monotonic), \
                patch.object(tray.select, 'select', clock.select), \
                patch.object(tray, 'wecom_process_alive', lambda prefix: alive(clock.now)):
            helper.run(Path('/target-prefix'))
        return clock.now, scans, attached

    def test_process_alive_without_window_beyond_120_seconds(self):
        stopped, scans, attached = self.run_helper(lambda now: now < 180)
        self.assertEqual(stopped, 186)
        self.assertIn(178, scans)
        self.assertTrue(all(window == 0 for window in attached))

    def test_failed_launch_has_bounded_idle_wait(self):
        stopped, scans, _ = self.run_helper(lambda now: False)
        self.assertEqual(stopped, 120)
        self.assertEqual(scans[-1], 118)

    def test_hidden_or_missing_window_does_not_end_helper(self):
        stopped, _, attached = self.run_helper(lambda now: now < 200,
                                               lambda now: 42 if 10 <= now < 20 else 0)
        self.assertEqual(stopped, 206)
        self.assertIn(42, attached)
        self.assertEqual(attached[-1], 0)

    def test_startup_process_handoff_is_tolerated(self):
        stopped, scans, _ = self.run_helper(lambda now: (now < 10 or 14 <= now < 180))
        self.assertEqual(stopped, 186)
        self.assertIn(14, scans)

    def test_unknown_inspection_is_not_application_exit(self):
        def alive(now):
            return True if now < 10 else None if now < 20 else False
        stopped, _, _ = self.run_helper(alive)
        self.assertEqual(stopped, 26)


class ProcessFilteringTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.prefix = self.root / 'prefix'
        self.prefix.mkdir()
        self.proc = self.root / 'proc'
        self.proc.mkdir()

    def process(self, pid, comm='WXWork.exe', command=b'C:\\WXWork\\WXWork.exe', prefix=None):
        directory = self.proc / str(pid)
        directory.mkdir()
        (directory / 'comm').write_text(comm + '\n')
        (directory / 'cmdline').write_bytes(command + b'\0')
        value = str(prefix or self.prefix).encode()
        (directory / 'environ').write_bytes(b'WINEPREFIX=' + value + b'\0')
        return directory

    def test_exact_process_and_resolved_prefix(self):
        self.process(101, command=b'C:\\WXWork\\wxwork.EXE', prefix=self.prefix / '.')
        self.assertIs(tray.wecom_process_alive(self.prefix, self.proc), True)

    def test_renamed_main_thread_still_counts_and_unrelated_uithread_does_not(self):
        self.process(101, comm='UIThread', command=b'C:\\OtherApp.exe')
        self.assertIs(tray.wecom_process_alive(self.prefix, self.proc), False)
        self.process(102, comm='UIThread')
        self.assertIs(tray.wecom_process_alive(self.prefix, self.proc), True)

    def test_other_prefix_and_process_names_do_not_count(self):
        self.process(101, prefix=self.root / 'another-prefix')
        self.process(102, comm='WXWorkHelper.exe')
        self.process(103, command=b'C:\\WXWorkHelper.exe\0WXWork.exe')
        self.process(104, command=b'WXWork.exe.bak')
        self.assertIs(tray.wecom_process_alive(self.prefix, self.proc), False)

    def test_unrelated_process_environment_is_never_read(self):
        directory = self.process(101, comm='unrelated-app')
        (directory / 'environ').unlink()
        self.assertIs(tray.wecom_process_alive(self.prefix, self.proc), False)

    def test_duplicate_or_empty_prefix_does_not_match(self):
        directory = self.process(101)
        for environment in (b'WINEPREFIX=\0', b'WINEPREFIX=' + str(self.prefix).encode()
                            + b'\0WINEPREFIX=' + str(self.prefix).encode() + b'\0'):
            (directory / 'environ').write_bytes(environment)
            self.assertIs(tray.wecom_process_alive(self.prefix, self.proc), False)

    def test_permissions_are_uncertain_and_disappeared_pid_is_absent(self):
        directory = self.process(101)
        original = Path.read_bytes
        def read_bytes(path):
            if path == directory / 'environ':
                raise PermissionError('denied')
            return original(path)
        with patch.object(Path, 'read_bytes', read_bytes):
            self.assertIsNone(tray.wecom_process_alive(self.prefix, self.proc))
        (directory / 'environ').unlink()
        self.assertIs(tray.wecom_process_alive(self.prefix, self.proc), False)

    def test_other_users_are_skipped_before_environment_inspection(self):
        directory = self.process(101)
        original = Path.stat
        def stat(path, *args, **kwargs):
            if path == directory:
                return type('Info', (), {'st_uid': tray.os.getuid() + 1})()
            return original(path, *args, **kwargs)
        with patch.object(Path, 'stat', stat):
            self.assertIs(tray.wecom_process_alive(self.prefix, self.proc), False)

    def test_non_pid_directories_are_ignored(self):
        self.process('not-a-pid')
        self.assertIs(tray.wecom_process_alive(self.prefix, self.proc), False)


class TitlebarStartupTests(unittest.TestCase):
    def test_tray_menu_group_harness(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / 'tray-menu-group'
            subprocess.run(['gcc', '-std=c11', '-ffunction-sections', '-fdata-sections',
                            '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast', '-Wl,--gc-sections',
                            str(ROOT / 'tests/tray-menu-group.c'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True, capture_output=True, text=True)

    def test_tray_restore_harness(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / 'tray-restore'
            subprocess.run(['gcc', '-std=c11', '-ffunction-sections', '-fdata-sections',
                            '-Wno-pointer-to-int-cast', '-Wno-int-to-pointer-cast', '-Wl,--gc-sections',
                            str(ROOT / 'tests/tray-restore.c'), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
            self.assertEqual(result.stdout.strip(), 'tray restore: 12 scenarios passed')

    def test_restore_guard_harness(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / 'titlebar-restore'
            subprocess.run(['gcc', '-std=c11', '-ffunction-sections', '-fdata-sections',
                            '-Wno-pointer-to-int-cast', '-Wl,--gc-sections',
                            str(ROOT / 'tests/titlebar-restore.c'), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
            self.assertEqual(result.stdout.strip(), 'titlebar restore: 8 scenarios passed')

    def test_native_mock_harness(self):
        with tempfile.TemporaryDirectory() as temporary:
            binary = Path(temporary) / 'titlebar-startup'
            subprocess.run(['gcc', '-std=c11', '-ffunction-sections', '-fdata-sections',
                            '-Wno-pointer-to-int-cast', '-Wl,--gc-sections',
                            str(ROOT / 'tests/titlebar-startup.c'), '-o', str(binary)], check=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
            self.assertEqual(result.stdout.strip(), 'titlebar startup: 7 scenarios passed')


if __name__ == '__main__':
    unittest.main()
