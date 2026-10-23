/* Original synthetic field/call observations; no game input or GPU. */
#include "platform.h"
#include "cpu_internal.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
static uintptr_t guarded;
static _Thread_local unsigned guard_queries;
unsigned runtime_memory_trap_reasons(uintptr_t at) { ++guard_queries; return guarded && at>=guarded && at-guarded<8; }
void *runtime_low_map(size_t bytes,int prot) { return bb_low_map(bytes,prot); }
#include "cpu_fault_diagnostics.h"
#include "cpu_field_trace.h"
#define SOTC_DIAGNOSTIC_CONTROL_DUMP(fd) do { sotc_cpu_field_trace_freeze(); sotc_cpu_field_trace_dump_limited((fd),128); } while (0)
#include "diagnostic_control.h"
#undef SOTC_DIAGNOSTIC_CONTROL_DUMP
static unsigned char *code;
static atomic_flag minimum_reporting=ATOMIC_FLAG_INIT;
static void minimum_fault(int sig) {
    const unsigned queries=guard_queries;
    sotc_cpu_field_trace_fatal_minimum(STDERR_FILENO,(unsigned)sig,0x1234,0x5678,0x9abc);
    assert(queries==guard_queries);
    if (atomic_flag_test_and_set(&minimum_reporting)) _Exit(128+sig);
    raise(sig); // Original deterministic nested-fatal control, not a guest fault.
    _Exit(99);
}
static unsigned traps;
static int trap(BbGuestRegs *regs) {
    const uint64_t offset=(regs->rip-(uintptr_t)code)%16384;
    BbTrapState state;assert(bbcpu_trap_state(&state));
    if (offset==1) {
        for(unsigned r=0;r<16;++r) for(unsigned b=0;b<32;++b)
            state.ymm[r][b]=(unsigned char)(r*17+b);
    } else if(offset==11) {
        for(unsigned r=0;r<16;++r) for(unsigned b=0;b<32;++b)
            assert(state.ymm[r][b]==(unsigned char)(r*17+b));
    } else return 0;
    ++traps;return 1;
}
static void reset_ring(void) {
    for(unsigned i=0;i<SOTC_TRACE_RECORDS;++i) {
        atomic_store(&sotc_trace_records[i].sequence,0);
        atomic_store(&sotc_trace_records[i].busy,0);
    }
    atomic_store(&sotc_trace_next,0);atomic_store(&sotc_trace_busy_drops,0);
    atomic_store(&sotc_trace_filtered,0);
    atomic_store(&sotc_trace_frozen,0);
    atomic_store(&sotc_trace_suppressed,0);memset(sotc_trace_seen,0,sizeof(sotc_trace_seen));
}
static void *writer(void *object) {
    uint64_t regs[16]={0};regs[7]=(uintptr_t)object;
    for(unsigned i=0;i<1024;++i) sotc_cpu_field_probe(regs,0x1234,0);
    return NULL;
}
static _Atomic unsigned freeze_ready, freeze_release;
static void *freeze_writer(void *object) {
    uint64_t regs[16]={0};regs[7]=(uintptr_t)object;
    sotc_cpu_field_probe(regs,0x1234,0);
    atomic_fetch_add_explicit(&freeze_ready,1,memory_order_release);
    while (!atomic_load_explicit(&freeze_release,memory_order_acquire)) sched_yield();
    guard_queries=0;errno=ERANGE;
    for (unsigned i=0;i<1000;++i) sotc_cpu_field_probe(regs,0x1234,0);
    assert(!guard_queries && errno==ERANGE && regs[7]==(uintptr_t)object);
    return NULL;
}
static char *dump(size_t *size) {
    FILE *file=tmpfile();assert(file);sotc_cpu_field_trace_dump(fileno(file));
    assert(!fseek(file,0,SEEK_END));long bytes=ftell(file);assert(bytes>=0 && bytes<800000);
    char *text=calloc((size_t)bytes+1,1);assert(text);rewind(file);
    assert(fread(text,1,(size_t)bytes,file)==(size_t)bytes);fclose(file);*size=(size_t)bytes;return text;
}
int main(void) {
    assert(!sotc_cpu_field_trace_init(NULL) && !sotc_trace_site_count);
    assert(!sotc_cpu_field_trace_init("0x40:7:37,0x80:3:-8"));
    const char *bad[]={"1:16:0","1:7:","1:7:0,","1:7:0,1:7:0",
        "-1:7:0","1:7:9223372036854775808","1:7:-9223372036854775809"," 1:7:0",
        "1:7:0:2","1:7:0:","1:7:0:1:1","1:7:0:0:16:0",
        "1:7:0:0:6:","1:7:0:0:6:-1","1:7:0:0:6:0:1"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        assert(sotc_cpu_field_trace_init(bad[i])==-1 && sotc_trace_site_count==2);
    }
    char many[1024]="";
    for(unsigned i=0;i<SOTC_TRACE_SITES;++i) {
        char item[32];snprintf(item,sizeof(item),"%s%u:7:0",i?",":"",i);strcat(many,item);
    }
    assert(!sotc_cpu_field_trace_init(many) && sotc_trace_site_count==SOTC_TRACE_SITES);
    strcat(many,",100:7:0");assert(sotc_cpu_field_trace_init(many)==-1);
    char oversized[2049];memset(oversized,'1',2048);oversized[2048]=0;
    assert(sotc_cpu_field_trace_init(oversized)==-1);
    assert(!sotc_cpu_field_trace_init("0:7:-9223372036854775808"));
    assert(!sotc_cpu_field_trace_init("0:7:37"));
    unsigned char *object=bb_low_map(16384,PROT_READ|PROT_WRITE);assert(object);
    const uint64_t marker=UINT64_C(0x8765432101234567);memcpy(object+37,&marker,8);
    uint64_t regs[16]={0},before[16];regs[7]=(uintptr_t)object;memcpy(before,regs,sizeof(regs));
    if (getenv("SOTC_TEST_FIBER_FAILURE")) {
        for(unsigned i=0;i<300;++i) sotc_cpu_field_probe(regs,0x1234,0);
        SotcFiberFailure evidence={.fiber=0x1234,.stack=(uintptr_t)object,.size=16384,
            .observed=0x4321,.expected=0x4320};
        evidence.regs[4]=(uintptr_t)object+8192;
        if (getenv("SOTC_TEST_FIBER_BAD_BOUNDS")) evidence.size=UINT64_MAX;
        else if (getenv("SOTC_TEST_FIBER_BAD_RSP")) evidence.regs[4]=(uintptr_t)object-16;
        sotc_cpu_field_trace_fiber_failure(STDERR_FILENO,&evidence);
        assert(atomic_load(&sotc_trace_frozen)==1);
        _Exit(23);
    }
    if (getenv("SOTC_TEST_FATAL_MINIMUM")) {
        for(unsigned i=0;i<300;++i) sotc_cpu_field_probe(regs,0x1234,0);
        struct sigaction action={0};action.sa_handler=minimum_fault;action.sa_flags=SA_NODEFER;
        sigemptyset(&action.sa_mask);assert(!sigaction(SIGSEGV,&action,NULL));
        raise(SIGSEGV);return 99;
    }
    if (getenv("SOTC_TEST_COOPERATIVE")) {
        for(unsigned i=0;i<300;++i) {
            const uint64_t value=i+1;memcpy(object+37,&value,sizeof(value));
            sotc_cpu_field_probe(regs,0x1234,0);
        }
        assert(!sotc_diagnostic_control_start());
#ifdef F_GETNOSIGPIPE
        assert(fcntl(STDERR_FILENO,F_GETNOSIGPIPE)==1);
#endif
        puts("FIELD_TRACE_READY");fflush(stdout);
        for (;;) pause();
    }
    errno=EDOM;sotc_cpu_field_probe(regs,0x1234,0);assert(errno==EDOM && !memcmp(before,regs,sizeof(regs)));
    assert(atomic_load(&sotc_trace_records[0].value)==marker &&
           (atomic_load(&sotc_trace_records[0].status)&3)==3);
    guarded=(uintptr_t)object+37;sotc_cpu_field_probe(regs,0x1234,0);
    assert((atomic_load(&sotc_trace_records[1].status)&3)==1);guarded=0;
    regs[7]=UINT64_MAX-3;sotc_cpu_field_probe(regs,0x1234,0);
    assert(!(atomic_load(&sotc_trace_records[2].status)&3));
    regs[7]=0;sotc_cpu_field_probe(regs,0x1234,0);
    assert((atomic_load(&sotc_trace_records[3].status)&3)==1);
    reset_ring();assert(!sotc_cpu_field_trace_init("0:7:37:1"));regs[7]=(uintptr_t)object;
    for(unsigned i=0;i<20;++i) sotc_cpu_field_probe(regs,0x1234,0);
    assert(atomic_load(&sotc_trace_next)==1 && atomic_load(&sotc_trace_suppressed)==19);
    uint64_t changed=marker^17;memcpy(object+37,&changed,8);sotc_cpu_field_probe(regs,0x1234,0);
    assert(atomic_load(&sotc_trace_next)==2 && atomic_load(&sotc_trace_records[1].value)==changed);
    memcpy(object+37,&marker,8);assert(!sotc_cpu_field_trace_init("0:7:37"));
    /* Unrelated formatting traffic must not evict the selected observations,
     * dereference fields or update the on-change cache. */
    reset_ring();sotc_trace_image_base=(uintptr_t)object;
    assert(!sotc_cpu_field_trace_init("0:7:37:1:6:128"));
    regs[7]=(uintptr_t)object;regs[6]=(uintptr_t)object+128;
    sotc_cpu_field_probe(regs,0x1234,0);
    assert(atomic_load(&sotc_trace_next)==1);
    guard_queries=0;regs[7]=UINT64_MAX;regs[6]=(uintptr_t)object+129;errno=ERANGE;
    for(unsigned i=0;i<100000;++i) sotc_cpu_field_probe(regs,0x1234,0);
    assert(errno==ERANGE && !guard_queries && atomic_load(&sotc_trace_next)==1 &&
           atomic_load(&sotc_trace_filtered)==100000 && !atomic_load(&sotc_trace_suppressed));
    regs[7]=(uintptr_t)object;regs[6]=(uintptr_t)object+128;
    sotc_cpu_field_probe(regs,0x1234,0);
    assert(atomic_load(&sotc_trace_next)==1 && atomic_load(&sotc_trace_suppressed)==1);
    memcpy(object+37,&changed,8);sotc_cpu_field_probe(regs,0x1234,0);
    assert(atomic_load(&sotc_trace_next)==2 && atomic_load(&sotc_trace_records[1].value)==changed);
    sotc_trace_image_base=UINT64_MAX-64;guard_queries=0;
    sotc_cpu_field_probe(regs,0x1234,0);
    assert(!guard_queries && atomic_load(&sotc_trace_filtered)==100001);
    sotc_trace_image_base=0;memcpy(object+37,&marker,8);
    assert(!sotc_cpu_field_trace_init("0:7:37"));
    reset_ring();regs[7]=(uintptr_t)object;
    for(unsigned i=0;i<SOTC_TRACE_RECORDS+17;++i) sotc_cpu_field_probe(regs,0x1234,0);
    size_t bytes;char *text=dump(&bytes);unsigned events=0;
    for(char *p=text;(p=strstr(p,"CPU field trace event:"));++p) ++events;
    assert(events==SOTC_TRACE_RECORDS && strstr(text,"older_than_ring=0x11"));free(text);
    uint64_t next=atomic_load(&sotc_trace_next);SotcTraceRecord *slot=&sotc_trace_records[next%SOTC_TRACE_RECORDS];
    atomic_store(&slot->busy,1);sotc_cpu_field_probe(regs,0x1234,0);
    assert(atomic_load(&sotc_trace_busy_drops)==1);atomic_store(&slot->busy,0);
    atomic_store(&slot->sequence,0);text=dump(&bytes);assert(strstr(text,"busy_dropped=0x1"));free(text);
    reset_ring();pthread_t threads[8];
    for(unsigned i=0;i<8;++i)assert(!pthread_create(threads+i,NULL,writer,object));
    /* A concurrent best-effort reader must not wait for writers. */
    text=dump(&bytes);free(text);
    for(unsigned i=0;i<8;++i)assert(!pthread_join(threads[i],NULL));
    assert(atomic_load(&sotc_trace_next)==8192);text=dump(&bytes);free(text);

    /* Eight live producers must retain their pre-stop observations and never
     * read memory/reserve slots on newly entering post-freeze callbacks. */
    reset_ring();
    for(unsigned i=0;i<8;++i)assert(!pthread_create(threads+i,NULL,freeze_writer,object));
    while (atomic_load_explicit(&freeze_ready,memory_order_acquire)!=8) sched_yield();
    sotc_cpu_field_trace_freeze();
    atomic_store_explicit(&freeze_release,1,memory_order_release);
    for(unsigned i=0;i<8;++i)assert(!pthread_join(threads[i],NULL));
    assert(atomic_load(&sotc_trace_next)==8);
    text=dump(&bytes);assert(strstr(text,"frozen=0x1") && strstr(text,"retained=0x8"));free(text);

    /* Independent ordinary function entry and call-continuation sites. The
     * control copy is never probed; both copies execute the same guest bytes. */
    reset_ring();code=bb_low_map(49152,PROT_READ|PROT_WRITE);assert(code);
    const unsigned char caller[]={0xcc,0x53,0x48,0x89,0xfb,0xe8,0x36,0,0,0,
                                  0xcc,0x48,0x8b,0x43,0x18,0x5b,0xc3};
    const unsigned char setter[]={0x48,0x89,0x77,0x18,0xc3};
    for(unsigned copy=0;copy<3;++copy) {
        memcpy(code+copy*16384,caller,sizeof(caller));
        memcpy(code+copy*16384+64,setter,sizeof(setter));
    }
    bbcpu_add_guest_code((uintptr_t)code,49152);
    assert(!sotc_cpu_field_trace_init("0x1:7:24,0x40:7:24,0xa:3:24"));
    sotc_cpu_field_trace_install((uintptr_t)code);bbcpu_set_trap_handler(trap);
    for(unsigned copy=0;copy<2;++copy) for(unsigned sample=0;sample<32;++sample) {
        unsigned char *instance=object+256+sample*64;
        const uint64_t initial=0,expected=marker^sample;
        memset(instance,0x5a,64);memcpy(instance+24,&initial,8);
        const uint64_t args[]={(uintptr_t)instance,expected};traps=0;
        assert(bbcpu_call((uintptr_t)code+copy*16384,2,args)==expected && traps==2);
        uint64_t observed;memcpy(&observed,instance+24,8);assert(observed==expected);
        assert(instance[23]==0x5a && instance[32]==0x5a);
    }
    assert(atomic_load(&sotc_trace_next)==96); // Three probes in only one copy.
    for(unsigned sample=0;sample<32;++sample) {
        assert(atomic_load(&sotc_trace_records[sample*3].value)==0);
        assert(atomic_load(&sotc_trace_records[sample*3+1].value)==0);
        assert(atomic_load(&sotc_trace_records[sample*3+2].value)==(marker^sample));
    }
    /* A fresh third copy exercises matching and rejecting probes through the
     * real CPU callback bridge, including its vector save/restore contract. */
    reset_ring();
    assert(!sotc_cpu_field_trace_init(
        "0x8001:7:24:0:6:128,0x8040:7:24:0:6:128,0x800a:3:24:0:6:128"));
    sotc_cpu_field_trace_install((uintptr_t)code);
    for(unsigned sample=0;sample<32;++sample) {
        unsigned char *instance=object+256+sample*64;memset(instance,0x5a,64);
        const uint64_t initial=0,expected=(uintptr_t)code+128+(sample&1);
        memcpy(instance+24,&initial,8);const uint64_t args[]={(uintptr_t)instance,expected};traps=0;
        assert(bbcpu_call((uintptr_t)code+32768,2,args)==expected && traps==2);
        uint64_t observed;memcpy(&observed,instance+24,8);assert(observed==expected);
        assert(instance[23]==0x5a && instance[32]==0x5a);
    }
    assert(atomic_load(&sotc_trace_next)==48 && atomic_load(&sotc_trace_filtered)==48);
    assert(!sotc_cpu_fault_fields_init(NULL));
    FILE *file=tmpfile();assert(file);BbGuestRegs empty={0};
    sotc_cpu_fault_context(fileno(file),&empty,0,0);fclose(file);
    puts("PASS: original JIT/interpreter probes, 32 moving objects/control copy, preserved vectors/bytes/errno, protected reads/overflow, parser bounds, bounded concurrent ring, 100000 unrelated calls filtered before reads/cache/reservation and changed selected values retained; no game/GPU");
}
