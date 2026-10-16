#!/usr/bin/env python3
import argparse
import hashlib
import json
import pathlib
import struct
import sys


SOURCE_SHA256 = '305967f575bc139b13975d080f620103993b65a6bbef9672c07d3f036743c59f'
OUTPUT_SHA256 = '57d538f19f59cb2d319d753f0d33dec803c48b761fd92f478fd4fcf08647f3d4'
PAGE_SIZE = 4096


def align(value):
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)


def realign_bytes(original):
    pe_offset = struct.unpack_from('<I', original, 60)[0]
    if original[pe_offset:pe_offset + 4] != b'PE\0\0':
        raise ValueError('Not a PE image')
    count = struct.unpack_from('<H', original, pe_offset + 6)[0]
    optional_size = struct.unpack_from('<H', original, pe_offset + 20)[0]
    optional = pe_offset + 24
    magic = struct.unpack_from('<H', original, optional)[0]
    if magic not in (0x10b, 0x20b):
        raise ValueError('Unsupported optional header')
    section_alignment = struct.unpack_from('<I', original, optional + 32)[0]
    if section_alignment < PAGE_SIZE:
        raise ValueError('Section alignment below host page size')
    old_headers = struct.unpack_from('<I', original, optional + 60)[0]
    header_size = align(old_headers)
    result = bytearray(original[:old_headers])
    result.extend(bytes(header_size - len(result)))
    struct.pack_into('<I', result, optional + 36, PAGE_SIZE)
    struct.pack_into('<I', result, optional + 60, header_size)
    struct.pack_into('<I', result, optional + 64, 0)
    sections = []
    section_table = optional + optional_size
    for index in range(count):
        header = section_table + index * 40
        name = original[header:header + 8].rstrip(b'\0').decode(errors='replace')
        virtual_size, virtual_address, raw_size, old_offset = struct.unpack_from('<IIII', original, header + 8)
        if not raw_size:
            continue
        if old_offset + raw_size > len(original):
            raise ValueError('Invalid section range')
        new_offset = len(result)
        payload = original[old_offset:old_offset + raw_size]
        result.extend(payload)
        result.extend(bytes(align(raw_size) - raw_size))
        struct.pack_into('<II', result, header + 16, align(raw_size), new_offset)
        sections.append(dict(name=name, rva=virtual_address, virtual_size=virtual_size, old_offset=old_offset,
                             new_offset=new_offset, raw_size=raw_size, sha256=hashlib.sha256(payload).hexdigest()))
    if not sections:
        raise ValueError('PE image has no raw sections')
    overlay_start = max(section['old_offset'] + section['raw_size'] for section in sections)
    overlay_new = len(result)
    result.extend(original[overlay_start:])

    def translated(offset):
        if not offset:
            return 0
        if offset < old_headers:
            return offset
        for section in sections:
            if section['old_offset'] <= offset < section['old_offset'] + section['raw_size']:
                return section['new_offset'] + offset - section['old_offset']
        if offset >= overlay_start:
            return overlay_new + offset - overlay_start
        raise ValueError('Unmapped file offset')

    symbol_pointer = struct.unpack_from('<I', original, pe_offset + 12)[0]
    struct.pack_into('<I', result, pe_offset + 12, translated(symbol_pointer))
    directories = optional + (112 if magic == 0x20b else 96)
    struct.pack_into('<II', result, directories + 4 * 8, 0, 0)
    debug_rva, debug_size = struct.unpack_from('<II', original, directories + 6 * 8)
    if bool(debug_rva) != bool(debug_size) or debug_size % 28:
        raise ValueError('Invalid debug directory size')
    if debug_rva:
        debug_section = next((section for section in sections
                              if section['rva'] <= debug_rva < section['rva'] + section['raw_size']), None)
        if debug_section is None:
            raise ValueError('Debug directory is outside raw section data')
        debug_delta = debug_rva - debug_section['rva']
        if debug_size > debug_section['raw_size'] - debug_delta:
            raise ValueError('Debug directory exceeds raw section data')
        debug_offset = debug_section['new_offset'] + debug_delta
        for entry in range(debug_offset, debug_offset + debug_size, 28):
            pointer = struct.unpack_from('<I', result, entry + 24)[0]
            struct.pack_into('<I', result, entry + 24, translated(pointer))
    return bytes(result), sections


def write_new_file(destination, data):
    destination.parent.mkdir(parents=True, exist_ok=True)
    created = False
    try:
        with destination.open('xb') as output:
            created = True
            output.write(data)
    except BaseException:
        if created:
            destination.unlink(missing_ok=True)
        raise


def transform(source, destination):
    source = pathlib.Path(source)
    destination = pathlib.Path(destination)
    if source.resolve(strict=True) == destination.resolve(strict=False):
        raise ValueError('Source and output must be different files')
    original = source.read_bytes()
    source_sha256 = hashlib.sha256(original).hexdigest()
    if source_sha256 != SOURCE_SHA256:
        raise ValueError('Unsupported source SHA256')
    result, sections = realign_bytes(original)
    output_sha256 = hashlib.sha256(result).hexdigest()
    if output_sha256 != OUTPUT_SHA256:
        raise ValueError('Unexpected output SHA256; refusing to write')
    write_new_file(destination, result)
    return dict(source_sha256=source_sha256, output_sha256=output_sha256,
                original_bytes=len(original), output_bytes=len(result), sections=sections,
                signature='invalidated; certificate directory cleared')


def main(argv=None):
    parser = argparse.ArgumentParser(description='Create the verified page-aligned CEF image copy.')
    parser.add_argument('--source', required=True, type=pathlib.Path)
    parser.add_argument('--output', required=True, type=pathlib.Path)
    arguments = parser.parse_args(argv)
    try:
        summary = transform(arguments.source, arguments.output)
    except (OSError, ValueError, struct.error, StopIteration) as error:
        parser.error(str(error))
    print(json.dumps(summary, indent=2))
    return 0


if __name__ == '__main__':
    sys.exit(main())
