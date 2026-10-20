/* Original synthetic SSE4.2/AVX fixtures; no game code or Rosetta required.
 * The scalar oracle constructs a comparison matrix from Intel SDM Vol. 2B
 * section 4.1's validity rules. The runtime uses direct string operations.
 * These checks are not a hardware x86 oracle or exhaustive program coverage.
 */
#include "bootstrap_contracts.h"
#include "cpu_internal.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

enum { FORMS = 5, STRIDE = 64, CONTROLS = 256, SAMPLES = 24 };
typedef struct {
    uintptr_t entry, end;
    unsigned control, vex, form, seen;
    unsigned char a[16], b[16], initial[16][32], output[16][32];
    uint64_t flags, selected[6];
    _Alignas(16) unsigned char memory[33];
} Case;
static Case current;
static int source1(unsigned form) { return form == 1 ? 0 : form == 2 ? 15 : form == 4 ? 2 : 1; }
static int source2(unsigned form) { return form == 1 ? 0 : form == 2 ? 14 : 2; }
static int trap(BbGuestRegs *regs) {
    BbTrapState state; assert(bbcpu_trap_state(&state));
    if (regs->rip == current.entry + 1) {
        for (unsigned r = 0; r < 16; ++r) for (unsigned b = 0; b < 32; ++b)
            state.ymm[r][b] = (unsigned char)(r * 17 + b + 0x41);
        memcpy(state.ymm[source1(current.form)], current.a, 16);
        if (current.form != 1 && current.form != 4)
            memcpy(state.ymm[source2(current.form)], current.b, 16);
        memcpy(current.memory + 1, current.b, 16);
        regs->r[RDI] = (uintptr_t)(current.memory + 1);
        *state.flags = F_ARITH | F_DF | 2; *state.mxcsr = 0x9fc0;
        memcpy(current.initial, state.ymm, sizeof(current.initial));
    } else if (regs->rip == current.end) {
        memcpy(current.output, state.ymm, sizeof(current.output));
        current.flags = *state.flags;
        for (unsigned i = 0; i < 6; ++i) current.selected[i] = regs->r[R8 + i] & 255;
        assert(*state.mxcsr == 0x9fc0);
    } else return 0;
    ++current.seen; return 1;
}
/* Independent validity/matrix oracle, including near-boundary ordered matches. */
static unsigned oracle(const unsigned char *a, const unsigned char *b, unsigned c,
                         unsigned *na, unsigned *nb) {
    const unsigned n = c & 1 ? 8 : 16, mode = (c >> 2) & 3;
    int32_t x[16], y[16]; int va[16], vb[16], matrix[16][16];
    int live_a = 1, live_b = 1; *na = *nb = 0;
    for (unsigned i = 0; i < n; ++i) {
        unsigned ax = c & 1 ? a[2 * i] + ((unsigned)a[2 * i + 1] << 8) : a[i];
        unsigned by = c & 1 ? b[2 * i] + ((unsigned)b[2 * i + 1] << 8) : b[i];
        const unsigned sign = c & 1 ? 0x8000 : 0x80;
        x[i] = c & 2 ? (int32_t)(ax ^ sign) - (int32_t)sign : (int32_t)ax;
        y[i] = c & 2 ? (int32_t)(by ^ sign) - (int32_t)sign : (int32_t)by;
        va[i] = live_a = live_a && ax != 0; vb[i] = live_b = live_b && by != 0;
        *na += va[i]; *nb += vb[i];
    }
    for (unsigned j = 0; j < n; ++j) for (unsigned i = 0; i < n; ++i) {
        int value = mode == 1 ? (i & 1 ? y[j] <= x[i] : y[j] >= x[i]) : y[j] == x[i];
        if (!va[i] || !vb[j])
            value = mode == 2 ? !va[i] && !vb[j] : mode == 3 ? !va[i] : 0;
        matrix[j][i] = value;
    }
    unsigned mask = 0;
    for (unsigned j = 0; j < n; ++j) {
        int bit = mode == 3;
        if (mode == 0) for (unsigned i = 0; i < n; ++i) bit |= matrix[j][i];
        else if (mode == 1) for (unsigned i = 0; i < n; i += 2) bit |= matrix[j][i] && matrix[j][i + 1];
        else if (mode == 2) bit = matrix[j][j];
        else for (unsigned i = 0; i + j < n; ++i) bit &= matrix[i + j][i];
        if ((c & 16) && (!(c & 32) || vb[j])) bit = !bit;
        mask |= (unsigned)bit << j;
    }
    return mask;
}
static uint32_t random_state = 0x7152a9b3;
static uint32_t random_value(void) {
    random_state ^= random_state << 13; random_state ^= random_state >> 17;
    random_state ^= random_state << 5; return random_state;
}
static void inputs(unsigned sample, unsigned control) {
    const unsigned n = control & 1 ? 8 : 16, bytes = control & 1 ? 2 : 1;
    for (unsigned i = 0; i < n; ++i) {
        unsigned a = (random_value() % 7) + 1, b = (random_value() % 7) + 1;
        if (sample & 1) { a |= 1u << (bytes * 8 - 1); b |= 1u << (bytes * 8 - 1); }
        if (sample == 0) a = b = 0;
        if (sample == 1) a = b = 1;
        if (sample == 2) { a = 1; b = 2; }
        if (sample == 3) {
            const unsigned sign = 1u << (bytes * 8 - 1);
            const unsigned ranges[] = {sign + 1, 2, 1, sign + 3};
            const unsigned probes[] = {sign, sign + 1, (sign << 1) - 1, 1, 2, 3};
            a = ranges[i % 4]; b = probes[i % 6];
        }
        if (sample == 4) { a = i % 3 + 1; b = 4; }
        if (bytes == 2) { current.a[2 * i] = (unsigned char)a; current.a[2 * i + 1] = a >> 8;
                          current.b[2 * i] = (unsigned char)b; current.b[2 * i + 1] = b >> 8; }
        else { current.a[i] = (unsigned char)a; current.b[i] = (unsigned char)b; }
    }
    if (sample == 4) {
        memset(current.a + 3 * bytes, 0, bytes);
        memset(current.b + (n - 2) * bytes, 0, bytes * 2);
        current.b[(n - 2) * bytes] = 1; current.b[(n - 1) * bytes] = 2;
    }
    if (sample >= 5) {
        const unsigned a_end = (sample * 7) % (n + 1), b_end = (sample * 11 + 3) % (n + 1);
        if (a_end < n) memset(current.a + a_end * bytes, 0, bytes);
        if (b_end < n) memset(current.b + b_end * bytes, 0, bytes);
    }
    if (current.form == 1 || current.form == 4) memcpy(current.b, current.a, 16);
}
static size_t fixture(unsigned char *out, unsigned control, unsigned vex, unsigned form) {
    size_t at = 0; out[at++] = 0xcc;
    const unsigned a = source1(form), b = source2(form);
    if (vex) { out[at++] = 0xc4; out[at++] = form == 2 ? 0x43 : 0xe3; out[at++] = 0x79; }
    else { out[at++] = 0x66; if (form == 2) out[at++] = 0x45; out[at++] = 0x0f; out[at++] = 0x3a; }
    out[at++] = 0x62;
    out[at++] = form == 3 ? 0x0f : (unsigned char)(0xc0 | ((a & 7) << 3) | (b & 7));
    out[at++] = (unsigned char)control;
    /* Consume the new arithmetic flags in translated SETcc instructions. */
    const unsigned char conditions[] = {0x92, 0x94, 0x98, 0x90, 0x9a, 0x97};
    for (unsigned i = 0; i < 6; ++i) {
        out[at++] = 0x41; out[at++] = 0x0f; out[at++] = conditions[i]; out[at++] = (unsigned char)(0xc0 + i);
    }
    out[at++] = 0xcc; const size_t end = at; out[at++] = 0xc3;
    assert(at <= STRIDE); return end;
}
int main(void) {
    bootstrap_initialize(1); bbcpu_set_trap_handler(trap);
    const size_t size = CONTROLS * 2 * FORMS * STRIDE;
    unsigned char *code = runtime_low_map(size, PROT_READ | PROT_WRITE); assert(code);
    size_t ends[CONTROLS][2][FORMS];
    for (unsigned c = 0; c < CONTROLS; ++c) for (unsigned v = 0; v < 2; ++v)
        for (unsigned f = 0; f < FORMS; ++f)
            ends[c][v][f] = fixture(code + ((c * 2 + v) * FORMS + f) * STRIDE, c, v, f);
    bbcpu_add_guest_code((uintptr_t)code, size);
    size_t checked = 0;
    for (unsigned c = 0; c < CONTROLS; ++c) for (unsigned v = 0; v < 2; ++v)
        for (unsigned f = 0; f < FORMS; ++f) {
            current.control = c; current.vex = v; current.form = f;
            current.entry = (uintptr_t)code + ((c * 2 + v) * FORMS + f) * STRIDE;
            current.end = current.entry + ends[c][v][f];
            const BbInsn *in = &bbcpu_block(current.entry + 1)->insn[0];
            assert(in->mnemonic == (v ? ZYDIS_MNEMONIC_VPCMPISTRM : ZYDIS_MNEMONIC_PCMPISTRM));
            assert(in->count == 3 && (in->fwrites & F_ARITH) == F_ARITH);
            for (unsigned sample = 0; sample < SAMPLES; ++sample) {
                inputs(sample, c); current.seen = 0;
                unsigned na, nb; const unsigned mask = oracle(current.a, current.b, c, &na, &nb);
                const unsigned n = c & 1 ? 8 : 16;
                /* Closed-form self comparisons check validity/polarity independently. */
                if (f == 1 || f == 4) {
                    if ((c & 0x3c) == 8) assert(mask == (1u << n) - 1);
                    if ((c & 0x3c) == 0x18) assert(mask == 0);
                    if ((c & 0x3c) == 0x38) assert(mask == ((1u << n) - 1) - ((1u << na) - 1));
                }
                bbcpu_call(current.entry, 0, NULL); assert(current.seen == 2);
                unsigned char expected[16] = {0};
                if (c & 64) for (unsigned i = 0; i < n; ++i) {
                    const unsigned bytes = c & 1 ? 2 : 1;
                    memset(expected + bytes * i, mask & (1u << i) ? 255 : 0, bytes);
                } else { expected[0] = mask & 255; expected[1] = mask >> 8; }
                const uint64_t flags = F_DF | 2 | (mask ? F_CF : 0) | (mask & 1 ? F_OF : 0) |
                    (na < n ? F_SF : 0) | (nb < n ? F_ZF : 0);
                if (memcmp(current.output[0], expected, 16) || current.flags != flags) {
                    fprintf(stderr, "FAIL: control=%#x VEX=%u form=%u sample=%u mask=%#x flags=%#llx expected=%#llx\n",
                        c, v, f, sample, mask, (unsigned long long)current.flags, (unsigned long long)flags); abort();
                }
                unsigned char zero[16] = {0};
                assert(!memcmp(current.output[0] + 16, v ? zero : current.initial[0] + 16, 16));
                assert(!memcmp(current.output[1], current.initial[1], 15 * 32));
                const uint64_t selected[] = {!!(flags & F_CF), !!(flags & F_ZF), !!(flags & F_SF),
                    !!(flags & F_OF), !!(flags & F_PF), !(flags & (F_CF | F_ZF))};
                assert(!memcmp(current.selected, selected, sizeof(selected))); ++checked;
            }
        }
    printf("PASS: %zu synthetic PCMPISTRM/VPCMPISTRM cases, all control bytes, mask/flags, SSE/VEX upper lanes, alias/high registers, unaligned memory and translated flag consumers\n", checked);
    return 0;
}
