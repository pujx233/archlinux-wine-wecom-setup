#!/usr/bin/env python3
"""Install the validated local Wine vprot artifact without modifying a prefix or system Wine."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'configs/wine-vprot-runtime.sha256'
UNIX = Path('/usr/lib/wine/x86_64-unix')
PE = Path('/usr/lib/wine')
SHARE = Path('/usr/share/wine')
BINARIES = ('ntdll.so', 'wine', 'wine-preloader')
SYSTEM_FILES = ('/usr/lib/wine/x86_64-unix/ntdll.so', '/usr/bin/wineserver',
                '/usr/lib/wine/x86_64-unix/win32u.so')


def verify(root, manifest):
    entries = {}
    for line in manifest.read_text().splitlines():
        expected, filename = line.split('  ', 1)
        if filename in entries or not re.fullmatch('[0-9a-f]{64}', expected):
            raise ValueError('Invalid runtime manifest')
        entries[filename] = expected
    required = {str(Path('lib/wine/x86_64-unix') / name) for name in BINARIES} | set(SYSTEM_FILES)
    if set(entries) != required:
        raise ValueError('Incomplete or unexpected runtime manifest')
    for filename, expected in entries.items():
        source = root / filename
        with source.open('rb') as stream:
            actual = hashlib.file_digest(stream, 'sha256').hexdigest()
        if actual != expected:
            raise ValueError('Hash mismatch: ' + filename)


def install(source, output):
    source = source.resolve(strict=True)
    output = output.absolute()
    if os.path.lexists(output):
        raise ValueError('Output already exists; choose a new directory')
    version = subprocess.check_output(['pacman', '-Q', 'wine'], text=True).strip()
    if version != 'wine 11.19-1':
        raise ValueError('Only the validated system Wine 11.19-1 is supported')
    verify(source, MANIFEST)
    for name in BINARIES:
        candidate = source / 'lib/wine/x86_64-unix' / name
        if candidate.is_symlink() or not candidate.is_file():
            raise ValueError('Expected a copied regular binary: ' + name)
    for dependency in (UNIX, PE / 'x86_64-windows', PE / 'i386-windows', SHARE):
        if not dependency.is_dir():
            raise ValueError('Missing system Wine directory: ' + str(dependency))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.mkdir(mode=0o700)
    try:
        unix = output / 'lib/wine/x86_64-unix'
        unix.mkdir(parents=True)
        for name in BINARIES:
            shutil.copyfile(source / 'lib/wine/x86_64-unix' / name, unix / name)
            (unix / name).chmod(0o755)
        for module in UNIX.iterdir():
            if module.name not in BINARIES:
                (unix / module.name).symlink_to(module)
        for architecture in ('x86_64-windows', 'i386-windows'):
            (unix.parent / architecture).symlink_to(PE / architecture, target_is_directory=True)
        (output / 'share').mkdir()
        (output / 'share/wine').symlink_to(SHARE, target_is_directory=True)
        verify(output, MANIFEST)
        shutil.copyfile(MANIFEST, output / 'runtime.sha256')
    except BaseException:
        shutil.rmtree(output)
        raise
    print('Installed verified runtime: ' + str(output))
    print('No prefix, launcher, profile or system Wine file was changed.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path,
                        help='Local validated runtime root containing lib/wine/x86_64-unix')
    parser.add_argument('--output', required=True, type=Path, help='New dedicated runtime directory')
    arguments = parser.parse_args()
    try:
        install(arguments.source, arguments.output)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
