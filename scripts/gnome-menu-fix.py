#!/usr/bin/env python3
"""Repair WeCom menu rendering on GNOME; WECOM_PREFIX selects the Wine prefix."""
import argparse
import ctypes as C
import fcntl
import locale
import os
from pathlib import Path
import select
import signal
import time


class Attributes(C.Structure):
    _fields_ = [(name, C.c_int) for name in ('x', 'y', 'width', 'height', 'border', 'depth')] + [
        ('visual', C.c_void_p), ('root', C.c_ulong), ('window_class', C.c_int), ('bit_gravity', C.c_int),
        ('win_gravity', C.c_int), ('backing_store', C.c_int), ('backing_planes', C.c_ulong),
        ('backing_pixel', C.c_ulong), ('save_under', C.c_int), ('colormap', C.c_ulong),
        ('map_installed', C.c_int), ('map_state', C.c_int), ('all_event_masks', C.c_long),
        ('your_event_mask', C.c_long), ('do_not_propagate_mask', C.c_long),
        ('override_redirect', C.c_int), ('screen', C.c_void_p)]


class WindowEvent(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int),
                ('display', C.c_void_p), ('event', C.c_ulong), ('window', C.c_ulong)]


class Event(C.Union):
    _fields_ = [('window', WindowEvent), ('pad', C.c_long * 24)]


class Error(C.Structure):
    _fields_ = [('type', C.c_int), ('display', C.c_void_p), ('resource', C.c_ulong),
                ('serial', C.c_ulong), ('code', C.c_ubyte), ('request', C.c_ubyte), ('minor', C.c_ubyte)]


class TextProperty(C.Structure):
    _fields_ = [('value', C.c_void_p), ('encoding', C.c_ulong), ('format', C.c_int), ('count', C.c_ulong)]


def process_stamp(pid, prefix):
    try:
        process = Path('/proc') / str(pid)
        if process.stat().st_uid != os.getuid():
            return None
        command = (process / 'cmdline').read_bytes().split(b'\0', 1)[0]
        if command.replace(b'\\', b'/').rsplit(b'/', 1)[-1].lower() != b'wxwork.exe':
            return None
        values = [entry[11:] for entry in (process / 'environ').read_bytes().split(b'\0')
                  if entry.startswith(b'WINEPREFIX=')]
        if len(values) != 1 or not values[0] or Path(os.fsdecode(values[0])).resolve() != prefix:
            return None
        return (process / 'stat').read_text().rsplit(')', 1)[1].split()[19]
    except (OSError, IndexError):
        return None


