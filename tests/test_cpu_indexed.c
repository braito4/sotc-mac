/* Original scalar memory fixture: independent values and modular-address
 * oracle, not extracted game instructions. No game, graphics or Rosetta. */
#include "bootstrap_contracts.h"
#include "cpu_internal.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static unsigned seen;
static uint64_t expected_index;
static int observe(BbGuestRegs *regs) {
    assert(regs->r[R10]==expected_index);
    ++seen;return 1;
}
int main(void) {
    bootstrap_initialize(1);bbcpu_set_trap_handler(observe);
    const size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *guard=runtime_low_map(3*page,PROT_NONE);assert(guard);
    unsigned char *data=guard+page;assert(!mprotect(data,page,PROT_READ|PROT_WRITE));
    unsigned char *code=runtime_low_map(page,PROT_READ|PROT_WRITE);assert(code);
    /* mov r10,[rdi+16]; and r10,-8; mov rax,[rsi+r10*S+D]; int3; ret */
    const unsigned char fixture[]={0x4c,0x8b,0x57,0x10,0x49,0x83,0xe2,0xf8,
                                  0x4a,0x8b,0x44,0x16,0x18,0xcc,0xc3};
    for (unsigned scale=0;scale<4;++scale) for (unsigned d=0;d<2;++d) {
        unsigned char *fn=code+(scale*2+d)*32;
        memcpy(fn,fixture,sizeof(fixture));fn[11]=(unsigned char)(0x16|(scale<<6));
        fn[12]=d ? 0xe0 : 0x18;
    }
    bbcpu_add_guest_code((uintptr_t)code,page);
    const uint64_t indices[]={0,8,120,UINT64_C(0x100000028),UINT64_C(0x10000000028),
                              UINT64_C(0x8000000000000028),UINT64_MAX-7};
    size_t checked=0;
    for (unsigned scale=0;scale<4;++scale) for (unsigned d=0;d<2;++d)
    for (unsigned index=0;index<sizeof(indices)/sizeof(indices[0]);++index)
    for (unsigned flags=0;flags<8;++flags) for (unsigned alignment=0;alignment<18;++alignment) {
        /* Two exact-width page-edge cases and all 16 ordered-access alignments. */
        unsigned char *target=alignment==16 ? data : alignment==17 ? data+page-8 : data+64+alignment;
        unsigned char metadata[48];memset(metadata,0x5a,sizeof(metadata));
        unsigned char *object=metadata+(alignment&7);
        uint64_t value=indices[index]|flags;memcpy(object+16,&value,8);
        expected_index=indices[index];
        uint64_t expected=UINT64_C(0x8123456789abcdef)^(checked*UINT64_C(0x100000001));
        memcpy(target,&expected,8);
        const uint64_t displacement=d ? (uint64_t)-32 : 24;
        const uint64_t base=(uintptr_t)target-(indices[index]*(UINT64_C(1)<<scale)+displacement);
        const uint64_t args[]={(uintptr_t)object,base};seen=0;
        assert(bbcpu_call((uintptr_t)code+(scale*2+d)*32,2,args)==expected && seen==1);
        uint64_t after;memcpy(&after,object+16,8);assert(after==value);
        ++checked;
    }
    printf("PASS: %zu scalar indexed loads, masks, high bits, wraparound, alignments and protected page edges\n",checked);
    return 0;
}
