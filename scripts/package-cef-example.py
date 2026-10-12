#!/usr/bin/env python3
"""Package only the verified CEF manifest files, without prefix or account data."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cef-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='New .tar.xz file')
    parser.add_argument('--cef-profile', choices=('full', 'winetricks', 'wine-builtin'), default='full',
                        help='Package full (89), winetricks (78), or wine-builtin (30) CEF files; configure the matching runtimes separately')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Output already exists; choose a new filename')
    manifest = json.loads((ROOT / 'materials/cef64-vendor-manifest.json').read_text())
    versions = json.loads((ROOT / 'materials/versions.json').read_text())
    excluded = set()
    if args.cef_profile != 'full':
        profile = json.loads((ROOT / f'materials/cef64-{args.cef_profile}-profile.json').read_text())
        if profile['component'] != versions['cef']['component']:
            parser.error('Selected profile does not match the CEF version')
        excluded = set(profile['excluded_files'])
    packaged = 0
    omitted = set()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    # Fixed metadata avoids leaking local usernames, timestamps, or permissions.
    # Read each file once and verify the bytes actually added to the archive.
    with args.output.open('xb') as output:
        try:
            with tarfile.open(fileobj=output, mode='w:xz', preset=3) as archive:
                for name, metadata in sorted(manifest['file_group'].items()):
                    version, component, relative = name.replace('\\', '/').split('/', 2)
                    if (version != versions['cef']['component'] or component != 'cef'
                            or Path(relative).is_absolute() or '..' in Path(relative).parts):
                        raise ValueError(f'Unexpected manifest path: {name}')
                    if relative in excluded:
                        omitted.add(relative)
                        continue
                    source = args.cef_dir / relative
                    if source.is_symlink() or not source.is_file():
                        raise ValueError(f'Expected a regular CEF file: {relative}')
                    data = source.read_bytes()
                    if hashlib.md5(data).hexdigest() != metadata['md5']:
                        raise ValueError(f'CEF changed while packaging: {relative}')
                    info = tarfile.TarInfo('assets/cef/' + relative)
                    info.size = len(data)
                    info.mode = 0o644
                    archive.addfile(info, io.BytesIO(data))
                    packaged += 1
                if omitted != excluded:
                    raise ValueError('Profile exclusions are not present in the vendor manifest')
        except BaseException:
            args.output.unlink()
            raise
    with args.output.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    print(f'{digest}  {args.output.name}')
    print(f'{packaged} files; {args.output.stat().st_size} bytes; profile={args.cef_profile}')


if __name__ == '__main__':
    main()
