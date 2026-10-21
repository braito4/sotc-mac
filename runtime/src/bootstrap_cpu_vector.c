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

/* AMD APM volume 4, revision 3.27: EXTRQ/INSERTQ (SSE4a).
 * https://docs.amd.com/v/u/en-US/26568_3.27_APM_Vol4_PUB
 * Zydis describes the written destination as 64 bits, not an entire XMM.
 * Preserve undefined bits 64..127 as a deterministic diagnostic choice and
 * preserve the architectural legacy-SSE upper YMM half. For ranges extending
 * past bit 63, use QEMU v10.1.0's shift/mask/truncate compatibility policy:
 * https://github.com/qemu/qemu/blob/v10.1.0/target/i386/ops_sse.h
 * AMD leaves these results undefined; this is not a hardware guarantee.
 * All host shifts remain defined (index < 64 and full-width masks special-cased). */
static void sse4a_field(BbCpu *cpu, const BbInsn *in) {
    const int insert = in->mnemonic == ZYDIS_MNEMONIC_INSERTQ;
    const int immediate = in->count == (insert ? 4 : 3);
    const BbOp *dst = &in->op[0];
    if (in->vex || dst->type != OP_REG || dst->kind != RK_VEC || dst->size != 8 ||
        dst->reg >= 16 || (!immediate && in->count != 2))
        bbcpu_fatal(cpu, in, "SSE4a operand shape");
    const BbVec original = cpu->v[dst->reg];
    BbVec source = {0};
    if (insert || !immediate) {
        const BbOp *src = &in->op[1];
        if (src->type != OP_REG || src->kind != RK_VEC || src->reg >= 16 ||
            src->size != (immediate ? 8 : 16))
            bbcpu_fatal(cpu, in, "SSE4a source shape");
        source = cpu->v[src->reg]; /* Capture before writes, including aliases. */
    }
    unsigned length, index;
    if (immediate) {
        const unsigned first = insert ? 2 : 1;
        if (in->op[first].type != OP_IMM || in->op[first+1].type != OP_IMM)
            bbcpu_fatal(cpu, in, "SSE4a immediate shape");
        length = (unsigned)in->op[first].disp & 63;
        index = (unsigned)in->op[first+1].disp & 63;
    } else {
        const unsigned byte = insert ? 8 : 0;
        length = source.b[byte] & 63;
        index = source.b[byte+1] & 63;
    }
    if (!length) length = 64;
    const uint64_t mask = length == 64 ? UINT64_MAX : (UINT64_C(1) << length) - 1;
    cpu->v[dst->reg].q[0] = insert ?
        (original.q[0] & ~(mask << index)) | ((source.q[0] & mask) << index) :
        (original.q[0] >> index) & mask;
}

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
    if (in->mnemonic == ZYDIS_MNEMONIC_EXTRQ || in->mnemonic == ZYDIS_MNEMONIC_INSERTQ) {
        sse4a_field(cpu, in); return 1;
    }
    if (in->mnemonic == ZYDIS_MNEMONIC_PCMPISTRM || in->mnemonic == ZYDIS_MNEMONIC_VPCMPISTRM) {
        implicit_string_mask(cpu, in); return 1;
    }
    return sotc_pinned_exec_vec(cpu, in);
}
