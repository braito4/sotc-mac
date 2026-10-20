/* Original synthetic UUID guest ABI, native field layout and concurrent IDs. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

typedef struct {
    uint32_t time_low;
    uint16_t time_mid, time_hi_and_version;
    uint8_t clock_hi, clock_low, node[6];
} GuestUuid;
_Static_assert(sizeof(GuestUuid)==16 && offsetof(GuestUuid,time_hi_and_version)==6, "native uuidgen ABI");
enum { WORKERS=4, SAMPLES=256 };
static GuestUuid samples[WORKERS][SAMPLES];
static uintptr_t entry;
static int32_t call(void *out) {
    uint64_t arg=(uintptr_t)out;
    return (int32_t)bbcpu_call(entry,1,&arg);
}
static void check(const void *bytes) {
    GuestUuid uuid; memcpy(&uuid,bytes,sizeof(uuid));
    assert((uuid.time_hi_and_version >> 12)==4 && (uuid.clock_hi & 0xc0)==0x80);
}
static void *worker(void *opaque) {
    size_t index=(uintptr_t)opaque;
    *runtime_errno()=29;
    for(size_t i=0;i<SAMPLES;++i) {
        assert(call(&samples[index][i])==0);check(&samples[index][i]);
    }
    assert(*runtime_errno()==29);
    return NULL;
}
static int compare(const void *a,const void *b) { return memcmp(a,b,sizeof(GuestUuid)); }
void bootstrap_test_uuid(void) {
    const char *name="Xjoosiw+XPI#0#F";
    uintptr_t fn=bootstrap_resolve(name,0);
    assert(fn && !bootstrap_resolve(name,1) && !bootstrap_resolve("Xjoosiw+XPI#p#F",0));
    assert(!bootstrap_resolve("Xjoosiw+XPI#a#a",0));
    unsigned char *code=runtime_low_map(16384,PROT_READ|PROT_WRITE);assert(code);
    code[0]=0x48;code[1]=0xb8;memcpy(code+2,&fn,8);code[10]=0xff;code[11]=0xe0;
    entry=(uintptr_t)code;bbcpu_add_guest_code(entry,16384);
    *runtime_errno()=17;
    assert((uint32_t)call(NULL)==0x80020016 && *runtime_errno()==17);
    unsigned char output[24];memset(output,0xa5,sizeof(output));
    assert(call(output+1)==0);check(output+1);
    assert(output[0]==0xa5);
    for(size_t i=17;i<sizeof(output);++i) assert(output[i]==0xa5);
    unsigned char *guard=runtime_low_map(32768,PROT_READ|PROT_WRITE);assert(guard);
    assert(!mprotect(guard+16384,16384,PROT_NONE));
    assert(call(guard+16384-16)==0);check(guard+16384-16);
    pthread_t threads[WORKERS];
    for(size_t i=0;i<WORKERS;++i) assert(!pthread_create(&threads[i],NULL,worker,(void *)(uintptr_t)i));
    for(size_t i=0;i<WORKERS;++i) assert(!pthread_join(threads[i],NULL));
    qsort(samples,WORKERS*SAMPLES,sizeof(GuestUuid),compare);
    const unsigned char *sorted=(const unsigned char *)samples;
    for(size_t i=1;i<WORKERS*SAMPLES;++i) assert(memcmp(sorted+(i-1)*16,sorted+i*16,16));
    assert(*runtime_errno()==17);
    puts("PASS: UUID guest ABI, native 16-byte field layout, v4/variant bits, guarded/unaligned output, null/scope errors, concurrent non-repeating samples and errno");
}
