/* Synthetic generated-store observer controls; no guest game data or GPU. */
#include "bootstrap_contracts.h"
#include "cpu_internal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

typedef struct { uintptr_t address; uint64_t low,high,size,rip; unsigned route; } Event;
static Event events[4];
static unsigned event_count;
void *runtime_low_map(size_t size,int prot) {
    return mmap(NULL,size,prot,MAP_PRIVATE|MAP_ANON,-1,0);
}
static void put64(uint8_t *p,uint64_t x) { memcpy(p,&x,8); }
void sotc_cpu_shader_write_observe(uintptr_t address,uint64_t low,uint64_t high,
        uint64_t size,uint64_t rip,unsigned route) {
    if(event_count<4)events[event_count++]=(Event){address,low,high,size,rip,route};
}
int main(int argc,char **argv) {
    assert(argc==2);const int trace=!strcmp(argv[1],"trace");
    if(trace)setenv("SOTC_SHADER_COPY_TRACE","1",1);else unsetenv("SOTC_SHADER_COPY_TRACE");
    uint8_t *memory=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);assert(memory!=MAP_FAILED);
    uint8_t *code=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);assert(code!=MAP_FAILED);
    uintptr_t target=(uintptr_t)memory;size_t at=0;
    /* mov rax,target; mov rcx,imm64; mov [rax],rcx; ret */
    code[at++]=0x48;code[at++]=0xb8;put64(code+at,target);at+=8;
    code[at++]=0x48;code[at++]=0xb9;put64(code+at,UINT64_C(0x1122334455667788));at+=8;
    uintptr_t scalar_rip=(uintptr_t)(code+at);
    code[at++]=0x48;code[at++]=0x89;code[at++]=0x08;code[at++]=0xc3;
    bbcpu_add_guest_code((uintptr_t)code,4096);bbcpu_call((uintptr_t)code,0,NULL);
    assert(*(uint64_t*)memory==UINT64_C(0x1122334455667788));
    if(trace&&getenv("BB_JIT")[0]!='0') {
        assert(event_count==1);assert(events[0].address==target&&events[0].size==8&&events[0].low==UINT64_C(0x1122334455667788));
        assert(events[0].rip==scalar_rip&&events[0].route==1);
    } else assert(event_count==0);

    memset(memory,0,32);event_count=0;at=64;
    /* mov rax,target+8; movdqu [rax],xmm0; ret */
    code[at++]=0x48;code[at++]=0xb8;put64(code+at,target+8);at+=8;
    uintptr_t vector_rip=(uintptr_t)(code+at);
    code[at++]=0xf3;code[at++]=0x0f;code[at++]=0x7f;code[at++]=0x00;code[at++]=0xc3;
    uint8_t xmm[8][16]={{0}};const uint64_t vlo=UINT64_C(0xa1a2a3a4a5a6a7a8),vhi=UINT64_C(0xb1b2b3b4b5b6b7b8);
    memcpy(xmm[0],&vlo,8);memcpy(xmm[0]+8,&vhi,8);
    uint64_t in[6]={0},out[2]={0};uint8_t out_xmm[2][16]={{0}};
    bbcpu_call_full((uintptr_t)(code+64),in,(const uint8_t (*)[16])xmm,out,out_xmm);
    assert(!memcmp(memory+8,xmm[0],16));
    if(trace&&getenv("BB_JIT")[0]!='0') {
        assert(event_count==1);assert(events[0].address==target+8&&events[0].size==16);
        if(events[0].low!=vlo||events[0].high!=vhi||events[0].rip!=vector_rip||events[0].route!=2)
            fprintf(stderr,"vector event low=%#llx high=%#llx rip=%#llx expected=%#llx route=%u\n",
                (unsigned long long)events[0].low,(unsigned long long)events[0].high,
                (unsigned long long)events[0].rip,(unsigned long long)vector_rip,events[0].route);
        assert(events[0].low==vlo&&events[0].high==vhi&&events[0].rip==vector_rip&&events[0].route==2);
    } else assert(event_count==0);

    /* Misaligned 8-byte scalar store uses the block's out-of-line plain path. */
    memset(memory,0,32);event_count=0;at=128;
    munmap(code,4096);code=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);assert(code!=MAP_FAILED);
    bbcpu_add_guest_code((uintptr_t)code,4096);
    code[at++]=0x48;code[at++]=0xb8;put64(code+at,target+1);at+=8;
    code[at++]=0x48;code[at++]=0xb9;put64(code+at,UINT64_C(0x8877665544332211));at+=8;
    uintptr_t slow_rip=(uintptr_t)(code+at);
    code[at++]=0x48;code[at++]=0x89;code[at++]=0x08;code[at++]=0xc3;
    bbcpu_call((uintptr_t)(code+128),0,NULL);
    uint64_t expected=UINT64_C(0x8877665544332211);
    assert(!memcmp(memory+1,&expected,8));
    if(trace&&getenv("BB_JIT")[0]!='0') {
        unsigned matches=0;for(unsigned i=0;i<event_count;++i)matches+=events[i].address==target+1&&events[i].size==8&&events[i].low==expected&&events[i].rip==slow_rip&&events[i].route==1;
        assert(matches==1);
    } else assert(event_count==0);
    printf("PASS: generated scalar/vector stores preserve memory results; trace=%d jit=%s events=%u\n",
        trace,getenv("BB_JIT"),event_count);
    return 0;
}
