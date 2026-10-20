/* Native event flags: FIFO/single/multiple waiters, AND/OR and clear modes.
 * Contracts follow AjncSystem/ShadowOfTheColossus-PC runtime_sotc.c
 * 2250f328feafab953aac85e758e7bddb2e80e891 and shadPS4 event_flag.cpp.
 * Registry/deferred deletion follows bloodborne-native runtime_sema.c
 * 3170535f59cadbe5ce9c38e6bb961ed5fd215194. GPL-2.0-or-later.
 * Foundations: deadinside28/bbport, bloodborne-native and shadPS4.
 * Priority queues and named open/close are deliberately not approximated.
 */
#include "bootstrap_contracts.h"
#include "runtime.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ERR(n) ((int32_t)(UINT32_C(0x80020000) | (n)))
enum { AND = 1, OR = 2, CLEAR_ALL = 0x10, CLEAR_PATTERN = 0x20 };
typedef struct Waiter {
    pthread_cond_t changed;
    uint64_t bits, result;
    uint32_t mode;
    int done;
    int32_t error;
    struct Waiter *next;
} Waiter;
typedef struct Flag {
    uintptr_t handle;
    uint64_t bits;
    unsigned active;
    int multi, deleted;
    Waiter *first;
    struct Flag *next;
} Flag;
/* Every registry/state access is locked. Waits release the lock. Monotonic
 * opaque handles are never dereferenced, reused or exposed as host pointers. */
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static Flag *registry;
static uintptr_t next_handle = UINT64_C(0x100000000);
static size_t created, deleted, waited, signaled, canceled, timed_out, objects;
static void check(int error) {
    if (error) { fprintf(stderr, "STOP: event-flag host synchronization error %d\n", error); _Exit(23); }
}
static Flag *find(uintptr_t handle) {
    for (Flag *f = registry; f; f = f->next) if (f->handle == handle) return f;
    return NULL;
}
static void release(Flag *f) { --objects; free(f); }
static int valid_mode(uint32_t mode) {
    const unsigned wait = mode & 15, clear = mode & 0xf0;
    return !(mode & ~UINT32_C(0x33)) && (wait == AND || wait == OR) &&
           (!clear || clear == CLEAR_ALL || clear == CLEAR_PATTERN);
}
static int met(const Flag *f, uint64_t bits, uint32_t mode) {
    return (mode & 15) == AND ? (f->bits & bits) == bits : (f->bits & bits) != 0;
}
static void consume(Flag *f, uint64_t bits, uint32_t mode) {
    if (mode & CLEAR_ALL) f->bits = 0;
    else if (mode & CLEAR_PATTERN) f->bits &= ~bits;
}
static ABI int32_t create_flag(uintptr_t *out, const char *name, uint32_t attr,
                                uint64_t pattern, const void *options) {
    const unsigned queue = attr & 15, threads = attr & 0xf0;
    if (!out || !name || options || (attr & ~UINT32_C(0x33)) || queue > 2 ||
        (threads && threads != 0x10 && threads != 0x20)) return ERR(22);
    if (strnlen(name, 32) == 32) return ERR(63);
    if (queue == 2) return ERR(45); /* No PS4 priority scheduler: ENOTSUP. */
    Flag *f = calloc(1, sizeof(*f));
    if (!f) return ERR(12);
    check(pthread_mutex_lock(&gate));
    if (next_handle == (UINT64_C(1) << 40)) {
        check(pthread_mutex_unlock(&gate)); free(f); return ERR(28);
    }
    f->handle = next_handle++; f->bits = pattern; f->multi = threads == 0x20;
    f->next = registry; registry = f; ++created; ++objects;
    *out = f->handle;
    check(pthread_mutex_unlock(&gate));
    return 0;
}
static void complete_all(Flag *f, int32_t error) {
    while (f->first) {
        Waiter *w = f->first; f->first = w->next;
        w->error = error; w->result = f->bits; w->done = 1;
        check(pthread_cond_signal(&w->changed));
    }
}
static ABI int32_t delete_flag(uintptr_t handle) {
    check(pthread_mutex_lock(&gate));
    Flag **p = &registry;
    while (*p && (*p)->handle != handle) p = &(*p)->next;
    if (!*p) { check(pthread_mutex_unlock(&gate)); return ERR(3); }
    Flag *f = *p; *p = f->next; f->deleted = 1; ++deleted;
    complete_all(f, ERR(13)); /* Blocked waits receive EACCES on deletion. */
    if (!f->active) release(f);
    check(pthread_mutex_unlock(&gate));
    return 0;
}
static ABI int32_t set_flag(uintptr_t handle, uint64_t bits) {
    check(pthread_mutex_lock(&gate));
    Flag *f = find(handle);
    if (!f) { check(pthread_mutex_unlock(&gate)); return ERR(3); }
    f->bits |= bits; ++signaled;
    for (Waiter **p = &f->first; *p;) {
        Waiter *w = *p;
        if (!met(f, w->bits, w->mode)) { p = &w->next; continue; }
        *p = w->next; w->result = f->bits;
        consume(f, w->bits, w->mode); w->done = 1;
        check(pthread_cond_signal(&w->changed));
    }
    check(pthread_mutex_unlock(&gate));
    return 0;
}
static ABI int32_t clear_flag(uintptr_t handle, uint64_t mask) {
    check(pthread_mutex_lock(&gate));
    Flag *f = find(handle);
    if (f) f->bits &= mask; /* The API takes a keep-mask, not bits to remove. */
    const int32_t result = f ? 0 : ERR(3);
    check(pthread_mutex_unlock(&gate));
    return result;
}
static ABI int32_t cancel_flag(uintptr_t handle, uint64_t pattern, int32_t *count) {
    check(pthread_mutex_lock(&gate));
    Flag *f = find(handle);
    if (!f) { check(pthread_mutex_unlock(&gate)); return ERR(3); }
    int32_t n = 0;
    for (Waiter *w = f->first; w; w = w->next) ++n;
    f->bits = pattern; complete_all(f, ERR(85)); ++canceled;
    if (count) *count = n;
    check(pthread_mutex_unlock(&gate));
    return 0;
}
static int32_t wait_common(uintptr_t handle, uint64_t bits, uint32_t mode,
                            uint64_t *result, uint32_t *timeout, int block) {
    check(pthread_mutex_lock(&gate));
    Flag *f = find(handle);
    int32_t error = 0;
    if (!f) error = ERR(3);
    else if (!bits || !valid_mode(mode)) error = ERR(22);
    else if (!f->multi && f->active) error = ERR(1);
    if (error) { check(pthread_mutex_unlock(&gate)); return error; }
    if (met(f, bits, mode)) {
        if (result) *result = f->bits;
        consume(f, bits, mode);
        check(pthread_mutex_unlock(&gate)); return 0;
    }
    if (!block || (timeout && !*timeout)) {
        if (result) *result = f->bits;
        if (block) ++timed_out;
        check(pthread_mutex_unlock(&gate)); return block ? ERR(60) : ERR(16);
    }
    if (f->active == INT_MAX) { check(pthread_mutex_unlock(&gate)); return ERR(28); }
    Waiter w = {.bits = bits, .mode = mode};
    check(bb_cond_init_monotonic(&w.changed));
    Waiter **tail = &f->first;
    while (*tail) tail = &(*tail)->next;
    *tail = &w; ++f->active; ++waited;
    const uint64_t deadline = timeout ? runtime_wait_clock() + (uint64_t)*timeout * 1000 : 0;
    const struct timespec end = {(time_t)(deadline / 1000000000), (long)(deadline % 1000000000)};
    while (!w.done) {
        int e = timeout ? bb_cond_timedwait_monotonic(&w.changed, &gate, &end)
                        : pthread_cond_wait(&w.changed, &gate);
        if (e == ETIMEDOUT && !w.done) {
            Waiter **p = &f->first;
            while (*p != &w) p = &(*p)->next;
            *p = w.next; w.result = f->bits; w.error = ERR(60); w.done = 1; ++timed_out;
        } else if (e != ETIMEDOUT) check(e);
    }
    if (result) *result = w.result;
    if (timeout) {
        const uint64_t now = runtime_wait_clock();
        *timeout = w.error == ERR(60) || now >= deadline ? 0 : (uint32_t)((deadline - now) / 1000);
    }
    check(pthread_cond_destroy(&w.changed));
    if (!--f->active && f->deleted) release(f);
    check(pthread_mutex_unlock(&gate));
    return w.error;
}
static ABI int32_t wait_flag(uintptr_t f, uint64_t b, uint32_t m, uint64_t *r, uint32_t *t) {
    return wait_common(f, b, m, r, t, 1);
}
static ABI int32_t poll_flag(uintptr_t f, uint64_t b, uint32_t m, uint64_t *r) {
    return wait_common(f, b, m, r, NULL, 0);
}
/* Host-only observability for deterministic contention/lifetime tests. */
unsigned bootstrap_event_flag_waiters(uintptr_t handle) {
    check(pthread_mutex_lock(&gate));
    Flag *f = find(handle); unsigned n = 0;
    if (f) for (Waiter *w = f->first; w; w = w->next) ++n;
    check(pthread_mutex_unlock(&gate)); return n;
}
size_t bootstrap_event_flag_objects(void) {
    check(pthread_mutex_lock(&gate)); const size_t n = objects;
    check(pthread_mutex_unlock(&gate)); return n;
}
uintptr_t bootstrap_event_flags_resolve(const char *name) {
    static const struct { const char *name; void *fn; } table[] = {
        {"BpFoboUJoZU#0#F", create_flag}, {"8mql9OcQnd4#0#F", delete_flag},
        {"IOnSvHzqu6A#0#F", set_flag}, {"7uhBFWRAS60#0#F", clear_flag},
        {"PZku4ZrXJqg#0#F", cancel_flag}, {"JTvBflhYazQ#0#F", wait_flag},
        {"9lvj5DjHZiA#0#F", poll_flag},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(*table); ++i)
        if (!strcmp(name, table[i].name)) return (uintptr_t)table[i].fn;
    return 0;
}
void bootstrap_event_flags_report(void) {
    check(pthread_mutex_lock(&gate));
    printf("Runtime: event flags created=%zu, deleted=%zu, waits=%zu, sets=%zu, cancels=%zu, timeouts=%zu, objects=%zu\n",
           created, deleted, waited, signaled, canceled, timed_out, objects);
    check(pthread_mutex_unlock(&gate));
}
