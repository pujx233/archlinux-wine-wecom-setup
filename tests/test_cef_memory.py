import importlib.util
import hashlib
import json
import pathlib
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = pathlib.Path(__file__).resolve().parents[1] / 'scripts/align-cef-memory.py'
SPEC = importlib.util.spec_from_file_location('align_cef_memory', SCRIPT)
aligner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(aligner)


def synthetic_pe(debug_size=28):
    data = bytearray(0x820)
    pe_offset = 0x80
    optional = pe_offset + 24
    optional_size = 240
    section_table = optional + optional_size
    struct.pack_into('<I', data, 60, pe_offset)
    data[pe_offset:pe_offset + 4] = b'PE\0\0'
    struct.pack_into('<H', data, pe_offset + 4, 0x8664)
    struct.pack_into('<H', data, pe_offset + 6, 2)
    struct.pack_into('<H', data, pe_offset + 20, optional_size)
    struct.pack_into('<H', data, optional, 0x20b)
    struct.pack_into('<I', data, optional + 32, 4096)
    struct.pack_into('<I', data, optional + 36, 512)
    struct.pack_into('<I', data, optional + 60, 0x400)
    struct.pack_into('<I', data, optional + 64, 0x12345678)
    struct.pack_into('<I', data, optional + 108, 16)
    directories = optional + 112
    struct.pack_into('<II', data, directories + 4 * 8, 0x5000, 0x100)
    struct.pack_into('<II', data, directories + 6 * 8, 0x2000, debug_size)
    sections = ((b'.text', 0x1000, 0x200, 0x400), (b'.rdata', 0x2000, 0x200, 0x600))
    for index, (name, rva, raw_size, raw_offset) in enumerate(sections):
        header = section_table + index * 40
        data[header:header + len(name)] = name
        struct.pack_into('<IIII', data, header + 8, raw_size, rva, raw_size, raw_offset)
        data[raw_offset:raw_offset + raw_size] = bytes([index + 1]) * raw_size
    struct.pack_into('<I', data, pe_offset + 12, 0x804)
    struct.pack_into('<I', data, 0x600 + 24, 0x620)
    data[0x800:] = b'overlay-data-for-test!!'
    return bytes(data)


class AlignCefMemoryTests(unittest.TestCase):
    def test_recorded_fingerprints_match_converter(self):
        metadata = json.loads((SCRIPT.parents[1] / 'materials/cef64-page-aligned.json').read_text())
        self.assertEqual(metadata['source_sha256'], aligner.SOURCE_SHA256)
        self.assertEqual(metadata['output_sha256'], aligner.OUTPUT_SHA256)
        self.assertEqual(metadata['host_page_size'], aligner.PAGE_SIZE)

    def test_cli_rejects_unrecognized_source_without_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / 'source.dll'
            output = root / 'aligned.dll'
            source.write_bytes(synthetic_pe())
            result = subprocess.run(
                [sys.executable, str(SCRIPT), '--source', str(source), '--output', str(output)],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unsupported source SHA256', result.stderr)
            self.assertFalse(output.exists())

    def test_transform_refuses_in_place_output(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / 'source.dll'
            source.write_bytes(synthetic_pe())
            before = source.read_bytes()
            with self.assertRaisesRegex(ValueError, 'different files'):
                aligner.transform(source, source)
            self.assertEqual(source.read_bytes(), before)

    def test_writer_refuses_to_replace_existing_file(self):
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / 'existing.dll'
            output.write_bytes(b'keep this')
            with self.assertRaises(FileExistsError):
                aligner.write_new_file(output, b'new bytes')
            self.assertEqual(output.read_bytes(), b'keep this')

    def test_transform_rejects_unexpected_output_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            source = pathlib.Path(directory) / 'source.dll'
            output = pathlib.Path(directory) / 'aligned.dll'
            original = synthetic_pe()
            source.write_bytes(original)
            with mock.patch.object(aligner, 'SOURCE_SHA256', hashlib.sha256(original).hexdigest()):
                with self.assertRaisesRegex(ValueError, 'Unexpected output SHA256'):
                    aligner.transform(source, output)
            self.assertFalse(output.exists())

    def test_writer_cleans_partial_output(self):
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / 'partial.dll'
            stream = output.open('xb')
            with mock.patch.object(pathlib.Path, 'open') as patched_open:
                patched_open.return_value.__enter__.return_value.write.side_effect = OSError('disk full')
                with self.assertRaisesRegex(OSError, 'disk full'):
                    aligner.write_new_file(output, b'bytes')
            stream.close()
            self.assertFalse(output.exists())

    def test_realign_preserves_rvas_and_translates_debug_and_symbol_pointers(self):
        original = synthetic_pe()
        result, sections = aligner.realign_bytes(original)
        pe_offset = struct.unpack_from('<I', original, 60)[0]
        optional = pe_offset + 24
        directories = optional + 112
        self.assertEqual([(section['rva'], section['new_offset']) for section in sections],
                         [(0x1000, 0x1000), (0x2000, 0x2000)])
        self.assertEqual(struct.unpack_from('<I', result, pe_offset + 12)[0], 0x3004)
        self.assertEqual(struct.unpack_from('<I', result, 0x2000 + 24)[0], 0x2020)
        self.assertEqual(struct.unpack_from('<II', result, directories + 4 * 8), (0, 0))
        self.assertEqual(struct.unpack_from('<I', result, optional + 64)[0], 0)
        self.assertEqual(struct.unpack_from('<I', result, optional + 36)[0], 4096)
        self.assertEqual(result[0x1000:0x1200], original[0x400:0x600])
        self.assertEqual(result[0x2000:0x2018], original[0x600:0x618])
        self.assertEqual(result[0x201c:0x2200], original[0x61c:0x800])

    def test_realign_rejects_partial_debug_entry(self):
        with self.assertRaisesRegex(ValueError, 'debug directory size'):
            aligner.realign_bytes(synthetic_pe(debug_size=29))

    def test_realign_rejects_debug_directory_past_raw_section(self):
        with self.assertRaisesRegex(ValueError, 'exceeds raw section'):
            aligner.realign_bytes(synthetic_pe(debug_size=0x214))


if __name__ == '__main__':
    unittest.main()
