/* Context bridge for bloodborne-native/bloodborne-recomp
 * f749c155c2ff3bb932795de85c9c8fe9c4aa59ad. The original cpu.c is included
 * unchanged; its object is omitted from the diagnostic's private archive.
 * The JIT and decoder remain unchanged. A separate vector extension wraps the
 * pinned interpreter for additional instructions.
 * Foundations: deadinside28/bbport and shadPS4.
 */
#include "../upstream/recomp/src/cpu/cpu.c"
#include "bootstrap_cpu_context.h"

void bootstrap_cpu_save(BbCpu *out) { *out = *current_cpu(); }
void bootstrap_cpu_restore(const BbCpu *saved) {
    BbCpu *cpu = current_cpu();
    /* Cache, watch records and instruction totals belong to the destination
     * host thread. Architectural state belongs to the suspended fiber. */
    struct BbJitCacheEntry *cache = cpu->jit_cache;
    BbWatch *watch = cpu->watch;
    const uint64_t instructions = cpu->instructions;
    *cpu = *saved;
    cpu->jit_cache = cache; cpu->watch = watch; cpu->instructions = instructions;
    cpu->jit_link = 0;
    apply_thread_pointer(cpu);
}
int bootstrap_cpu_suspend(BbCpu *out) {
    BbTrapState trap;
    if (bbcpu_trap_state(&trap)) return 0;
    BbCpu *cpu = current_cpu();
    *out = *cpu;
    if (bbcpu_is_guest_code(cpu->rip)) {
        /* The interpreter calls a host CALL target without pushing a guest
         * return address. Its continuation is the instruction after CALL. */
        const BbBlock *block = bbcpu_block(cpu->rip);
        if (!block || !block->count) return 0;
        if (block->insn[0].mnemonic == ZYDIS_MNEMONIC_CALL) {
            out->rip = cpu->rip + block->insn[0].length;
        } else if (block->insn[0].mnemonic == ZYDIS_MNEMONIC_JMP) {
            /* The interpreter also invokes host targets directly from a PLT
             * tail jump, while rip still names the guest JMP instruction. */
            out->rip = bb_load(cpu->r[RSP], 8);
            out->r[RSP] += 8;
        } else return 0;
    } else {
        /* JIT host exits and guest PLT tail jumps have a guest return address. */
        out->rip = bb_load(cpu->r[RSP], 8);
        out->r[RSP] += 8;
    }
    out->r[RAX] = 0;
    out->jit_link = 0;
    return bbcpu_is_guest_code(out->rip);
}
uint64_t bootstrap_cpu_start(uintptr_t entry, uint64_t first, uint64_t second,
                             void *stack, size_t size, int depth, int reset_fpu, uint64_t *stop) {
    BbCpu *cpu = current_cpu();
    memset(cpu->r, 0, sizeof(cpu->r));
    memset(cpu->v, 0, sizeof(cpu->v));
    cpu->flags = 2;
    cpu->depth = depth;
    if (reset_fpu) { cpu->fcw = 0x037f; cpu->mxcsr = 0x9fc0; }
    const uint64_t args[2] = {first, second};
    *stop = SENTINEL(cpu->depth + 1);
    return bbcpu_call_on_stack(entry, 2, args, stack, size);
}
void bootstrap_cpu_resume(const BbCpu *saved, uint64_t stop) {
    bootstrap_cpu_restore(saved);
    bbcpu_run(current_cpu(), stop);
}
void bootstrap_cpu_set_fpu(uint32_t mxcsr, uint16_t control, uint16_t status) {
    BbCpu *cpu = current_cpu();
    cpu->mxcsr = mxcsr; cpu->fcw = control; cpu->fsw = status;
}
void bootstrap_cpu_thread_finished(void) {
    BbCpu *cpu = current_cpu();
    /* pthread_exit may have left translated/nested call frames through the
     * backend's longjmp. No guest callback is allowed to inherit that depth. */
    cpu->depth = 0; cpu->r[RSP] = 0; cpu->rip = 0; cpu->stack_top = 0; cpu->jit_link = 0;
}
