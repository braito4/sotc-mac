"""Read-only PSARC manifest audit and packed-pointer arithmetic diagnostics.

Original implementation for sotc-mac. Archive format reference:
https://www.psdevwiki.com/ps3/PlayStation_archive_(PSARC)
The DehydratedFlask SotC-Mac-Port review motivated independent bounds, case
collision and pointer-window checks; none of its source is incorporated.
Reports are private local data, not distributable game content.
"""
import argparse
from collections import Counter
import json
import os
from pathlib import Path, PurePosixPath
import struct
import zlib

MAX_METADATA = 64 << 20
MAX_BLOCKS = 4_000_000


def pointer_window(start, length):
    """Checked 2 GiB window arithmetic, not a host mapping or image resolver."""
    if not 0 <= start < 1 << 64 or not 0 <= length < 1 << 64:
        raise ValueError('region values must fit unsigned 64-bit')
    end = start + length
    base = start & ~0x7fffffff
    if end >= 1 << 64 or end - base > 1 << 31:
        raise ValueError('region overflows its packed-pointer window')
    return base, end


def decode_word(word, base):
    """Report raw static values separately; never turn them into host pointers."""
    if not 0 <= word < 1 << 32 or not 0 <= base < 1 << 64:
        raise ValueError('invalid word/base width')
    tagged = bool(word & 0x80000000)
    value = (base if tagged else 0) + (word & 0x7fffffff)
    if value >= 1 << 64:
        raise ValueError('packed address addition overflow')
    return {'kind': 'null' if value == 0 else 'heap' if tagged else 'static', 'value': value}


def read_exact(stream, length):
    data = stream.read(length)
    if len(data) != length:
        raise ValueError('truncated archive')
    return data


def audit_archive(path):
    """Validate entry extents, decode only the name manifest, and count types."""
    with Path(path).open('rb') as f:
        before = os.fstat(f.fileno())
        header = struct.unpack('>4sHH4sIIIII', read_exact(f, 32))
        magic, major, minor, codec, toc_end, stride, count, block, flags = header
        if (magic, major, minor) != (b'PSAR', 1, 4) or codec not in (b'zlib', b'none'):
            raise ValueError('unsupported PSARC header/version/codec')
        if stride != 30 or count < 1 or not 0 < block <= MAX_METADATA:
            raise ValueError('invalid entry layout or block size')
        if not 32 + count * stride <= toc_end <= min(before.st_size, MAX_METADATA):
            raise ValueError('invalid or oversized table of contents')
        toc = read_exact(f, toc_end - 32)
        width = max(1, ((block - 1).bit_length() + 7) // 8)
        table = toc[count * stride:]
        if len(table) % width or len(table) // width > MAX_BLOCKS:
            raise ValueError('invalid or oversized block table')
        blocks = [int.from_bytes(table[i:i + width], 'big') for i in range(0, len(table), width)]
        sums = [0]
        for stored in blocks:
            if stored > block:
                raise ValueError('stored block exceeds block size')
            sums.append(sums[-1] + (stored or block))
        entries = []
        for i in range(count):
            row = toc[i * stride:(i + 1) * stride]
            first = int.from_bytes(row[16:20], 'big')
            size = int.from_bytes(row[20:25], 'big')
            offset = int.from_bytes(row[25:30], 'big')
            needed = (size + block - 1) // block
            if offset > before.st_size or first + needed > len(blocks):
                raise ValueError('entry outside archive/block table')
            if size and (offset < toc_end or offset + sums[first + needed] - sums[first] > before.st_size):
                raise ValueError('entry payload overlaps metadata or exceeds archive')
            entries.append((first, size, offset))
        first, size, offset = entries[0]
        if size > MAX_METADATA:
            raise ValueError('name manifest exceeds inspection budget')
        f.seek(offset)
        manifest = bytearray()
        for index in range(first, first + (size + block - 1) // block):
            wanted = min(block, size - len(manifest))
            stored = blocks[index]
            raw = read_exact(f, stored or block)
            if stored == 0:
                chunk = raw[:wanted]
            elif stored == wanted:
                chunk = raw
            else:
                if codec != b'zlib':
                    raise ValueError('compressed block in uncompressed archive')
                decoder = zlib.decompressobj()
                chunk = decoder.decompress(raw, wanted + 1)
                if not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
                    raise ValueError('invalid or oversized compressed name block')
            if len(chunk) != wanted:
                raise ValueError('name block decoded to an unexpected size')
            manifest.extend(chunk)
        separator = b'\0' if b'\0' in manifest else b'\n'
        names = [p.decode('utf-8') for p in manifest.split(separator) if p]
        if len(names) != count - 1:
            raise ValueError('manifest/entry count mismatch')
        seen, folded, collisions = set(), {}, []
        kinds = Counter()
        for name in names:
            parts = name.split('/')
            if name.startswith('/') or '\\' in name or '..' in parts or name in seen:
                raise ValueError('unsafe or duplicate exact member path')
            seen.add(name)
            key = name.casefold()
            if key in folded and folded[key] != name:
                collisions.append([folded[key], name])
            else:
                folded[key] = name
            kinds[PurePosixPath(name).suffix.lower() or '(none)'] += 1
        after = Path(path).stat()
        signature = lambda s: (s.st_dev, s.st_ino, s.st_size, s.st_mtime_ns, s.st_ctime_ns)
        if signature(before) != signature(after) or signature(before) != signature(os.fstat(f.fileno())):
            raise ValueError('archive changed during audit')
        return dict(archive=Path(path).name, bytes=before.st_size, members=len(names),
                    codec=codec.decode(), flags=flags, extensions=dict(kinds),
                    case_collisions=len(collisions), collision_examples=collisions[:8])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game-root', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--region-start', type=lambda v: int(v, 0))
    parser.add_argument('--region-size', type=lambda v: int(v, 0))
    parser.add_argument('--word', type=lambda v: int(v, 0), action='append', default=[])
    args = parser.parse_args()
    window = None
    if args.region_start is not None or args.region_size is not None or args.word:
        if args.region_start is None or args.region_size is None:
            parser.error('pointer words require both region start and size')
        base, end = pointer_window(args.region_start, args.region_size)
        window = dict(base=base, end=end, decoded=[decode_word(w, base) for w in args.word],
                      host_mapping_verified=False, static_resolution_verified=False)
    if args.report.resolve().is_relative_to(args.game_root.resolve()):
        raise SystemExit('The report must be outside the read-only game root.')
    archives = sorted(p for p in args.game_root.iterdir() if p.is_file() and p.suffix.lower() == '.psarc')
    if not archives:
        raise SystemExit('No PSARC files found; input is never extracted or modified.')
    reports = [audit_archive(p) for p in archives]
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({'archives': reports, 'pointer_window': window, 'asset_decode_verified': False}, indent=2) + '\n')
    for report in reports:
        print(f"{report['archive']}: {report['members']} members; {report['case_collisions']} case collisions")
    print('PASS: archive extents and name manifests; asset payloads were not decoded.')


if __name__ == '__main__':
    main()
