/* Synthetic SSE4a machine code and independent bit-by-bit scalar oracle.
 * AMD APM volume 4 revision 3.27, EXTRQ/INSERTQ. No hardware x86 oracle,
 * game bytes or Rosetta. Undefined upper XMM bits are checked only against
 * this diagnostic's explicit preservation policy, not an architectural value. */
#include "bootstrap_contracts.h"
#include "cpu_internal.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

enum { STRIDE=64, PAIRS=4, FORMS=4, SAMPLES=8 };
static const unsigned destinations[]={4,0,15,15}, sources[]={2,0,14,15};
static struct {
    uintptr_t entry,end;
    unsigned form,pair,length,index,sample,seen;
    unsigned char initial[16][32],output[16][32];
    uint64_t flags,selected[6];
} current;
static const uint64_t data[]={0,UINT64_MAX,UINT64_C(0x8000000000000001),
    UINT64_C(0x123456789abcdef0),UINT64_C(0xaaaaaaaaaaaaaaaa),
    UINT64_C(0x5555555555555555),UINT64_C(0xfedcba9876543210),1};
static uint64_t initial_flags(void) {
    static const uint64_t patterns[]={0,F_ARITH,F_CF|F_PF,F_OF|F_SF,
        F_AF|F_ZF,F_OF|F_CF|F_AF,F_ZF|F_PF|F_SF,F_OF|F_AF|F_PF};
    return 2 | F_DF | patterns[current.sample];
}
static int trap(BbGuestRegs *regs) {
    BbTrapState state; assert(bbcpu_trap_state(&state));
    if (regs->rip == current.entry+1) {
        for (unsigned r=0;r<16;++r) for (unsigned b=0;b<32;++b)
            state.ymm[r][b]=(unsigned char)(r*17+b+current.sample*31);
        const unsigned dst=destinations[current.pair], src=sources[current.pair];
        memcpy(state.ymm[dst],&data[current.sample],8);
        memcpy(state.ymm[src],&data[7-current.sample],8);
        if (!(current.form & 1)) {
            const unsigned byte=current.form>=2 ? 8 : 0;
            state.ymm[src][byte]=(unsigned char)(current.length | ((current.sample&3)<<6));
            state.ymm[src][byte+1]=(unsigned char)(current.index | (((current.sample+1)&3)<<6));
        }
        *state.flags=initial_flags(); *state.mxcsr=0x9fc0;
        memcpy(current.initial,state.ymm,sizeof(current.initial));
    } else if (regs->rip == current.end) {
        memcpy(current.output,state.ymm,sizeof(current.output));
        current.flags=*state.flags;
        for (unsigned i=0;i<6;++i) current.selected[i]=regs->r[R8+i]&255;
        assert(*state.mxcsr==0x9fc0);
    } else return 0;
    ++current.seen; return 1;
}
static size_t fixture(unsigned char *out,unsigned form,unsigned pair,unsigned length,unsigned index) {
    size_t at=0; out[at++]=0xcc;
    const int insert=form>=2, immediate=form&1;
    const unsigned dst=destinations[pair],src=sources[pair];
    out[at++]=insert ? 0xf2 : 0x66;
    const unsigned rex=0x40 | ((insert || !immediate) && dst>=8 ? 4 : 0) |
        ((immediate && !insert ? dst : src)>=8 ? 1 : 0);
    if (rex!=0x40) out[at++]=(unsigned char)rex;
    out[at++]=0x0f; out[at++]=immediate ? 0x78 : 0x79;
    out[at++]=(unsigned char)(0xc0 | ((immediate && !insert ? 0 : dst&7)<<3) |
        (immediate && !insert ? dst&7 : src&7));
    if (immediate) {
        out[at++]=(unsigned char)(length | ((index&3)<<6));
        out[at++]=(unsigned char)(index | ((length&3)<<6));
    }
    const unsigned char conditions[]={0x92,0x94,0x98,0x90,0x9a,0x97};
    for (unsigned i=0;i<6;++i) {
        out[at++]=0x41; out[at++]=0x0f; out[at++]=conditions[i]; out[at++]=(unsigned char)(0xc0+i);
    }
    out[at++]=0xcc; const size_t end=at; out[at++]=0xc3;
    assert(at<=STRIDE); return end;
}
static uint64_t oracle(void) {
    uint64_t destination,source;
    memcpy(&destination,current.initial[destinations[current.pair]],8);
    memcpy(&source,current.initial[sources[current.pair]],8);
    const unsigned length=current.length ? current.length : 64;
    uint64_t result=current.form>=2 ? destination : 0;
    for (unsigned bit=0;bit<length;++bit) {
        if (current.form>=2) {
            const uint64_t target=UINT64_C(1)<<(current.index+bit);
            if ((source>>bit)&1) result|=target; else result&=~target;
        } else if ((destination>>(current.index+bit))&1) result|=UINT64_C(1)<<bit;
    }
    return result;
}
int main(int argc,char **argv) {
    bootstrap_initialize(1); bbcpu_set_trap_handler(trap);
    const size_t slots=FORMS*PAIRS*64*64, bytes=slots*STRIDE+16384;
    unsigned char *code=runtime_low_map(bytes,PROT_READ|PROT_WRITE); assert(code);
    size_t *ends=calloc(slots,sizeof(*ends)); assert(ends);
    for (unsigned f=0;f<FORMS;++f) for (unsigned p=0;p<PAIRS;++p)
        for (unsigned l=0;l<64;++l) for (unsigned i=0;i<64;++i) {
            const size_t slot=(((f*PAIRS+p)*64+l)*64+i);
            ends[slot]=fixture(code+slot*STRIDE,f,p,l,i);
        }
    bbcpu_add_guest_code((uintptr_t)code,bytes);
    const unsigned char cpuid[]={0xb8,1,0,0,0x80,0x31,0xc9,0x0f,0xa2,0x89,0xc8,0xc3};
    memcpy(code+slots*STRIDE,cpuid,sizeof(cpuid));
    assert(bbcpu_call((uintptr_t)code+slots*STRIDE,0,NULL)&(1u<<6));
    if (argc==3 && !strcmp(argv[1],"--undefined")) {
        current.form=(unsigned)strtoul(argv[2],NULL,10); assert(current.form<FORMS);
        current.length=0; current.index=1;
        const size_t slot=current.form*PAIRS*64*64+1;
        current.entry=(uintptr_t)code+slot*STRIDE; current.end=current.entry+ends[slot];
        bbcpu_call(current.entry,0,NULL); return 1;
    }
    size_t checked=0;
    for (unsigned f=0;f<FORMS;++f) for (unsigned p=0;p<PAIRS;++p)
        for (unsigned l=0;l<64;++l) for (unsigned i=0;i<64;++i) {
            if ((l ? l : 64)+i>64) continue;
            const size_t slot=(((f*PAIRS+p)*64+l)*64+i);
            current.form=f; current.pair=p; current.length=l; current.index=i;
            current.entry=(uintptr_t)code+slot*STRIDE; current.end=current.entry+ends[slot];
            const BbInsn *in=&bbcpu_block(current.entry+1)->insn[0];
            assert(in->mnemonic==(f>=2 ? ZYDIS_MNEMONIC_INSERTQ : ZYDIS_MNEMONIC_EXTRQ));
            assert(in->count==(f&1 ? (f>=2 ? 4 : 3) : 2));
            assert(in->op[0].size==8 && !in->fwrites && !in->freads);
            for (unsigned s=0;s<SAMPLES;++s) {
                current.sample=s; current.seen=0;
                bbcpu_call(current.entry,0,NULL); assert(current.seen==2);
                unsigned char expected[16][32]; memcpy(expected,current.initial,sizeof(expected));
                const uint64_t result=oracle(); memcpy(expected[destinations[p]],&result,8);
                if (memcmp(expected,current.output,sizeof(expected))) {
                    fprintf(stderr,"SSE4a mismatch: form=%u pair=%u length=%u index=%u sample=%u\n",f,p,l,i,s);abort();
                }
                const uint64_t flags=initial_flags(); assert(current.flags==flags);
                const uint64_t selected[]={!!(flags&F_CF),!!(flags&F_ZF),!!(flags&F_SF),
                    !!(flags&F_OF),!!(flags&F_PF),!(flags&(F_CF|F_ZF))};
                assert(!memcmp(selected,current.selected,sizeof(selected))); ++checked;
            }
        }
    printf("PASS: %zu synthetic EXTRQ/INSERTQ cases: all defined 6-bit ranges, register/immediate forms, high control bits, aliases/high XMM, full-width fields, untouched vectors/MXCSR/flags and translated flag consumers; SSE4a CPUID retained\n",checked);
    return 0;
}
