"""Extract only our relocation-free synthetic fixture's text and entry offsets."""
import struct
import sys
from pathlib import Path

data = Path(sys.argv[1]).read_bytes()
hdr = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
assert hdr[0][:7] == b'\x7fELF\x02\x01\x01' and hdr[2] == 62
sections = [struct.unpack_from('<IIQQQQIIQQ', data, hdr[6] + i * hdr[11]) for i in range(hdr[12])]
names = sections[hdr[13]]
strings = data[names[4]:names[4] + names[5]]
def name(offset):
    return strings[offset:strings.index(0, offset)].decode()
code = bytearray()
starts = {}
for index, text in enumerate(sections):
    if not (text[2] & 4) or not text[5]:
        continue
    assert not any(s[1] in (4, 9) and s[7] == index and s[5] for s in sections), 'fixture has text relocations'
    code.extend(b'\x90' * (-len(code) % 16))
    starts[index] = len(code)
    code.extend(data[text[4]:text[4] + text[5]])
sym = next(s for s in sections if s[1] == 2)
strtab = sections[sym[6]]
strings = data[strtab[4]:strtab[4] + strtab[5]]
exports = []
for offset in range(sym[4], sym[4] + sym[5], sym[9]):
    n, info, _, section, value, size = struct.unpack_from('<IBBHQQ', data, offset)
    if section in starts and info & 15 == 2:
        exports.append((name(n), starts[section] + value))
array_name = sys.argv[3] if len(sys.argv) > 3 else 'fiber_fixture_code'
if not array_name.isidentifier():
    raise SystemExit('Invalid fixture array name.')
out = ['/* Generated from our synthetic fixture, not game code. */',
       f'static const unsigned char {array_name}[] = {{',
       ','.join(str(b) for b in code), '};']
out += [f'#define {n}_offset {v}' for n, v in exports]
Path(sys.argv[2]).write_text('\n'.join(out) + '\n')
