"""Checked private interpreter correction; leave the pinned checkout unchanged."""
import hashlib
from pathlib import Path
import sys

PINNED_SHA256='53d8005e88a0be91352e150edb8084d0bbf06bfdcb4fa621f06f15822817ca13'


def prepare(data):
    if hashlib.sha256(data).hexdigest() not in (PINNED_SHA256,
            '32026e0a46a6529fa2ecadf15ad03ee17d3375ca84519874a7363d214fe42d37'):
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
    source=source.replace(old,new)
    old='''uint64_t bbcpu_step_insn(BbCpu *cpu, const BbInsn *in) {
    const int locked = in->lock || (in->mnemonic == M(XCHG) &&
                                    (in->op[0].type == OP_MEM || in->op[1].type == OP_MEM));
    if (!locked) return step(cpu, in);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    const uint64_t next = step(cpu, in);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return next;
}'''
    new='''/* The terminating diagnostic can otherwise recover guest registers only
 * from generated JIT code. Keep the current interpreter CPU in thread-local
 * state for the duration of exactly one original instruction. The fault path
 * only copies register values; it does not read, repair or retry the operand. */
static _Thread_local BbCpu *sotc_interp_fault_cpu;

int sotc_cpu_interp_fault_guest_regs(BbGuestRegs *regs) {
    BbCpu *cpu=__atomic_load_n(&sotc_interp_fault_cpu,__ATOMIC_RELAXED);
    if(!cpu || !regs)return 0;
    memcpy(regs->r,cpu->r,sizeof(regs->r));
    regs->rip=cpu->rip;
    return 1;
}

uint64_t bbcpu_step_insn(BbCpu *cpu, const BbInsn *in) {
    BbCpu *outer=__atomic_exchange_n(&sotc_interp_fault_cpu,cpu,__ATOMIC_RELAXED);
    const int locked = in->lock || (in->mnemonic == M(XCHG) &&
                                    (in->op[0].type == OP_MEM || in->op[1].type == OP_MEM));
    uint64_t next;
    if (!locked) next=step(cpu,in);
    else {
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        next=step(cpu,in);
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
    }
    __atomic_store_n(&sotc_interp_fault_cpu,outer,__ATOMIC_RELAXED);
    return next;
}'''
    if source.count(old)!=1:raise ValueError('Unexpected interpreter step wrapper.')
    return source.replace(old,new)


if __name__=='__main__':
    Path(sys.argv[2]).write_text(prepare(Path(sys.argv[1]).read_bytes()))
