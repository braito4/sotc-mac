/* Original register XADD fixtures and arithmetic/flag oracle; no game or GPU. */
#include "bootstrap_contracts.h"
#include "cpu_internal.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>

static const unsigned pairs[][2]={{RAX,RAX},{RCX,RCX},{R8,R8},{R15,R15},
    {RAX,RCX},{RCX,RAX},{R8,R15},{R15,R8}};
static const unsigned observed[]={RAX,RCX,R8,R15};
static const uint64_t values[]={0,1,15,UINT64_MAX,UINT64_C(0x7fffffffffffffff),
    UINT64_C(0x8000000000000000),UINT64_C(0x1234567887654321),UINT64_C(0xfedcba9876543210)};
static uintptr_t entry,end;
static unsigned pair,size,sample,seen;
static uint64_t before[16],after[16],flags;
static int trap(BbGuestRegs* regs) {
    BbTrapState state;assert(bbcpu_trap_state(&state));
    if(regs->rip==entry+1) {
        for(unsigned i=0;i<4;++i)regs->r[observed[i]]=UINT64_C(0xdeadbeef76543210)+i;
        regs->r[pairs[pair][0]]=values[sample];
        if(pairs[pair][0]!=pairs[pair][1])regs->r[pairs[pair][1]]=values[(sample+3)%8];
        memcpy(before,regs->r,sizeof(before));*state.flags=2|F_DF|F_ARITH;
    }else if(regs->rip==end) {memcpy(after,regs->r,sizeof(after));flags=*state.flags;}
    else return 0;
    ++seen;return 1;
}
static void write_expected(uint64_t* reg,unsigned bytes,uint64_t value) {
    if(bytes==8)*reg=value;
    else if(bytes==4)*reg=(uint32_t)value;
    else {uint64_t mask=(UINT64_C(1)<<(bytes*8))-1;*reg=(*reg&~mask)|(value&mask);}
}
int main(void) {
    bootstrap_initialize(1);bbcpu_set_trap_handler(trap);
    unsigned char* code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);
    size_t slot=0;
    for(unsigned p=0;p<8;++p)for(unsigned bytes=1;bytes<=8;bytes*=2) {
        unsigned char* fn=code+slot++*16;size_t at=0;fn[at++]=0xcc;
        if(bytes==2)fn[at++]=0x66;
        unsigned rex=0x40|(bytes==8?8:0)|(pairs[p][1]>=8?4:0)|(pairs[p][0]>=8?1:0);
        if(rex!=0x40)fn[at++]=(unsigned char)rex;
        fn[at++]=0x0f;fn[at++]=bytes==1?0xc0:0xc1;
        fn[at++]=(unsigned char)(0xc0|((pairs[p][1]&7)<<3)|(pairs[p][0]&7));
        fn[at++]=0xcc;fn[at++]=0xc3;
    }
    bbcpu_add_guest_code((uintptr_t)code,16384);slot=0;size_t checked=0;
    for(pair=0;pair<8;++pair)for(size=1;size<=8;size*=2) {
        unsigned rex=0x40|(size==8?8:0)|(pairs[pair][1]>=8?4:0)|(pairs[pair][0]>=8?1:0);
        entry=(uintptr_t)code+slot++*16;end=entry+1+(size==2)+(rex!=0x40)+3+1;
        for(sample=0;sample<8;++sample) {
            seen=0;bbcpu_call(entry,0,NULL);assert(seen==2);
            uint64_t mask=size==8?UINT64_MAX:(UINT64_C(1)<<(size*8))-1;
            uint64_t a=before[pairs[pair][0]]&mask,b=before[pairs[pair][1]]&mask;
            uint64_t sum=(a+b)&mask,expected[16];memcpy(expected,before,sizeof(expected));
            write_expected(&expected[pairs[pair][1]],size,a);
            write_expected(&expected[pairs[pair][0]],size,sum);
            uint64_t want=2|F_DF,sign=UINT64_C(1)<<(size*8-1);
            if(a>mask-b)want|=F_CF;
            if(!sum)want|=F_ZF;
            if(sum&sign)want|=F_SF;
            if(((a&sign)==(b&sign))&&((sum&sign)!=(a&sign)))want|=F_OF;
            if((a&15)+(b&15)>15)want|=F_AF;
            unsigned ones=0;for(unsigned bit=0;bit<8;++bit)ones+=(unsigned)((sum>>bit)&1);
            if(!(ones&1))want|=F_PF;
            for(unsigned i=0;i<4;++i)if(after[observed[i]]!=expected[observed[i]]) {
                fprintf(stderr,"FAIL XADD pair=%u bytes=%u sample=%u reg=%u got=%#llx expected=%#llx\n",
                    pair,size,sample,observed[i],(unsigned long long)after[observed[i]],(unsigned long long)expected[observed[i]]);return 1;
            }
            assert((flags&(F_ARITH|F_DF|2))==want);++checked;
        }
    }
    printf("PASS: %zu register XADD alias/distinct cases, 8/16/32/64-bit results and arithmetic flags\n",checked);
    return 0;
}
