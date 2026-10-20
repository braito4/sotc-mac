/* Original synthetic guest key ABI/errors, capacity and cross-thread reuse. */
#include "bootstrap_contracts.h"
#include "runtime.h"
#include "bbcpu.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

static unsigned char *code;
static size_t offset;
static uintptr_t bridge(const char *name) {
    uintptr_t fn = bootstrap_resolve(name, 0);
    assert(fn && !bootstrap_resolve(name, 1) && offset+16 <= 16384);
    unsigned char *p = code+offset; offset += 16;
    p[0]=0x48; p[1]=0xb8; memcpy(p+2, &fn, 8); p[10]=0xff; p[11]=0xe0;
    return (uintptr_t)p;
}
static uint64_t call(uintptr_t fn, uint64_t a, uint64_t b) {
    uint64_t args[] = {a,b}; return bbcpu_call(fn, 2, args);
}
static uintptr_t get, set;
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int ready, release;
static uint32_t shared_key;
static void *reuse_worker(void *unused) {
    (void)unused;
    assert(call(get,shared_key,0)==0 && call(set,shared_key,0x2222)==0);
    assert(pthread_mutex_lock(&gate)==0);
    ready=1; assert(pthread_cond_broadcast(&changed)==0);
    while (!release) assert(pthread_cond_wait(&changed,&gate)==0);
    assert(pthread_mutex_unlock(&gate)==0);
    assert(call(get,shared_key,0)==0); /* Deleted generation cannot leak its old value. */
    assert(call(set,shared_key,0x3333)==0 && call(get,shared_key,0)==0x3333);
    return NULL;
}
int main(void) {
    bootstrap_initialize(0);
    assert(!bootstrap_resolve("mqULNdimTn0#p#F",0));
    bootstrap_initialize(1);
    assert(!bootstrap_resolve("mqULNdimTn0#0#F",0));
    assert(!bootstrap_resolve("geDaqgH9lTg#p#F",0));
    code=runtime_low_map(16384,PROT_READ|PROT_WRITE); assert(code);
    bbcpu_add_guest_code((uintptr_t)code,16384);
    uintptr_t create=bridge("mqULNdimTn0#p#F"),del=bridge("6BpEZuDT7YI#p#F");
    get=bridge("0-KXaS70xy4#p#F"); set=bridge("WrOLvHU0yQM#p#F");
    uintptr_t kernel_create=bridge("geDaqgH9lTg#0#F"),kernel_del=bridge("PrdHuuDekhY#0#F");
    uintptr_t kernel_set=bridge("+BzXYkqYeLE#0#F"),kernel_get=bridge("eoht7mQOCmo#0#F");
    *runtime_errno()=77;
    assert(call(create,0,0)==22 && (uint32_t)call(kernel_create,0,0)==0x80020016);
    unsigned char output[8];memset(output,0xa5,sizeof(output));
    assert(call(create,(uintptr_t)(output+1),(uintptr_t)main)==22);
    for (size_t i=0;i<sizeof(output);++i) assert(output[i]==0xa5);
    assert(call(create,(uintptr_t)(output+1),0)==0);
    uint32_t key;memcpy(&key,output+1,4);
    assert(output[0]==0xa5 && output[5]==0xa5 && key>0);
    const uint64_t wide=UINT64_C(0x1234567800000000)|key;
    const uint64_t value=UINT64_C(0xfedcba9876543210);
    assert(call(get,key,0)==0 && call(kernel_set,wide,value)==0 && call(get,wide,0)==value);
    assert(call(kernel_get,key,0)==value && call(set,key,0)==0 && call(get,key,0)==0);
    assert(call(del,key,0)==0 && call(del,key,0)==22 && call(set,key,1)==22 && !call(get,key,0));
    assert((uint32_t)call(kernel_del,key,0)==0x80020016);
    assert((uint32_t)call(kernel_set,UINT32_MAX,1)==0x80020016);
    unsigned char *guard=runtime_low_map(32768,PROT_READ|PROT_WRITE);assert(guard);
    assert(!mprotect(guard+16384,16384,PROT_NONE));
    assert(call(kernel_create,(uintptr_t)(guard+16384-4),0)==0);
    memcpy(&key,guard+16384-4,4);assert(call(del,key,0)==0);
    /* Original synthetic destructor: increment the pointed-to uint32_t; ret. */
    uintptr_t destructor=(uintptr_t)(code+1024);
    const unsigned char increment[]={0xff,0x07,0xc3};
    memcpy((void *)destructor,increment,sizeof(increment));
    uint32_t destroyed=0;
    assert(call(create,(uintptr_t)&key,destructor)==0);
    assert(call(set,key,(uintptr_t)&destroyed)==0 && call(del,key,0)==0);
    assert(call(create,(uintptr_t)&key,destructor)==0);
    bootstrap_keys_cleanup();assert(destroyed==0); /* Delete/recreate never calls old destructor. */
    assert(call(set,key,(uintptr_t)&destroyed)==0);
    bootstrap_keys_cleanup();assert(destroyed==1 && call(get,key,0)==0);
    bootstrap_keys_cleanup();assert(destroyed==1 && call(del,key,0)==0);

    assert(call(create,(uintptr_t)&shared_key,0)==0 && call(set,shared_key,0x1111)==0);
    pthread_t worker;assert(!pthread_create(&worker,NULL,reuse_worker,NULL));
    assert(!pthread_mutex_lock(&gate));
    while(!ready) assert(!pthread_cond_wait(&changed,&gate));
    uint32_t replacement;
    assert(call(del,shared_key,0)==0 && call(create,(uintptr_t)&replacement,0)==0);
    assert(replacement==shared_key && call(get,shared_key,0)==0);
    release=1;assert(!pthread_cond_broadcast(&changed) && !pthread_mutex_unlock(&gate));
    assert(!pthread_join(worker,NULL) && call(get,shared_key,0)==0 && call(del,shared_key,0)==0);

    uint32_t all[255];
    for (unsigned i=0;i<255;++i) {
        assert(call(create,(uintptr_t)&all[i],0)==0);
        for (unsigned j=0;j<i;++j) assert(all[i]!=all[j]);
    }
    memset(output,0xa5,sizeof(output));
    assert(call(create,(uintptr_t)(output+1),0)==35);
    assert((uint32_t)call(kernel_create,(uintptr_t)(output+1),0)==0x80020023);
    for (size_t i=0;i<sizeof(output);++i) assert(output[i]==0xa5);
    for(unsigned i=0;i<255;++i) assert(call(del,all[i],0)==0);
    assert(*runtime_errno()==77);
    puts("PASS: POSIX/kernel keys through guest ABI, four-byte guards, full-width values, errors/capacity, per-thread isolation, deleted-slot generations and errno");
}
