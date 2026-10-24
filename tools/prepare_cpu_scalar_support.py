"""Checked private interpreter correction; leave the pinned checkout unchanged."""
import hashlib
from pathlib import Path
import sys

PINNED_SHA256='53d8005e88a0be91352e150edb8084d0bbf06bfdcb4fa621f06f15822817ca13'


def prepare(data):
    if hashlib.sha256(data).hexdigest()!=PINNED_SHA256:
        raise ValueError('Unexpected pinned scalar interpreter; refusing unchecked adaptation.')
    source=data.decode()
    old='''    uint64_t a = bbcpu_read(cpu, in, dst) & m, r = a;
    if (!count) return;'''
    new='''    uint64_t a = bbcpu_read(cpu, in, dst) & m, r = a;
    if (!count) {
        /* A masked-zero shift preserves flags, but a 32-bit register
         * destination still clears its upper half. No extra memory write. */
        if (dst->type == OP_REG && size == 4 &&
            (in->mnemonic == M(SHL) || in->mnemonic == M(SHR) || in->mnemonic == M(SAR)))
            write_gpr(cpu, dst, size, a);
        return;
    }'''
    if source.count(old)!=1:raise ValueError('Unexpected scalar shift zero-count path.')
    source=source.replace(old,new)
    old='''        } else {
            a = bbcpu_read(cpu, in, dst);
            bbcpu_write(cpu, in, dst, (a + b) & bb_mask(size));
        }
        flags_add(cpu, a, b, 0, a + b, size);
        bbcpu_write(cpu, in, src, a);'''
    new='''        } else {
            a = bbcpu_read(cpu, in, dst);
            /* XADD assigns the original destination to the source first.
             * When both operands alias, the sum must be the final value. */
            bbcpu_write(cpu, in, src, a);
            bbcpu_write(cpu, in, dst, (a + b) & bb_mask(size));
        }
        flags_add(cpu, a, b, 0, a + b, size);
        if (dst->type == OP_MEM) bbcpu_write(cpu, in, src, a);'''
    if source.count(old)!=1:raise ValueError('Unexpected register XADD write ordering.')
    return source.replace(old,new)


if __name__=='__main__':
    Path(sys.argv[2]).write_text(prepare(Path(sys.argv[1]).read_bytes()))
