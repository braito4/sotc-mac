/* Synthetic guest fiber scheduling tests; no game files, renderer or Rosetta. */
#include "bootstrap_contracts.h"
#include "bootstrap_cpu_context.h"
#include "fiber_fixture.h"
#include "fiber_fixture_code.h"
#include "runtime.h"
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

typedef struct { _Alignas(16) unsigned char data[256]; } PublicFiber;
typedef struct {
    uint64_t size, entry, initialize_arg, stack, stack_size;
    char name[32]; uint64_t margin; unsigned char pad[48];
} Info;
static int32_t (*init)(void *, const char *, uintptr_t, uint64_t, void *, uint64_t, const void *, uint32_t);
static int32_t (*run)(void *, uint64_t, uint64_t *);
static int32_t (*finish)(void *);
static uintptr_t code, run_entry;
static int nested_callback(uint64_t return_fn) {
    return (int)bbcpu_call(code + fixture_nested_offset, 1, &return_fn);
}
static void fault(int sig, siginfo_t *info, void *context) {
    (void)info;
    if (sig == SIGBUS && bbcpu_handle_alignment_fault(context)) return;
    char description[2048];
    if (bbcpu_describe_fault(context, description, sizeof(description)))
        fputs(description, stderr);
    _Exit(128 + sig);
}
static uintptr_t host(const char *name) { uintptr_t fn = bootstrap_resolve(name, 0); assert(fn); return fn; }
static uintptr_t tail(uintptr_t target) {
    static unsigned char *bridges; static size_t used;
    if (!bridges) { bridges = runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(bridges); }
    unsigned char *p = bridges + used; used += 16;
    p[0] = 0x48; p[1] = 0xb8; memcpy(p + 2, &target, 8); p[10] = 0xff; p[11] = 0xe0;
    bbcpu_add_guest_code((uintptr_t)p, 12);
    return (uintptr_t)p;
}
typedef struct { void *fiber; uint64_t tcb, result; } Migration;
static void *migrate(void *opaque) {
    Migration *m = opaque;
    bbcpu_set_thread_pointer((uintptr_t)&m->tcb);
    const uint64_t args[] = {(uintptr_t)run, (uintptr_t)m->fiber, 201, (uintptr_t)&m->result};
    assert(bbcpu_call(run_entry, 4, args) == 0);
    BbCpu after; bootstrap_cpu_save(&after);
    assert(!after.depth && after.fs_base == (uintptr_t)&m->tcb);
    return NULL;
}
static void schedule(int plt, int migration) {
    printf("Checking fibers: PLT=%d migration=%d\n", plt, migration);
    PublicFiber a = {0}, b = {0};
    Fixture x = {0}, y = {0};
    uintptr_t ret = host("B0ZX2hx9DMw#s#C"), sw = host("PFT2S-tJ7Uk#s#C");
    if (plt) { ret = tail(ret); sw = tail(sw); }
    x.return_fn = y.return_fn = ret; x.switch_fn = y.switch_fn = sw;
    x.self_fn = host("p+zLIOg27zU#s#C");
    x.self = (uintptr_t)&a; x.other = (uintptr_t)&b; y.other = (uintptr_t)&a;
    unsigned char *sa = runtime_low_map(65536, PROT_READ | PROT_WRITE);
    unsigned char *sb = runtime_low_map(65536, PROT_READ | PROT_WRITE);
    assert(sa && sb);
    uint64_t opt = 0;
    int32_t (*opt_init)(void *) = (void *)host("asjUJJ+aa8s#s#C");
    assert(opt_init(&opt) == 0 && opt == 0xbb40e64d);
    InitFixture ia = {host("7+OJIpko9RY#s#C"), (uintptr_t)&a, (uintptr_t)"worker A",
        code + fixture_a_offset, (uintptr_t)&x, (uintptr_t)sa, 65536, (uintptr_t)&opt, 0x10, 0x03500000};
    InitFixture ib = {host("hVYD7Ou2pCQ#s#C"), (uintptr_t)&b, (uintptr_t)"worker B",
        code + fixture_b_offset, (uintptr_t)&y, (uintptr_t)sb, 65536, 0, 0, 0x03500000};
    uint64_t arg = (uintptr_t)&ia;
    assert(bbcpu_call(code + fixture_initialize9_offset, 1, &arg) == 0);
    arg = (uintptr_t)&ib;
    assert(bbcpu_call(code + fixture_initialize8_offset, 1, &arg) == 0);
    Info info = {.size = sizeof(info)};
    int32_t (*get_info)(void *, Info *) = (void *)host("uq2Y5BFz0PE#s#C");
    assert(get_info(&a, &info) == 0 && info.margin == 65528 && info.initialize_arg == (uintptr_t)&x);
    assert(info.entry == code + fixture_a_offset && info.stack == (uintptr_t)sa && !strcmp(info.name, "worker A"));
    if (getenv("SOTC_TEST_CORRUPT_FIBER_GUARD")) {
        *(uint64_t *)sa^=1; /* Synthetic overwrite with an otherwise valid guest stack. */
        run(&a,101,NULL);
        _Exit(99); /* A corrupted guard must retain real fatal exit 23. */
    }
    uint64_t returned = 0;
    BbCpu before, after;
    bootstrap_cpu_save(&before);
    assert(run(&a, 101, &returned) == 0 && returned == 11 && x.first == 101);
    bootstrap_cpu_save(&after);
    assert(before.r[RSP] == after.r[RSP] && before.depth == after.depth && before.mxcsr == after.mxcsr);
    assert(x.initial_mxcsr == 0x9fc0);
    uint64_t tcb = 0x123;
    bbcpu_set_thread_pointer((uintptr_t)&tcb);
    if (migration) {
        Migration m = {&a, 0x456, 0}; pthread_t worker;
        assert(pthread_create(&worker, NULL, migrate, &m) == 0 && pthread_join(worker, NULL) == 0);
        assert(m.result == 501 && x.thread == 0x456);
    } else {
        assert(run(&a, 201, &returned) == 0 && returned == 501 && x.thread == 0x123);
    }
    assert(x.resumed == 201 && x.switched == 401 && x.canary == 0x76543210fedcba98ULL);
    assert(x.local == 0x123456789abcdef0ULL && x.mxcsr == 0x3f80 && !x.errors);
    assert(y.first == 301);
    assert(run(&b, 601, &returned) == 0 && returned == 701);
    assert(y.switched == 601 && y.local == 0xabcdef0123456789ULL && y.mxcsr == 0x5f80 && !y.errors);
    info.size = sizeof(info);
    assert(get_info(&a, &info) == 0 && info.margin < 65528 && info.margin > 60000);
    assert(finish(&a) == 0 && finish(&b) == 0);
    assert((uint32_t)finish(&a) == 0x80590006);
    bbcpu_set_thread_pointer(0);
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    struct sigaction action = {.sa_sigaction = fault, .sa_flags = SA_SIGINFO};
    sigemptyset(&action.sa_mask);
    assert(sigaction(SIGSEGV, &action, NULL) == 0 && sigaction(SIGBUS, &action, NULL) == 0);
    bootstrap_initialize(1);
    assert(!bootstrap_resolve("a0LLrZWac0M#s#C", 1) && !bootstrap_resolve("a0LLrZWac0M#0#F", 0));
    code = (uintptr_t)runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(code);
    assert(sizeof(fiber_fixture_code) < 16384);
    memcpy((void *)code, fiber_fixture_code, sizeof(fiber_fixture_code));
    bbcpu_add_guest_code(code, sizeof(fiber_fixture_code)); run_entry = code + fixture_run_offset;
    init = (void *)host("hVYD7Ou2pCQ#s#C"); run = (void *)host("a0LLrZWac0M#s#C");
    finish = (void *)host("JeNX5F-NzQU#s#C");
    int32_t (*load)(uint16_t) = (void *)host("g8cM39EUZ6o#F#G");
    int32_t (*loaded)(uint16_t) = (void *)host("fMP5NHUOaMk#F#G");
    int32_t (*unload)(uint16_t) = (void *)host("eR2bZFAAU0Q#F#G");
    assert(!bootstrap_resolve("g8cM39EUZ6o#M#N", 0));
    assert((uint32_t)load(0) == 0x805a1000 && (uint32_t)loaded(6) == 0x805a1001);
    assert(load(6) == 0 && load(6) == 0 && loaded(6) == 0);
    assert(unload(6) == 0 && loaded(6) == 0 && unload(6) == 0);
    assert((uint32_t)loaded(6) == 0x805a1001 && (uint32_t)unload(6) == 0x805a1001);
    assert((uint32_t)load(0xad) == 0x805a1001 && (uint32_t)loaded(0xad) == 0x805a1001);
    assert((uint32_t)init(NULL, "x", code, 0, NULL, 0, NULL, 0) == 0x80590001);
    PublicFiber f = {0};
    assert((uint32_t)init((void *)(f.data + 1), "x", code, 0, NULL, 0, NULL, 0) == 0x80590002);
    assert((uint32_t)init(&f, "x", code, 0, NULL, 16, NULL, 0) == 0x80590003);
    assert((uint32_t)init(&f, "x", code, 0, NULL, 512, NULL, 0) == 0x80590004);
    assert((uint32_t)run(&f, 0, NULL) == 0x80590004);
    int32_t (*ret)(uint64_t, uint64_t *) = (void *)host("B0ZX2hx9DMw#s#C");
    assert((uint32_t)ret(0, NULL) == 0x80590005);
    schedule(0, 0); schedule(1, 0); schedule(0, 1); schedule(1, 1);
    Fixture restart = {.return_fn = (uintptr_t)ret};
    assert(init(&f, "contextless", code + fixture_contextless_offset, (uintptr_t)&restart, NULL, 0, NULL, 0) == 0);
    for (uint64_t i = 1; i <= 3; ++i) {
        uint64_t returned = 0;
        const uint64_t args[] = {(uintptr_t)run, (uintptr_t)&f, i, (uintptr_t)&returned};
        assert(bbcpu_call(run_entry, 4, args) == 0 && returned == i + 1 && restart.first == i);
    }
    assert(finish(&f) == 0);
    Fixture permissions = {.return_fn = (uintptr_t)ret, .self = (uintptr_t)&f,
        .run_fn = (uintptr_t)run, .switch_fn = host("PFT2S-tJ7Uk#s#C"),
        .other = (uintptr_t)finish, .nested_fn = (uintptr_t)nested_callback};
    assert(init(&f, "attached", code + fixture_permissions_offset, (uintptr_t)&permissions, NULL, 0, NULL, 0) == 0);
    void *attached = runtime_low_map(65536, PROT_READ | PROT_WRITE); assert(attached);
    int32_t (*attach_run)(void *, void *, uint64_t, uint64_t, uint64_t *) = (void *)host("avfGJ94g36Q#s#C");
    uint64_t returned = 0;
    assert((uint32_t)attach_run(&f, (char *)attached + 1, 65536, 0, &returned) == 0x80590002);
    assert(attach_run(&f, attached, 65536, 0, &returned) == 0 && returned == 1000 && !permissions.errors);
    assert((uint32_t)attach_run(&f, attached, 65536, 0, &returned) == 0x80590004);
    assert(finish(&f) == 0);
    puts("PASS: x86-to-ARM64 fiber initialize ABI, yield/resume/switch, register/stack/FPU state, contextless restart and host-thread migration (direct and PLT calls)");
    puts("PASS: fiber attach-context, invalid lifecycle/nested-callback rejection, and truthful system-module load/unload availability");
}
