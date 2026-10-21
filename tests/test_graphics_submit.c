/* Synthetic submit/flip ABI and metadata validation; never execute GPU commands. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static const char *const submit_names[]={"jRcI8VcgTz4#A#B","Ga6r7H6Y0RI#A#B",
    "zwY0YV91TTI#A#B","xbxNatawohc#A#B"};
static uint64_t arguments[10],received[10];
static unsigned backend_calls;
static int32_t backend_result;
static int missing;
static void unused_provider(void) {}
static int32_t capture_submit(uint32_t workload,uint32_t count,const uint32_t *const *dcb,
                               const uint32_t *sizes,const uint32_t *const *ccb,const uint32_t *csizes) {
    ++backend_calls;
    uint64_t values[]={workload,count,(uintptr_t)dcb,(uintptr_t)sizes,(uintptr_t)ccb,(uintptr_t)csizes};
    memcpy(received,values,sizeof(values));
    if(!count) assert(dcb && sizes && !ccb && !csizes);
    return backend_result;
}
static int32_t capture_flip(uint32_t workload,uint32_t count,const uint32_t *const *dcb,
                             const uint32_t *sizes,const uint32_t *const *ccb,const uint32_t *csizes,
                             uint32_t handle,uint32_t index,uint32_t mode,int64_t argument) {
    (void)capture_submit(workload,count,dcb,sizes,ccb,csizes);
    received[6]=handle;received[7]=index;received[8]=mode;received[9]=(uint64_t)argument;
    return backend_result;
}
static uintptr_t submit_resolver(const char *name) {
    if(!strcmp(name,submit_names[0])) return missing==1 ? 0 : (uintptr_t)capture_submit;
    if(!strcmp(name,submit_names[1])) return missing==2 ? 0 : (uintptr_t)capture_flip;
    return (uintptr_t)unused_provider;
}
/* bbcpu_call takes six register arguments. These original synthetic x86 thunks
 * construct real SysV stack slots for arguments 7..10 before calling the target.
 * The unchanged host bridge must deliver them to native AAPCS registers/stack. */
static uintptr_t submit_bridge(uintptr_t target,unsigned count) {
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);unsigned n=0;
#define BYTES(...) do {const unsigned char b[]={__VA_ARGS__};memcpy(code+n,b,sizeof(b));n+=sizeof(b);} while(0)
    BYTES(0x48,0x83,0xec,0x28); /* sub rsp,40: aligned call + four stack arguments */
    BYTES(0x48,0xb8);uintptr_t p=(uintptr_t)arguments;memcpy(code+n,&p,8);n+=8;
    for(unsigned i=6;i<count;++i) {
        BYTES(0x4c,0x8b,0x50,(unsigned char)(i*8));
        BYTES(0x4c,0x89,0x54,0x24,(unsigned char)((i-6)*8));
    }
    BYTES(0x48,0x8b,0x38,0x48,0x8b,0x70,8,0x48,0x8b,0x50,16,
          0x48,0x8b,0x48,24,0x4c,0x8b,0x40,32,0x4c,0x8b,0x48,40);
    BYTES(0x48,0xb8);memcpy(code+n,&target,8);n+=8;
    BYTES(0xff,0xd0,0x48,0x83,0xc4,0x28,0xc3);
