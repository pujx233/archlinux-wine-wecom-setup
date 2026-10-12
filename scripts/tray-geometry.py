#!/usr/bin/env python3
"""Print guarded Wine tray correction parameters for KDE Wayland/XWayland."""
import ctypes as C
import json
import os
import subprocess
import sys


class Monitor(C.Structure):
    _fields_ = [("name", C.c_ulong), ("primary", C.c_int),
                ("automatic", C.c_int), ("noutput", C.c_int),
                ("x", C.c_int), ("y", C.c_int),
                ("width", C.c_int), ("height", C.c_int),
                ("mwidth", C.c_int), ("mheight", C.c_int),
                ("outputs", C.POINTER(C.c_ulong))]


def correction(xmonitors, outputs):
    """Map each output's native Qt coordinates into the XWayland desktop."""
    primary = [m for m in xmonitors if m["primary"]]
    if len(primary) != 1:
        return None
    p = primary[0]
    enabled = [o for o in outputs if o.get("enabled")]
    if len(enabled) != len(xmonitors) or len(xmonitors) > 16:
        return None
    mappings = []
    for monitor in xmonitors:
        matches = [o for o in enabled if o.get("name") == monitor["name"]]
        if len(matches) != 1:
            return None
        output = matches[0]
        source = (output["pos"]["x"] - p["x"], output["pos"]["y"] - p["y"],
                  output["size"]["width"], output["size"]["height"])
        target = (monitor["x"] - p["x"], monitor["y"] - p["y"],
                  monitor["width"], monitor["height"])
        if min(source[2:] + target[2:]) <= 0:
            return None
        mappings.append(source + target)
    if all(row[:4] == row[4:] for row in mappings):
        return None
    left, top = min(m["x"] for m in xmonitors), min(m["y"] for m in xmonitors)
    right = max(m["x"] + m["width"] for m in xmonitors)
    bottom = max(m["y"] + m["height"] for m in xmonitors)
    header = (2, left - p["x"], top - p["y"], right - left, bottom - top,
              p["width"], p["height"], len(xmonitors))
    return ";".join(",".join(map(str, row)) for row in [header] + mappings)


def x11_monitors():
    x, r = C.CDLL("libX11.so.6"), C.CDLL("libXrandr.so.2")
    x.XOpenDisplay.argtypes = [C.c_char_p]
    x.XOpenDisplay.restype = C.c_void_p
    x.XCloseDisplay.argtypes = [C.c_void_p]
    x.XDefaultRootWindow.argtypes = [C.c_void_p]
    x.XDefaultRootWindow.restype = C.c_ulong
    x.XGetAtomName.argtypes = [C.c_void_p, C.c_ulong]
    x.XGetAtomName.restype = C.c_void_p
    x.XFree.argtypes = [C.c_void_p]
    r.XRRGetMonitors.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.POINTER(C.c_int)]
    r.XRRGetMonitors.restype = C.POINTER(Monitor)
    r.XRRFreeMonitors.argtypes = [C.POINTER(Monitor)]
    display = x.XOpenDisplay(None)
    if not display:
        return []
    monitors = None
    try:
        count = C.c_int()
        monitors = r.XRRGetMonitors(display, x.XDefaultRootWindow(display), 1, C.byref(count))
        result = []
        for i in range(count.value if monitors else 0):
            m = monitors[i]
            name = x.XGetAtomName(display, m.name)
            if not name:
                continue
            try:
                item = {key: getattr(m, key) for key in
                        ("primary", "x", "y", "width", "height")}
                item["name"] = C.string_at(name).decode("utf-8")
                result.append(item)
            finally:
                x.XFree(name)
        return result
    finally:
        if monitors:
            r.XRRFreeMonitors(monitors)
        x.XCloseDisplay(display)


def main():
    if (os.environ.get("XDG_SESSION_TYPE") != "wayland" or
            "KDE" not in os.environ.get("XDG_CURRENT_DESKTOP", "").split(":") or
            os.environ.get("WECOM_TRAY_FIX") == "0"):
        return
    try:
        result = subprocess.run(["kscreen-doctor", "-j"], capture_output=True,
                                text=True, timeout=3, check=True)
        parameters = correction(x11_monitors(), json.loads(result.stdout)["outputs"])
        if parameters:
            print(parameters)
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as exc:
        print(f"Tray coordinate correction unavailable: {type(exc).__name__}", file=sys.stderr)


if __name__ == "__main__":
    main()
