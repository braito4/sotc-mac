/* Original CPU-only interior-probe/flags/loop control. No game instructions. */
#include "platform.h"
#include "cpu_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

uint64_t bb_image_base;
void *runtime_low_map(size_t bytes,int prot) {return bb_low_map(bytes,prot);}
static unsigned hits[4];
static void observe(const uint64_t regs[16],uint64_t at,uint32_t tag) {
    (void)at;assert(tag<4);
    if(tag==0 || tag==3)assert(regs[RAX]==7 && regs[RDI]==11);
    if(tag==1)assert(regs[RAX]==10);
    if(tag==2)assert(regs[RCX]==3-hits[2] && regs[RAX]==12+2*hits[2]);
    ++hits[tag];
}
int main(int argc,char **argv) {
    const int probes=argc>1 && strcmp(argv[1],"none");
    const int missed=argc>1 && !strcmp(argv[1],"missed");
    const size_t page=(size_t)sysconf(_SC_PAGESIZE);
    unsigned char *code=bb_low_map(page,PROT_READ|PROT_WRITE);assert(code);
    /* mov eax,7; cmp edi,11; jne failure; add eax,3; mov ecx,3;
     * loop: add eax,2; dec ecx; jnz loop; ret; failure: mov eax,0xdead; ret */
    const unsigned char body[]={0xb8,7,0,0,0,0x83,0xff,11,0x75,16,
        0x83,0xc0,3,0xb9,3,0,0,0,0x83,0xc0,2,0xff,0xc9,0x75,0xf9,0xc3,
        0xb8,0xad,0xde,0,0,0xc3};
    memcpy(code,body,sizeof(body));bbcpu_add_guest_code((uintptr_t)code,page);
    /* Pre-decoding must not make registration before execution ineffective. */
    assert(bbcpu_block((uintptr_t)code)->count>1);
    if(probes) {
        bbcpu_set_probe((uintptr_t)code+8,observe,0);
        bbcpu_set_probe((uintptr_t)code+13,observe,1);
        bbcpu_set_probe((uintptr_t)code+21,observe,2);
        bbcpu_set_probe((uintptr_t)code+8,observe,3);
    }
    const uint64_t arg=11;
    assert(bbcpu_call((uintptr_t)code,1,&arg)==16);
    if(!probes || missed)assert(!hits[0] && !hits[1] && !hits[2] && !hits[3]);
    else assert(hits[0]==1 && hits[1]==1 && hits[2]==3 && hits[3]==1);
    printf("PASS: flags/result/registers; interior hits=%u,%u,%u,%u\n",hits[0],hits[1],hits[2],hits[3]);
    return 0;
}
