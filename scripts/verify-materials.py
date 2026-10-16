#!/usr/bin/env python3
"""Read-only verification of the recorded WeCom installation materials."""
import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
versions = json.loads((ROOT / 'materials/versions.json').read_text())

def material_path(value):
    path = Path(value).expanduser()
    return path if path.is_absolute() else ROOT / path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--installer', type=Path, default=material_path(versions['installer']['path']))
parser.add_argument('--cef-dir', type=Path, default=material_path(versions['cef']['directory']))
parser.add_argument('--richedit', type=Path, default=material_path(versions['richedit']['path']))
parser.add_argument('--fonts-dir', type=Path, help='Flat directory with the selected font files; default: assets/fonts inside this repository')
parser.add_argument('--fonts-profile', choices=('snapshot', 'cn'), default='snapshot',
                    help='All historical fonts or only the four Adobe CN OTF files')
parser.add_argument('--only', choices=('installer', 'cef', 'richedit', 'fonts'),
                    help='Verify one material group; default: verify all groups')
parser.add_argument('--cef-profile', choices=('full', 'winetricks', 'wine-builtin'), default='full',
                    help='Expected CEF files: full=89, winetricks=78, wine-builtin=30; runtime configuration is checked separately')
parser.add_argument('--cef-layout', choices=('vendor', 'page-aligned'), default='vendor',
                    help='Vendor hashes by default; page-aligned accepts only the recorded experimental libcef.dll SHA256')
args = parser.parse_args()
errors = []

def check(path, expected, algorithm='sha256'):
    try:
        digest = hashlib.new(algorithm)
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
        if digest.hexdigest() != expected:
            errors.append(f'Hash mismatch: {path}')
    except OSError as exc:
        errors.append(f'{path}: {exc.strerror}')

def check_pe(path, expected):
    try:
        with path.open('rb') as stream:
            if stream.read(2) != b'MZ':
                raise ValueError('missing MZ header')
            stream.seek(0x3c)
            offset, = struct.unpack('<I', stream.read(4))
            stream.seek(offset)
            if stream.read(4) != b'PE\0\0':
                raise ValueError('missing PE signature')
            machine, = struct.unpack('<H', stream.read(2))
            if machine != expected:
                raise ValueError(f'PE architecture {machine:#x}, expected {expected:#x}')
    except (OSError, ValueError, struct.error) as exc:
        errors.append(f'{path}: {exc}')

checked = []
if args.only in (None, 'installer'):
    check(args.installer, versions['installer']['sha256'])
    checked.append('installer SHA256')
if args.only in (None, 'richedit'):
    check(args.richedit, versions['richedit']['sha256'])
    check_pe(args.richedit, 0x14c)
    checked.append('x86 RichEdit SHA256')
if args.only in (None, 'cef'):
    manifest = json.loads((ROOT / 'materials/cef64-vendor-manifest.json').read_text())
    alignment = None
    if args.cef_layout == 'page-aligned':
        alignment = json.loads((ROOT / 'materials/cef64-page-aligned.json').read_text())
        if alignment['component'] != versions['cef']['component']:
            raise SystemExit('Selected layout does not match the CEF version')
    excluded = set()
    if args.cef_profile != 'full':
        profile = json.loads((ROOT / f'materials/cef64-{args.cef_profile}-profile.json').read_text())
        if profile['component'] != versions['cef']['component']:
            raise SystemExit('Selected profile does not match the CEF version')
        excluded = set(profile['excluded_files'])
        for name in excluded:
            if (args.cef_dir / name).exists():
                errors.append(f'Unexpected app-local file in selected profile: {name}')
    count = 0
    omitted = set()
    for name, metadata in manifest['file_group'].items():
        version, component, relative = name.replace('\\', '/').split('/', 2)
        if (version != versions['cef']['component'] or component != 'cef'
                or Path(relative).is_absolute() or '..' in Path(relative).parts):
            raise SystemExit(f'Unexpected manifest path: {name}')
        if relative in excluded:
            omitted.add(relative)
            continue
        if alignment and relative == 'libcef.dll':
            check(args.cef_dir / relative, alignment['output_sha256'])
        else:
            check(args.cef_dir / relative, metadata['md5'], 'md5')
        count += 1
    if omitted != excluded:
        errors.append('Profile exclusions are not present in the vendor manifest')
    for name in ('WXWorkWeb.exe', 'libcef.dll'):
        check_pe(args.cef_dir / name, 0x8664)
    checked.append(f'{count} CEF files ({args.cef_profile}, {args.cef_layout}) and x64 PE headers')
if args.only in (None, 'fonts'):
    fonts = json.loads((ROOT / 'materials/fonts.json').read_text())
    if args.fonts_profile == 'cn':
        fonts = [font for font in fonts if font['filename'].endswith('.otf')]
        if len(fonts) != 4:
            raise SystemExit('Expected exactly four CN OTF fonts')
    for font in fonts:
        path = args.fonts_dir / font['filename'] if args.fonts_dir else material_path(font['source'])
        check(path, font['sha256'])
    checked.append(f'{len(fonts)} font SHA256 hashes')
if errors:
    print('\n'.join(errors))
    raise SystemExit(1)
print('PASS: ' + '; '.join(checked) + '.')
