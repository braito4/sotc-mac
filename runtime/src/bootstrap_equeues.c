/* Native user-event queues. ABI and user-event behavior follow shadPS4's
 * kernel/equeue.{cpp,h}; independent pthread registry and lifetime management.
 * GPL-2.0-or-later. Foundations: bloodborne-native, bbport and shadPS4.
 * No graphics, timer, descriptor or POSIX-kevent filters are synthesized.
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
typedef struct {
    uint64_t ident;
    int16_t filter;
    uint16_t flags;
    uint32_t fflags;
    uint64_t data;
    void *udata;
} Event;
_Static_assert(sizeof(Event) == 32 && offsetof(Event, udata) == 24, "guest event layout");
typedef struct Registration {
    Event event;
    int ready;
    struct Registration *next;
} Registration;
typedef struct Queue {
    uintptr_t handle;
    pthread_cond_t changed;
    unsigned active;
    int deleted;
    Registration *events;
    struct Queue *next;
} Queue;
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static Queue *registry;
/* Separate opaque namespace; neither host pointers nor host file descriptors. */
static uintptr_t next_handle = UINT64_C(0x200000000);
static size_t objects, created, deleted, waits, triggers, timeouts;
static void check(int error) {
    if (error) { fprintf(stderr, "STOP: equeue host synchronization error %d\n", error); _Exit(23); }
}
static Queue *find(uintptr_t handle) {
    for (Queue *q = registry; q; q = q->next) if (q->handle == handle) return q;
    return NULL;
}
static void release(Queue *q) {
    while (q->events) { Registration *r = q->events; q->events = r->next; free(r); }
    check(pthread_cond_destroy(&q->changed)); --objects; free(q);
}
static ABI int32_t create_queue(uintptr_t *out, const char *name) {
    if (!out || !name) return ERR(22);
    /* Match the reference implementation's accepted length (32 name bytes). */
    if (strnlen(name, 33) > 32) return ERR(63);
    Queue *q = calloc(1, sizeof(*q));
    if (!q) return ERR(12);
    check(bb_cond_init_monotonic(&q->changed));
    check(pthread_mutex_lock(&gate));
    if (next_handle == (UINT64_C(1) << 40)) {
        check(pthread_mutex_unlock(&gate)); check(pthread_cond_destroy(&q->changed)); free(q); return ERR(28);
    }
    q->handle = next_handle++; q->next = registry; registry = q;
    *out = q->handle; ++objects; ++created;
    check(pthread_mutex_unlock(&gate)); return 0;
}
static ABI int32_t delete_queue(uintptr_t handle) {
    check(pthread_mutex_lock(&gate));
    Queue **p = &registry;
    while (*p && (*p)->handle != handle) p = &(*p)->next;
    if (!*p) { check(pthread_mutex_unlock(&gate)); return ERR(9); }
    Queue *q = *p; *p = q->next; q->deleted = 1; ++deleted;
    /* Local deletion policy: wake in-flight waits with EBADF; reclaim last. */
    check(pthread_cond_broadcast(&q->changed));
    if (!q->active) release(q);
    check(pthread_mutex_unlock(&gate)); return 0;
}
static int collect(Queue *q, Event *events, int capacity) {
    int n = 0;
    for (Registration *r = q->events; r && n < capacity; r = r->next) {
        if (!r->ready) continue;
        events[n++] = r->event;
        if (r->event.flags & 0x20) r->ready = 0;
    }
    return n;
}
static ABI int32_t wait_queue(uintptr_t handle, Event *events, int32_t capacity,
                              int32_t *out, const uint32_t *timeout) {
    check(pthread_mutex_lock(&gate));
    Queue *q = find(handle);
    int32_t error = !q ? ERR(9) : !events || !out ? ERR(14) : 0;
    if (error) { check(pthread_mutex_unlock(&gate)); return error; }
    *out = 0;
    if (capacity < 1) { check(pthread_mutex_unlock(&gate)); return ERR(22); }
    const uint32_t micros = timeout ? *timeout : 0;
    const uint64_t deadline = timeout ? runtime_wait_clock() + (uint64_t)micros * 1000 : 0;
    const struct timespec end = {(time_t)(deadline / 1000000000), (long)(deadline % 1000000000)};
    ++waits;
    while (!(*out = collect(q, events, capacity))) {
        if (timeout && (!micros || runtime_wait_clock() >= deadline)) { error = ERR(60); ++timeouts; break; }
        if (q->active == UINT_MAX) { error = ERR(28); break; }
        ++q->active;
        const int e = timeout ? bb_cond_timedwait_monotonic(&q->changed, &gate, &end)
                              : pthread_cond_wait(&q->changed, &gate);
        --q->active;
        if (e != ETIMEDOUT) check(e);
        if (q->deleted) { error = ERR(9); if (!q->active) release(q); break; }
        /* Recheck readiness even after ETIMEDOUT to resolve a concurrent trigger. */
    }
    check(pthread_mutex_unlock(&gate)); return error;
}
static int32_t add(uintptr_t handle, int32_t id, uint16_t flags) {
    check(pthread_mutex_lock(&gate));
    Queue *q = find(handle);
    if (!q) { check(pthread_mutex_unlock(&gate)); return ERR(9); }
    Registration **p = &q->events;
    const uint64_t ident = (uint64_t)(int64_t)id;
    while (*p && (*p)->event.ident != ident) p = &(*p)->next;
    if (*p) {
        /* Re-registration preserves trigger state and flags, resets user data. */
        (*p)->event.udata = NULL;
    } else {
        Registration *r = calloc(1, sizeof(*r));
        if (!r) { check(pthread_mutex_unlock(&gate)); return ERR(12); }
        r->event.ident = ident; r->event.filter = -11; r->event.flags = flags; *p = r;
    }
    check(pthread_mutex_unlock(&gate)); return 0;
}
static ABI int32_t add_user(uintptr_t q, int32_t id) { return add(q, id, 0); }
static ABI int32_t add_edge(uintptr_t q, int32_t id) { return add(q, id, 0x20); }
static ABI int32_t trigger_user(uintptr_t handle, int32_t id, void *data) {
    check(pthread_mutex_lock(&gate));
    Queue *q = find(handle);
    Registration *r = q ? q->events : NULL;
    while (r && r->event.ident != (uint64_t)(int64_t)id) r = r->next;
    const int32_t error = !q ? ERR(9) : !r ? ERR(2) : 0;
    if (!error) {
        r->ready = 1; r->event.udata = data; ++triggers;
        check(pthread_cond_broadcast(&q->changed));
    }
    check(pthread_mutex_unlock(&gate)); return error;
}
static ABI int32_t delete_user(uintptr_t handle, int32_t id) {
    check(pthread_mutex_lock(&gate));
    Queue *q = find(handle);
    if (!q) { check(pthread_mutex_unlock(&gate)); return ERR(9); }
    Registration **p = &q->events;
    while (*p && (*p)->event.ident != (uint64_t)(int64_t)id) p = &(*p)->next;
    const int32_t error = *p ? 0 : ERR(2);
    if (*p) { Registration *r = *p; *p = r->next; free(r); }
    check(pthread_mutex_unlock(&gate)); return error;
}
static ABI void *event_user_data(const Event *e) { return e->udata; }
static ABI uint64_t event_id(const Event *e) { return e->ident; }
static ABI int32_t event_filter(const Event *e) { return e->filter; }
static ABI uint64_t event_data(const Event *e) { return e->data; }
unsigned bootstrap_equeue_waiters(uintptr_t handle) {
    check(pthread_mutex_lock(&gate)); Queue *q = find(handle); unsigned n = q ? q->active : 0;
    check(pthread_mutex_unlock(&gate)); return n;
}
size_t bootstrap_equeue_objects(void) {
    check(pthread_mutex_lock(&gate)); size_t n = objects;
    check(pthread_mutex_unlock(&gate)); return n;
}
uintptr_t bootstrap_equeues_resolve(const char *name) {
    static const struct { const char *name; void *fn; } table[] = {
        {"D0OdFMjp46I#0#F", create_queue}, {"jpFjmgAC5AE#0#F", delete_queue},
        {"fzyMKs9kim0#0#F", wait_queue}, {"4R6-OvI2cEA#0#F", add_user},
        {"WDszmSbWuDk#0#F", add_edge}, {"F6e0kwo4cnk#0#F", trigger_user},
        {"LJDwdSNTnDg#0#F", delete_user}, {"vz+pg2zdopI#0#F", event_user_data},
        {"mJ7aghmgvfc#0#F", event_id}, {"23CPPI1tyBY#0#F", event_filter},
        {"kwGyyjohI50#0#F", event_data},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(*table); ++i)
        if (!strcmp(name, table[i].name)) return (uintptr_t)table[i].fn;
    return 0;
}
void bootstrap_equeues_report(void) {
    check(pthread_mutex_lock(&gate));
    printf("Runtime: event queues created=%zu, deleted=%zu, waits=%zu, triggers=%zu, timeouts=%zu, objects=%zu\n",
           created, deleted, waits, triggers, timeouts, objects);
    check(pthread_mutex_unlock(&gate));
}
