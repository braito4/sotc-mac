/* Original synthetic guest-thread tests. No game executable or shared-memory pool. */
#include "bootstrap_contracts.h"
#include "bootstrap_cpu_context.h"
#include "thread_fixture.h"
#include "thread_fixture_code.h"
#include "runtime.h"
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static uintptr_t code;
static uint64_t main_numeric_id;
static uintptr_t host(const char *name) { uintptr_t result = bootstrap_resolve(name, 0); assert(result); return result; }
typedef int32_t (*Create)(void **, void **, uintptr_t, void *, const char *);
typedef int32_t (*Join)(void *, void **);
typedef int32_t (*Slot)(void **);
static Create create;
static Join join;
static Slot lock_guest, unlock_guest, broadcast;
static uintptr_t key_get, key_set;
static uint32_t key;
static int32_t (*wait_guest)(void **, void **);
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    unsigned completed;
} JoinGate;
typedef struct { JoinGate *gate; void *thread, *value; int32_t error; } JoinAttempt;
static void *join_competitor(void *opaque) {
    JoinAttempt *a = opaque;
    a->error = join(a->thread, &a->value);
    assert(pthread_mutex_lock(&a->gate->mutex) == 0);
    ++a->gate->completed;
    assert(pthread_cond_broadcast(&a->gate->changed) == 0);
    assert(pthread_mutex_unlock(&a->gate->mutex) == 0);
    return NULL;
}
static void initialize_fixture(ThreadFixture *f, ThreadShared *shared, uint64_t tag) {
    *f = (ThreadFixture){
        .numeric_id_fn=host("EI-5-jlq2dE#0#F"),
        .tls_fn = host("vNe1w4diLCs#0#F"), .self_fn = host("EotR8a3ASf4#0#F"),
        .join_fn = (uintptr_t)join, .exit_fn = host("3kg7rT0NQIs#0#F"),
        .errno_fn = host("9BcDykPmo1I#0#F"), .lock_fn = (uintptr_t)lock_guest,
        .unlock_fn = (uintptr_t)unlock_guest, .wait_fn = (uintptr_t)wait_guest,
        .signal_fn = (uintptr_t)broadcast, .shared = shared, .tag = tag,
        .path = "/app0/content.bin", .open_fn = host("1G3lF1Gg1k8#0#F"),
        .read_fn = host("Cg4srZ6TKbU#0#F"), .pread_fn = host("+r3rMFwItV4#0#F"),
        .close_fn = host("UK2Tl2DWUns#0#F"), .stat_fn = host("eV9wAD2riIA#0#F"),
        .key_get_fn = key_get, .key_set_fn = key_set, .key = key, .destructor_limit = 2,
    };
}
static void check_fixture(const ThreadFixture *f, void *handle) {
    assert(f->numeric_id_start>0 && f->numeric_id_start<=INT32_MAX);
    assert(f->numeric_id_start==f->numeric_id_end && f->numeric_id_start!=main_numeric_id);
    assert(!f->errors && f->self == (uintptr_t)handle && f->tcb && f->dtv && f->dtv_valid);
    assert(f->tcb < (UINT64_C(1) << 40) && f->tls < (UINT64_C(1) << 40));
    assert(f->stack_pointer < (UINT64_C(1) << 40));
    assert(f->initial_static == 0x1122334455667788ULL && f->final_static == f->tag + 100);
    assert(f->initial_module == 0xaabbccddeeff0011ULL && !f->initial_errno);
    assert(f->self_join_error == UINT32_C(0x8002000b) && f->inherited_mxcsr == 0x3f80);
    assert(f->reads == 32);
    assert(!f->destructor_errors && f->destructor_calls ==
           (f->destructor_limit < 4 ? f->destructor_limit : 4));
}
static void release_workers(ThreadShared *shared, unsigned ready) {
    assert(lock_guest(&shared->mutex) == 0);
    while (shared->ready < ready) assert(wait_guest(&shared->condition, &shared->mutex) == 0);
    shared->release = 1;
    assert(broadcast(&shared->condition) == 0 && unlock_guest(&shared->mutex) == 0);
}
static void fault(int sig, siginfo_t *info, void *context) {
    (void)info;
    if (sig == SIGBUS && bbcpu_handle_alignment_fault(context)) return;
    char description[2048];
    if (bbcpu_describe_fault(context, description, sizeof(description))) fputs(description, stderr);
    _Exit(128 + sig);
}
int main(int argc, char **argv) {
    assert(argc==1 || (argc==2 && !strcmp(argv[1],"--recursive-key-exit-check")));
    setvbuf(stdout, NULL, _IONBF, 0);
    struct sigaction action = {.sa_sigaction = fault, .sa_flags = SA_SIGINFO}; sigemptyset(&action.sa_mask);
    assert(sigaction(SIGSEGV, &action, NULL) == 0 && sigaction(SIGBUS, &action, NULL) == 0);
    alarm(15);
    bootstrap_initialize(1);
    assert(!bootstrap_resolve("6UgtwV+0zb4#1#u", 0) && !bootstrap_resolve("6UgtwV+0zb4#0#F", 1));
    assert(bootstrap_resolve("geDaqgH9lTg#0#F", 0));
    assert(!bootstrap_resolve("EotR8a3ASf4#1#u",0) && !bootstrap_resolve("EotR8a3ASf4#0#F",1));
    bootstrap_initialize(0);assert(!bootstrap_resolve("EI-5-jlq2dE#0#F",0));bootstrap_initialize(1);
    assert(!bootstrap_resolve("EI-5-jlq2dE#1#u",0) && !bootstrap_resolve("EI-5-jlq2dE#0#F",1));
    assert(!bootstrap_resolve("EI-5-jlq2dE#p#F",0)); /* Kernel scope only. */
    static const uint64_t main_tls[4] = {0, 0, 0x1122334455667788ULL, 0};
    static const uint64_t libc_tls = 0xaabbccddeeff0011ULL;
    static const uint32_t fios_tls = 0x13572468;
    runtime_set_main_tls(main_tls, sizeof(main_tls), sizeof(main_tls), 16);
    bootstrap_set_module_tls(2, &libc_tls, sizeof(libc_tls), 64);
    bootstrap_set_module_tls(3, &fios_tls, sizeof(fios_tls), 32);
    runtime_thread_attach_main();
    bootstrap_cpu_set_fpu(0x3f80, 0x37f, 0);
    *runtime_errno() = 77;
    uint64_t index[] = {2, 0};
    uint64_t *parent_tls = bootstrap_tls_get_addr(index); *parent_tls = 0x55;
    index[0] = 3; uint64_t *parent_other = bootstrap_tls_get_addr(index); *parent_other = 0x66;
    assert(bootstrap_tls_live_blocks() == 2);
    BbCpu parent; bootstrap_cpu_save(&parent);
    char root[] = "/tmp/sotc-threads-XXXXXX", path[512]; assert(mkdtemp(root));
    assert(snprintf(path, sizeof(path), "%s/Content.bin", root) < (int)sizeof(path));
    const unsigned char content[] = {'a', 0, 'b', 'c'};
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(fd >= 0 && write(fd, content, sizeof(content)) == (ssize_t)sizeof(content) && close(fd) == 0);
    assert(bootstrap_mount_game_root(root) == 0);
    code = (uintptr_t)runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(code);
    assert(sizeof(thread_fixture_code) < 16384); memcpy((void *)code, thread_fixture_code, sizeof(thread_fixture_code));
    bbcpu_add_guest_code(code, sizeof(thread_fixture_code));
    const uint64_t numeric_argument[]={host("EI-5-jlq2dE#0#F")};
    main_numeric_id=bbcpu_call(code+thread_numeric_identity_offset,1,numeric_argument);
    assert(main_numeric_id>0 && main_numeric_id<=INT32_MAX && *runtime_errno()==77);
    assert(bbcpu_call(code+thread_numeric_identity_offset,1,numeric_argument)==main_numeric_id);
    const char *self_names[]={"EotR8a3ASf4#0#F","EotR8a3ASf4#p#F","aI+OeCz8xrQ#0#F"};
    const uintptr_t parent_identity=(uintptr_t)((void *(*)(void))host("aI+OeCz8xrQ#0#F"))();
    assert(parent_identity);
    for(unsigned i=0;i<sizeof(self_names)/sizeof(*self_names);++i) {
        const uint64_t argument[]={host(self_names[i])};
        assert(bbcpu_call(code+thread_identity_offset,1,argument)==parent_identity);
        assert(*runtime_errno()==77);
    }
    const char *yield_names[]={"6XG4B33N09g#0#F","6XG4B33N09g#p#F"};
    for(unsigned i=0;i<sizeof(yield_names)/sizeof(*yield_names);++i) {
        const uint64_t argument[]={host(yield_names[i])};
        assert(!bootstrap_resolve(yield_names[i],1));
        for(unsigned n=0;n<32;++n) {
            assert((int32_t)bbcpu_call(code+thread_numeric_identity_offset,1,argument)==0);
            assert(*runtime_errno()==77);
        }
        assert(bbcpu_call(code+thread_numeric_identity_offset,1,numeric_argument)==main_numeric_id);
    }
    assert(!bootstrap_resolve("6XG4B33N09g#1#u",0));
    bootstrap_initialize(0);
    assert(!bootstrap_resolve(yield_names[0],0) && !bootstrap_resolve(yield_names[1],0));
    bootstrap_initialize(1);
    puts("PASS: scoped sched_yield calls the native scheduler through the guest ABI, preserving guest errno and thread identity");
    key_get = host("0-KXaS70xy4#p#F"); key_set = host("WrOLvHU0yQM#p#F");
    int32_t (*key_create)(uint32_t *, uintptr_t) = (void *)host("mqULNdimTn0#p#F");
    int32_t (*key_delete)(uint32_t) = (void *)host("6BpEZuDT7YI#p#F");
    assert(key_create(&key, code+thread_key_destructor_offset) == 0);
    assert(((int32_t (*)(uint32_t, void *))key_set)(key, (void *)0x1234) == 0);
    create = (Create)host("6UgtwV+0zb4#0#F"); join = (Join)host("onNY9Byn-W8#0#F");
    lock_guest = (Slot)host("9UK1vLZQft4#0#F"); unlock_guest = (Slot)host("tn3VlD0hG60#0#F");
    broadcast = (Slot)host("JGgj7Uvrl+A#0#F");
    wait_guest = (void *)host("WKAXJ4XBPQ4#0#F");
    int32_t (*mutex_init)(void **, void **, const char *) = (void *)host("cmo1RIYva9o#0#F");
    int32_t (*cond_init)(void **, void **, const char *) = (void *)host("2Tb92quprl0#0#F");
    Slot mutex_destroy = (Slot)host("2Of0f+3mhhE#0#F"), cond_destroy = (Slot)host("g+PZd2hiacg#0#F");
    Slot attr_init = (Slot)host("nsYoNRywwNg#0#F"), attr_destroy = (Slot)host("62KCwEMmzcM#0#F");
    int32_t (*stack_size)(void **, uint64_t) = (void *)host("UTXzJbWhhTE#0#F");
    void *attr = NULL; assert(attr_init(&attr) == 0 && stack_size(&attr, 16384) == 0);
    ThreadShared shared = {0};
    assert(mutex_init(&shared.mutex, NULL, "fixture") == 0 && cond_init(&shared.condition, NULL, "fixture") == 0);
    ThreadFixture a, b; initialize_fixture(&a, &shared, 101); initialize_fixture(&b, &shared, 202); b.explicit_exit = 1;
    b.destructor_limit = 100; /* A perpetually rearming callback stops after four passes. */
    if (argc==2) b.destructor_limit=0;
    void *one = NULL, *two = NULL, *result = NULL;
    assert((uint32_t)create(NULL, NULL, code, &a, NULL) == 0x80020016);
    assert((uint32_t)create(&one, NULL, (uintptr_t)main, &a, NULL) == 0x80020016);
    assert((uint32_t)join((void *)(uintptr_t)1, NULL) == 0x80020003);
    assert(create(&one, &attr, code + thread_worker_offset, &a, "fixture-return") == 0);
    assert(create(&two, NULL, code + thread_worker_offset, &b, "fixture-exit") == 0);
    assert(attr_destroy(&attr) == 0);
    assert(lock_guest(&shared.mutex) == 0);
    while (shared.ready < 2) assert(wait_guest(&shared.condition, &shared.mutex) == 0);
    assert(bootstrap_tls_live_blocks() == 6 && bootstrap_threads_live() == 2);
    assert(a.numeric_id_start!=b.numeric_id_start);
    assert(a.tls != b.tls && a.tcb != b.tcb && a.dtv != b.dtv && a.other_tls != b.other_tls);
    assert(a.tls != (uintptr_t)parent_tls && a.other_tls != (uintptr_t)parent_other && a.tcb != parent.tcb);
    shared.release = 1;
    assert(broadcast(&shared.condition) == 0 && unlock_guest(&shared.mutex) == 0);
    assert(join(one, &result) == 0 && (uintptr_t)result == 101 + 0x2000);
    assert(join(two, &result) == 0 && (uintptr_t)result == 202 + 0x1000);
    check_fixture(&a, one); check_fixture(&b, two);
    assert((uint32_t)join(one, NULL) == 0x80020016);
    assert(!bootstrap_threads_live() && bootstrap_tls_live_blocks() == 2);
    assert(((void *(*)(uint32_t))key_get)(key) == (void *)0x1234);
    assert(*parent_tls == 0x55 && *parent_other == 0x66 && *runtime_errno() == 77);
    BbCpu after; bootstrap_cpu_save(&after);
    assert(after.tcb == parent.tcb && after.mxcsr == parent.mxcsr);

    const uint64_t retired_ids[]={a.numeric_id_start,b.numeric_id_start};
    /* POSIX entry points and detached completion use the same checked backend. */
    int32_t (*posix_create)(void **, void **, uintptr_t, void *) = (void *)host("OxhIB8LB-PQ#p#F");
    Join posix_join = (Join)host("h9CcP3J0oVM#p#F");
    assert(posix_create(NULL, NULL, code, &a) == 22 && posix_join((void *)(uintptr_t)1, NULL) == 3);
    shared.ready = 0; shared.release = 1; initialize_fixture(&a, &shared, 303);
    assert(posix_create(&one, NULL, code + thread_worker_offset, &a) == 0);
    assert(posix_join(one, &result) == 0 && (uintptr_t)result == 303 + 0x2000);
    check_fixture(&a, one);
    assert(a.numeric_id_start!=retired_ids[0] && a.numeric_id_start!=retired_ids[1]);
    int32_t (*detach)(void *) = (void *)host("4qGrR6eoP9Y#0#F");
    shared.ready = 0; shared.release = 0; initialize_fixture(&a, &shared, 404);
    assert(create(&one, NULL, code + thread_worker_offset, &a, "fixture-detach") == 0);
    assert(detach(one) == 0 && (uint32_t)detach(one) == 0x80020016);
    assert((uint32_t)join(one, NULL) == 0x80020016);
    release_workers(&shared, 1);
    for (unsigned n = 0; bootstrap_threads_live() && n < 2000; ++n) {
        struct timespec pause = {0, 1000000}; nanosleep(&pause, NULL);
    }
    assert(!bootstrap_threads_live() && bootstrap_tls_live_blocks() == 2);
    check_fixture(&a, one);
    shared.ready = 0; shared.release = 0; initialize_fixture(&a, &shared, 505);
    assert(create(&one, NULL, code + thread_worker_offset, &a, "fixture-double-join") == 0);
    JoinGate gate = {.mutex = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    JoinAttempt attempts[2] = {{.gate = &gate, .thread = one}, {.gate = &gate, .thread = one}};
    pthread_t competitors[2];
    assert(pthread_create(&competitors[0], NULL, join_competitor, &attempts[0]) == 0);
    assert(pthread_create(&competitors[1], NULL, join_competitor, &attempts[1]) == 0);
    assert(pthread_mutex_lock(&gate.mutex) == 0);
    while (!gate.completed) assert(pthread_cond_wait(&gate.changed, &gate.mutex) == 0);
    assert(gate.completed == 1); /* The owner waits; the competing join is refused. */
    assert(pthread_mutex_unlock(&gate.mutex) == 0);
    release_workers(&shared, 1);
    assert(pthread_join(competitors[0], NULL) == 0 && pthread_join(competitors[1], NULL) == 0);
    assert((attempts[0].error == 0 && (uint32_t)attempts[1].error == 0x80020016) ||
           (attempts[1].error == 0 && (uint32_t)attempts[0].error == 0x80020016));
    assert((uintptr_t)(attempts[0].error ? attempts[1].value : attempts[0].value) == 505 + 0x2000);
    assert(pthread_cond_destroy(&gate.changed) == 0 && pthread_mutex_destroy(&gate.mutex) == 0);
    check_fixture(&a, one);
    assert(!bootstrap_threads_live() && bootstrap_tls_live_blocks() == 2);
    assert(cond_destroy(&shared.condition) == 0 && mutex_destroy(&shared.mutex) == 0);
    bootstrap_unmount_game_root(); assert(unlink(path) == 0 && rmdir(root) == 0);
    assert(key_delete(key) == 0);
    alarm(0);
    puts("PASS: guest threads return/exit/join/detach, kernel/POSIX self aliases, handle identity and stable unique numeric IDs (main/workers/destructors), independent TLS/keys/errno, guest key destructors with live TLS and four-pass rearming, DTV, low stacks, inherited FPU, synchronization and read-only I/O");
}
