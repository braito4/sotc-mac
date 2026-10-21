/* Synthetic SSE4.1/AVX/AVX2 widening family, including pinned native JIT paths.
 * Byte-copy/sign-fill oracle; guarded exact-width memory and all vector state.
 * No game, hardware x86 oracle or Rosetta. */
#include "bootstrap_contracts.h"
#include "cpu_internal.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#define M(name) ZYDIS_MNEMONIC_##name

enum { PAIRS=6, SIGNS=2, ENCODINGS=3, FORMS=7, SAMPLES=256, STRIDE=48 };
static const unsigned from_bytes[]={1,1,1,2,2,4}, to_bytes[]={2,4,8,4,8,8};
static const unsigned destinations[]={5,5,15,15,5,5,15}, sources[]={2,5,14,15,0,0,0};
static const ZydisMnemonic names[SIGNS][ENCODINGS][PAIRS]={
    {{M(PMOVZXBW),M(PMOVZXBD),M(PMOVZXBQ),M(PMOVZXWD),M(PMOVZXWQ),M(PMOVZXDQ)},
     {M(VPMOVZXBW),M(VPMOVZXBD),M(VPMOVZXBQ),M(VPMOVZXWD),M(VPMOVZXWQ),M(VPMOVZXDQ)},
     {M(VPMOVZXBW),M(VPMOVZXBD),M(VPMOVZXBQ),M(VPMOVZXWD),M(VPMOVZXWQ),M(VPMOVZXDQ)}},
    {{M(PMOVSXBW),M(PMOVSXBD),M(PMOVSXBQ),M(PMOVSXWD),M(PMOVSXWQ),M(PMOVSXDQ)},
     {M(VPMOVSXBW),M(VPMOVSXBD),M(VPMOVSXBQ),M(VPMOVSXWD),M(VPMOVSXWQ),M(VPMOVSXDQ)},
     {M(VPMOVSXBW),M(VPMOVSXBD),M(VPMOVSXBQ),M(VPMOVSXWD),M(VPMOVSXWQ),M(VPMOVSXDQ)}}};
