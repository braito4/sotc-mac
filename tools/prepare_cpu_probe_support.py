"""Prepare a checked private JIT copy with scalar correction and probe boundaries.

The pinned source checkout is unchanged. Probes must be registered before guest
execution. An optional immutable range uses the existing interpreter dispatcher
for a controlled comparison; no guest opcode, operand or protection is changed.
"""
import hashlib
from pathlib import Path
import sys

PINNED_SHA256='6088b98b870f59beba0779c227af18c7a32016786624a037ded28cc01c142818'


def prepare(data):
    if hashlib.sha256(data).hexdigest()!=PINNED_SHA256:
        raise ValueError('Unexpected pinned CPU JIT; refusing unchecked probe adaptation.')
    source=data.decode()
    # A 32-bit register destination clears its upper half even when the
    # masked shift count is zero. ARM64 W-register moves preserve NZCV.
    old='''    if (!count) return 1; /* no change, flags kept */'''
    new='''    if (!count) {
        if (size == 4 && dst->type == OP_REG)
            a64_mov(a, 0, G[dst->reg], G[dst->reg]);
        return 1; /* flags kept; a 32-bit destination still zero-extends */
    }'''
    if source.count(old)!=1:raise ValueError('Unexpected immediate shift zero-count path.')
    source=source.replace(old,new)
    old='''    a64_logic_imm(a, 0, 0, T5, G[RCX], 0, sf ? 6 : 5); /* count */
    uint32_t *skip = a64_here(a);'''
    new='''    a64_logic_imm(a, 0, 0, T5, G[RCX], 0, sf ? 6 : 5); /* count */
    if (size == 4 && dst->type == OP_REG)
        a64_mov(a, 0, G[dst->reg], G[dst->reg]);
    uint32_t *skip = a64_here(a);'''
    if source.count(old)!=1:raise ValueError('Unexpected CL shift zero-count path.')
    source=source.replace(old,new)
    old='''    for (uint32_t i = 0; i < block->count && !ended && !recorded; ++i) {
        const BbInsn *in = &block->insn[i];'''
    new='''    for (uint32_t i = 0; i < block->count && !ended && !recorded; ++i) {
        /* Diagnostic probes can name an interior instruction of a cached
         * decoded block. Return before translating it, preserving normal exit
         * state; the dispatcher then runs its unchanged bbcpu_step/probe path.
         * Registration must finish before execution, as in the original API. */
        if (i && bbcpu_probe_count && bbcpu_probe_at(rip)) break;
        const BbInsn *in = &block->insn[i];'''
    if source.count(old)!=1:raise ValueError('Unexpected JIT translation loop.')
    source=source.replace(old,new)
    anchor='static const uint8_t G[16] = '
    if source.count(anchor)!=1:raise ValueError('Unexpected JIT register map.')
    support='''#include "cpu_interp_region.h"
static SotcCpuInterpRegion sotc_cpu_region;
static uint64_t sotc_cpu_region_steps;
static void sotc_cpu_region_init(void) {
    static int ready;
    if(ready)return; /* callers hold jit_lock; selection is immutable */
    if(sotc_cpu_region_parse(getenv("SOTC_CPU_INTERPRET_RANGE"),&sotc_cpu_region)) {
        fputs("STOP_CPU_EXPERIMENT: invalid interpreter range\\n",stderr);
        _Exit(24);
    }
    ready=1;
    if(sotc_cpu_region.enabled)
        fprintf(stderr,"CPU region interpreter configured: begin=%#llx end=%#llx\\n",
            (unsigned long long)sotc_cpu_region.begin,(unsigned long long)sotc_cpu_region.end);
}
static int sotc_cpu_region_at(uint64_t rip) {
    return sotc_cpu_region_contains(&sotc_cpu_region,rip,bb_image_base_for_reports());
}
uint64_t sotc_cpu_region_step_count(void) {
    return __atomic_load_n(&sotc_cpu_region_steps,__ATOMIC_RELAXED);
}

'''
    source=source.replace(anchor,support+anchor)
    old='static void init_deny(void) {\n'
    if source.count(old)!=1:raise ValueError('Unexpected JIT deny initialization.')
    source=source.replace(old,old+'    sotc_cpu_region_init();\n')
    old='''        if (i && bbcpu_probe_count && bbcpu_probe_at(rip)) break;
        const BbInsn *in = &block->insn[i];'''
    new='''        if (i && bbcpu_probe_count && bbcpu_probe_at(rip)) break;
        /* A block beginning outside the range must not translate across it.
         * Selected instructions get no code/cache entry, so direct chaining
         * and indirect-cache hits cannot bypass the interpreter dispatcher. */
        if (sotc_cpu_region_at(rip)) break;
        const BbInsn *in = &block->insn[i];'''
    if source.count(old)!=1:raise ValueError('Unexpected diagnostic block boundary.')
    source=source.replace(old,new)
    old='''        BbBlock *block = (BbBlock *)bbcpu_block(cpu->rip);
        void *code = __atomic_load_n(&block->jit_code, __ATOMIC_ACQUIRE);'''
    new='''        BbBlock *block = (BbBlock *)bbcpu_block(cpu->rip);
        if (sotc_cpu_region_at(cpu->rip)) {
            cpu->jit_link=0;
            jit_to_x86(cpu);
            if(__atomic_fetch_add(&sotc_cpu_region_steps,1,__ATOMIC_RELAXED)==0)
                fprintf(stderr,"CPU region interpreter entered: offset=%#llx\\n",
                    (unsigned long long)(cpu->rip-bb_image_base_for_reports()));
            cpu->rip=bbcpu_step(cpu,&block->insn[0]);
            x86_to_jit(cpu);
            continue;
        }
        void *code = __atomic_load_n(&block->jit_code, __ATOMIC_ACQUIRE);'''
    if source.count(old)!=1:raise ValueError('Unexpected JIT dispatcher.')
    source=source.replace(old,new)
    return '/* Private CPU build: zero-count shifts, probe boundaries and optional interpreter range. */\n'+source


if __name__=='__main__':
    Path(sys.argv[2]).write_text(prepare(Path(sys.argv[1]).read_bytes()))
