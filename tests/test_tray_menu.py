"""Host-state guards and real inotify transport for minimized-owner menus."""
import importlib.util
import os
from pathlib import Path
import struct
import tempfile
import time
import unittest
from unittest.mock import Mock, patch

spec = importlib.util.spec_from_file_location('tray', Path(__file__).resolve().parents[1] / 'scripts/tray-icon.py')
tray = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tray)


class NativeMenuTests(unittest.TestCase):
    def setUp(self):
        self.x = object.__new__(tray.X11)
        self.x.x = Mock()
        self.x.display = None
        self.x.root = 1
        def geometry(display, window, root, x, y, width, height, border, depth):
            width._obj.value=7296; height._obj.value=2160; return 1
        self.x.x.XGetGeometry.side_effect = geometry
        self.props = {(w, 'WM_CLASS'): b'wxwork.exe\0wxwork.exe\0' for w in (100, 200)}
        self.props.update({(w, 'WM_STATE'): [3, 0] for w in (100, 200)})
        self.props.update({(w, '_NET_WM_PID'): [5739] for w in (100, 200)})
        self.props[200, 'WM_TRANSIENT_FOR'] = [100]
        self.x.property = lambda w, name: self.props.get((w, name))

    def restore(self, matches=True):
        with patch.object(tray, 'process_matches_prefix', return_value=matches):
            return self.x.restore_menu(100, 200, Path('/prefix'), (6500, 1900, 116, 97))

    def test_only_menu_is_mapped_with_captured_geometry_and_without_activation(self):
        self.assertTrue(self.restore())
        self.x.x.XMapWindow.assert_called_once_with(None, 200)
        self.x.x.XMoveResizeWindow.assert_called_once_with(None, 200, 6500, 1900, 116, 97)

    def test_wrong_identity_owner_or_state_is_rejected(self):
        for key, value in [((100, 'WM_CLASS'), b'other'), ((200, 'WM_CLASS'), b'other'),
                           ((200, 'WM_TRANSIENT_FOR'), [99]), ((100, 'WM_STATE'), [1, 0]),
                           ((200, '_NET_WM_PID'), [99]), ((100, '_NET_WM_PID'), None)]:
            with self.subTest(key=key, value=value):
                old = self.props[key]
                self.props[key] = value
                self.assertFalse(self.restore())
                self.x.x.XMapWindow.assert_not_called()
                self.props[key] = old

    def test_unsettled_host_state_waits_without_mapping(self):
        for state in (None, [0, 0], [1, 0]):
            self.props[200, 'WM_STATE'] = state
            self.assertIsNone(self.restore())
            self.x.x.XMapWindow.assert_not_called()
        self.props[200, 'WM_STATE'] = [3, 0]
        self.assertTrue(self.restore())

    def test_invalid_rectangle_is_rejected(self):
        for rect in ((-1, 0, 116, 97), (7200, 100, 116, 97), (100, 100, 0, 97), (100, 100, 116, 900)):
            with patch.object(tray, 'process_matches_prefix', return_value=True):
                self.assertFalse(self.x.restore_menu(100, 200, Path('/prefix'), rect))
        self.x.x.XMapWindow.assert_not_called()

    def test_other_prefix_is_rejected(self):
        self.assertFalse(self.restore(False))
        self.x.x.XMapWindow.assert_not_called()


class TransportTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.prefix = Path(self.tmp.name)
        (self.prefix / 'drive_c/windows/temp').mkdir(parents=True)
        self.receiver = tray.MenuRequests(self.prefix)
        self.addCleanup(self.receiver.close)
        self.native = Mock()
        self.payload = struct.pack('<8I', 0x57434d32, 100, 200, 42, 6500, 1900, 116, 97)

    def test_closed_write_dispatched_once_and_idle_does_not_replay(self):
        self.receiver.path.write_bytes(self.payload)
        self.receiver.dispatch(self.native, self.prefix)
        self.receiver.dispatch(self.native, self.prefix)
        self.native.restore_menu.assert_called_once_with(100, 200, self.prefix, (6500, 1900, 116, 97))

    def test_incomplete_write_is_not_dispatched_before_close(self):
        with self.receiver.path.open('wb') as f:
            f.write(self.payload[:8]); f.flush()
            self.receiver.dispatch(self.native, self.prefix)
            self.native.restore_menu.assert_not_called()
            f.write(self.payload[8:])
        self.receiver.dispatch(self.native, self.prefix)
        self.native.restore_menu.assert_called_once()

    def test_invalid_partial_stale_and_self_requests_are_rejected(self):
        for data in [b'bad', self.payload[:8], self.payload + b'x', struct.pack('<8I', 0,100,200,42,6500,1900,116,97),
                     struct.pack('<8I',0x57434d32,100,100,42,6500,1900,116,97), struct.pack('<8I',0x57434d32,0,200,42,6500,1900,116,97)]:
            self.receiver.path.write_bytes(data)
            self.receiver.dispatch(self.native, self.prefix)
        self.receiver.path.write_bytes(self.payload)
        os.utime(self.receiver.path, (time.time()-10, time.time()-10))
        self.receiver.dispatch(self.native, self.prefix)
        self.native.restore_menu.assert_not_called()

    def test_unrelated_files_are_ignored(self):
        self.receiver.path.with_name('unrelated').write_bytes(self.payload)
        self.receiver.dispatch(self.native, self.prefix)
        self.native.restore_menu.assert_not_called()

    def test_pending_request_retries_until_host_is_ready_without_a_new_write(self):
        self.native.restore_menu.side_effect = [None, True]
        self.receiver.path.write_bytes(self.payload)
        self.receiver.dispatch(self.native, self.prefix)
        self.assertIsNotNone(self.receiver.pending)
        self.receiver.dispatch(self.native, self.prefix)
        self.assertIsNone(self.receiver.pending)
        self.assertEqual(self.native.restore_menu.call_count, 2)

    def test_pending_request_expires_and_is_not_replayed(self):
        self.native.restore_menu.return_value = None
        self.receiver.path.write_bytes(self.payload)
        with patch.object(tray.time, 'monotonic', return_value=10):
            self.receiver.dispatch(self.native, self.prefix)
        with patch.object(tray.time, 'monotonic', return_value=11):
            self.receiver.dispatch(self.native, self.prefix)
        self.assertIsNone(self.receiver.pending)
        self.native.restore_menu.assert_called_once()
