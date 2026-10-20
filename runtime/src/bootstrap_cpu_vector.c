/* Interpreter extension for pinned bloodborne-recomp
 * f749c155c2ff3bb932795de85c9c8fe9c4aa59ad (GPL-2.0-or-later).
 * The original vector interpreter is included unchanged and handles every
 * other mnemonic. JIT execution reaches this extension through its existing
 * instruction fallback, including register/flag synchronization.
 * Foundations: deadinside28/bbport, bloodborne-native and shadPS4.
 * PCMPISTRM semantics: Intel SDM Vol. 2B, section 4.1 and instruction entry:
 * https://cdrdv2-public.intel.com/774487/253667-sdm-vol-2b.pdf
 */
#include "cpu_internal.h"
#define bbcpu_exec_vec sotc_pinned_exec_vec
#include "../upstream/recomp/src/cpu/interp_vec.c"
#undef bbcpu_exec_vec

static unsigned implicit_length(const int32_t *values, unsigned lanes) {
    unsigned n = 0;
    while (n < lanes && values[n]) ++n;
    return n;
}
static void implicit_string_mask(BbCpu *cpu, const BbInsn *in) {
    if (in->count != 3 || in->op[0].type != OP_REG || in->op[0].kind != RK_VEC ||
        in->op[0].size != 16 || in->op[1].size != 16 || in->op[2].type != OP_IMM ||
        (in->vl && in->vl != 16)) bbcpu_fatal(cpu, in, "string comparison operand shape");
    /* Both sources must be captured before the implicit XMM0 destination. */
    const BbVec a = vread(cpu, in, &in->op[0]), b = vread(cpu, in, &in->op[1]);
    const unsigned control = (unsigned)in->op[2].disp & 0xff;
    const unsigned words = control & 1, lanes = words ? 8 : 16;
    int32_t left[16], right[16];
    for (unsigned i = 0; i < lanes; ++i) {
        left[i] = words ? (control & 2 ? a.sw[i] : a.w[i]) : (control & 2 ? a.sb[i] : a.b[i]);
        right[i] = words ? (control & 2 ? b.sw[i] : b.w[i]) : (control & 2 ? b.sb[i] : b.b[i]);
    }
    const unsigned na = implicit_length(left, lanes), nb = implicit_length(right, lanes);
    unsigned mask = 0;
    for (unsigned j = 0; j < lanes; ++j) {
        int match = 0;
        switch ((control >> 2) & 3) {
        case 0: /* Membership in the valid first string. */
            if (j < nb) for (unsigned i = 0; i < na; ++i) match |= left[i] == right[j];
            break;
        case 1: /* Complete endpoint pairs only; an odd trailing endpoint is invalid. */
            if (j < nb) for (unsigned i = 0; i + 1 < na; i += 2)
                match |= left[i] <= right[j] && right[j] <= left[i + 1];
            break;
        case 2:
            match = j >= na || j >= nb ? j >= na && j >= nb : left[j] == right[j];
            break;
        case 3: /* Matches may end at the physical vector boundary. */
            match = 1;
            for (unsigned i = 0; i < na && j + i < lanes; ++i)
                if (j + i >= nb || left[i] != right[j + i]) { match = 0; break; }
            break;
        }
        if (match) mask |= 1u << j;
    }
    const unsigned all = (1u << lanes) - 1;
    if (((control >> 4) & 3) == 1) mask ^= all;
    else if (((control >> 4) & 3) == 3) mask ^= (1u << nb) - 1;
    cpu->flags = (cpu->flags & ~(uint64_t)F_ARITH) | (mask ? F_CF : 0) |
        (na < lanes ? F_SF : 0) | (nb < lanes ? F_ZF : 0) | (mask & 1 ? F_OF : 0);
    BbVec result = {0};
    if (control & 0x40) {
        for (unsigned i = 0; i < lanes; ++i)
            if (words) result.w[i] = mask & (1u << i) ? UINT16_MAX : 0;
            else result.b[i] = mask & (1u << i) ? UINT8_MAX : 0;
    } else result.w[0] = (uint16_t)mask;
    memcpy(cpu->v[0].b, result.b, 16);
    if (in->vex) memset(cpu->v[0].b + 16, 0, 16);
}
int bbcpu_exec_vec(BbCpu *cpu, const BbInsn *in) {
    if (in->mnemonic == ZYDIS_MNEMONIC_PCMPISTRM || in->mnemonic == ZYDIS_MNEMONIC_VPCMPISTRM) {
        implicit_string_mask(cpu, in); return 1;
    }
    return sotc_pinned_exec_vec(cpu, in);
}
