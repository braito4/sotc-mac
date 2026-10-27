/* Original CPU comparison: loops, nested calls, flags and guarded output.
 * No game instructions or GPU. */
#include "platform.h"
#include "cpu_internal.h"
#include "cpu_interp_region.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

uint64_t bb_image_base;
void *runtime_low_map(size_t n,int prot) {return bb_low_map(n,prot);}
extern uint64_t sotc_cpu_region_step_count(void);
extern void sotc_cpu_region_dump(void);
static _Thread_local uint64_t initial[16];
static _Thread_local unsigned visits;
static unsigned char *code;
static int concurrent_snapshot;
static void snapshot_signal(int signum) {(void)signum;sotc_cpu_region_dump();}
static void observe(const uint64_t regs[16],uint64_t at,uint32_t tag) {
    (void)at;
    if(tag==0)memcpy(initial,regs,sizeof(initial));
    else if(tag==1) {
        const unsigned keep[]={RBX,RBP,RSP,RDI,R12,R13,R14,R15};
        for(unsigned i=0;i<sizeof(keep)/sizeof(*keep);++i)assert(regs[keep[i]]==initial[keep[i]]);
        assert(regs[RAX]==22);
    } else {
        const uint64_t before[]={5,12,20};
        assert(regs[RCX]==3-visits%3 && regs[RAX]==before[visits%3]);
        ++visits;
    }
}
static void *worker(void *unused) {
    (void)unused;
    for(unsigned n=0;n<40;++n) {
        uint64_t words[]={0x12345678,0,0,0,0xabcdef01};
        const uint64_t arg=(uintptr_t)(words+1);
        assert(bbcpu_call((uintptr_t)code,1,&arg)==22);
        assert(words[0]==0x12345678 && words[4]==0xabcdef01);
        assert(words[1]==22 && words[2]==15 && words[3]==7);
        if(concurrent_snapshot && n==39)raise(SIGUSR1);
    }
    assert(visits==120);return NULL;
}
static void fault(int signum) { sotc_cpu_region_dump(); _Exit(signum==SIGSEGV || signum==SIGBUS ? 139 : 99); }
static void parser_controls(void) {
    SotcCpuInterpRegion r;
    assert(!sotc_cpu_region_parse(NULL,&r) && !r.enabled);
    const char *bad[]={"","0:1","0x:0x2","0x1:0x1","0x2:0x1","0x0:0x10001",
        "-0x1:0x2","0x1:0x2 ","0x1:0x2,0x3:0x4","0x0:0x20000001",
        "0x00000000000000000:0x1","0x0:0x1:0x2","0x1:"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i)assert(sotc_cpu_region_parse(bad[i],&r));
    assert(!sotc_cpu_region_parse("0x0:0x1",&r));
    assert(sotc_cpu_region_contains(&r,4096,4096));
    assert(!sotc_cpu_region_contains(&r,4095,4096));
    assert(!sotc_cpu_region_contains(&r,4097,4096));
    assert(sotc_cpu_region_contains(&r,UINT64_MAX,UINT64_MAX));
}
static int field578_fixture(const char *mode) {
    const unsigned char body[]={0x48,0x8b,0x8f,0x78,0x05,0x00,0x00,
        0x48,0x8b,0x31,0x48,0x89,0xf0,0xc3};
    memcpy(code+512,body,sizeof(body));
    unsigned char object[0x580+8]={0};
    uint64_t value=UINT64_C(0x8309008f80),field_value=(uintptr_t)&value;
    if(!strcmp(mode,"field-invalid"))field_value=1;
    memcpy(object+0x578,&field_value,sizeof(field_value));
    if(!strcmp(mode,"field-load"))assert(!setenv("SOTC_CPU_INTERPRET_RANGE","0x200:0x207",1));
    else if(!strcmp(mode,"field-consumer") || !strcmp(mode,"field-invalid"))
        assert(!setenv("SOTC_CPU_INTERPRET_RANGE","0x207:0x20a",1));
    else if(!strcmp(mode,"field-both"))assert(!setenv("SOTC_CPU_INTERPRET_RANGE","0x200:0x20a",1));
    else if(!strcmp(mode,"field-base"))unsetenv("SOTC_CPU_INTERPRET_RANGE");
    else return 0;
    if(!strcmp(mode,"field-invalid")) {
        signal(SIGSEGV,fault);signal(SIGBUS,fault);
    }
    const uint64_t arg=(uintptr_t)object;
    const uint64_t result=bbcpu_call((uintptr_t)code+512,1,&arg);
    assert(strcmp(mode,"field-invalid")!=0);
    assert(result==value && field_value==(uintptr_t)&value);
    sotc_cpu_region_dump();
    printf("PASS: field578 %s result=%#llx\n",mode,(unsigned long long)result);
    return 1;
}
static int object_selection_fixture(const char *mode) {
    if(strcmp(mode,"selection-base") && strcmp(mode,"selection-interpreted"))return 0;
    /* Match the prepared image's two selected instructions exactly:
     * mov rax,[r14+0x2008]; mov rdi,[rax+rbx*8].  The short prologue places
     * fixed call arguments in the corresponding guest registers. */
    static const unsigned char body[]={
        0x49,0x89,0xfe,                         /* mov r14,rdi */
        0x48,0x89,0xf3,                         /* mov rbx,rsi */
        0x49,0x8b,0x86,0x08,0x20,0x00,0x00,   /* selected begin */
        0x48,0x8b,0x3c,0xd8,                   /* selected end */
        0x48,0x89,0xf8,0xc3                    /* return selected object */
    };
    memcpy(code+768,body,sizeof(body));
    uint64_t table[4]={UINT64_C(0x1111222233334444),UINT64_C(0x5555666677778888),
                       UINT64_C(0x9999aaaabbbbcccc),UINT64_C(0xddddeeeeffff0000)};
    unsigned char *root=calloc(1,0x2010);assert(root);
    memcpy(root+0x2008,&(uint64_t){(uintptr_t)table},sizeof(uint64_t));
    if(!strcmp(mode,"selection-interpreted"))
        assert(!setenv("SOTC_CPU_INTERPRET_RANGE","0x306:0x311",1));
    else unsetenv("SOTC_CPU_INTERPRET_RANGE");
    uint64_t args[]={(uintptr_t)root,2};
    const uint64_t result=bbcpu_call((uintptr_t)code+768,2,args);
    assert(result==table[2]);
    sotc_cpu_region_dump();
    printf("PASS: object selection %s result=%#llx\n",mode,(unsigned long long)result);
    free(root);
    return 1;
}
int main(int argc,char **argv) {
    assert(argc>=2);parser_controls();
    const char *mode=argv[1];unsigned per_call=0;char range[64];
    const size_t page=(size_t)sysconf(_SC_PAGESIZE);
    code=bb_low_map(page,PROT_READ|PROT_WRITE);assert(code);bb_image_base=(uintptr_t)code;
    /* main: three calls, accumulating into three guarded output words. */
    const unsigned char main_code[]={0xb9,3,0,0,0,0x31,0xc0,0xe8,52,0,0,0,
        0x48,0x89,0x44,0xcf,0xf8,0xff,0xc9,0x75,0xf2,0xc3};
    /* callee: conditional arithmetic and a nested helper call. */
    const unsigned char callee[]={0x53,0x89,0xcb,0x83,0xc0,5,0x83,0xf9,2,0x75,3,
        0x83,0xc0,1,0x51,0xe8,44,0,0,0,0x59,0x39,0xcb,0x75,11,0x5b,0xc3};
    const unsigned char failure[]={0xb8,0xad,0xde,0,0,0x5b,0xc3};
    const unsigned char helper[]={0x83,0xc0,2,0xc3};
    memcpy(code,main_code,sizeof(main_code));memcpy(code+64,callee,sizeof(callee));
    memcpy(code+100,failure,sizeof(failure));memcpy(code+128,helper,sizeof(helper));
    static const unsigned char field_code[]={0x48,0x8b,0x8f,0x78,0x05,0x00,0x00,
        0x48,0x8b,0x31,0x48,0x89,0xf0,0xc3};
    memcpy(code+512,field_code,sizeof(field_code));
    bb_image_base=(uintptr_t)code;bbcpu_add_guest_code((uintptr_t)code,page);
    /* Decode before selection; every trial starts a fresh process. */
    assert(bbcpu_block((uintptr_t)code+64)->count>2);
    if(field578_fixture(argv[1]))return 0;
    if(object_selection_fixture(argv[1]))return 0;
    unsigned begin=0,end=0;
    if(!strcmp(mode,"caller")) {begin=0;end=22;per_call=15;}
    else if(!strcmp(mode,"callee")) {begin=64;end=91;per_call=37;}
    else if(!strcmp(mode,"helper")) {begin=128;end=132;per_call=6;}
    else if(!strcmp(mode,"partial")) {begin=67;end=70;per_call=3;}
    else if(!strcmp(mode,"branch")) {begin=73;end=75;per_call=3;}
    else if(!strcmp(mode,"fault")) {
        const unsigned char access[]={0x31,0xc0,0x89,0,0xc3};
        memcpy(code+256,access,sizeof(access));begin=256;end=261;
    } else if(!strcmp(mode,"unsupported")) {code[256]=0x0f;code[257]=0x0b;begin=256;end=258;}
    else assert(!strcmp(mode,"none") || !strcmp(mode,"invalid"));
    if(end) {snprintf(range,sizeof(range),"0x%x:0x%x",begin,end);assert(!setenv("SOTC_CPU_INTERPRET_RANGE",range,1));}
    else if(strcmp(mode,"invalid"))unsetenv("SOTC_CPU_INTERPRET_RANGE");
    if(!strcmp(mode,"fault")) {
        signal(SIGSEGV,fault);signal(SIGBUS,fault);
        bbcpu_call((uintptr_t)code+256,0,NULL);return 98;
    }
    if(!strcmp(mode,"unsupported")) {bbcpu_call((uintptr_t)code+256,0,NULL);return 98;}
    bbcpu_set_probe((uintptr_t)code,observe,0);
    bbcpu_set_probe((uintptr_t)code+21,observe,1);
    bbcpu_set_probe((uintptr_t)code+70,observe,2);
    concurrent_snapshot=argc>2 && !strcmp(argv[2],"snapshot");
    if(concurrent_snapshot)signal(SIGUSR1,snapshot_signal);
    unsigned count=argc>2 ? 4 : 1;
    if(count==1)worker(NULL);
    else {
        pthread_t threads[4];
        for(unsigned i=0;i<count;++i)assert(!pthread_create(threads+i,NULL,worker,NULL));
        for(unsigned i=0;i<count;++i)assert(!pthread_join(threads[i],NULL));
    }
    sotc_cpu_region_dump();
    const int jit=!getenv("BB_JIT") || strcmp(getenv("BB_JIT"),"0");
    assert(sotc_cpu_region_step_count()==(jit ? (uint64_t)per_call*count*40 : 0));
    if(jit) {
        if(!strcmp(mode,"callee"))assert(!bbcpu_block((uintptr_t)code+64)->jit_code);
        else assert(bbcpu_block((uintptr_t)code+64)->jit_code);
        if(!strcmp(mode,"helper"))assert(!bbcpu_block((uintptr_t)code+128)->jit_code);
        else assert(bbcpu_block((uintptr_t)code+128)->jit_code);
    }
    printf("PASS: region %s, workers %u, selected steps %llu; flags/calls/returns/guards agree\n",
        mode,count,(unsigned long long)sotc_cpu_region_step_count());
    return 0;
}
