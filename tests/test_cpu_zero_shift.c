/* Original scalar shift fixtures. No game code, graphics or host x86 execution. */
#include "bootstrap_contracts.h"
#include "cpu_internal.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>

static uint64_t initial, result, flags;
static unsigned count, seen;
static unsigned destination;
static const unsigned destinations[]={RAX,RCX,R8,R15};
static uintptr_t entry, end;
static int observe(BbGuestRegs *regs) {
    BbTrapState state; assert(bbcpu_trap_state(&state));
    if (regs->rip==entry+1) {
        regs->r[RCX]=count; regs->r[destination]=initial;
        *state.flags=2|F_DF|F_ARITH;
    } else if (regs->rip==end) {
        result=regs->r[destination];flags=*state.flags;
    } else return 0;
    ++seen;return 1;
}
int main(void) {
    bootstrap_initialize(1); bbcpu_set_trap_handler(observe);
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);
    const unsigned counts[]={0,32,64,96,128,224,1,31,63};
    const uint64_t values[]={UINT64_C(0x1234567887654321),UINT64_MAX,UINT64_C(0x8000000080000000)};
    size_t slot=0,checked=0;
    for(unsigned d=0;d<4;++d) for(unsigned wide=0;wide<2;++wide) for(unsigned cl=0;cl<2;++cl)
    for(unsigned op=0;op<3;++op) for(unsigned c=0;c<sizeof(counts)/sizeof(*counts);++c) {
        unsigned char *fn=code+slot++*16;size_t at=0;fn[at++]=0xcc;
        if(wide||destinations[d]>=8)fn[at++]=(unsigned char)(0x40|(wide?8:0)|(destinations[d]>=8));
        fn[at++]=cl?0xd3:0xc1;fn[at++]=(unsigned char)((op==2?0xf8:0xe0+op*8)|(destinations[d]&7));
        if(!cl)fn[at++]=(unsigned char)counts[c];
        fn[at++]=0xcc;fn[at++]=0xc3;
    }
    bbcpu_add_guest_code((uintptr_t)code,16384);slot=0;
    for(unsigned d=0;d<4;++d) for(unsigned wide=0;wide<2;++wide) for(unsigned cl=0;cl<2;++cl)
    for(unsigned op=0;op<3;++op) for(unsigned c=0;c<sizeof(counts)/sizeof(*counts);++c) {
        destination=destinations[d];
        entry=(uintptr_t)code+slot++*16;end=entry+1+(wide||destination>=8)+2+!cl+1;count=counts[c];
        unsigned masked=count&(wide?63:31);
        for(unsigned v=0;v<sizeof(values)/sizeof(*values);++v) {
            initial=values[v];
            if(cl&&destination==RCX)initial=(initial&~UINT64_C(255))|count;
            seen=0;bbcpu_call(entry,0,NULL);assert(seen==2);
            uint64_t a=wide?initial:(uint32_t)initial, expected;
            if(op==0)expected=a<<masked;
            else if(op==1)expected=a>>masked;
            else expected=wide?(uint64_t)((int64_t)a>>masked):(uint32_t)((int32_t)a>>masked);
            if(!wide)expected=(uint32_t)expected;
            if(result!=expected) {
                fprintf(stderr,"FAIL shift wide=%u cl=%u op=%u count=%u input=%#llx got=%#llx expected=%#llx\n",
                    wide,cl,op,count,(unsigned long long)initial,(unsigned long long)result,(unsigned long long)expected);
                return 1;
            }
            if(!masked)assert(flags==(2|F_DF|F_ARITH));
            ++checked;
        }
    }
    printf("PASS: %zu scalar shifts, masked zero counts, upper halves and preserved zero-count flags\n",checked);
    return 0;
}
