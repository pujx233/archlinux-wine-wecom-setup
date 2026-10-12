#!/usr/bin/env python3
"""Replace WeCom's observed black tray blink frame with a blank placeholder."""
import argparse
import ctypes as C
import fcntl
import os
from pathlib import Path
import select
import time


def placeholder(pixels):
    """Match only the captured 16x16 black square in a 20x20 ARGB icon."""
    if len(pixels) != 400:
        return None
    for i, pixel in enumerate(pixels):
        expected = 0xff000000 if 2 <= i % 20 < 18 and 2 <= i // 20 < 18 else 0
        if pixel != expected:
            return None
    # KDE ignores completely transparent updates. Alpha 1/255 looks blank,
    # while keeping Wine's existing icon hit area and the SNI layout slot.
    return [0x01000000 if pixel else 0 for pixel in pixels]


class X11:
    def __init__(self):
        self.x = C.CDLL('libX11.so.6')
        self.damage = C.CDLL('libXdamage.so.1')
        p, u, i = C.c_void_p, C.c_ulong, C.c_int
        signatures = {
            'XOpenDisplay': ([C.c_char_p], p), 'XCloseDisplay': ([p], i),
            'XDefaultRootWindow': ([p], u), 'XConnectionNumber': ([p], i),
            'XInternAtom': ([p, C.c_char_p, i], u),
            'XGetWindowProperty': ([p, u, u, C.c_long, C.c_long, i, u,
                                   C.POINTER(u), C.POINTER(i), C.POINTER(u),
                                   C.POINTER(u), C.POINTER(p)], i),
            'XQueryTree': ([p, u, C.POINTER(u), C.POINTER(u),
                            C.POINTER(C.POINTER(u)), C.POINTER(C.c_uint)], i),
            'XGetGeometry': ([p, u, C.POINTER(u), C.POINTER(i), C.POINTER(i)] +
                             [C.POINTER(C.c_uint)] * 4, i),
            'XFree': ([p], i), 'XPending': ([p], i), 'XNextEvent': ([p, p], i),
            'XSync': ([p, i], i), 'XGetImage': ([p, u, i, i, C.c_uint, C.c_uint, u, i], p),
            'XGetPixel': ([p, i, i], u), 'XPutPixel': ([p, i, i, u], i),
            'XDestroyImage': ([p], i), 'XCreateGC': ([p, u, u, p], p),
            'XFreeGC': ([p, p], i),
            'XPutImage': ([p, u, p, p, i, i, i, i, C.c_uint, C.c_uint], i),
        }
        for name, (args, result) in signatures.items():
            function = getattr(self.x, name)
            function.argtypes, function.restype = args, result
        for name, args, result in (
            ('XDamageQueryExtension', [p, C.POINTER(i), C.POINTER(i)], i),
            ('XDamageCreate', [p, u, i], u),
            ('XDamageSubtract', [p, u, u, u], None),
            ('XDamageDestroy', [p, u], None),
        ):
            function = getattr(self.damage, name)
            function.argtypes, function.restype = args, result
        self.display = self.x.XOpenDisplay(None)
        if not self.display:
            raise OSError('X display unavailable')
        # Windows may disappear between discovery and a request.
        callback = C.CFUNCTYPE(i, p, p)
        self.error_handler = callback(lambda display, event: 0)
        self.x.XSetErrorHandler.argtypes = [callback]
        self.x.XSetErrorHandler(self.error_handler)
        self.root = self.x.XDefaultRootWindow(self.display)
        self.fd = self.x.XConnectionNumber(self.display)
        self.atoms = {}
        self.window = self.gc = self.watch = 0
        event, error = i(), i()
        if not self.damage.XDamageQueryExtension(self.display, C.byref(event), C.byref(error)):
            self.x.XCloseDisplay(self.display)
            raise OSError('XDamage unavailable')

    def property(self, window, name):
        if name not in self.atoms:
            self.atoms[name] = self.x.XInternAtom(self.display, name.encode(), 0)
        actual, count, remaining = C.c_ulong(), C.c_ulong(), C.c_ulong()
        fmt, data = C.c_int(), C.c_void_p()
        status = self.x.XGetWindowProperty(self.display, window, self.atoms[name], 0, 64,
            0, 0, C.byref(actual), C.byref(fmt), C.byref(count), C.byref(remaining), C.byref(data))
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

    def windows(self):
        queue, seen = [self.root], set()
        while queue and len(seen) < 4096:
            window = queue.pop()
            if window in seen:
                continue
            seen.add(window)
            root, parent, count = C.c_ulong(), C.c_ulong(), C.c_uint()
            children = C.POINTER(C.c_ulong)()
            if self.x.XQueryTree(self.display, window, C.byref(root), C.byref(parent),
                                 C.byref(children), C.byref(count)):
                if children:
                    queue.extend(children[:count.value])
                    self.x.XFree(children)
            yield window

    def icon_geometry(self, window):
        root, x, y = C.c_ulong(), C.c_int(), C.c_int()
        width, height, border, depth = (C.c_uint() for _ in range(4))
        ok = self.x.XGetGeometry(self.display, window, C.byref(root), C.byref(x), C.byref(y),
            C.byref(width), C.byref(height), C.byref(border), C.byref(depth))
        return bool(ok and (width.value, height.value, depth.value) == (20, 20, 32))

    def target(self, prefix):
        main_found, icons, processes = False, [], {}
        for window in self.windows():
            cls = self.property(window, 'WM_CLASS')
            if cls not in (b'wxwork.exe\0wxwork.exe\0', b'explorer.exe\0explorer.exe\0'):
                continue
            pid = self.property(window, '_NET_WM_PID')
            if not pid or len(pid) != 1:
                continue
            if pid[0] not in processes:
                try:
                    env = Path('/proc/%d/environ' % pid[0]).read_bytes().split(b'\0')
                    values = [os.fsdecode(e.split(b'=', 1)[1]) for e in env if e.startswith(b'WINEPREFIX=')]
                    processes[pid[0]] = len(values) == 1 and Path(values[0]).resolve() == prefix
                except OSError:
                    processes[pid[0]] = False
            if not processes[pid[0]]:
                continue
            if cls.startswith(b'wxwork.exe\0'):
                main_found = True
            elif self.property(window, '_XEMBED_INFO') == [0, 1] and self.icon_geometry(window):
                icons.append(window)
        # An ambiguous prefix with multiple embedded icons is left unchanged.
        return main_found, icons[0] if main_found and len(icons) == 1 else 0

    def attach(self, window):
        if window == self.window:
            return
        if self.watch:
            self.damage.XDamageDestroy(self.display, self.watch)
        if self.gc:
            self.x.XFreeGC(self.display, self.gc)
        self.window, self.watch, self.gc = window, 0, 0
        if window:
            self.gc = self.x.XCreateGC(self.display, window, 0, None)
            self.watch = self.damage.XDamageCreate(self.display, window, 3)
            print('Tray placeholder: watching X11 icon %#x' % window, flush=True)
        self.x.XSync(self.display, 0)

    def repaint(self):
        if not self.window or not self.gc:
            return False
        self.damage.XDamageSubtract(self.display, self.watch, 0, 0)
        image = self.x.XGetImage(self.display, self.window, 0, 0, 20, 20, 0xffffffff, 2)
        if not image:
            return False
        try:
            pixels = [self.x.XGetPixel(image, x, y) for y in range(20) for x in range(20)]
            replacement = placeholder(pixels)
            if replacement is None:
                return False
            for i, pixel in enumerate(replacement):
                self.x.XPutPixel(image, i % 20, i // 20, pixel)
            self.x.XPutImage(self.display, self.window, self.gc, image, 0, 0, 0, 0, 20, 20)
            self.x.XSync(self.display, 0)
            return True
        finally:
            self.x.XDestroyImage(image)

    def run(self, prefix):
        deadline, next_scan, absent, seen, count = time.monotonic() + 120, 0, 0, False, 0
        event = (C.c_long * 24)()  # XEvent includes a 24-long padding member.
        while True:
            now = time.monotonic()
            if now >= next_scan:
                found, window = self.target(prefix)
                seen = seen or found
                absent = 0 if found else absent + 1
                if (seen and absent >= 3) or (not seen and now >= deadline):
                    return
                self.attach(window)
                next_scan = now + 2
            dirty = False
            while self.x.XPending(self.display):
                self.x.XNextEvent(self.display, event)
                dirty = True
            if dirty and self.repaint():
                count += 1
                if count == 1:
                    print('Tray placeholder: black blink frame replaced', flush=True)
            if not self.x.XPending(self.display):
                select.select([self.fd], [], [], max(0, min(1, next_scan - time.monotonic())))

    def close(self):
        self.attach(0)
        self.x.XCloseDisplay(self.display)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', required=True, type=Path)
    args = parser.parse_args()
    if (os.environ.get('XDG_SESSION_TYPE') != 'wayland' or
            'KDE' not in os.environ.get('XDG_CURRENT_DESKTOP', '').split(':') or
            os.environ.get('WECOM_TRAY_ICON_FIX') == '0'):
        return
    connection = None
    try:
        prefix = args.prefix.resolve(strict=True)
        with (prefix / 'wecom-compat/.tray-icon.lock').open('a') as guard:
            try:
                fcntl.flock(guard, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return
            connection = X11()
            connection.run(prefix)
    except (OSError, KeyboardInterrupt) as error:
        print('Tray placeholder: stopped (%s)' % error, flush=True)
    finally:
        if connection:
            connection.close()


if __name__ == '__main__':
    main()