class MenuFix:
    def __init__(self, prefix):
        self.prefix = prefix
        self.x = C.CDLL('libX11.so.6')
        p, u, i = C.c_void_p, C.c_ulong, C.c_int
        signatures = {
            'XOpenDisplay': ([C.c_char_p], p), 'XCloseDisplay': ([p], i),
            'XDefaultRootWindow': ([p], u), 'XConnectionNumber': ([p], i),
            'XInternAtom': ([p, C.c_char_p, i], u), 'XFree': ([p], i),
            'XSelectInput': ([p, u, C.c_long], i), 'XPending': ([p], i),
            'XNextEvent': ([p, C.POINTER(Event)], i), 'XSync': ([p, i], i),
            'XUnmapWindow': ([p, u], i), 'XMapRaised': ([p, u], i),
            'XGetWindowAttributes': ([p, u, C.POINTER(Attributes)], i),
            'XQueryTree': ([p, u, C.POINTER(u), C.POINTER(u),
                            C.POINTER(C.POINTER(u)), C.POINTER(C.c_uint)], i),
            'XGetWindowProperty': ([p, u, u, C.c_long, C.c_long, i, u, C.POINTER(u), C.POINTER(i),
                                   C.POINTER(u), C.POINTER(u), C.POINTER(p)], i),
            'XGetWMName': ([p, u, C.POINTER(TextProperty)], i),
            'Xutf8TextPropertyToTextList': ([p, C.POINTER(TextProperty),
                                           C.POINTER(C.POINTER(C.c_char_p)), C.POINTER(i)], i),
            'XFreeStringList': ([C.POINTER(C.c_char_p)], None),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.x, name)
            function.argtypes, function.restype = args, result
        self.errors = []
        callback = C.CFUNCTYPE(i, p, C.POINTER(Error))
        self.error_handler = callback(self.xerror)
        self.x.XSetErrorHandler.argtypes = [callback]
        self.x.XSetErrorHandler.restype = p
        self.x.XSetErrorHandler(self.error_handler)
        self.display = self.x.XOpenDisplay(None)
        if not self.display:
            raise OSError('X display unavailable')
        self.root = self.x.XDefaultRootWindow(self.display)
        self.fd = self.x.XConnectionNumber(self.display)
        self.atoms, self.pending, self.processed = {}, {}, {}
        self.process = None
        self.running = True
        self.x.XSelectInput(self.display, self.root, 1 << 19)  # SubstructureNotifyMask, not redirect.
        self.x.XSync(self.display, 0)

    def xerror(self, display, event):
        error = event.contents
        self.errors.append((error.code, error.resource, error.request))
        del self.errors[:-64]
        if error.code != 3:  # BadWindow is expected when a popup closes between requests.
            print('X11 error code=%d window=%#x request=%d' % self.errors[-1], flush=True)
        return 0

    def atom(self, name):
        if name not in self.atoms:
            self.atoms[name] = self.x.XInternAtom(self.display, name.encode(), 0)
        return self.atoms[name]

    def property(self, window, name):
        actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
        fmt, data = C.c_int(), C.c_void_p()
        status = self.x.XGetWindowProperty(self.display, window, self.atom(name), 0, 256, 0, 0,
            C.byref(actual), C.byref(fmt), C.byref(count), C.byref(remaining), C.byref(data))
        try:
            if status or not data.value or remaining.value:
                return None
            if fmt.value == 8:
                return C.string_at(data, count.value)
            if fmt.value == 32:
                return list(C.cast(data, C.POINTER(C.c_ulong))[:count.value])
        finally:
            if data.value:
                self.x.XFree(data)
        return None

    def title(self, window):
        prop, count, names = TextProperty(), C.c_int(), C.POINTER(C.c_char_p)()
        try:
            if not self.x.XGetWMName(self.display, window, C.byref(prop)) or not prop.value:
                return None
            status = self.x.Xutf8TextPropertyToTextList(self.display, C.byref(prop),
                                                       C.byref(names), C.byref(count))
            if status == 0 and count.value == 1:
                return names[0].decode('utf-8', errors='strict')
        finally:
            if names:
                self.x.XFreeStringList(names)
            if prop.value:
                self.x.XFree(prop.value)
        return None

    def attributes(self, window):
        attrs = Attributes()
        return attrs if self.x.XGetWindowAttributes(self.display, window, C.byref(attrs)) else None

    def children(self):
        root, parent, count, children = C.c_ulong(), C.c_ulong(), C.c_uint(), C.POINTER(C.c_ulong)()
        if not self.x.XQueryTree(self.display, self.root, C.byref(root), C.byref(parent),
                                 C.byref(children), C.byref(count)):
            return []
        try:
            return list(children[:count.value]) if children else []
        finally:
            if children:
                self.x.XFree(children)

    def identity(self, window):
        if self.property(window, 'WM_CLASS') != b'wxwork.exe\0wxwork.exe\0':
            return None
        pid = self.property(window, '_NET_WM_PID')
        return pid[0] if isinstance(pid, list) and len(pid) == 1 and pid[0] else None

    def is_main(self, window):
        return (not self.property(window, 'WM_TRANSIENT_FOR') and self.title(window) == '企业微信' and
                self.property(window, '_NET_WM_WINDOW_TYPE') == [self.atom('_NET_WM_WINDOW_TYPE_NORMAL')])

    def target(self, window):
        pid = self.identity(window)
        if not pid or self.property(window, 'WM_NAME') != b'menu':
            return None
        stamp = process_stamp(pid, self.prefix)
        if stamp is None or (self.process and self.process != (pid, stamp)):
            return None
        ancestor, seen = window, set()
        for _ in range(16):
            if ancestor in seen or self.identity(ancestor) != pid:
                return None
            seen.add(ancestor)
            if ancestor != window and self.is_main(ancestor):
                self.process = (pid, stamp)
                return pid, ancestor, stamp
            if self.property(ancestor, 'WM_NAME') != b'menu':
                return None
            owner = self.property(ancestor, 'WM_TRANSIENT_FOR')
            if not isinstance(owner, list) or len(owner) != 1 or not owner[0]:
                return None
            ancestor = owner[0]
        return None

    def hide_shadow(self, window, menu, target):
        attrs = self.attributes(window)
        if (not attrs or attrs.map_state != 2 or attrs.depth != 32 or self.identity(window) != target[0] or
                self.property(window, 'WM_TRANSIENT_FOR') != [menu] or self.target(menu) != target):
            return
        self.x.XUnmapWindow(self.display, window)
        self.x.XSync(self.display, 0)
        after = self.attributes(window)
        print('menu shadow unmap window=%#x owner=%#x before=2 after=%s' %
              (window, menu, after.map_state if after else 'gone'), flush=True)

    def consider(self, window):
        if not self.process:
            pid = self.identity(window)
            stamp = process_stamp(pid, self.prefix) if pid and self.is_main(window) else None
            if stamp is not None:
                self.process = (pid, stamp)
        owner = self.property(window, 'WM_TRANSIENT_FOR')
        if isinstance(owner, list) and len(owner) == 1 and owner[0] in self.processed:
            self.hide_shadow(window, owner[0], self.processed[owner[0]])
        if window in self.processed or window in self.pending:
            return
        target = self.target(window)
        attrs = self.attributes(window) if target else None
        # Wine may map a 1x1 placeholder before its first real ConfigureNotify.
        if attrs and attrs.map_state == 2 and attrs.width > 1 and attrs.height > 1:
            self.pending[window] = (time.monotonic() + .05, 'unmap', target)

    def advance(self):
        for window, (due, stage, target) in list(self.pending.items()):
            if time.monotonic() < due:
                continue
            del self.pending[window]
            self.errors.clear()
            attrs = self.attributes(window)
            if not attrs or self.target(window) != target:
                continue
            if stage == 'unmap':
                if attrs.map_state != 2 or attrs.width <= 1 or attrs.height <= 1:
                    continue
                print('menu remap before window=%#x pid=%d owner=%#x rect=%s map=%d' %
                      (window, target[0], target[1], (attrs.x, attrs.y, attrs.width, attrs.height),
                       attrs.map_state), flush=True)
                self.processed[window] = target  # Ignore our own MapNotify; reset only on DestroyNotify.
                for shadow in self.children():
                    self.hide_shadow(shadow, window, target)
                self.x.XUnmapWindow(self.display, window)
                self.x.XSync(self.display, 0)
                self.pending[window] = (time.monotonic() + .1, 'map', target)
            else:
                self.x.XMapRaised(self.display, window)
                self.x.XSync(self.display, 0)
                after = self.attributes(window)
                print('menu remap after window=%#x rect=%s map=%s errors=%s' %
                      (window, (after.x, after.y, after.width, after.height) if after else 'gone',
                       after.map_state if after else 'gone', self.errors), flush=True)

    def run(self):
        missing_since = time.monotonic()
        next_scan = missing_since + 1
        clients = self.property(self.root, '_NET_CLIENT_LIST') or []
        for window in set(self.children() + clients):
            self.consider(window)
        print('GNOME menu watcher ready prefix=%s' % self.prefix, flush=True)
        while self.running:
            now = time.monotonic()
            if self.process and process_stamp(self.process[0], self.prefix) != self.process[1]:
                print('WeCom process exited; waiting up to 120 seconds for its replacement', flush=True)
                self.process = None
                self.pending.clear()
                self.processed.clear()
                missing_since = next_scan = now
            while self.x.XPending(self.display):
                event = Event()
                self.x.XNextEvent(self.display, C.byref(event))
                window, kind = event.window.window, event.window.type
                if kind == 17:  # DestroyNotify
                    self.pending.pop(window, None)
                    self.processed.pop(window, None)
                elif kind in (19, 22):  # MapNotify, ConfigureNotify
                    self.consider(window)
            if not self.process and now >= next_scan:
                # A replacement may have mapped before the previous process exited.
                clients = self.property(self.root, '_NET_CLIENT_LIST') or []
                for window in set(self.children() + clients):
                    self.consider(window)
                next_scan = now + 1
            self.advance()
            if not self.process and time.monotonic() - missing_since >= 120:
                print('WeCom main process not found within 120 seconds', flush=True)
                break
            timeout = min([1.0] + [max(0, value[0] - time.monotonic()) for value in self.pending.values()])
            if not self.process:
                timeout = min(timeout, max(0, min(next_scan, missing_since + 120) - time.monotonic()))
            if not self.x.XPending(self.display):
                select.select([self.fd], [], [], timeout)

    def stop(self, signum, frame):
        self.running = False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    if 'gnome' not in os.environ.get('XDG_CURRENT_DESKTOP', '').lower().split(':'):
        parser.error('this helper is limited to GNOME')
    value = os.environ.get('WECOM_PREFIX')
    if not value or not Path(value).is_dir():
        parser.error('WECOM_PREFIX must name the target Wine prefix')
    prefix = Path(value).resolve()
    lock = (prefix / '.wecom-gnome-menu-fix.lock').open('a')
    try:
        fcntl.flock(lock.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        lock.close()
        print('GNOME menu watcher already running for this prefix', flush=True)
        return
    locale.setlocale(locale.LC_CTYPE, '')
    helper = MenuFix(prefix)
    signal.signal(signal.SIGTERM, helper.stop)
    signal.signal(signal.SIGINT, helper.stop)
    try:
        helper.run()
    finally:
        # A launcher stop during the 100 ms gap must not strand a live menu unmapped.
        helper.pending = {window: (0, stage, target)
                          for window, (_, stage, target) in helper.pending.items() if stage == 'map'}
        helper.advance()
        helper.x.XCloseDisplay(helper.display)
        lock.close()


if __name__ == '__main__':
    main()
