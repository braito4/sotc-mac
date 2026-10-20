/* Original synthetic x86 calls and pthread contention; no game input. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
enum { CREATE, DELETE, SET, CLEAR, CANCEL, WAIT, POLL, OPERATIONS };
static uintptr_t entries[OPERATIONS];
static uint32_t call(unsigned op, int n, const uint64_t *args) {
    return (uint32_t)bbcpu_call(entries[op], n, args);
}
static uintptr_t make(uint32_t attr, uint64_t bits) {
    struct { uint64_t head; uintptr_t handle; uint64_t tail; } out = {0xa123, 0, 0xb456};
    const uint64_t args[] = {(uintptr_t)&out.handle, (uintptr_t)"fixture", attr, bits, 0};
    assert(call(CREATE, 5, args) == 0 && out.head == 0xa123 && out.tail == 0xb456);
    assert(out.handle > UINT32_MAX && out.handle < (UINT64_C(1) << 40));
    return out.handle;
}
static uint32_t unary(unsigned op, uintptr_t handle, uint64_t bits) {
    const uint64_t args[] = {handle, bits}; return call(op, op == DELETE ? 1 : 2, args);
}
static uint32_t poll_event(uintptr_t f, uint64_t bits, uint32_t mode, uint64_t *result) {
    const uint64_t args[] = {f, bits, mode, (uintptr_t)result}; return call(POLL, 4, args);
}
static uint32_t wait_event(uintptr_t f, uint64_t bits, uint32_t mode, uint64_t *result, uint32_t *timeout) {
    const uint64_t args[] = {f, bits, mode, (uintptr_t)result, (uintptr_t)timeout}; return call(WAIT, 5, args);
}
typedef struct {
    uintptr_t flag;
    uint64_t bits, result;
    uint32_t mode, timeout, error;
    int timed;
    _Atomic int done;
} Worker;
static void *worker(void *opaque) {
    Worker *w = opaque; *runtime_errno() = 42;
    w->error = wait_event(w->flag, w->bits, w->mode, &w->result, w->timed ? &w->timeout : NULL);
    assert(*runtime_errno() == 42);
    atomic_store(&w->done, 1); return NULL;
}
static void queued(uintptr_t flag, unsigned count) {
    const uint64_t deadline = runtime_wait_clock() + UINT64_C(3000000000);
    while (bootstrap_event_flag_waiters(flag) != count) {
        assert(runtime_wait_clock() < deadline); usleep(1000);
    }
}
void bootstrap_test_event_flags(void) {
    const char *names[] = {"BpFoboUJoZU#0#F", "8mql9OcQnd4#0#F", "IOnSvHzqu6A#0#F",
        "7uhBFWRAS60#0#F", "PZku4ZrXJqg#0#F", "JTvBflhYazQ#0#F", "9lvj5DjHZiA#0#F"};
    unsigned char *code = runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(code);
    for (unsigned i = 0; i < OPERATIONS; ++i) {
        const uintptr_t target = bootstrap_resolve(names[i], 0);
        assert(target && !bootstrap_resolve(names[i], 1));
        unsigned char *p = code + i * 16;
        p[0] = 0x48; p[1] = 0xb8; memcpy(p + 2, &target, 8); p[10] = 0xff; p[11] = 0xe0;
        entries[i] = (uintptr_t)p;
    }
    bbcpu_add_guest_code((uintptr_t)code, OPERATIONS * 16);
    assert(!bootstrap_resolve("BpFoboUJoZU#p#F", 0) && !bootstrap_resolve("BpFoboUJoZU#1#u", 0));
    assert(!bootstrap_resolve("1vDaenmJtyA#0#F", 0) && !bootstrap_resolve("s9-RaxukuzQ#0#F", 0));
    assert(bootstrap_event_flag_objects() == 0);
    uintptr_t out = 0x123456789;
    uint64_t args[] = {(uintptr_t)&out, (uintptr_t)"fixture", 0x21, 0, 0};
    const uint32_t invalid_attrs[] = {3, 4, 0x30, 0x100, UINT32_MAX};
    for (unsigned i = 0; i < sizeof(invalid_attrs) / sizeof(*invalid_attrs); ++i) {
        args[2] = invalid_attrs[i]; assert(call(CREATE, 5, args) == 0x80020016 && out == 0x123456789);
    }
    args[2] = 0x22; assert(call(CREATE, 5, args) == 0x8002002d && out == 0x123456789);
    args[2] = 0x21; args[4] = 1; assert(call(CREATE, 5, args) == 0x80020016); args[4] = 0;
    args[0] = 0; assert(call(CREATE, 5, args) == 0x80020016); args[0] = (uintptr_t)&out;
    args[1] = 0; assert(call(CREATE, 5, args) == 0x80020016);
    char name[33]; memset(name, 'x', 32); name[32] = 0; args[1] = (uintptr_t)name;
    assert(call(CREATE, 5, args) == 0x8002003f && out == 0x123456789);
    name[31] = 0; assert(call(CREATE, 5, args) == 0 && unary(DELETE, out, 0) == 0);
    const uint32_t fifo_attrs[] = {0, 1, 0x10, 0x11, 0x20, 0x21};
    for (unsigned i = 0; i < sizeof(fifo_attrs) / sizeof(*fifo_attrs); ++i)
        assert(unary(DELETE, make(fifo_attrs[i], 0), 0) == 0);
    const uint64_t high = UINT64_C(1) << 63;
    uintptr_t f = make(0x21, high | 7);
    struct { uint64_t head, bits, tail; } result = {0xa123, 0, 0xb456};
    *runtime_errno() = 17;
    assert(poll_event(f, high | 3, 1, &result.bits) == 0 && result.bits == (high | 7));
    assert(poll_event(f, high, 0x22, &result.bits) == 0 && result.bits == (high | 7));
    assert(unary(CLEAR, f, ~UINT64_C(2)) == 0);
    assert(poll_event(f, 5, 0x11, &result.bits) == 0 && result.bits == 5);
    assert(poll_event(f, 1, 2, &result.bits) == 0x80020010 && result.bits == 0);
    const uint32_t modes[] = {0, 3, 0x31, 0x1001, UINT32_MAX};
    for (unsigned i = 0; i < sizeof(modes) / sizeof(*modes); ++i) {
        result.bits = 123; assert(poll_event(f, 1, modes[i], &result.bits) == 0x80020016 && result.bits == 123);
    }
    assert(poll_event(f, 0, 1, &result.bits) == 0x80020016 && result.bits == 123);
    struct { uint32_t head, micros, tail; } timeout = {0xa123, 0, 0xb456};
    assert(wait_event(f, 1, 1, &result.bits, &timeout.micros) == 0x8002003c && result.bits == 0);
    assert(unary(SET, f, high) == 0 && wait_event(f, high, 1, NULL, &timeout.micros) == 0);
    timeout.micros = 5000;
    const uint64_t started = runtime_wait_clock();
    assert(wait_event(f, 1, 1, &result.bits, &timeout.micros) == 0x8002003c);
    const uint64_t elapsed = runtime_wait_clock() - started;
    assert(elapsed >= 4000000 && elapsed < UINT64_C(2000000000) && timeout.micros == 0 && result.bits == high);
    assert(timeout.head == 0xa123 && timeout.tail == 0xb456 && result.head == 0xa123 && result.tail == 0xb456);
    assert(*runtime_errno() == 17 && unary(DELETE, f, 0) == 0);
    assert(unary(SET, f, 1) == 0x80020003 && unary(CLEAR, f, 0) == 0x80020003);
    assert(unary(DELETE, f, 0) == 0x80020003 && poll_event(0xdead, 1, 1, (void *)1) == 0x80020003);

    alarm(20);
    f = make(0x21, 0);
    Worker a = {.flag = f, .bits = 1, .mode = 0x21}, b = a;
    pthread_t first, second;
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(f, 1);
    assert(pthread_create(&second, NULL, worker, &b) == 0); queued(f, 2);
    assert(unary(SET, f, 1) == 0 && pthread_join(first, NULL) == 0);
    assert(a.error == 0 && a.result == 1 && !atomic_load(&b.done)); queued(f, 1);
    assert(poll_event(f, 1, 1, &result.bits) == 0x80020010 && result.bits == 0);
    assert(unary(SET, f, 1) == 0 && pthread_join(second, NULL) == 0 && b.error == 0 && b.result == 1);
    assert(unary(DELETE, f, 0) == 0);

    f = make(0x21, 0);
    a = (Worker){.flag = f, .bits = 3, .mode = 0x11};
    b = (Worker){.flag = f, .bits = 4, .mode = 2};
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(f, 1);
    assert(pthread_create(&second, NULL, worker, &b) == 0); queued(f, 2);
    assert(unary(SET, f, 1) == 0 && !atomic_load(&a.done) && !atomic_load(&b.done));
    assert(unary(SET, f, 4) == 0 && pthread_join(second, NULL) == 0 && b.error == 0 && b.result == 5);
    queued(f, 1); assert(!atomic_load(&a.done));
    assert(unary(SET, f, 2) == 0 && pthread_join(first, NULL) == 0 && a.error == 0 && a.result == 7);
    assert(poll_event(f, 1, 1, &result.bits) == 0x80020010 && result.bits == 0);
    assert(unary(DELETE, f, 0) == 0);

    f = make(0x21, 0);
    a = (Worker){.flag = f, .bits = 1, .mode = 2}; b = a;
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(f, 1);
    assert(pthread_create(&second, NULL, worker, &b) == 0); queued(f, 2);
    assert(unary(SET, f, 1) == 0 && pthread_join(first, NULL) == 0 && pthread_join(second, NULL) == 0);
    assert(a.error == 0 && b.error == 0 && a.result == 1 && b.result == 1);
    assert(unary(DELETE, f, 0) == 0);

    f = make(0x21, 0x80);
    a = (Worker){.flag = f, .bits = 1, .mode = 1}; b = (Worker){.flag = f, .bits = 2, .mode = 2};
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(f, 1);
    assert(pthread_create(&second, NULL, worker, &b) == 0); queued(f, 2);
    struct { int32_t head, count, tail; } count = {0xa123, -1, 0xb456};
    const uint64_t cancel_args[] = {f, 0x41, (uintptr_t)&count.count};
    assert(call(CANCEL, 3, cancel_args) == 0 && count.count == 2 && count.head == 0xa123 && count.tail == 0xb456);
    assert(pthread_join(first, NULL) == 0 && pthread_join(second, NULL) == 0);
    assert(a.error == 0x80020055 && b.error == 0x80020055 && a.result == 0x41 && b.result == 0x41);
    assert(poll_event(f, 1, 1, &result.bits) == 0 && result.bits == 0x41 && unary(DELETE, f, 0) == 0);

    f = make(0x21, 0x80);
    a = (Worker){.flag = f, .bits = 1, .mode = 1}; b = (Worker){.flag = f, .bits = 2, .mode = 2};
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(f, 1);
    assert(pthread_create(&second, NULL, worker, &b) == 0); queued(f, 2);
    assert(unary(DELETE, f, 0) == 0 && unary(SET, f, 1) == 0x80020003);
    const uintptr_t replacement = make(0x21, 0); assert(replacement != f);
    assert(pthread_join(first, NULL) == 0 && pthread_join(second, NULL) == 0);
    assert(a.error == 0x8002000d && b.error == 0x8002000d && a.result == 0x80 && b.result == 0x80);
    assert(unary(DELETE, replacement, 0) == 0 && bootstrap_event_flag_objects() == 0);

    f = make(0x11, 0);
    a = (Worker){.flag = f, .bits = 1, .mode = 1};
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(f, 1);
    assert(unary(SET, f, 2) == 0 && poll_event(f, 2, 1, &result.bits) == 0x80020001);
    assert(unary(SET, f, 1) == 0 && pthread_join(first, NULL) == 0 && a.error == 0 && a.result == 3);
    assert(unary(DELETE, f, 0) == 0);

    f = make(0x21, 0);
    a = (Worker){.flag = f, .bits = high, .mode = 2, .timed = 1, .timeout = 2000000};
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(f, 1);
    assert(unary(SET, f, high) == 0 && pthread_join(first, NULL) == 0);
    assert(a.error == 0 && a.result == high && a.timeout > 0 && a.timeout <= 2000000);
    assert(unary(DELETE, f, 0) == 0 && bootstrap_event_flag_objects() == 0);
    alarm(0);
    puts("PASS: event-flag guest ABI, 64-bit patterns, AND/OR/clear masks, FIFO/single/multiple waits, cancellation/deletion, stale handles, real timeouts and errno");
}
