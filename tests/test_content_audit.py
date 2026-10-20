"""Original tiny PSARC fixtures and address arithmetic checks; no game data."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

spec = importlib.util.spec_from_file_location('audit_content', Path(__file__).parents[1] / 'tools/audit_content.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def archive(names, *, compressed=False):
    block = 32
    payloads = [b'\0'.join(n.encode() for n in names)] + [b'fixture' for _ in names]
    rows, lengths, content = [], [], bytearray()
    for payload in payloads:
        rows.append((len(lengths), len(payload), len(content)))
        for offset in range(0, len(payload), block):
            chunk = payload[offset:offset + block]
            packed = zlib.compress(chunk)
            stored = packed if compressed and len(packed) < len(chunk) else chunk
            lengths.append(len(stored) if len(stored) != block else 0)
            content.extend(stored)
    end = 32 + len(rows) * 30 + len(lengths)
    header = struct.pack('>4sHH4sIIIII', b'PSAR', 1, 4, b'zlib', end, 30, len(rows), block, 0)
    entries = b''.join(bytes(16) + first.to_bytes(4, 'big') + size.to_bytes(5, 'big')
                       + (end + offset).to_bytes(5, 'big') for first, size, offset in rows)
    return header + entries + bytes(lengths) + content


class ContentAuditTests(unittest.TestCase):
    def check(self, data):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'fixture.psarc'
            path.write_bytes(data)
            result = audit.audit_archive(path)
            self.assertEqual(path.read_bytes(), data)
            return result

    def test_stored_and_compressed(self):
        names = ['a/' + 'b' * 100 + '.csdr', 'materials/fixture.cmat', 'nested.psarc']
        for compressed in (False, True):
            report = self.check(archive(names, compressed=compressed))
            self.assertEqual(report['members'], 3)
            self.assertEqual(report['extensions'], {'.csdr': 1, '.cmat': 1, '.psarc': 1})

    def test_case_collision_not_merge(self):
        report = self.check(archive(['Object.ctxr', 'object.ctxr']))
        self.assertEqual(report['members'], 2)
        self.assertEqual(report['case_collisions'], 1)

    def test_unsafe_or_duplicate_names(self):
        for names in (['../escape'], ['/absolute'], ['a\\b'], ['same', 'same']):
            with self.subTest(names=names), self.assertRaises(ValueError):
                self.check(archive(names))

    def test_truncation_and_extent_validation(self):
        data = archive(['a/b.ctxr'])
        for truncated in (data[:12], data[:-1]):
            with self.assertRaises(ValueError): self.check(truncated)
        broken = bytearray(data)
        broken[57:62] = (0).to_bytes(5, 'big')  # manifest overlaps metadata
        with self.assertRaises(ValueError): self.check(broken)
        broken = bytearray(data)
        broken[48:52] = (0xffffffff).to_bytes(4, 'big')
        with self.assertRaises(ValueError): self.check(broken)

    def test_unsupported_header(self):
        data = bytearray(archive(['file']))
        for start, value in ((0, b'FAIL'), (8, b'lzma'), (16, (29).to_bytes(4, 'big'))):
            changed = bytearray(data); changed[start:start + len(value)] = value
            with self.assertRaises(ValueError): self.check(changed)

    def test_corrupt_zlib(self):
        data = bytearray(archive(['a' * 100 + '.ctxr'], compressed=True))
        end = int.from_bytes(data[12:16], 'big')
        data[end] ^= 0xff
        with self.assertRaises((ValueError, zlib.error)): self.check(data)

    def test_manifest_count_mismatch(self):
        with self.assertRaises(ValueError): self.check(archive(['bad\nname']))

    def test_pointer_width_null_static_and_heap(self):
        base, end = audit.pointer_window(0x100010000, 0x10000)
        self.assertEqual((base, end), (0x100000000, 0x100020000))
        self.assertEqual(audit.decode_word(0, base), {'kind': 'null', 'value': 0})
        self.assertEqual(audit.decode_word(0x1234, base), {'kind': 'static', 'value': 0x1234})
        self.assertEqual(audit.decode_word(0x80000000, base), {'kind': 'heap', 'value': base})
        self.assertEqual(audit.decode_word(0xffffffff, base)['value'], base + 0x7fffffff)
        # Decoding is arithmetic, not proof of heap bounds or object validity.
        self.assertGreater(audit.decode_word(0xffffffff, base)['value'], end)

    def test_pointer_invalid_windows_and_overflow(self):
        for start, size in ((-1, 1), (1 << 64, 0), ((1 << 64) - 1, 1), (1, 1 << 31)):
            with self.assertRaises(ValueError): audit.pointer_window(start, size)
        for word, base in ((1 << 32, 0), (-1, 0), (0x80000001, (1 << 64) - 1)):
            with self.assertRaises(ValueError): audit.decode_word(word, base)


if __name__ == '__main__':
    unittest.main()
