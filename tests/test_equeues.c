/* Original synthetic guest ABI and concurrent queue-lifetime fixtures. */
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
enum { CREATE, DELETE, WAIT, ADD, EDGE, TRIGGER, REMOVE, UDATA, ID, FILTER, DATA, OPS };
static uintptr_t entries[OPS];
typedef struct {
    uint64_t id;
    int16_t filter;
    uint16_t flags;
    uint32_t fflags;
    uint64_t data, udata;
} Event;
_Static_assert(sizeof(Event) == 32, "event ABI");
static uint64_t call(unsigned op, int n, const uint64_t *args) { return bbcpu_call(entries[op], n, args); }
static uint32_t operation(unsigned op, uintptr_t q, int32_t id, uint64_t data) {
    const uint64_t args[] = {q, (uint32_t)id, data}; return (uint32_t)call(op, 3, args);
}
static uintptr_t make(void) {
    struct { uint64_t head; uintptr_t q; uint64_t tail; } o = {11, 0, 22};
    const uint64_t args[] = {(uintptr_t)&o.q, (uintptr_t)"fixture"};
    assert(call(CREATE, 2, args) == 0 && o.q && o.head == 11 && o.tail == 22); return o.q;
}
static uint32_t wait_events(uintptr_t q, Event *events, int32_t capacity, int32_t *out, const uint32_t *timeout) {
    const uint64_t args[] = {q, (uintptr_t)events, (uint32_t)capacity, (uintptr_t)out, (uintptr_t)timeout};
    return (uint32_t)call(WAIT, 5, args);
}
static void event_is(const Event *e, uint64_t id, uint16_t flags, uint64_t udata) {
    assert(e->id == id && e->filter == -11 && e->flags == flags && !e->fflags && !e->data && e->udata == udata);
}
typedef struct { uintptr_t q; Event event; int32_t count; uint32_t error; _Atomic int done; } Worker;
static void *worker(void *opaque) {
    Worker *w = opaque; *runtime_errno() = 43;
    w->error = wait_events(w->q, &w->event, 1, &w->count, NULL);
    assert(*runtime_errno() == 43); atomic_store(&w->done, 1); return NULL;
}
static void queued(uintptr_t q, unsigned n) {
    const uint64_t deadline = runtime_wait_clock() + UINT64_C(3000000000);
    while (bootstrap_equeue_waiters(q) != n) { assert(runtime_wait_clock() < deadline); usleep(1000); }
}
void bootstrap_test_equeues(void) {
    const char *names[] = {"D0OdFMjp46I#0#F", "jpFjmgAC5AE#0#F", "fzyMKs9kim0#0#F",
        "4R6-OvI2cEA#0#F", "WDszmSbWuDk#0#F", "F6e0kwo4cnk#0#F", "LJDwdSNTnDg#0#F",
        "vz+pg2zdopI#0#F", "mJ7aghmgvfc#0#F", "23CPPI1tyBY#0#F", "kwGyyjohI50#0#F"};
    unsigned char *code = runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(code);
    for (unsigned i = 0; i < OPS; ++i) {
        uintptr_t fn = bootstrap_resolve(names[i], 0); assert(fn && !bootstrap_resolve(names[i], 1));
        unsigned char *p = code + 16 * i; p[0] = 0x48; p[1] = 0xb8;
        memcpy(p + 2, &fn, 8); p[10] = 0xff; p[11] = 0xe0; entries[i] = (uintptr_t)p;
    }
    bbcpu_add_guest_code((uintptr_t)code, OPS * 16);
    assert(!bootstrap_resolve("D0OdFMjp46I#1#u", 0));
    assert(!bootstrap_resolve("57ZK+ODEXWY#0#F", 0)); /* Timer remains unresolved. */
    assert(bootstrap_equeue_objects() == 0);
    *runtime_errno() = 17; alarm(20);
    uintptr_t handle = 0x12345678;
    uint64_t args[] = {(uintptr_t)&handle, 0};
    assert((uint32_t)call(CREATE, 2, args) == 0x80020016 && handle == 0x12345678);
    args[0] = 0; args[1] = (uintptr_t)"fixture";
    assert((uint32_t)call(CREATE, 2, args) == 0x80020016);
    char name[34]; memset(name, 'x', 33); name[33] = 0;
    args[0] = (uintptr_t)&handle; args[1] = (uintptr_t)name;
    assert((uint32_t)call(CREATE, 2, args) == 0x8002003f && handle == 0x12345678);
    name[32] = 0; assert(call(CREATE, 2, args) == 0);
    assert(operation(DELETE, handle, 0, 0) == 0);
    uintptr_t q = make(), other = make(); assert(q != other && q != handle);
    struct { uint64_t head; Event events[2]; uint64_t tail; } out;
    memset(&out, 0xa5, sizeof(out));
    struct { int32_t head, n, tail; } count = {11, -1, 22};
    uint32_t poll = 0;
    assert(wait_events(q, out.events, 2, &count.n, &poll) == 0x8002003c && count.n == 0);
    Event unchanged; memset(&unchanged, 0xa5, sizeof(unchanged));
    assert(!memcmp(&out.events[0], &unchanged, sizeof(Event)));
    count.n = 99;
    assert(wait_events(0, NULL, 0, NULL, (void *)1) == 0x80020009);
    assert(wait_events(q, NULL, 1, &count.n, &poll) == 0x8002000e && count.n == 99);
    assert(wait_events(q, out.events, 1, NULL, &poll) == 0x8002000e);
    assert(wait_events(q, out.events, 0, &count.n, &poll) == 0x80020016 && count.n == 0);
    assert(wait_events(q, out.events, -1, &count.n, &poll) == 0x80020016);
    for (unsigned op = ADD; op <= REMOVE; ++op) assert(operation(op, 0, 3, 0) == 0x80020009);
    assert(operation(TRIGGER, q, 3, 0) == 0x80020002 && operation(REMOVE, q, 3, 0) == 0x80020002);
    assert(operation(ADD, q, -7, 0) == 0 && operation(EDGE, q, 8, 0) == 0);
    assert(operation(TRIGGER, q, 8, 111) == 0 && operation(TRIGGER, q, -7, UINT64_C(0x123456789)) == 0);
    assert(wait_events(q, out.events, 1, &count.n, &poll) == 0 && count.n == 1);
    event_is(out.events, (uint64_t)(int64_t)-7, 0, UINT64_C(0x123456789));
    assert(wait_events(q, out.events, 2, &count.n, &poll) == 0 && count.n == 2);
    event_is(out.events + 1, 8, 0x20, 111);
    assert(wait_events(q, out.events, 2, &count.n, &poll) == 0 && count.n == 1);
    assert(operation(REMOVE, q, -7, 0) == 0);
    assert(wait_events(q, out.events, 2, &count.n, &poll) == 0x8002003c);
    /* Edge triggers coalesce; latest user data wins. Duplicate add preserves
       pending status and original edge/level mode while resetting user data. */
    assert(operation(TRIGGER, q, 8, 222) == 0 && operation(TRIGGER, q, 8, 333) == 0);
    assert(operation(ADD, q, 8, 0) == 0);
    assert(wait_events(q, out.events, 2, &count.n, &poll) == 0 && count.n == 1);
    event_is(out.events, 8, 0x20, 0);
    assert(wait_events(q, out.events, 2, &count.n, &poll) == 0x8002003c);
    assert(operation(TRIGGER, q, 8, 444) == 0);
    assert(wait_events(other, out.events, 2, &count.n, &poll) == 0x8002003c);
    assert(operation(REMOVE, q, 8, 0) == 0);
    assert(wait_events(q, out.events, 2, &count.n, &poll) == 0x8002003c);
    const uint32_t timeout = 12000;
    const uint64_t start = runtime_wait_clock();
    assert(wait_events(q, out.events, 2, &count.n, &timeout) == 0x8002003c);
    assert(runtime_wait_clock() - start >= UINT64_C(12000000) && timeout == 12000);
    assert(count.head == 11 && count.tail == 22 && out.head == UINT64_C(0xa5a5a5a5a5a5a5a5) && out.tail == out.head);
    Event e = {.id = UINT64_C(0xfedcba9876543210), .filter = -14,
               .data = UINT64_C(0x8877665544332211), .udata = UINT64_C(0x123456789)};
    const uint64_t get[] = {(uintptr_t)&e};
    assert(call(ID, 1, get) == e.id && (int32_t)call(FILTER, 1, get) == -14);
    assert(call(DATA, 1, get) == e.data && call(UDATA, 1, get) == e.udata);
    /* Two consumers of an edge event: exactly one is released per trigger. */
    assert(operation(EDGE, q, 1, 0) == 0);
    Worker a = {.q = q}, b = {.q = q}; pthread_t first, second;
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(q, 1);
    assert(pthread_create(&second, NULL, worker, &b) == 0); queued(q, 2);
    assert(operation(TRIGGER, q, 1, 555) == 0); queued(q, 1);
    assert(operation(TRIGGER, q, 1, 555) == 0);
    assert(pthread_join(first, NULL) == 0 && pthread_join(second, NULL) == 0);
    assert(a.error == 0 && b.error == 0 && a.count == 1 && b.count == 1);
    event_is(&a.event, 1, 0x20, 555); event_is(&b.event, 1, 0x20, 555);
    /* Level event stays ready for both consumers. */
    assert(operation(ADD, q, 2, 0) == 0);
    a = (Worker){.q = q}; b = (Worker){.q = q};
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(q, 1);
    assert(pthread_create(&second, NULL, worker, &b) == 0); queued(q, 2);
    assert(operation(TRIGGER, q, 2, 666) == 0);
    assert(pthread_join(first, NULL) == 0 && pthread_join(second, NULL) == 0);
    assert(a.error == 0 && b.error == 0); event_is(&a.event, 2, 0, 666); event_is(&b.event, 2, 0, 666);
    assert(operation(REMOVE, q, 2, 0) == 0);
    /* Delete wakes all waiters, with no dangling registry object. */
    a = (Worker){.q = q}; b = (Worker){.q = q};
    assert(pthread_create(&first, NULL, worker, &a) == 0); queued(q, 1);
    assert(pthread_create(&second, NULL, worker, &b) == 0); queued(q, 2);
    assert(operation(DELETE, q, 0, 0) == 0);
    assert(pthread_join(first, NULL) == 0 && pthread_join(second, NULL) == 0);
    assert(a.error == 0x80020009 && b.error == 0x80020009 && a.count == 0 && b.count == 0);
    assert(bootstrap_equeue_objects() == 1 && operation(DELETE, q, 0, 0) == 0x80020009);
    assert(wait_events(q, out.events, 1, &count.n, &poll) == 0x80020009);
    assert(operation(DELETE, other, 0, 0) == 0 && bootstrap_equeue_objects() == 0);
    assert(*runtime_errno() == 17); alarm(0);
    puts("PASS: equeue guest ABI/layout, lifecycle/errors, level/edge users, coalescing/batching, getters, const microsecond timeouts, contention, deletion with blocked waiters and errno");
}