#undef BYTES
    bbcpu_add_guest_code((uintptr_t)code,16384);return (uintptr_t)code;
}
static int32_t invoke_submit(uintptr_t target,const uint64_t args[10]) {
    memcpy(arguments,args,sizeof(arguments));return (int32_t)bbcpu_call(target,0,NULL);
}
/* Shared with original/native invalid-input checks: no backend path is reached. */
static void check_submit_rejections(const uintptr_t targets[2]) {
    const uint32_t *dcb[2]={(const uint32_t *)1,(const uint32_t *)3};
    const uint32_t *ccb[2]={(const uint32_t *)5,(const uint32_t *)7};
    uint32_t sizes[2]={4,4},csizes[2]={0,0};
    *runtime_errno()=17;
    for(unsigned flip=0;flip<2;++flip) {
        uint64_t a[10]={UINT64_MAX,UINT64_C(0x1234000000000001),0,1,1,0,1,2,3,UINT64_MAX};
        assert(invoke_submit(targets[flip],a)==(int32_t)UINT32_C(0x80d11000));
        a[2]=(uintptr_t)dcb;a[3]=0;
        assert(invoke_submit(targets[flip],a)==(int32_t)UINT32_C(0x80d11000));
        a[3]=(uintptr_t)sizes;
        const uint32_t invalid[]={0,0x3ffffd,UINT32_MAX};
        for(unsigned i=0;i<3;++i) {
            sizes[0]=invalid[i];
            assert(invoke_submit(targets[flip],a)==(int32_t)UINT32_C(0x80d11000));
        }
        sizes[0]=4;a[4]=(uintptr_t)ccb;a[5]=(uintptr_t)csizes;
        for(unsigned i=1;i<3;++i) {
            csizes[0]=invalid[i];
            assert(invoke_submit(targets[flip],a)==(int32_t)UINT32_C(0x80d11000));
        }
        csizes[0]=0;
        /* Validate every element: the second DCB/CCB must not escape checks. */
        a[1]=2;sizes[1]=0;
        assert(invoke_submit(targets[flip],a)==(int32_t)UINT32_C(0x80d11000));sizes[1]=4;
        csizes[1]=0x3ffffd;
        assert(invoke_submit(targets[flip],a)==(int32_t)UINT32_C(0x80d11000));csizes[1]=0;
        assert(*runtime_errno()==17);
    }
}
static void check_submit_adapters(const uintptr_t targets[4]) {
    const uint32_t command[2]={UINT32_C(0xc0001000),0}; /* Inert fixture, never submitted. */
    const uint32_t *dcb[2]={command,command},*ccb[2]={command,command};
    uint32_t sizes[2]={4,0x3ffffc},csizes[2]={0,0x3ffffc};
    const int32_t results[]={0,(int32_t)UINT32_C(0x81234567),-1};
    unsigned char snapshot[sizeof(sizes)+sizeof(csizes)];
    memcpy(snapshot,sizes,sizeof(sizes));memcpy(snapshot+sizeof(sizes),csizes,sizeof(csizes));
    *runtime_errno()=17;
    for(unsigned kind=0;kind<4;++kind) for(unsigned result=0;result<3;++result) {
        uint64_t canonical[10]={UINT64_C(0x1234000087654321),UINT64_C(0x5678000000000002),
            (uintptr_t)dcb,(uintptr_t)sizes,(uintptr_t)ccb,(uintptr_t)csizes,
            UINT64_C(0x9abc000080000001),UINT64_C(0xdef0000076543210),UINT64_C(0x1234000000000001),
            result==0 ? UINT64_C(0x8877665544332211) : result==1 ? UINT64_MAX : 0};
        uint64_t a[10];memcpy(a,canonical,sizeof(a));
        if(kind>=2) memmove(a,a+1,9*8); /* Plain exports omit workload. */
        backend_result=results[result];unsigned before=backend_calls;
        assert(invoke_submit(targets[kind],a)==results[result] && backend_calls==before+1);
        assert(received[0]==(kind>=2 ? 2 : (uint32_t)canonical[0]) && received[1]==2);
        for(unsigned i=2;i<6;++i) assert(received[i]==canonical[i]);
        if(kind&1) {
            for(unsigned i=6;i<9;++i) assert(received[i]==(uint32_t)canonical[i]);
            assert(received[9]==canonical[9]);
        }
        assert(!memcmp(snapshot,sizes,sizeof(sizes)) && !memcmp(snapshot+sizeof(sizes),csizes,sizeof(csizes)));
        assert(*runtime_errno()==17);
    }
    /* Count-zero submit reaches the real backend with safe empty storage.
     * Count-zero flip is explicitly rejected before selecting a final DCB. */
    for(unsigned kind=0;kind<4;++kind) {
        uint64_t a[10]={7,0,1,1,1,1,1,1,1,1};if(kind>=2) memmove(a,a+1,9*8);
        unsigned before=backend_calls;backend_result=(int32_t)UINT32_C(0x81234567);
        int32_t got=invoke_submit(targets[kind],a);
        assert(got==((kind&1) ? (int32_t)UINT32_C(0x80d11000) : backend_result));
        assert(backend_calls==before+((kind&1) ? 0 : 1));
    }
    /* Original rejects missing CCB arrays before looking at DCB size entries. */
    for(unsigned flip=0;flip<2;++flip) {
        uint64_t a[10]={0,1,(uintptr_t)dcb,1,0,1,1,1,1,1};unsigned before=backend_calls;
        assert(invoke_submit(targets[flip],a)==(int32_t)UINT32_C(0x80d11000));
        assert(backend_calls==before);
    }
    unsigned before=backend_calls;check_submit_rejections(targets);assert(backend_calls==before);
}
int main(void) {
    bootstrap_initialize(1);
    for(unsigned i=0;i<4;++i) assert(!bootstrap_resolve(submit_names[i],0));
    for(missing=1;missing<=2;++missing) assert(bootstrap_graphics_bind(submit_resolver)==-1);
    missing=0;assert(bootstrap_graphics_bind(submit_resolver)==0);
    uintptr_t targets[4];const unsigned counts[]={6,10,5,9};
    for(unsigned i=0;i<4;++i) {
        assert(bootstrap_graphics_owns(submit_names[i]) && !bootstrap_resolve(submit_names[i],1));
        bootstrap_initialize(0);assert(!bootstrap_resolve(submit_names[i],0));bootstrap_initialize(1);
        char wrong[64];snprintf(wrong,sizeof(wrong),"%.*s#G#H",(int)strcspn(submit_names[i],"#"),submit_names[i]);
        assert(!bootstrap_resolve(wrong,0));
        targets[i]=submit_bridge(bootstrap_resolve(submit_names[i],0),counts[i]);
    }
    check_submit_adapters(targets);
    puts("PASS: four submit/flip adapters: metadata validation/precedence, multi-buffer sizes, empty submissions, 5/6/9/10 argument guest ABI, full-width flip argument, backend errors, guards/errno and scoped activation");
    return 0;
}
