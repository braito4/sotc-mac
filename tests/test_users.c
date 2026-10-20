/* Original synthetic UserService guest calls. No game, account or user files. */
#include "bootstrap_contracts.h"
#include "bbcpu.h"
#include "runtime.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

enum { INIT, TERM, INITIAL, LIST, NAME, EVENT, OPERATIONS };
static uintptr_t entries[OPERATIONS];
static const char *names[OPERATIONS] = {"j3YMu1MVNNo#I#J", "bwFjS+bX9mA#I#J",
    "CdWp0oHWGr0#I#J", "fPhymKNvK-A#I#J", "1xxcMiGu2fo#I#J", "yH17Q6NWtVg#I#J"};
#define NOT_INIT UINT32_C(0x80960002)
#define ALREADY_INIT UINT32_C(0x80960003)
#define INVALID UINT32_C(0x80960005)
#define NO_EVENT UINT32_C(0x80960007)
#define NOT_LOGGED_IN UINT32_C(0x80960009)
#define SHORT_BUFFER UINT32_C(0x8096000a)
static uint32_t call0(int which) { return (uint32_t)bbcpu_call(entries[which], 0, NULL); }
static uint32_t call1(int which, void *pointer) {
    const uint64_t args[] = {(uintptr_t)pointer};
    return (uint32_t)bbcpu_call(entries[which], 1, args);
}
static uint32_t name_call(int32_t id, char *out, uint64_t size) {
    const uint64_t args[] = {(uint64_t)(int64_t)id, (uintptr_t)out, size};
    return (uint32_t)bbcpu_call(entries[NAME], 3, args);
}
typedef struct {
    uint64_t head;
    int32_t initial;
    int32_t ids[4];
    uint64_t middle;
    int32_t event[2];
    char name[32];
    uint64_t tail;
} Outputs;
static void uninitialized(void) {
    Outputs out, original; memset(&out, 0xa5, sizeof(out)); memcpy(&original, &out, sizeof(out));
    assert(call0(TERM) == NOT_INIT);
    assert(call1(INITIAL, &out.initial) == NOT_INIT && call1(INITIAL, NULL) == NOT_INIT);
    assert(call1(LIST, out.ids) == NOT_INIT && call1(LIST, NULL) == NOT_INIT);
    assert(name_call(1, out.name, sizeof(out.name)) == NOT_INIT);
    assert(call1(EVENT, out.event) == NOT_INIT && call1(EVENT, NULL) == NOT_INIT);
    assert(!memcmp(&out, &original, sizeof(out)));
}
static void check_queries(void) {
    Outputs out, expected; memset(&out, 0xa5, sizeof(out)); memcpy(&expected, &out, sizeof(out));
    assert(call1(INITIAL, &out.initial) == 0); expected.initial = 1;
    assert(!memcmp(&out, &expected, sizeof(out)));
    assert(call1(LIST, out.ids) == 0);
    expected.ids[0] = 1; expected.ids[1] = expected.ids[2] = expected.ids[3] = -1;
    assert(!memcmp(&out, &expected, sizeof(out)));
    assert(call1(INITIAL, NULL) == INVALID && call1(LIST, NULL) == INVALID);
    assert(name_call(-1, out.name, sizeof(out.name)) == INVALID);
    assert(name_call(0, out.name, sizeof(out.name)) == NOT_LOGGED_IN);
    assert(name_call(2, out.name, sizeof(out.name)) == NOT_LOGGED_IN);
    assert(name_call(0xff, out.name, sizeof(out.name)) == NOT_LOGGED_IN);
    assert(name_call(1, NULL, sizeof(out.name)) == INVALID);
    assert(name_call(1, out.name, 0) == SHORT_BUFFER);
    assert(name_call(1, out.name, 6) == SHORT_BUFFER);
    assert(!memcmp(&out, &expected, sizeof(out)));
    assert(name_call(1, out.name, 7) == 0); memcpy(expected.name, "Wander", 7);
    assert(!memcmp(&out, &expected, sizeof(out)));
    memset(out.name, 0xa5, sizeof(out.name));
    /* The size is 64-bit; success writes only the name plus its terminator. */
    assert(name_call(1, out.name, UINT64_C(1) << 32) == 0);
    assert(!memcmp(&out, &expected, sizeof(out)));
}
static void check_events(void) {
    Outputs out, expected; memset(&out, 0xa5, sizeof(out)); memcpy(&expected, &out, sizeof(out));
    assert(call1(EVENT, NULL) == INVALID); /* Must not consume the pending login. */
    assert(call1(EVENT, out.event) == 0); expected.event[0] = 0; expected.event[1] = 1;
    assert(!memcmp(&out, &expected, sizeof(out)));
    memset(&out, 0xa5, sizeof(out)); memcpy(&expected, &out, sizeof(out));
    assert(call1(EVENT, out.event) == NO_EVENT && !memcmp(&out, &expected, sizeof(out)));
    assert(call1(INIT, NULL) == ALREADY_INIT); /* Must not requeue a login. */
    assert(call1(EVENT, out.event) == NO_EVENT && !memcmp(&out, &expected, sizeof(out)));
}
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    unsigned ready;
    int go;
} Gate;
typedef struct { Gate *gate; unsigned index; int initialize; uint32_t result; } Worker;
static void *compete(void *opaque) {
    Worker *w = opaque;
    assert(pthread_mutex_lock(&w->gate->lock) == 0);
    ++w->gate->ready; assert(pthread_cond_broadcast(&w->gate->changed) == 0);
    while (!w->gate->go) assert(pthread_cond_wait(&w->gate->changed, &w->gate->lock) == 0);
    assert(pthread_mutex_unlock(&w->gate->lock) == 0);
    *runtime_errno() = 101 + (int32_t)w->index;
    if (w->initialize) w->result = call1(INIT, NULL);
    else {
        Outputs out, expected; memset(&out, 0xa5, sizeof(out)); memcpy(&expected, &out, sizeof(out));
        w->result = call1(EVENT, out.event);
        if (!w->result) { expected.event[0] = 0; expected.event[1] = 1; }
        assert(!memcmp(&out, &expected, sizeof(out)));
        for (unsigned i = 0; i < 100; ++i) check_queries();
    }
    assert(*runtime_errno() == 101 + (int32_t)w->index);
    return NULL;
}
static void concurrent_round(int initialize) {
    Gate gate = {.lock = PTHREAD_MUTEX_INITIALIZER, .changed = PTHREAD_COND_INITIALIZER};
    Worker workers[8]; pthread_t threads[8];
    for (unsigned i = 0; i < 8; ++i) {
        workers[i] = (Worker){.gate = &gate, .index = i, .initialize = initialize};
        assert(pthread_create(&threads[i], NULL, compete, &workers[i]) == 0);
    }
    assert(pthread_mutex_lock(&gate.lock) == 0);
    while (gate.ready != 8) assert(pthread_cond_wait(&gate.changed, &gate.lock) == 0);
    gate.go = 1; assert(pthread_cond_broadcast(&gate.changed) == 0);
    assert(pthread_mutex_unlock(&gate.lock) == 0);
    unsigned successes = 0;
    for (unsigned i = 0; i < 8; ++i) {
        assert(pthread_join(threads[i], NULL) == 0);
        if (!workers[i].result) ++successes;
        else assert(workers[i].result == (initialize ? ALREADY_INIT : NO_EVENT));
    }
    assert(successes == 1);
    assert(pthread_cond_destroy(&gate.changed) == 0 && pthread_mutex_destroy(&gate.lock) == 0);
}
int main(void) {
    alarm(20);
    bootstrap_initialize(0);
    for (int i = 0; i < OPERATIONS; ++i) assert(!bootstrap_resolve(names[i], 0));
    bootstrap_initialize(1);
    unsigned char *code = runtime_low_map(16384, PROT_READ | PROT_WRITE); assert(code);
    for (int i = 0; i < OPERATIONS; ++i) {
        const uintptr_t fn = bootstrap_resolve(names[i], 0);
        assert(fn && !bootstrap_resolve(names[i], 1));
        char wrong[64]; snprintf(wrong, sizeof(wrong), "%.11s#D#E", names[i]);
        assert(!bootstrap_resolve(wrong, 0));
        snprintf(wrong, sizeof(wrong), "%.11s#I#F", names[i]);
        assert(!bootstrap_resolve(wrong, 0));
        unsigned char *p = code + i * 16;
        p[0] = 0x48; p[1] = 0xb8; memcpy(p + 2, &fn, 8); p[10] = 0xff; p[11] = 0xe0;
        entries[i] = (uintptr_t)p;
    }
    bbcpu_add_guest_code((uintptr_t)code, OPERATIONS * 16);
    /* General login/logout, account and storage operations are unavailable. */
    assert(!bootstrap_resolve("unknown#I#J", 0));
    *runtime_errno() = 17; uninitialized();
    const size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    unsigned char *params = mmap(NULL, page_size * 2, PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANON, -1, 0);
    assert(params != MAP_FAILED && mprotect(params + page_size, page_size, PROT_NONE) == 0);
    uint32_t *last_field = (uint32_t *)(params + page_size - sizeof(uint32_t));
    const uint32_t priorities[] = {0, 0xff, 0x300, UINT32_MAX, 0x100, 0x2ff};
    for (unsigned i = 0; i < sizeof(priorities) / sizeof(*priorities); ++i) {
        *last_field = priorities[i]; assert(mprotect(params, page_size, PROT_READ) == 0);
        assert(call1(INIT, last_field) == (i < 4 ? INVALID : 0));
        assert(*last_field == priorities[i]);
        assert(mprotect(params, page_size, PROT_READ | PROT_WRITE) == 0);
        if (i >= 4) {
            check_queries(); check_events();
            *last_field = 0x300; assert(call1(INIT, last_field) == INVALID);
            assert(call0(TERM) == 0); uninitialized();
        } else uninitialized();
    }
    assert(munmap(params, page_size * 2) == 0);
    assert(call1(INIT, NULL) == 0); check_queries(); check_events();
    assert(call0(TERM) == 0); uninitialized();
    concurrent_round(1); /* One initializer owns the transition. */
    concurrent_round(0); /* Exactly one guest caller consumes the login event. */
    assert(call0(TERM) == 0); uninitialized();
    assert(*runtime_errno() == 17);
    alarm(0);
    puts("PASS: local UserService guest ABI, guarded 32-bit init parameters, lifecycle/priority errors, user/list/name layouts, 64-bit size, one-shot login events, concurrent initialization/queries/consumption and errno preservation");
    return 0;
}