static struct {
    uintptr_t entry,end;
    unsigned pair,sign,encoding,form,sample,seen;
    size_t page;
    unsigned char *memory,*operand,input[16],initial[16][32],output[16][32];
    uint64_t flags,selected[6];
} current;
static unsigned width(void) {return current.encoding==2 ? 32 : 16;}
static unsigned source_width(void) {return width()/to_bytes[current.pair]*from_bytes[current.pair];}
static uint64_t flags(void) {
    static const uint64_t f[]={0,F_ARITH,F_OF|F_SF,F_CF|F_AF,F_ZF|F_PF,F_OF|F_CF,F_SF|F_AF,F_ZF};
    return 2|F_DF|f[current.sample&7];
}
static void input(void) {
    const unsigned from=from_bytes[current.pair], lanes=source_width()/from;
    static const uint32_t boundaries[]={0,1,0x7f,0x80,0xff,0x7fff,0x8000,0xffff,
        0x7fffffff,0x80000000,0xffffffff,0xaaaaaaaa,0x55555555,0x100,0x10000,0x12345678};
    for (unsigned i=0;i<lanes;++i) {
        const uint32_t value=from==1 ? (current.sample+i*17)&255 : current.sample<16 ?
            boundaries[(current.sample+i)%16] : (current.sample*UINT32_C(0x9e3779b9))^(i*UINT32_C(0x85ebca6b));
        for (unsigned b=0;b<from;++b) current.input[i*from+b]=(unsigned char)(value>>(8*b));
    }
}
static int trap(BbGuestRegs *regs) {
    BbTrapState state; assert(bbcpu_trap_state(&state));
    if (regs->rip==current.entry+1) {
        for (unsigned r=0;r<16;++r) for (unsigned b=0;b<32;++b)
            state.ymm[r][b]=(unsigned char)(r*13+b+current.sample);
        if (current.form<4) memcpy(state.ymm[sources[current.form]],current.input,source_width());
        else {
            current.operand=current.form==4 ? current.memory+1 : current.form==5 ?
                current.memory+current.page-source_width() : current.memory;
            memcpy(current.operand,current.input,source_width());
            regs->r[RDI]=(uintptr_t)current.operand;
        }
        *state.flags=flags(); *state.mxcsr=0x9fc0;
        memcpy(current.initial,state.ymm,sizeof(current.initial));
    } else if (regs->rip==current.end) {
        memcpy(current.output,state.ymm,sizeof(current.output)); current.flags=*state.flags;
        for (unsigned i=0;i<6;++i) current.selected[i]=regs->r[R8+i]&255;
        assert(*state.mxcsr==0x9fc0);
        if (current.form>=4) assert(!memcmp(current.operand,current.input,source_width()));
    } else return 0;
    ++current.seen; return 1;
}
static size_t fixture(unsigned char *code,unsigned sign,unsigned encoding,unsigned pair,unsigned form) {
    unsigned at=0;code[at++]=0xcc;
    const unsigned dst=destinations[form],src=sources[form],memory=form>=4;
    if (!encoding) {
        code[at++]=0x66;
        const unsigned rex=0x40|(dst>=8 ? 4 : 0)|(!memory && src>=8 ? 1 : 0);
        if (rex!=0x40) code[at++]=(unsigned char)rex;
        code[at++]=0x0f;code[at++]=0x38;
    } else {
        code[at++]=0xc4;
        code[at++]=(unsigned char)((dst<8 ? 0x80 : 0)|0x40|(memory || src<8 ? 0x20 : 0)|2);
        code[at++]=encoding==2 ? 0x7d : 0x79;
    }
    code[at++]=(unsigned char)((sign ? 0x20 : 0x30)+pair);
    code[at++]=(unsigned char)((memory ? 0 : 0xc0)|((dst&7)<<3)|(memory ? 7 : src&7));
    const unsigned char conditions[]={0x92,0x94,0x98,0x90,0x9a,0x97};
    for (unsigned i=0;i<6;++i) {
        code[at++]=0x41;code[at++]=0x0f;code[at++]=conditions[i];code[at++]=(unsigned char)(0xc0+i);
    }
    code[at++]=0xcc;const size_t end=at;code[at++]=0xc3;assert(at<=STRIDE);return end;
}
int main(void) {
    bootstrap_initialize(1);bbcpu_set_trap_handler(trap);
    current.page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *guard=runtime_low_map(3*current.page,PROT_NONE);assert(guard);
    current.memory=guard+current.page;
    assert(!mprotect(current.memory,current.page,PROT_READ|PROT_WRITE));
    const size_t slots=SIGNS*ENCODINGS*PAIRS*FORMS;
    unsigned char *code=runtime_low_map(slots*STRIDE,PROT_READ|PROT_WRITE);assert(code);
    size_t ends[SIGNS][ENCODINGS][PAIRS][FORMS];
    for (unsigned s=0;s<SIGNS;++s) for (unsigned v=0;v<ENCODINGS;++v)
        for (unsigned p=0;p<PAIRS;++p) for (unsigned f=0;f<FORMS;++f) {
            const size_t slot=(((s*ENCODINGS+v)*PAIRS+p)*FORMS+f);
            ends[s][v][p][f]=fixture(code+slot*STRIDE,s,v,p,f);
        }
    bbcpu_add_guest_code((uintptr_t)code,slots*STRIDE);
    size_t checked=0;
    for (unsigned s=0;s<SIGNS;++s) for (unsigned v=0;v<ENCODINGS;++v)
        for (unsigned p=0;p<PAIRS;++p) for (unsigned f=0;f<FORMS;++f) {
            current.sign=s;current.encoding=v;current.pair=p;current.form=f;
            const size_t slot=(((s*ENCODINGS+v)*PAIRS+p)*FORMS+f);
            current.entry=(uintptr_t)code+slot*STRIDE;current.end=current.entry+ends[s][v][p][f];
            const BbInsn *in=&bbcpu_block(current.entry+1)->insn[0];
            assert(in->mnemonic==names[s][v][p] && in->count==2 && !in->fwrites && !in->freads);
            assert(in->op[0].size==width());
            if (f>=4) assert(in->op[1].type==OP_MEM && in->op[1].size==source_width());
            for (unsigned sample=0;sample<SAMPLES;++sample) {
                current.sample=sample;current.seen=0;input();bbcpu_call(current.entry,0,NULL);assert(current.seen==2);
                unsigned char expected[16][32];memcpy(expected,current.initial,sizeof(expected));
                unsigned char *out=expected[destinations[f]];
                if (v) memset(out,0,32);
                const unsigned from=from_bytes[p],to=to_bytes[p];
                for (unsigned i=0;i<width()/to;++i) {
                    memcpy(out+i*to,current.input+i*from,from);
                    const unsigned fill=s && (current.input[i*from+from-1]&128) ? 255 : 0;
                    memset(out+i*to+from,(int)fill,to-from);
                }
                if (memcmp(current.output,expected,sizeof(expected))) {
                    fprintf(stderr,"Widening mismatch: %s width=%u form=%u sample=%u\n",ZydisMnemonicGetString(names[s][v][p]),width(),f,sample);return 1;
                }
                const uint64_t value=flags();assert(current.flags==value);
                const uint64_t selected[]={!!(value&F_CF),!!(value&F_ZF),!!(value&F_SF),
                    !!(value&F_OF),!!(value&F_PF),!(value&(F_CF|F_ZF))};
                assert(!memcmp(current.selected,selected,sizeof(selected)));++checked;
            }
        }
    printf("PASS: %zu widening cases across all 12 signed/unsigned pairs, SSE/AVX128/AVX256, aliases/high registers, exact-width guarded/unaligned memory, upper lanes and flags/MXCSR; no hardware x86 oracle\n",checked);
    return 0;
}
